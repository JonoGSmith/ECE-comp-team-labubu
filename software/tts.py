# tts.py
import os, json, base64, wave, asyncio, datetime
import numpy as np, sounddevice as sd, websockets
from config import (
    ELEVEN_API_KEY, VOICE_ID, EL_MODEL_ID, OUTPUT_DIR,
    WS_PING_INTERVAL, WS_PING_TIMEOUT, WS_MAX_SIZE, VERBOSE
)

async def tts_streaming_ws(text: str, save_wav: bool = False) -> str | None:
    if not text.strip():
        raise ValueError("No text to synthesize.")
    if not ELEVEN_API_KEY:
        raise RuntimeError("Missing ELEVENLABS_API_KEY.")

    ws_url = (
        f"wss://api.elevenlabs.io/v1/text-to-speech/{VOICE_ID}/stream-input"
        f"?model_id={EL_MODEL_ID}&output_format=pcm_16000&auto_mode=true"
    )
    headers = {"xi-api-key": ELEVEN_API_KEY}

    writer = None
    out_path = None
    if save_wav:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        out_path = os.path.join(OUTPUT_DIR, f"session_{datetime.datetime.now():%Y%m%d_%H%M%S}.wav")
        writer = wave.open(out_path, "wb")
        writer.setnchannels(1)
        writer.setsampwidth(2)
        writer.setframerate(16000)

    out_stream = sd.OutputStream(samplerate=16000, channels=1, dtype="int16", latency="low")
    await asyncio.to_thread(out_stream.start)

    try:
        async with websockets.connect(
            ws_url,
            additional_headers=headers,
            ping_interval=WS_PING_INTERVAL,
            ping_timeout=WS_PING_TIMEOUT,
            max_size=WS_MAX_SIZE
        ) as ws:
            await ws.send(json.dumps({
                "text": " ",
                "voice_settings": {
                    "stability": 0.3,
                    "similarity_boost": 0.7,
                    "style": 0.0,
                    "use_speaker_boost": True
                }
            }))
            await ws.send(json.dumps({"text": text, "try_trigger_generation": True}))
            await ws.send(json.dumps({"text": ""}))  # end

            async for raw in ws:
                try:
                    msg = json.loads(raw)
                except Exception:
                    if VERBOSE:
                        print("[tts] Non-JSON frame (ignored)")
                    continue

                if msg.get("isFinal") or msg.get("type") == "finalOutput":
                    if VERBOSE:
                        print("[tts] Final output received")
                    break

                b64_audio = msg.get("audio")
                if not b64_audio:
                    if VERBOSE:
                        keys = ", ".join(k for k in msg.keys() if k != "audio")
                        print(f"[tts] Non-audio msg ({keys})")
                    continue

                try:
                    pcm_bytes = base64.b64decode(b64_audio)
                except Exception as e:
                    if VERBOSE:
                        print(f"[tts] base64 decode error: {e}")
                    continue

                await asyncio.to_thread(out_stream.write, np.frombuffer(pcm_bytes, dtype=np.int16))
                if writer is not None:
                    writer.writeframes(pcm_bytes)

    finally:
        try:
            await asyncio.to_thread(out_stream.stop)
            await asyncio.to_thread(out_stream.close)
        except Exception:
            pass
        if writer is not None:
            writer.close()

    return out_path
