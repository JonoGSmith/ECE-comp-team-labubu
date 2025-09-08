import os
import sounddevice as sd 
import numpy as np
from elevenlabs import ElevenLabs
import requests
import tempfile
import wave
import time
import keyboard  # pip install keyboard
from pathlib import Path
import re
import threading
import queue
from dotenv import load_dotenv

load_dotenv()
                                                                               
API_KEY = os.getenv("ELEVENLABS_API_KEY")
GOOGLE_API_KEY = os.getenv("GOOGLE_API_KEY")
client = ElevenLabs(api_key=API_KEY)
VOICE_ID = "A9evEp8yGjv4c3WsIKuY"
SAMPLE_RATE = 16000
                                                                                                                                                                                    

# Global variables for continuous recording
audio_queue = queue.Queue()
is_recording = False

def record_continuously():
  """Record audio continuously and put chunks in a queue."""
  global is_recording
  
  def callback(indata, frames, time_info, status):
    if status:
      print(status)
    if is_recording:
      audio_queue.put(indata.copy())
  
  with sd.InputStream(samplerate=SAMPLE_RATE, channels=1, dtype="int16", callback=callback):
    while is_recording:
      sd.sleep(100)  # Sleep to prevent CPU overuse

def save_wav(audio_data):
  temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".wav")
  with wave.open(temp_file.name, 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(2)  # 16-bit
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(audio_data.tobytes())
  return temp_file.name

def transcribe_with_elevenlabs(file_path):
  url = "https://api.elevenlabs.io/v1/speech-to-text"
  headers = {"xi-api-key": API_KEY}
  with open(file_path, "rb") as f:
    files = {"file": f, "model_id": (None, "scribe_v1")}
    print("Sending audio chunk to ElevenLabs...")
    response = requests.post(url, headers=headers, files=files)
    response.raise_for_status()
    return response.json()

def text_to_speech_elevenlabs(text):
  audio_gen = client.text_to_speech.convert(
    text=text,
    voice_id=VOICE_ID,
    model_id="eleven_multilingual_v2",
    output_format="pcm_16000"  
  )

  # Convert generator to bytes if needed
  if hasattr(audio_gen, '__iter__') and not isinstance(audio_gen, (bytes, bytearray)):
    audio_bytes = b''.join(audio_gen)
  else:
    audio_bytes = audio_gen

  # Convert bytes to NumPy array
  audio_array = np.frombuffer(audio_bytes, dtype=np.int16)
  sd.play(audio_array, samplerate=16000)
  sd.wait()
                                                                                                                                                                                                                                                                 
def query_gemini(prompt):
  """Send text to Gemini API and return the response."""
  url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent"
  headers = {
    "Content-Type": "application/json",
    "X-goog-api-key": GOOGLE_API_KEY
  }
  payload = {
    "contents": [
      {
        "parts": [
          {"text": prompt}
        ]
      }
    ]
  }
  response = requests.post(url, headers=headers, json=payload)
  response.raise_for_status()
  data = response.json()
  return data.get("candidates", [{}])[0].get("content", "")

def clean_markdown(text: str) -> str:
  """
  Remove Markdown formatting and extra newlines from a string.
  """
  # Remove bold/italic/strikethrough/inline code symbols
  text = re.sub(r"[*_~`]", "", text)

  # Remove bullets/hyphens at the start of lines
  text = re.sub(r"^-+\s*", "", text, flags=re.MULTILINE)

  # Collapse multiple newlines into a single space
  text = re.sub(r"\n+", " ", text)

  return text.strip()


def push_to_talk(key="space"):
  global is_recording
  
  print(f"Hold '{key}' to record...")
  try:
    while True:
      # Wait until key is pressed
      while not keyboard.is_pressed(key):
        time.sleep(0.01)

      print("Recording started...")
      is_recording = True
      transcripts = []  # collect transcripts during recording
      
      # Start recording thread
      recording_thread = threading.Thread(target=record_continuously)
      recording_thread.start()
      
      # Collect audio while key is pressed
      audio_chunks = []
      start_time = time.time()
      
      while keyboard.is_pressed(key):
        try:
          # Get audio from queue with timeout
          chunk = audio_queue.get(timeout=0.1)
          audio_chunks.append(chunk)
        except queue.Empty:
          continue
      
      timeout_time = time.time() + 1
      while time.time() < timeout_time:
        try:
          chunk = audio_queue.get_nowait()
          audio_chunks.append(chunk)
        except queue.Empty:
          break
      # Key released → stop recording
      is_recording = False
      recording_thread.join()
      print("⏹ Recording stopped.")
      
      # Process all collected audio
      if audio_chunks:
        # Combine all audio chunks
        full_audio = np.concatenate(audio_chunks)
        wav_path = save_wav(full_audio)

        try:
          # Transcribe the full audio
          result = transcribe_with_elevenlabs(wav_path)
          text = result.get("text", "")
          if text.strip():
            print("Full transcript:", text)
            transcripts.append(text)
        except Exception as e:
          print("Error:", e)
        finally:
          os.remove(wav_path)

      full_text = " ".join(transcripts).strip()
      if not full_text:
        continue

      try:
        # Ask Gemini
        short_prompt = f"{full_text}\nPlease respond in 2-3 sentences only, concisely." #TODO CHANGE PROMPT
        gemini_response = query_gemini(short_prompt)

        # Extract text from Gemini response
        gemini_text = ""
        if isinstance(gemini_response, dict):
          if "candidates" in gemini_response:
            gemini_text = gemini_response["candidates"][0].get("content", "")
          elif "parts" in gemini_response:
            for part in gemini_response["parts"]:
              gemini_text += part.get("text", "")
        else:
          gemini_text = str(gemini_response)

        # Clean Markdown
        gemini_text_clean = clean_markdown(gemini_text)

        # # Keep only the first 1–2 sentences
        # sentences = re.split(r'(?<=[.!?]) +', gemini_text_clean)
        # gemini_text_short = ' '.join(sentences[:2])

        print("Gemini:", gemini_text_clean)

        # Convert to speech
        print("Playing Gemini response...")
        text_to_speech_elevenlabs(gemini_text_clean)

      except Exception as e:
        print("Gemini error:", e)

  except KeyboardInterrupt:
    is_recording = False
    print("Stopped manually.")


                                                                                                                                                                                                                                                                                   

if __name__ == "__main__":
  push_to_talk("space")