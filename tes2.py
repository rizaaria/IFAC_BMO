# # Continue
# import socket
# import sounddevice as sd
# import numpy as np
# import struct

# # ==== konfigurasi ==== 
# LOCAL_IP = "0.0.0.0"  # terima dari semua interface
# LOCAL_PORT = 5005     # harus sama dengan PC_PORT ESP32
# SAMPLE_RATE = 16000
# FRAME_SAMPLES = 320
# FRAME_BYTES = FRAME_SAMPLES * 2
# HEADER_BYTES = 6      # seq(2) + timestamp(4)
# PACKET_SIZE = HEADER_BYTES + FRAME_BYTES

# # ==== setup UDP socket ====
# sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# sock.bind((LOCAL_IP, LOCAL_PORT))
# print(f"Listening on {LOCAL_IP}:{LOCAL_PORT}...")

# # ==== setup playback stream ====
# stream = sd.OutputStream(
#     samplerate=SAMPLE_RATE, channels=1, dtype='int16', blocksize=FRAME_SAMPLES
# )
# stream.start()

# while True:
#     pkt, addr = sock.recvfrom(PACKET_SIZE)
#     if len(pkt) != PACKET_SIZE:
#         print(f"Packet size mismatch: {len(pkt)} bytes")
#         continue

#     # parsing header (optional, bisa untuk debug seq/ts)
#     seq = pkt[0] | (pkt[1] << 8)
#     ts  = pkt[2] | (pkt[3] << 8) | (pkt[4] << 16) | (pkt[5] << 24)

#     # ambil audio PCM
#     pcm_bytes = pkt[HEADER_BYTES:]
#     pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

#     # kirim ke speaker
#     stream.write(pcm)





# Using speech recognition [Fix] + Without CYD
# import socket
# import sounddevice as sd
# import numpy as np
# import speech_recognition as sr
# import threading
# import queue
# import time

# # =============================
# # CONFIG
# # =============================
# LOCAL_IP = "0.0.0.0"
# LOCAL_PORT = 5005

# SAMPLE_RATE = 16000
# FRAME_SAMPLES = 320
# FRAME_BYTES = FRAME_SAMPLES * 2
# HEADER_BYTES = 6
# PACKET_SIZE = HEADER_BYTES + FRAME_BYTES

# RECOG_BUFFER_SECONDS = 3
# RECOG_SAMPLES = SAMPLE_RATE * RECOG_BUFFER_SECONDS

# # =============================
# # QUEUES
# # =============================
# playback_queue = queue.Queue(maxsize=200)
# recognition_queue = queue.Queue(maxsize=200)

# recognition_buffer = np.array([], dtype=np.int16)
# running = True

# # =============================
# # UDP RECEIVER
# # =============================
# def udp_receiver():
#     sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#     sock.bind((LOCAL_IP, LOCAL_PORT))
#     print(f"[UDP] Listening on {LOCAL_IP}:{LOCAL_PORT}")

#     while running:
#         pkt, _ = sock.recvfrom(PACKET_SIZE)
#         if len(pkt) != PACKET_SIZE:
#             continue

#         pcm_bytes = pkt[HEADER_BYTES:]
#         pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

#         # Gain boost optional
#         pcm = (pcm.astype(np.float32) * 2.5).clip(-32768, 32767).astype(np.int16)

#         try:
#             playback_queue.put_nowait(pcm)
#             recognition_queue.put_nowait(pcm)
#         except queue.Full:
#             pass

# # =============================
# # PLAYBACK THREAD
# # =============================
# def playback_worker():
#     with sd.OutputStream(
#         samplerate=SAMPLE_RATE,
#         channels=1,
#         dtype='int16',
#         blocksize=FRAME_SAMPLES
#     ) as stream:

#         print("[Audio] Playback started")

#         while running:
#             try:
#                 pcm = playback_queue.get(timeout=1)
#                 stream.write(pcm)
#             except queue.Empty:
#                 continue

# # =============================
# # RECOGNITION THREAD
# # =============================
# def recognition_worker():
#     global recognition_buffer

#     recognizer = sr.Recognizer()
#     print("[SpeechRecognition] Ready")

#     while running:
#         try:
#             pcm = recognition_queue.get(timeout=1)
#             recognition_buffer = np.concatenate((recognition_buffer, pcm))

