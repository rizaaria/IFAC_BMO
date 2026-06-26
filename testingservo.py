import socket
import time

ESP32_IP = "192.168.137.37"
PORT = 6002

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

while True:
    # pan=90, tilt=90
    sock.sendto(bytes([90, 90]), (ESP32_IP, PORT))
    print("Kirim 90,90")
    time.sleep(1)

    sock.sendto(bytes([0, 180]), (ESP32_IP, PORT))
    print("Kirim 0,180")
    time.sleep(1)
