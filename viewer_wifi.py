import socket
import numpy as np
import cv2
import time

WIDTH = 320
HEIGHT = 240
FRAME_SIZE = WIDTH * HEIGHT * 2

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))

print("Menunggu UDP RAW stream...")

frames = {}
frame_count = 0
fps_time = time.time()
fps = 0.0

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

    # cek frame lengkap
    if all(p is not None for p in frames[frameID]):

        raw = b''.join(frames[frameID])
        del frames[frameID]

        if len(raw) != FRAME_SIZE:
            continue

        img = np.frombuffer(raw, dtype=np.uint8).reshape((HEIGHT, WIDTH, 2))
        img = img[:, :, ::-1]
        img = cv2.cvtColor(img, cv2.COLOR_BGR5652BGR)

        frame_count += 1
        elapsed = time.time() - fps_time
        if elapsed >= 1:
            fps = frame_count / elapsed
            frame_count = 0
            fps_time = time.time()

        cv2.putText(img, f"FPS: {fps:.1f}", (10,20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0), 2)

        cv2.imshow("ESP32 RAW UDP", img)

        if cv2.waitKey(1) & 0xFF == 27:
            break
