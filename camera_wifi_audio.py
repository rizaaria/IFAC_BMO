import socket
import numpy as np

UDP_IP = "0.0.0.0"
UDP_PORT = 5005

ESP32_IP = "192.168.137.37"
ESP32_AUDIO_PORT = 6001

sock_rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_rx.bind((UDP_IP, UDP_PORT))

sock_tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("Audio clean + relay...")

NOISE_THRESHOLD = 250
GAIN = 25

while True:
    data, _ = sock_rx.recvfrom(2048)

    # 🔥 FIX DI SINI
    audio = np.frombuffer(data, dtype=np.int16).copy()

    # Noise gate
    audio[np.abs(audio) < NOISE_THRESHOLD] = 0

    # Gain
    audio = audio.astype(np.float32) * GAIN

    # Clip
    audio = np.clip(audio, -32768, 32767).astype(np.int16)

    sock_tx.sendto(audio.tobytes(), (ESP32_IP, ESP32_AUDIO_PORT))