#             if len(recognition_buffer) >= RECOG_SAMPLES:
#                 chunk = recognition_buffer[:RECOG_SAMPLES]
#                 recognition_buffer = recognition_buffer[RECOG_SAMPLES:]

#                 audio_data = sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2)

#                 try:
#                     text = recognizer.recognize_google(audio_data, language="id-ID")
#                     print(f"[Recognized] {text}")
#                 except sr.UnknownValueError:
#                     print("[Recognized] (tidak dikenali)")
#                 except sr.RequestError as e:
#                     print(f"[API Error] {e}")

#         except queue.Empty:
#             continue

# # =============================
# # MAIN
# # =============================
# try:
#     threading.Thread(target=udp_receiver, daemon=True).start()
#     threading.Thread(target=playback_worker, daemon=True).start()
#     threading.Thread(target=recognition_worker, daemon=True).start()

#     while True:
#         time.sleep(1)

# except KeyboardInterrupt:
#     running = False
#     print("Shutting down...")














# Using speech recognition [Fix] + With CYD
import socket
import sounddevice as sd
import numpy as np
import speech_recognition as sr
import threading
import queue
import time

# =============================
# CONFIG
# =============================
LOCAL_IP = "0.0.0.0"
LOCAL_PORT = 5005

ESP32_IP = "10.250.32.24"   # Ganti sesuai IP ESP32S3
ESP32_TEXT_PORT = 6000

SAMPLE_RATE = 16000
FRAME_SAMPLES = 320
FRAME_BYTES = FRAME_SAMPLES * 2
HEADER_BYTES = 6
PACKET_SIZE = HEADER_BYTES + FRAME_BYTES

RECOG_BUFFER_SECONDS = 3
RECOG_SAMPLES = SAMPLE_RATE * RECOG_BUFFER_SECONDS

# =============================
# QUEUES
# =============================
playback_queue = queue.Queue(maxsize=200)
recognition_queue = queue.Queue(maxsize=200)

recognition_buffer = np.array([], dtype=np.int16)
running = True

# =============================
# SOCKET TEXT
# =============================
text_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# # =============================
# # UDP RECEIVER (AUDIO)
# # =============================
def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, LOCAL_PORT))
    print(f"[UDP] Listening on {LOCAL_IP}:{LOCAL_PORT}")

    while running:
        pkt, _ = sock.recvfrom(PACKET_SIZE)
        if len(pkt) != PACKET_SIZE:
            continue

        pcm_bytes = pkt[HEADER_BYTES:]
        pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

        pcm = (pcm.astype(np.float32) * 2.5).clip(-32768, 32767).astype(np.int16)

        try:
            playback_queue.put_nowait(pcm)
            recognition_queue.put_nowait(pcm)
        except queue.Full:
            pass

# =============================
# UDP RECEIVER (AUDIO)
# =============================
# def udp_receiver():
#     sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#     sock.bind((LOCAL_IP, LOCAL_PORT))
#     print(f"[UDP] Listening on {LOCAL_IP}:{LOCAL_PORT}")

#     while running:
#         pkt, addr = sock.recvfrom(PACKET_SIZE)

#         print(f"[UDP] Packet received from {addr}, size={len(pkt)}")

#         if len(pkt) != PACKET_SIZE:
#             print("[UDP] Wrong packet size")
#             continue

#         pcm_bytes = pkt[HEADER_BYTES:]
#         pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

#         print(f"[UDP] PCM Max Amplitude BEFORE scaling: {np.max(np.abs(pcm))}")

#         pcm = (pcm.astype(np.float32) * 2.5).clip(-32768, 32767).astype(np.int16)

#         print(f"[UDP] PCM Max Amplitude AFTER scaling: {np.max(np.abs(pcm))}")

#         try:
#             playback_queue.put_nowait(pcm)
#             recognition_queue.put_nowait(pcm)
#         except queue.Full:
#             print("[UDP] Queue FULL")
#             pass



# def udp_receiver():
#     sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#     sock.bind((LOCAL_IP, LOCAL_PORT))
#     sock.settimeout(2)  # supaya bisa deteksi kalau tidak ada paket
#     print(f"[UDP] Listening on {LOCAL_IP}:{LOCAL_PORT}")

#     packet_counter = 0

#     while running:
#         try:
#             pkt, addr = sock.recvfrom(PACKET_SIZE)
#             packet_counter += 1

