import socket
import numpy as np
import cv2

UDP_IP = "0.0.0.0"
UDP_PORT = 5000

WIDTH = 320
HEIGHT = 240
UDP_PAYLOAD = 1400

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print("Listening...")

frame_buffer = {}
current_frame_id = None
expected_packets = 0

face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

while True:
    data, addr = sock.recvfrom(1500)

    if len(data) < 5:
        continue

    if data[0] != 0xAA or data[1] != 0x55:
        continue

    frame_id = data[2]
    total_packets = data[3]
    packet_id = data[4]

    payload = data[5:]

    if current_frame_id != frame_id:
        frame_buffer = {}
        current_frame_id = frame_id
        expected_packets = total_packets

    frame_buffer[packet_id] = payload

    if len(frame_buffer) == expected_packets:

        full_frame = b''.join(
            frame_buffer[i] for i in range(expected_packets)
        )

        img = np.frombuffer(full_frame, dtype=np.uint16)

        if img.size != WIDTH * HEIGHT:
            continue

        # kalau warna aneh → swap byte
        img = img.byteswap()

        img = img.reshape((HEIGHT, WIDTH))

        # ekstrak channel manual dari RGB565
        r = ((img >> 11) & 0x1F) << 3
        g = ((img >> 5) & 0x3F) << 2
        b = (img & 0x1F) << 3

        img = np.dstack((b, g, r)).astype(np.uint8)

        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

        faces = face_cascade.detectMultiScale(
            gray,
            scaleFactor=1.3,
            minNeighbors=5
        )

        for (x, y, w, h) in faces:
            cv2.rectangle(img, (x, y), (x+w, y+h), (0, 255, 0), 2)

        cv2.imshow("Face Detection", img)

        if cv2.waitKey(1) == 27:
            break
