# stt.py
import sys, json, asyncio, queue, ctypes
import numpy as np, sounddevice as sd, websockets
from typing import Tuple, Optional, List
from config import (
    DG_API_KEY, LANGUAGE, DG_MODEL, TARGET_SR, CHANNELS, CHUNK_MS,
    PUNCTUATE, SMART_FORMAT, WS_PING_INTERVAL, WS_PING_TIMEOUT, WS_MAX_SIZE,
    SHOW_PARTIALS, VERBOSE, FLUSH_IDLE_MS, FLUSH_HARD_TIMEOUT
)

# ===== Shared final-timing for flush =====
LAST_FINAL_TS = 0.0

def choose_device() -> Tuple[int, dict]:
    devs = sd.query_devices()
    d = sd.default.device[0] if sd.default.device else None
    i = d if (d is not None and d >= 0 and devs[d]["max_input_channels"] > 0) else next(
        (i for i, x in enumerate(devs) if x["max_input_channels"] > 0), None
    )
    if i is None:
        raise RuntimeError("No mic found.")
    return i, devs[i]

def resample_mono(x: np.ndarray, src: int, dst: int) -> np.ndarray:
    if src == dst:
        return x.astype(np.float32, copy=False)
    n = len(x)
    dur = n / src
    t = np.linspace(0, dur, n, endpoint=False)
    T = np.linspace(0, dur, max(1, int(round(dur * dst))), endpoint=False)
    return np.interp(T, t, x.astype(np.float64)).astype(np.float32)

class Chunker:
    def __init__(self, sr: int, out_q: "queue.Queue[bytes]", sending_event=None):
        self.sr = sr
        self.out_q = out_q
        self.sending_event = sending_event  # threading.Event (mirrors your single-file)

    def process(self, indata):
        # mirror your working code: only push audio when SENDING_AUDIO is set
        if self.sending_event is not None and not self.sending_event.is_set():
            return
        mono = indata[:, 0] if indata.ndim > 1 else indata
        y = resample_mono(mono, self.sr, TARGET_SR)
        pcm16 = (np.clip(y, -1, 1) * 32767.0).astype(np.int16).tobytes()
        self.out_q.put(pcm16)

def deepgram_url() -> str:
    p = dict(
        model=DG_MODEL, language=LANGUAGE,
        punctuate=str(PUNCTUATE).lower(), smart_format=str(SMART_FORMAT).lower(),
        encoding="linear16", sample_rate=str(TARGET_SR), channels=str(CHANNELS),
        interim_results="true"
    )
    return "wss://api.deepgram.com/v1/listen?" + "&".join(f"{k}={v}" for k, v in p.items())

async def ws_connect(url, headers, **kw):
    kw.setdefault("ping_interval", WS_PING_INTERVAL)
    kw.setdefault("ping_timeout", WS_PING_TIMEOUT)
    kw.setdefault("max_size", WS_MAX_SIZE)
    try:
        return await websockets.connect(url, additional_headers=headers, **kw)
    except TypeError:
        return await websockets.connect(url, additional_headers=headers, **kw)

def parse_result(m: dict) -> Tuple[Optional[str], bool]:
    if m.get("type") != "Results":
        return None, False
    for root, fin in (
        (m, m.get("is_final")),
        ((m.get("results") or {}), (m.get("results") or {}).get("is_final")),
    ):
        ch = root.get("channel") if isinstance(root.get("channel"), dict) else None
        if ch and ch.get("alternatives"):
            return (ch["alternatives"][0].get("transcript") or "").strip(), bool(fin)
        chs = root.get("channels") or []
        if isinstance(chs, list) and chs and chs[0].get("alternatives"):
            return (chs[0]["alternatives"][0].get("transcript") or "").strip(), bool(fin)
    return None, False

async def producer(ws, audio_q: "queue.Queue[bytes]", sending_evt, flush_evt, should_stop):
    step = int(TARGET_SR * (CHUNK_MS / 1000.0)) * 2
    while not should_stop.is_set():
        if flush_evt and flush_evt.is_set():
            try:
                await ws.send(json.dumps({"type": "Flush"}))
            except Exception:
                pass
            flush_evt.clear()

        try:
            buf = audio_q.get(timeout=0.05)
        except queue.Empty:
            await asyncio.sleep(0.005)
            continue

        if not sending_evt.is_set():
            continue

        for i in range(0, len(buf), step):
            if not sending_evt.is_set():
                break
            await ws.send(buf[i:i + step])
            await asyncio.sleep((CHUNK_MS / 1000.0) * 0.8)

    # Graceful close
    try:
        await ws.send(json.dumps({"type": "CloseStream"}))
    except Exception:
        pass

async def consumer(ws, collecting_evt, session_buffer: List[str], lock: "asyncio.Lock"):
    global LAST_FINAL_TS
    partial = ""

    def clear_partial():
        nonlocal partial
        if partial:
            print("\r" + " " * len(partial), end="\r", flush=True)
            partial = ""

    try:
        async for raw in ws:
            try:
                d = json.loads(raw)
            except Exception:
                continue

            t, fin = parse_result(d)
            if not t:
                continue

            if fin:
                if SHOW_PARTIALS:
                    clear_partial()
                print(t)
                LAST_FINAL_TS = asyncio.get_running_loop().time()
                if collecting_evt.is_set():
                    async with lock:
                        session_buffer.append(t)
            elif SHOW_PARTIALS and collecting_evt.is_set():
                partial = (t[:120] + ("…" if len(t) > 120 else ""))
                print("\r" + partial, end="", flush=True)
    finally:
        if SHOW_PARTIALS:
            clear_partial()

async def wait_for_finals(idle_ms: int = FLUSH_IDLE_MS, hard_ms: int = FLUSH_HARD_TIMEOUT):
    loop = asyncio.get_running_loop()
    t0 = loop.time()
    last_seen = LAST_FINAL_TS
    while True:
        await asyncio.sleep(0.02)
        now = loop.time()
        if LAST_FINAL_TS != last_seen:
            last_seen = LAST_FINAL_TS
            t0 = now
        if (now - t0) * 1000.0 >= idle_ms:
            break
        if (now - (t0 - (idle_ms / 1000.0))) * 1000.0 >= hard_ms:
            break
