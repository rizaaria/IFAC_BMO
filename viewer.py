import serial
import numpy as np
import cv2
import time

PORT = "COM7"       # GANTI sesuai board kamu
BAUD = 2000000       # Harus sama dengan firmware

WIDTH  = 320
HEIGHT = 240
FRAME_SIZE = WIDTH * HEIGHT * 2 # 76800 bytes for grayscale QVGA

def main():
    ser = serial.Serial(PORT, BAUD, timeout=2)
    ser.reset_input_buffer()  # flush data lama

    print(f"Menunggu kamera di {PORT} @ {BAUD} baud ...")
    print("Tekan ESC di jendela OpenCV untuk keluar.")

    frame_count = 0
    fps_time = time.time()
    fps = 0.0

    while True:
        # Cari header 0xAA 0x55
        b = ser.read(1)
        if len(b) == 0:
            continue
        if b != b'\xAA':
            continue

        b = ser.read(1)
        if len(b) == 0:
            continue
        if b != b'\x55':
            continue

        # Baca 4-byte frame size (big-endian)
        size_bytes = ser.read(4)
        if len(size_bytes) != 4:
            continue

        frame_size = int.from_bytes(size_bytes, 'big')

        # Validasi ukuran frame
        if frame_size != FRAME_SIZE:
            print(f"Ukuran frame tidak sesuai: {frame_size} (expected {FRAME_SIZE}), skip...")
            ser.reset_input_buffer()
            continue

        # Baca raw pixel data
        frame = ser.read(frame_size)
        if len(frame) != frame_size:
            print(f"Frame tidak lengkap: {len(frame)}/{frame_size}, skip...")
            ser.reset_input_buffer()
            continue

        # Baca sebagai uint8 2-channel
        img = np.frombuffer(frame, dtype=np.uint8).reshape((HEIGHT, WIDTH, 2))

        # Swap byte karena ESP32 kirim little-endian
        img = img[:, :, ::-1]

        # Convert RGB565 → BGR888
        img = cv2.cvtColor(img, cv2.COLOR_BGR5652BGR)

        # Hitung FPS
        frame_count += 1
        elapsed = time.time() - fps_time
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            fps_time = time.time()

        # Tampilkan FPS di gambar
        cv2.putText(img, f"FPS: {fps:.1f}", (10, 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, 255, 1)

        cv2.imshow("ESP32-S3 Camera (Grayscale QVGA)", img)

        if cv2.waitKey(1) & 0xFF == 27:  # ESC
            break

    ser.close()
    cv2.destroyAllWindows()
    print("Selesai.")

if __name__ == "__main__":
    main()
