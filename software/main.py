# main.py
import os, sys, json, asyncio, queue, threading, ctypes, datetime, time, contextlib
import sounddevice as sd

from config import (
    OUTPUT_FILE, OUTPUT_DIR, VERBOSE, CHUNK_MS, TARGET_SR, CHANNELS, DG_API_KEY,
)
from stt import (
    choose_device, Chunker, deepgram_url, ws_connect,
    producer, consumer, wait_for_finals
)
from llm import gemini_reply
from tts import tts_streaming_ws


# ============================================================
# Single-key "hold-to-talk" control (SPACE only), Windows focus
# ============================================================

# Robust GetAsyncKeyState setup (prevents flakiness with default ctypes)
_user32 = ctypes.WinDLL("user32", use_last_error=True)
_user32.GetAsyncKeyState.argtypes = (ctypes.c_int,)
_user32.GetAsyncKeyState.restype  = ctypes.c_short

VK_SPACE = 0x20  # space

def _space_down() -> bool:
    # High-order bit set means currently down
    return (_user32.GetAsyncKeyState(VK_SPACE) & 0x8000) != 0


# =======================
# Shared run-time state
# =======================
AUDIO_Q: "queue.Queue[bytes]" = queue.Queue()

# Mirror the original single-file semantics exactly:
SENDING_AUDIO = threading.Event()
COLLECTING    = threading.Event()
SHOULD_STOP   = threading.Event()

SESSION_BUFFER: list[str] = []
SESSION_LOCK = threading.Lock()   # protects SESSION_BUFFER for keyloop/producer
ASYNC_LOCK   = asyncio.Lock()     # used by async consumer when appending lines

FLUSH_REQUEST: "asyncio.Event|None" = None

JOB_Q: "asyncio.Queue[tuple[str, datetime.datetime]]" = asyncio.Queue()
N_WORKERS = int(os.getenv("N_WORKERS", "1"))

# Control/notifications from worker → keyloop
CTRL_Q: "asyncio.Queue[str]" = asyncio.Queue()   # values: "tts_done"

def _append(path: str, text: str):
    with open(path, "a", encoding="utf-8") as f:
        f.write(text)


async def finalize_session():
    """
    Stop sending, flush finals quickly, snapshot buffer, enqueue job; return immediately.
    Leaves the app in idle, ready for the next SPACE press (after TTS done).
    """
    SENDING_AUDIO.clear()
    if FLUSH_REQUEST:
        FLUSH_REQUEST.set()
    await wait_for_finals()

    with SESSION_LOCK:
        if not SESSION_BUFFER:
            return
        lines = SESSION_BUFFER.copy()
        SESSION_BUFFER.clear()

    user_text = "\n".join(l for l in lines if l.strip())
    if user_text.strip():
        await JOB_Q.put((user_text, datetime.datetime.now()))


# =======================
# Spacebar-only keyloop with robust reset-after-TTS
# =======================
async def keyloop():
    """
    ONLY spacebar has functionality:
      - HOLD SPACE     -> start recording (after a tiny debounce)
      - RELEASE SPACE  -> finalize & send (Gemini → TTS)
    After TTS finishes, we *require* seeing space released before allowing a new start.
    """
    was_down = False
    press_ts_ms = 0.0

    PRESS_DEBOUNCE_MS   = 50.0
    RELEASE_DEBOUNCE_MS = 30.0
    POLL_INTERVAL_S     = 0.01  # 10 ms

    waiting_tts_done = False
    waiting_space_up_after_tts = False
    space_up_confirm_ms = 0.0
    SPACE_UP_CONFIRM_WINDOW_MS = 60.0

    if VERBOSE:
        print("[ready]")

    while not SHOULD_STOP.is_set():
        # Non-blocking: drain CTRL messages first
        try:
            while True:
                msg = CTRL_Q.get_nowait()
                if msg == "tts_done":
                    waiting_tts_done = False
                    # Require fresh space-UP before next arm
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

        # Edge: SPACE just pressed
        if now_down and not was_down:
            press_ts_ms = time.monotonic() * 1000.0

        # Start recording
        if (not waiting_tts_done) and (not waiting_space_up_after_tts) and now_down and not COLLECTING.is_set():
            if (time.monotonic() * 1000.0 - press_ts_ms) >= PRESS_DEBOUNCE_MS:
                with SESSION_LOCK:
                    SESSION_BUFFER.clear()
                COLLECTING.set()
                SENDING_AUDIO.set()
                if VERBOSE:
                    print("\n[rec] START")

        # Edge: SPACE released → finalize
        if (not now_down) and was_down:
            await asyncio.sleep(RELEASE_DEBOUNCE_MS / 1000.0)
            if COLLECTING.is_set():
                if VERBOSE:
                    print("\n[rec] STOP")
                await finalize_session()
                COLLECTING.clear()
                waiting_tts_done = True  # block new starts until playback completes

        was_down = now_down
        await asyncio.sleep(POLL_INTERVAL_S)


