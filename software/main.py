# main.py
import os, sys, json, asyncio, queue, threading, ctypes, datetime, time, contextlib, platform
import sounddevice as sd

from config import (
    OUTPUT_FILE, OUTPUT_DIR, VERBOSE, CHUNK_MS, TARGET_SR, CHANNELS, DG_API_KEY,
)
from stt import (
    choose_device, Chunker, deepgram_url, ws_connect,
    consumer, wait_for_finals
)
from llm import gemini_reply
from tts import tts_streaming_ws


# ============================================================
# Single-key "hold-to-talk" control (SPACE only), cross-platform
# ============================================================

PYNPUT_LISTENER = None   # only used on macOS/Linux
SPACE_HELD = False       # shared flag for non-Windows

if platform.system() == "Windows":
    # Windows: use GetAsyncKeyState
    _user32 = ctypes.WinDLL("user32", use_last_error=True)
    _user32.GetAsyncKeyState.argtypes = (ctypes.c_int,)
    _user32.GetAsyncKeyState.restype  = ctypes.c_short

    VK_SPACE = 0x20  # space

    def _space_down() -> bool:
        return (_user32.GetAsyncKeyState(VK_SPACE) & 0x8000) != 0

else:
    # macOS / Linux: use pynput global listener
    try:
        from pynput import keyboard
    except Exception as e:
        raise RuntimeError(
            "pynput is required on macOS/Linux for spacebar detection. "
            "Install with: pip install pynput"
        ) from e

    def _on_press(key):
        global SPACE_HELD
        if key == keyboard.Key.space:
            SPACE_HELD = True

    def _on_release(key):
        global SPACE_HELD
        if key == keyboard.Key.space:
            SPACE_HELD = False

    PYNPUT_LISTENER = keyboard.Listener(on_press=_on_press, on_release=_on_release)
    PYNPUT_LISTENER.daemon = True
    PYNPUT_LISTENER.start()

    def _space_down() -> bool:
        return SPACE_HELD


# =======================
# Shared run-time state
# =======================
AUDIO_Q: "queue.Queue[bytes]" = queue.Queue()

SENDING_AUDIO = threading.Event()
COLLECTING    = threading.Event()
SHOULD_STOP   = threading.Event()

SESSION_BUFFER: list[str] = []
SESSION_LOCK = threading.Lock()
ASYNC_LOCK   = asyncio.Lock()

FLUSH_REQUEST: "asyncio.Event|None" = None

JOB_Q: "asyncio.Queue[tuple[str, datetime.datetime]]" = asyncio.Queue()
N_WORKERS = int(os.getenv("N_WORKERS", "1"))

CTRL_Q: "asyncio.Queue[str]" = asyncio.Queue()   # values: "tts_done"


def _append(path: str, text: str):
    with open(path, "a", encoding="utf-8") as f:
        f.write(text)


async def finalize_session():
    """Stop sending, flush finals, snapshot buffer, enqueue job."""
    SENDING_AUDIO.clear()              # stop feeding audio to queue
    if FLUSH_REQUEST:
        FLUSH_REQUEST.set()            # ask STT to finalize
    await wait_for_finals()            # wait a short idle window for last finals

    with SESSION_LOCK:
        if not SESSION_BUFFER:
            return
        lines = SESSION_BUFFER.copy()
        SESSION_BUFFER.clear()

    user_text = "\n".join(l for l in lines if l.strip())
    if user_text.strip():
        await JOB_Q.put((user_text, datetime.datetime.now()))


# =======================
# Catch-up producer (prebuffer then flush fast)
# =======================
async def producer_with_catchup(ws,
                                audio_q: "queue.Queue[bytes]",
                                sending_evt: "threading.Event",
                                flush_evt: "asyncio.Event|None",
                                should_stop: "threading.Event"):
    """
    Same as stt.producer, but supports prebuffer:
      - If there's backlog in audio_q, send frames without pacing until low watermark.
      - When backlog is small, pace normally (like 0.8 * CHUNK_MS).
    """
    step = int(TARGET_SR * (CHUNK_MS / 1000.0)) * 2
    HIGH_WATER = 12   # ~12 * 40ms = ~0.5s buffered (tune as needed)
    LOW_WATER  = 4

    catchup = False
    while not should_stop.is_set():
        if flush_evt and flush_evt.is_set():
            try:
                await ws.send(json.dumps({"type": "Flush"}))
            except Exception:
                pass
            flush_evt.clear()

        try:
            buf = audio_q.get(timeout=0.02)
        except queue.Empty:
            await asyncio.sleep(0.005)
            continue

        if not sending_evt.is_set():
            # Not recording -> discard stale chunk
            continue

        # Evaluate backlog; toggle catch-up mode
        try:
            qsz = audio_q.qsize()
        except Exception:
            qsz = 0

        if catchup:
            if qsz <= LOW_WATER:
                catchup = False
        else:
            if qsz >= HIGH_WATER:
                catchup = True

        for i in range(0, len(buf), step):
            if not sending_evt.is_set():
                break
            await ws.send(buf[i:i+step])
            if not catchup:
                await asyncio.sleep((CHUNK_MS / 1000.0) * 0.8)

    # graceful close (best-effort)
    try:
        await ws.send(json.dumps({"type": "CloseStream"}))
    except Exception:
        pass


