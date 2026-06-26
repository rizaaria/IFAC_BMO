import socket
import numpy as np
import speech_recognition as sr
import threading
import time
import pyttsx3

# ================= CONFIG =================
ESP32_IP = "192.168.137.37"

AUDIO_PORT = 5005
TTS_PORT   = 6001
SERVO_PORT = 6002
CAM_PORT   = 5000

SAMPLE_RATE = 16000
GAIN = 1.5
NOISE_THRESHOLD = 250

is_speaking = False

# ================= SOCKET =================
sock_audio = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_audio.bind(("0.0.0.0", AUDIO_PORT))

sock_tts = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_servo = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ================= SPEECH =================
recognizer = sr.Recognizer()
buffer = np.array([], dtype=np.int16)

# ================= CAMERA STATE (optional tracking) =================
last_face_center = (90, 90)

# ================= TTS =================
def send_servo(pan, tilt):
    sock_servo.sendto(bytes([pan, tilt]), (ESP32_IP, SERVO_PORT))


def send_tts(text):
    global is_speaking

    is_speaking = True

    engine = pyttsx3.init()
    engine.setProperty('rate', 170)

    tmp = "tts.wav"
    engine.save_to_file(text, tmp)
    engine.runAndWait()

    import wave

    with wave.open(tmp, 'rb') as wf:
        raw = wf.readframes(wf.getnframes())
        sr = wf.getframerate()
        ch = wf.getnchannels()

    audio = np.frombuffer(raw, dtype=np.int16)
    if ch == 2:
        audio = audio[::2]

    audio = np.interp(
        np.linspace(0, len(audio), int(len(audio)*16000/sr)),
        np.arange(len(audio)),
        audio
    ).astype(np.int16)

    for i in range(0, len(audio), 1400):
        sock_tts.sendto(audio[i:i+1400], (ESP32_IP, TTS_PORT))
        time.sleep(0.04)

        # 🔥 servo ikut “ngomong”
        send_servo(
            90 + np.random.randint(-4,4),
            90 + np.random.randint(-3,3)
        )

    send_servo(90, 90)
    is_speaking = False


# ================= AUDIO PROCESS =================
print("System running...")

while True:
    data, _ = sock_audio.recvfrom(2048)

    if is_speaking:
        continue

    audio = np.frombuffer(data, dtype=np.int16).copy()

    audio[np.abs(audio) < NOISE_THRESHOLD] = 0
    audio = audio.astype(np.float32) * GAIN
    audio = audio - np.mean(audio)
    audio = np.tanh(audio / 30000.0) * 30000
    audio = audio.astype(np.int16)

    buffer = np.concatenate((buffer, audio))

    # ===== SPEECH RECOG =====
    if len(buffer) > SAMPLE_RATE * 1.2:
        chunk = buffer[:SAMPLE_RATE]
        buffer = buffer[SAMPLE_RATE:]

        try:
            text = recognizer.recognize_google(
                sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2),
                language="id-ID"
            )

            print("[USER]:", text)

            response = "BMO mendengar kamu!"
            print("[BMO]:", response)

            threading.Thread(
                target=send_tts,
                args=(response,),
                daemon=True
            ).start()

        except:
            print("[...] tidak dikenali")