# =======================
# Worker: LLM + TTS
# =======================
async def process_job_worker(worker_id: int):
    """
    Consumes (user_text, ts) jobs; writes file, calls Gemini, then realtime TTS.
    Notifies keyloop with 'tts_done' when audio completes so the UI resets cleanly.
    """
    while not SHOULD_STOP.is_set():
        try:
            user_text, ts = await JOB_Q.get()
        except asyncio.CancelledError:
            break
        try:
            header_user = f"\n--- Session @ {ts:%Y-%m-%d %H:%M:%S} ---\n{user_text}\n"
            await asyncio.to_thread(_append, OUTPUT_FILE, header_user)

            # Gemini
            try:
                reply = await asyncio.to_thread(gemini_reply, user_text)
            except Exception as e:
                print(f"[worker{worker_id}] ❌ Gemini: {e}")
                await CTRL_Q.put("tts_done")
                continue

            # Write reply while TTS happens
            header_reply = f"--- Gemini reply @ {ts:%Y-%m-%d %H:%M:%S} ---\n{reply}\n"
            write_reply = asyncio.create_task(asyncio.to_thread(_append, OUTPUT_FILE, header_reply))

            # Realtime TTS
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
            # Always notify that playback phase is complete so keyloop can re-arm.
            await CTRL_Q.put("tts_done")
            JOB_Q.task_done()


# =======================
# STT connection watchdog (auto-reconnect)
# =======================
async def stt_run_forever():
    """
    Maintains a persistent Deepgram WS connection.
    If it drops (idle timeout, network blip), we reconnect with backoff.
    """
    url  = deepgram_url()
    hdrs = {"Authorization": f"Token {DG_API_KEY}", "Content-Type": "application/octet-stream"}

    backoff_seq = [2, 5, 10]  # seconds
    attempt = 0

    while not SHOULD_STOP.is_set():
        try:
            async with (await ws_connect(url, hdrs)) as ws:
                if VERBOSE:
                    print("[stt] connected")

                # Spawn producer/consumer bound to this ws
                prod = asyncio.create_task(producer(ws, AUDIO_Q, SENDING_AUDIO, FLUSH_REQUEST, SHOULD_STOP))
                cons = asyncio.create_task(consumer(ws, COLLECTING, SESSION_BUFFER, ASYNC_LOCK))

                # Run until either side ends (ws close or error)
                done, pending = await asyncio.wait(
                    {prod, cons},
                    return_when=asyncio.FIRST_COMPLETED
                )

                # If we get here, connection ended – cancel the other task
                for t in pending:
                    t.cancel()
                    with contextlib.suppress(asyncio.CancelledError):
                        await t

                # Drain & report errors if any
                for t in done:
                    exc = t.exception()
                    if exc and VERBOSE:
                        print(f"[stt] task ended with error: {exc!r}")

        except Exception as e:
            if VERBOSE:
                print(f"[stt] connect error: {e!r}")

        if SHOULD_STOP.is_set():
            break

        # Backoff before reconnect
        delay = backoff_seq[min(attempt, len(backoff_seq) - 1)]
        if VERBOSE:
            print(f"[stt] reconnecting in {delay}s …")
        await asyncio.sleep(delay)
        attempt += 1


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

    # Persistent Chunker; it checks SENDING_AUDIO internally (like your single-file)
    chunker = Chunker(src, AUDIO_Q, sending_event=SENDING_AUDIO)

    def audio_cb(indata, frames, time_info, status):
        if status and VERBOSE:
            print(f"[audio]{status}", file=sys.stderr)
        try:
            # Copy because sounddevice may reuse the buffer
            chunker.process(indata.copy())
        except Exception as e:
            if VERBOSE:
                print(f"[audio] err {e}", file=sys.stderr)

    stream = sd.InputStream(
        device=di, channels=CHANNELS, samplerate=src, dtype="float32",
        blocksize=int(src * (CHUNK_MS / 1000.0)), callback=audio_cb, latency="low"
    )

    FLUSH_REQUEST = asyncio.Event()

    workers = [asyncio.create_task(process_job_worker(i)) for i in range(N_WORKERS)]
    stt_task = asyncio.create_task(stt_run_forever())
    key_task = asyncio.create_task(keyloop())

    with stream:
        # Run until keyloop ends (Ctrl+C) or an unhandled exception bubbles up
        await key_task
        SHOULD_STOP.set()
        SENDING_AUDIO.clear()
        COLLECTING.clear()

        # shut down STT task
        stt_task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await stt_task

    await JOB_Q.join()
    for w in workers:
        if not w.done():
            w.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await w

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        SHOULD_STOP.set()
        print("\n[info] Interrupted")