# =======================
# Per-session STT lifecycle (open on press, close on release)
# =======================
class STTSession:
    """Holds per-press Deepgram connection + tasks."""
    def __init__(self):
        self.ws = None
        self.prod = None
        self.cons = None

    def active(self) -> bool:
        return self.ws is not None

    async def start(self, url: str, hdrs: dict):
        """Open WS and start producer/consumer tasks."""
        if self.active():
            return
        self.ws = await ws_connect(url, hdrs)
        if VERBOSE:
            print("[stt] connected")
        self.prod = asyncio.create_task(producer_with_catchup(self.ws, AUDIO_Q, SENDING_AUDIO, FLUSH_REQUEST, SHOULD_STOP))
        self.cons = asyncio.create_task(consumer(self.ws, COLLECTING, SESSION_BUFFER, ASYNC_LOCK))

    async def stop(self):
        """Stop tasks and close WS gracefully."""
        if not self.active():
            return
        # Cancel tasks if running
        for t in (self.prod, self.cons):
            if t and not t.done():
                t.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await t
        # Close websocket
        try:
            await self.ws.close()
        except Exception:
            pass
        self.ws = None
        self.prod = None
        self.cons = None
        if VERBOSE:
            print("[stt] disconnected")


# =======================
# Spacebar-only keyloop (prebuffer + connect-on-demand)
# =======================
async def keyloop(url: str, hdrs: dict, stt_session: STTSession):
    was_down = False
    press_ts_ms = 0.0

    PRESS_DEBOUNCE_MS   = 50.0
    RELEASE_DEBOUNCE_MS = 30.0
    POLL_INTERVAL_S     = 0.01

    waiting_tts_done = False
    waiting_space_up_after_tts = False
    space_up_confirm_ms = 0.0
    SPACE_UP_CONFIRM_WINDOW_MS = 60.0

    if VERBOSE:
        print("[ready]")

    while not SHOULD_STOP.is_set():
        # Drain worker notifications
        try:
            while True:
                msg = CTRL_Q.get_nowait()
                if msg == "tts_done":
                    waiting_tts_done = False
                    waiting_space_up_after_tts = True
                    space_up_confirm_ms = 0.0
                    if VERBOSE:
                        print("[ready]")
        except asyncio.QueueEmpty:
            pass

        now_down = _space_down()

        if waiting_space_up_after_tts:
            if not now_down:
                if space_up_confirm_ms == 0.0:
                    space_up_confirm_ms = time.monotonic() * 1000.0
                elif (time.monotonic() * 1000.0 - space_up_confirm_ms) >= SPACE_UP_CONFIRM_WINDOW_MS:
                    waiting_space_up_after_tts = False
            else:
                space_up_confirm_ms = 0.0

        # Edge: SPACE pressed
        if now_down and not was_down:
            press_ts_ms = time.monotonic() * 1000.0

        # Start recording: arm immediately (prebuffer), then connect WS
        if (not waiting_tts_done) and (not waiting_space_up_after_tts) and now_down and not COLLECTING.is_set():
            if (time.monotonic() * 1000.0 - press_ts_ms) >= PRESS_DEBOUNCE_MS:
                # Arm capture so chunks start filling AUDIO_Q right away
                with SESSION_LOCK:
                    SESSION_BUFFER.clear()
                COLLECTING.set()
                SENDING_AUDIO.set()
                if FLUSH_REQUEST and FLUSH_REQUEST.is_set():
                    FLUSH_REQUEST.clear()
                if VERBOSE:
                    print("\n[rec] START")

                # Connect WS concurrently; any prebuffer will be flushed by producer_with_catchup
                try:
                    await stt_session.start(url, hdrs)
                except Exception as e:
                    print(f"[stt] connect error: {e!r}")
                    # Disarm since we can't stream
                    SENDING_AUDIO.clear()
                    COLLECTING.clear()
                    was_down = now_down
                    await asyncio.sleep(POLL_INTERVAL_S)
                    continue

        # Edge: SPACE released → finalize and CLOSE WS
        if (not now_down) and was_down:
            await asyncio.sleep(RELEASE_DEBOUNCE_MS / 1000.0)
            if COLLECTING.is_set():
                if VERBOSE:
                    print("\n[rec] STOP")
                await finalize_session()      # stops capture + flushes finals + enqueues job
                COLLECTING.clear()
                # Close WS and STT tasks (we're done with this press)
                await stt_session.stop()
                waiting_tts_done = True       # block new starts until playback completes

        was_down = now_down
        await asyncio.sleep(POLL_INTERVAL_S)


