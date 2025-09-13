# llm.py
import requests
from config import GEMINI_API_KEY, GEMINI_MODEL, GEMINI_MAX_TOKENS, GEMINI_TEMPERATURE

HTTP = requests.Session()
HTTP.headers.update({"Connection": "keep-alive"})

def gemini_reply(prompt: str, max_tokens: int = GEMINI_MAX_TOKENS) -> str:
    if not GEMINI_API_KEY:
        raise RuntimeError("Missing GEMINI_API_KEY.")

    sys_prefix = "Answer briefly (1–2 sentences)."
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={GEMINI_API_KEY}"
    payload = {
        "contents": [{"role": "user", "parts": [{"text": f"{sys_prefix}\n\n{prompt}"}]}],
        "generationConfig": {
            "maxOutputTokens": int(max_tokens),
            "temperature": GEMINI_TEMPERATURE,
            "responseMimeType": "text/plain"
        }
    }
    r = HTTP.post(url, json=payload, timeout=20)
    r.raise_for_status()
    data = r.json()
    c = (data.get("candidates") or [{}])[0]
    parts = ((c.get("content") or {}).get("parts") or [])
    return "".join(p.get("text", "") for p in parts).strip() or "[Empty Gemini response]"
