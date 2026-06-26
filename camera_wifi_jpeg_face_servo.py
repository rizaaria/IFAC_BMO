import socket
import numpy as np
import cv2
import time

# ================= CONFIG =================
LOCAL_IP = "0.0.0.0"
CAM_PORT = 5000
ESP32_IP = "192.168.137.37"
SERVO_PORT = 6002

CAM_WIDTH  = 160
CAM_HEIGHT = 120

SERVO_CENTER_PAN  = 90
SERVO_CENTER_TILT = 90
SERVO_STEP = 1
SERVO_DEADZONE = 15

# ================= UDP =================
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LOCAL_IP, CAM_PORT))

servo_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# ================= FACE =================
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

# ================= STATE =================
frames = {}
current_pan  = SERVO_CENTER_PAN
current_tilt = SERVO_CENTER_TILT

last_servo_time = 0
SERVO_INTERVAL = 0.05

frame_skip = 3
frame_counter = 0

fps_time = time.time()
frame_count = 0
fps = 0

print("Camera + Servo Ready...")

# ================= LOOP =================
while True:
    data, addr = sock.recvfrom(2048)

    if len(data) < 5:
        continue

    if data[0] != 0xAA or data[1] != 0x55:
        continue

    frameID = data[2]
    totalPackets = data[3]
    packetIndex = data[4]
    payload = data[5:]

    if frameID not in frames:
        frames[frameID] = [None] * totalPackets

    frames[frameID][packetIndex] = payload

    if all(p is not None for p in frames[frameID]):

        jpg = b''.join(frames[frameID])
        del frames[frameID]

        img_array = np.frombuffer(jpg, dtype=np.uint8)
        img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        if img is None:
            continue

        frame_counter += 1

        # ===== SKIP FRAME =====
        if frame_counter % frame_skip != 0:
            cv2.imshow("Robot Cam", img)
            if cv2.waitKey(1) == 27:
                break
            continue

        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, 1.3, 5)

        if len(faces) > 0:
            (x, y, w, h) = max(faces, key=lambda f: f[2]*f[3])

            cx = x + w//2
            cy = y + h//2

            offset_x = cx - (CAM_WIDTH // 2)
            offset_y = cy - (CAM_HEIGHT // 2)

            # ===== PAN =====
            if abs(offset_x) > SERVO_DEADZONE:
                if offset_x > 0:
                    current_pan -= SERVO_STEP
                else:
                    current_pan += SERVO_STEP

            # ===== TILT =====
            if abs(offset_y) > SERVO_DEADZONE:
                if offset_y > 0:
                    current_tilt -= SERVO_STEP
                else:
                    current_tilt += SERVO_STEP

            current_pan  = max(0, min(180, current_pan))
            current_tilt = max(0, min(180, current_tilt))

            cv2.rectangle(img, (x,y), (x+w,y+h), (0,255,0), 2)

        # ===== SERVO SEND =====
        now = time.time()
        if now - last_servo_time > SERVO_INTERVAL:
            servo_data = bytes([int(current_pan), int(current_tilt)])
            servo_sock.sendto(servo_data, (ESP32_IP, SERVO_PORT))
            last_servo_time = now

        # ===== FPS =====
        frame_count += 1
        if time.time() - fps_time >= 1:
            fps = frame_count
            frame_count = 0
            fps_time = time.time()

        cv2.putText(img, f"FPS:{fps}", (10,20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 1)

        cv2.imshow("Robot Cam", img)

        if cv2.waitKey(1) == 27:
            break