#             print(f"[UDP] Packet #{packet_counter} from {addr} size={len(pkt)}")

#             if len(pkt) != PACKET_SIZE:
#                 print("[UDP] Wrong packet size")
#                 continue

#             pcm_bytes = pkt[HEADER_BYTES:]
#             pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

#             max_amp = np.max(np.abs(pcm))
#             print(f"[UDP] Max amplitude={max_amp}")

#             playback_queue.put_nowait(pcm)
#             recognition_queue.put_nowait(pcm)

#         except socket.timeout:
#             print("[UDP] No packet received in 2 seconds")
#         except queue.Full:
#             print("[UDP] Queue FULL")



# =============================
# PLAYBACK THREAD
# =============================
def playback_worker():
    with sd.OutputStream(
        samplerate=SAMPLE_RATE,
        channels=1,
        dtype='int16',
        blocksize=FRAME_SAMPLES
    ) as stream:

        print("[Audio] Playback started")

        while running:
            try:
                pcm = playback_queue.get(timeout=1)
                stream.write(pcm)
            except queue.Empty:
                continue

# def playback_worker():
#     print("[Audio] Available devices:")
#     print(sd.query_devices())

#     with sd.OutputStream(
#         samplerate=SAMPLE_RATE,
#         channels=1,
#         dtype='int16',
#         blocksize=FRAME_SAMPLES
#     ) as stream:

#         print("[Audio] Playback started")

#         while running:
#             try:
#                 pcm = playback_queue.get(timeout=1)
#                 print(f"[Playback] Writing frame, max={np.max(np.abs(pcm))}")
#                 stream.write(pcm)
#             except queue.Empty:
#                 continue

# def playback_worker():
#     devices = sd.query_devices()

#     output_device = None
#     for i, dev in enumerate(devices):
#         if dev['max_output_channels'] > 0 and "Speakers" in dev['name']:
#             output_device = i
#             print(f"[Audio] Using device {dev['name']} index={i}")
#             break

#     if output_device is None:
#         raise RuntimeError("No speaker device found")

#     with sd.OutputStream(
#         device=output_device,
#         samplerate=16000,
#         channels=1,
#         dtype='int16',
#         blocksize=FRAME_SAMPLES
#     ) as stream:

#         print("[Audio] Playback started")

#         while running:
#             try:
#                 pcm = playback_queue.get(timeout=1)
#                 print(f"[Playback] max={np.max(np.abs(pcm))}")
#                 stream.write(pcm)
#             except queue.Empty:
#                 continue

# =============================
# RECOGNITION THREAD
# =============================
def recognition_worker():
    global recognition_buffer

    recognizer = sr.Recognizer()
    print("[SpeechRecognition] Ready")

    while running:
        try:
            pcm = recognition_queue.get(timeout=1)
            recognition_buffer = np.concatenate((recognition_buffer, pcm))

            if len(recognition_buffer) >= RECOG_SAMPLES:
                chunk = recognition_buffer[:RECOG_SAMPLES]
                recognition_buffer = recognition_buffer[RECOG_SAMPLES:]

                audio_data = sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2)

                try:
                    text = recognizer.recognize_google(audio_data, language="id-ID")
                    print(f"[Recognized] {text}")

                    # Kirim ke ESP32S3
                    text_sock.sendto(text.encode("utf-8"), (ESP32_IP, ESP32_TEXT_PORT))

                except sr.UnknownValueError:
                    print("[Recognized] (tidak dikenali)")
                except sr.RequestError as e:
                    print(f"[API Error] {e}")

        except queue.Empty:
            continue

# =============================
# MAIN
# =============================
try:
    threading.Thread(target=udp_receiver, daemon=True).start()
    threading.Thread(target=playback_worker, daemon=True).start()
    threading.Thread(target=recognition_worker, daemon=True).start()

    while True:
        time.sleep(1)

except KeyboardInterrupt:
    running = False
    print("Shutting down...")






# Using speech recognition + CYD + ESPCAM ON
# import cv2

# url = "http://10.250.32.24/stream"   # ganti sesuai IP ESP32

# cap = cv2.VideoCapture(url)

# while True:
#     ret, frame = cap.read()
#     if not ret:
#         print("Frame error")
#         break

#     cv2.imshow("ESP32 Camera", frame)

#     if cv2.waitKey(1) & 0xFF == ord('q'):
#         break

# cap.release()
# cv2.destroyAllWindows()