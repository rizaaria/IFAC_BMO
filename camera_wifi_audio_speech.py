import socket
import numpy as np
import speech_recognition as sr
import pyttsx3
import threading
import time
import wave
import tempfile
import os

# ================= CONFIG =================
UDP_IP = "0.0.0.0"
AUDIO_PORT = 5005

ESP32_IP = "192.168.137.37"
ESP32_TTS_PORT = 6001

SAMPLE_RATE = 16000
CHUNK_SIZE = 320 * 2  # 640 bytes

NOISE_THRESHOLD = 250
GAIN = 1.5

is_speaking = False

BMO_RESPONSES = {
    "halo": ("Halooo! BMO senang bertemu kamu!", "HAPPY"),
    "hai": ("Haiii! Kamu apa kabar?", "HAPPY"),
    "siapa kamu": ("BMO adalah robot kecil yang lucu!", "HAPPY"),
    "sedih": ("Jangan sedih ya, BMO di sini!", "SAD"),
    "marah": ("Ups, jangan marah ya!", "SURPRISED"),
    "terima kasih": ("Sama-sama! BMO senang membantu!", "LOVE"),
}

# ================= SOCKET =================
sock_rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_rx.bind((UDP_IP, AUDIO_PORT))

sock_tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ================= SPEECH =================
recognizer = sr.Recognizer()
buffer = np.array([], dtype=np.int16)

# ================= TTS =================
def resample_audio(audio, orig_sr, target_sr=16000):
    if orig_sr == target_sr:
        return audio

    num_samples = int(len(audio) * target_sr / orig_sr)

    return np.interp(
        np.linspace(0, len(audio), num_samples),
        np.arange(len(audio)),
        audio.astype(np.float32)
    ).astype(np.int16)


def generate_tts(text):
    try:
        tmp = os.path.join(tempfile.gettempdir(), "tts.wav")

        engine = pyttsx3.init()
        engine.setProperty('rate', 170)
        engine.save_to_file(text, tmp)
        engine.runAndWait()

        with wave.open(tmp, 'rb') as wf:
            sr = wf.getframerate()
            ch = wf.getnchannels()
            raw = wf.readframes(wf.getnframes())

        audio = np.frombuffer(raw, dtype=np.int16)

        if ch == 2:
            audio = audio[::2]

        audio = resample_audio(audio, sr, 16000)

        # kecilin biar ga pecah
        audio = (audio.astype(np.float32) * 0.6).astype(np.int16)

        os.remove(tmp)

        return audio.tobytes()

    except Exception as e:
        print("TTS Error:", e)
        return None


def send_tts(text):
    global is_speaking

    is_speaking = True

    data = generate_tts(text)
    if data:
        for i in range(0, len(data), 1400):
            sock_tx.sendto(data[i:i+1400], (ESP32_IP, ESP32_TTS_PORT))
            time.sleep(0.04)

    time.sleep(0.3)  # kasih jeda biar speaker selesai
    is_speaking = False

def find_response(text):
    for key in BMO_RESPONSES:
        if key in text:
            return BMO_RESPONSES[key]
    return ("BMO mendengar kamu!", "HAPPY")


# ================= MAIN LOOP =================
print("Audio system running...")

while True:
    data, _ = sock_rx.recvfrom(2048)

    if is_speaking:
        continue

    audio = np.frombuffer(data, dtype=np.int16).copy()

    # 🔥 Noise gate
    audio[np.abs(audio) < NOISE_THRESHOLD] = 0

    # 🔥 Gain kecil (utama di ESP32)
    audio = audio.astype(np.float32) * GAIN

    # 🔥 Hilangin DC offset
    audio = audio - np.mean(audio)

    # 🔥 Soft limiter
    audio = np.tanh(audio / 30000.0) * 30000

    audio = audio.astype(np.int16)

    # ===== KIRIM BALIK KE ESP32 =====
    # sock_tx.sendto(audio.tobytes(), (ESP32_IP, ESP32_TTS_PORT))

    # ===== BUFFER UNTUK RECOGNITION =====
    buffer = np.concatenate((buffer, audio))

    if len(buffer) > SAMPLE_RATE * 1.2:
        chunk = buffer[:SAMPLE_RATE]
        buffer = buffer[SAMPLE_RATE:]

        audio_data = sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2)

        try:
            text = recognizer.recognize_google(audio_data, language="id-ID")
            print("[USER]:", text)

            # 🔥 Response sederhana
            response, expr = find_response(text.lower())
            print("[BMO]:", response)

            threading.Thread(target=send_tts, args=(response,), daemon=True).start()

        except:
            print("[...] tidak dikenali")
