import socket
import numpy as np
import cv2
import threading
import time
import speech_recognition as sr

# ================= CONFIG =================
LOCAL_IP = "0.0.0.0"
ESP32_IP = "192.168.137.109"

CAM_PORT   = 5000
AUDIO_PORT = 5005
TEXT_PORT  = 6000
TTS_PORT   = 6001
SERVO_PORT = 6002

HEADER_BYTES = 6
SAMPLE_RATE = 16000

running = True

# ================= GLOBAL =================
current_pan = 90
current_tilt = 90
recognition_buffer = np.array([], dtype=np.int16)

# sockets
servo_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
text_sock  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

# ============================================================
# CAMERA THREAD (DEBUG VERSION)
# ============================================================
def camera_thread():
    global current_pan, current_tilt

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, CAM_PORT))
    sock.settimeout(2)

    buffer = {}
    frame_id = None
    last_frame_time = time.time()

    frame_counter = 0

    print("[CAM] Listening...")

    while running:
        try:
            data, _ = sock.recvfrom(2048)
        except:
            print("[CAM] No data...")
            continue

        if len(data) < 5:
            print("[CAM] Packet too small")
            continue

        if data[0] != 0xAA or data[1] != 0x55:
            print("[CAM] Invalid header")
            continue

        fid = data[2]
        total = data[3]
        pid = data[4]

        if frame_id != fid:
            buffer = {}
            frame_id = fid

        buffer[pid] = data[5:]

        # ===== DEBUG: packet info =====
        if pid == 0:
            print(f"[CAM] Frame {fid} start, total packets={total}")

        if len(buffer) == total:
            try:
                frame = b''.join(buffer[i] for i in range(total))
            except:
                print("[CAM] Missing packet!")
                continue

            img = cv2.imdecode(np.frombuffer(frame, np.uint8), 1)

            if img is None:
                print("[CAM] Decode failed")
                continue

            # Resize biar ringan
            img_small = cv2.resize(img, (160,120))
            gray = cv2.cvtColor(img_small, cv2.COLOR_BGR2GRAY)

            faces = face_cascade.detectMultiScale(
                gray,
                scaleFactor=1.2,
                minNeighbors=4,
                minSize=(30,30)
            )

            print(f"[CAM] Faces detected: {len(faces)}")

            # Servo tracking
            if len(faces) > 0:
                x,y,w,h = faces[0]

                cx = x + w//2
                cy = y + h//2

                offset_x = cx - 80
                offset_y = cy - 60

                if abs(offset_x) > 10:
                    current_pan += -2 if offset_x > 0 else 2

                if abs(offset_y) > 10:
                    current_tilt += -2 if offset_y > 0 else 2

                current_pan = max(0,min(180,current_pan))
                current_tilt = max(0,min(180,current_tilt))

                servo_sock.sendto(bytes([current_pan,current_tilt]), (ESP32_IP,SERVO_PORT))

            # FPS
            frame_counter += 1
            now = time.time()
            if now - last_frame_time >= 1:
                print(f"[CAM] FPS: {frame_counter}")
                frame_counter = 0
                last_frame_time = now

            cv2.imshow("CAM", img)
            if cv2.waitKey(1)==27:
                break


# ============================================================
# AUDIO THREAD (DEBUG VERSION)
# ============================================================
def audio_thread():
    global recognition_buffer

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, AUDIO_PORT))
    sock.settimeout(2)

    rec = sr.Recognizer()

    packet_count = 0

    print("[AUDIO] Listening...")

    while running:
        try:
            pkt,_ = sock.recvfrom(2048)
        except:
            print("[AUDIO] No data...")
            continue

        packet_count += 1

        if len(pkt) <= HEADER_BYTES:
            print("[AUDIO] Packet too small")
            continue

        pcm = np.frombuffer(pkt[HEADER_BYTES:], np.int16)

        # RMS LEVEL (DEBUG)
        rms = np.sqrt(np.mean(pcm.astype(np.float32)**2))
        print(f"[AUDIO] Packet:{packet_count} RMS:{int(rms)}")

        # buffer
        recognition_buffer = np.concatenate((recognition_buffer, pcm))

        # limit
        if len(recognition_buffer) > SAMPLE_RATE*5:
            recognition_buffer = recognition_buffer[-SAMPLE_RATE*5:]

        if len(recognition_buffer) >= SAMPLE_RATE*3:
            chunk = recognition_buffer[:SAMPLE_RATE*3]
            recognition_buffer = recognition_buffer[SAMPLE_RATE*3:]

            audio = sr.AudioData(chunk.tobytes(),16000,2)

            try:
                print("[AUDIO] Recognizing...")
                text = rec.recognize_google(audio, language="id-ID")
                print(f"[VOICE] >>> {text}")

            except Exception as e:
                print("[VOICE] Failed:", e)


# ============================================================
# MAIN
# ============================================================
def main():
    print("=== DEBUG SERVER START ===")

    threading.Thread(target=camera_thread,daemon=True).start()
    threading.Thread(target=audio_thread,daemon=True).start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("EXIT")

if __name__ == "__main__":
    main()