# =======================
# Worker: LLM + TTS
# =======================
async def process_job_worker(worker_id: int):
    while not SHOULD_STOP.is_set():
        try:
            user_text, ts = await JOB_Q.get()
        except asyncio.CancelledError:
            break
        try:
            header_user = f"\n--- Session @ {ts:%Y-%m-%d %H:%M:%S} ---\n{user_text}\n"
            await asyncio.to_thread(_append, OUTPUT_FILE, header_user)

            try:
                reply = await asyncio.to_thread(gemini_reply, user_text)
            except Exception as e:
                print(f"[worker{worker_id}] ❌ Gemini: {e}")
                await CTRL_Q.put("tts_done")
                continue

            header_reply = f"--- Gemini reply @ {ts:%Y-%m-%d %H:%M:%S} ---\n{reply}\n"
            write_reply = asyncio.create_task(asyncio.to_thread(_append, OUTPUT_FILE, header_reply))

            try:
                saved = await tts_streaming_ws(reply, save_wav=False)
                if saved:
                    print(f"[worker{worker_id}] 🤖 {len(reply)} chars — ✅ TTS saved: {os.path.abspath(saved)}")
                else:
                    print(f"[worker{worker_id}] 🤖 {len(reply)} chars — ✅ TTS streamed")
            except Exception as e:
                print(f"[worker{worker_id}] ❌ TTS (stream): {e}")

            await write_reply
        finally:
            await CTRL_Q.put("tts_done")
            JOB_Q.task_done()


# =======================
# Main
# =======================
async def main():
    global FLUSH_REQUEST
    di, info = choose_device()
    src = int(info.get("default_samplerate") or 48000)
    src = src if src >= 8000 else 16000
    print(f"[audio] Using {di}: {info['name']}")
    print(f"[audio] {src} Hz -> {TARGET_SR} Hz")
    print(f"[file ] {os.path.abspath(OUTPUT_FILE)}")
    print(f"[tts  ] {os.path.abspath(OUTPUT_DIR)}")
    print("[keys ] HOLD SPACE to record; RELEASE to finalize & send. (Ctrl+C to quit)")

    chunker = Chunker(src, AUDIO_Q, sending_event=SENDING_AUDIO)

    def audio_cb(indata, frames, time_info, status):
        if status and VERBOSE:
            print(f"[audio]{status}", file=sys.stderr)
        try:
            chunker.process(indata.copy())
        except Exception as e:
            if VERBOSE:
                print(f"[audio] err {e}", file=sys.stderr)

    stream = sd.InputStream(
        device=di, channels=CHANNELS, samplerate=src, dtype="float32",
        blocksize=int(src * (CHUNK_MS / 1000.0)), callback=audio_cb, latency="low"
    )

    FLUSH_REQUEST = asyncio.Event()

    # Prepare static STT connection params
    url  = deepgram_url()
    hdrs = {"Authorization": f"Token {DG_API_KEY}", "Content-Type": "application/octet-stream"}

    # Per-press STT session holder
    stt_session = STTSession()

    # Workers for LLM+TTS
    workers = [asyncio.create_task(process_job_worker(i)) for i in range(N_WORKERS)]

    # Key loop (opens/closes STT per press, with prebuffer)
    key_task = asyncio.create_task(keyloop(url, hdrs, stt_session))

    with stream:
        await key_task
        SHOULD_STOP.set()
        SENDING_AUDIO.clear()
        COLLECTING.clear()

        # Ensure STT session is closed if still open
        await stt_session.stop()

    await JOB_Q.join()
    for w in workers:
        if not w.done():
            w.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await w

    # Clean up pynput listener if used
    if PYNPUT_LISTENER is not None:
        try:
            PYNPUT_LISTENER.stop()
        except Exception:
            pass


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        SHOULD_STOP.set()
        print("\n[info] Interrupted")
