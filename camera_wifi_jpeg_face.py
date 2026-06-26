# ============================================
# CAMERA SERVER ONLY (DEBUG VERSION)
# ESP32 JPEG UDP → PC + Face Tracking + Servo
# ============================================

import socket
import numpy as np
import cv2
import time

# ============================================
# NETWORK CONFIG
# ============================================
LOCAL_IP = "0.0.0.0"
CAM_PORT = 5000

ESP32_IP   = "192.168.137.37"
SERVO_PORT = 6002

# ============================================
# CAMERA CONFIG
# ============================================
CAM_WIDTH  = 320
CAM_HEIGHT = 240

# ============================================
# SERVO CONFIG
# ============================================
SERVO_CENTER_PAN  = 90
SERVO_CENTER_TILT = 90
SERVO_STEP        = 1
SERVO_DEADZONE    = 30

current_pan  = SERVO_CENTER_PAN
current_tilt = SERVO_CENTER_TILT

# UDP socket untuk kirim servo
servo_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Load face detection
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

# ============================================
# CAMERA THREAD
# ============================================
def camera_thread():
    global current_pan, current_tilt

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, CAM_PORT))
    sock.settimeout(2.0)

    print(f"[Camera] Listening on {LOCAL_IP}:{CAM_PORT}")

    frames = {}

    frame_count = 0
    fps_time = time.time()
    fps = 0.0

    faces = []

    while True:
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue

        # ===== VALIDASI HEADER =====
        if len(data) < 5:
            continue

        if data[0] != 0xAA or data[1] != 0x55:
            continue

        frame_id = data[2]
        total_packets = data[3]
        packet_id = data[4]
        payload = data[5:]

        # ===== BUFFER FRAME =====
        if frame_id not in frames:
            frames[frame_id] = [None] * total_packets

        frames[frame_id][packet_id] = payload

        # ===== FRAME COMPLETE =====
        if all(p is not None for p in frames[frame_id]):

            try:
                full_frame = b''.join(frames[frame_id])
            except:
                del frames[frame_id]
                continue

            del frames[frame_id]

            if len(full_frame) < 100:
                continue

            # ===== DECODE JPEG =====
            img_array = np.frombuffer(full_frame, dtype=np.uint8)
            img_bgr = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

            if img_bgr is None:
                continue

            # ===== FACE DETECTION =====
            gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)

            if frame_count % 3 == 0:
                faces = face_cascade.detectMultiScale(
                    gray, 1.3, 5, minSize=(30, 30)
                )

            for (x, y, w, h) in faces:
                cv2.rectangle(img_bgr, (x, y), (x + w, y + h), (0, 255, 0), 2)

            # ===== SERVO TRACKING =====
            if len(faces) > 0:
                largest = max(faces, key=lambda f: f[2] * f[3])
                fx, fy, fw, fh = largest

                cx = fx + fw // 2
                cy = fy + fh // 2

                offset_x = cx - (CAM_WIDTH // 2)
                offset_y = cy - (CAM_HEIGHT // 2)

                cv2.circle(img_bgr, (cx, cy), 5, (0, 0, 255), -1)

                # PAN
                if abs(offset_x) > SERVO_DEADZONE:
                    if offset_x > 0:
                        current_pan -= SERVO_STEP
                    else:
                        current_pan += SERVO_STEP

                # TILT
                if abs(offset_y) > SERVO_DEADZONE:
                    if offset_y > 0:
                        current_tilt -= SERVO_STEP
                    else:
                        current_tilt += SERVO_STEP

                # LIMIT
                current_pan = max(0, min(180, current_pan))
                current_tilt = max(0, min(180, current_tilt))

                # KIRIM KE ESP32
                servo_data = bytes([int(current_pan), int(current_tilt)])
                servo_sock.sendto(servo_data, (ESP32_IP, SERVO_PORT))

            # ===== FPS =====
            frame_count += 1
            elapsed = time.time() - fps_time
            if elapsed >= 1:
                fps = frame_count / elapsed
                frame_count = 0
                fps_time = time.time()

            cv2.putText(img_bgr, f"FPS: {fps:.1f}", (10, 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

            cv2.imshow("ESP32 Camera", img_bgr)

            if cv2.waitKey(1) == 27:
                break

        # ===== CLEANUP =====
        if len(frames) > 10:
            frames.clear()

    sock.close()
    cv2.destroyAllWindows()


# ============================================
# MAIN
# ============================================
if __name__ == "__main__":
    print("=== CAMERA DEBUG MODE ===")
    print(f"Listening UDP : {CAM_PORT}")
    print(f"Send Servo -> : {ESP32_IP}:{SERVO_PORT}")
    print("========================")

    camera_thread()
