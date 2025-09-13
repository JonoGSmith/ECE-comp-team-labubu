# config.py
import os

# ===== Deepgram (STT) =====
DG_API_KEY   = os.getenv("DG_API_KEY", "3ae66276babca4572da9fc7a68b11e1e4906c96c")
LANGUAGE     = "en-US"
DG_MODEL     = "nova-3"
TARGET_SR    = 16000
CHANNELS     = 1
CHUNK_MS     = 40
PUNCTUATE    = True
SMART_FORMAT = True

# ===== Output =====
OUTPUT_FILE   = "transcripts.txt"
OUTPUT_DIR    = "tts_outputs"
SHOW_PARTIALS = True
VERBOSE       = False

# ===== WebSocket keepalive =====                      
WS_PING_INTERVAL = 20
WS_PING_TIMEOUT  = None
WS_MAX_SIZE      = 10_000_000

# ===== Flush timings =====
FLUSH_IDLE_MS      = 90
FLUSH_HARD_TIMEOUT = 600

# ===== Keyboard (Windows virtual-key codes) =====
VK_SPACE  = 0x20
VK_RETURN = 0x0D

# ===== Gemini (LLM) =====
GEMINI_API_KEY     = os.getenv("GEMINI_API_KEY", "AIzaSyDe41I7PVDP9la9FCAzTrOJXXVnKn5Qy1M")
GEMINI_MODEL       = os.getenv("GEMINI_MODEL", "gemini-1.5-flash")
GEMINI_MAX_TOKENS  = int(os.getenv("GEMINI_MAX_TOKENS", "16"))
GEMINI_TEMPERATURE = float(os.getenv("GEMINI_TEMPERATURE", "0.2"))

# ===== ElevenLabs (TTS) =====
ELEVEN_API_KEY = os.getenv("ELEVENLABS_API_KEY", "d7e95a76f6267dc47258cb382f0f736d34216286c1d8414e437ca5ab10a0acfc")
VOICE_ID       = os.getenv("ELEVEN_VOICE_ID", "nPczCjzI2devNBz1zQrb")
EL_MODEL_ID    = os.getenv("ELEVEN_MODEL_ID", "eleven_flash_v2_5")
