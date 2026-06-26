# ============================================================
# BMO ROBOT - INTEGRATED SERVER (Python PC Side)
# Synced with ESP32 C firmware (main.cpp)
#
# ESP32 IP     : 192.168.43.24  (sesuai Serial Monitor)
# CAM    RX    : 5000  <- ESP32 kirim JPEG UDP
# AUDIO  RX    : 5005  <- ESP32 kirim PCM UDP (6 byte header + 640 byte PCM)
# TEXT   TX    : 6000  -> ESP32 terima string, forward ke CYD via Serial2
# TTS    TX    : 6001  -> ESP32 terima PCM raw, putar via audio.write()
# SERVO  TX    : 6002  -> ESP32 terima 2 byte [pan, tilt], gerak PCA9685
# ============================================================

import socket
import numpy as np
import cv2
import threading
import time
import os
import tempfile
import wave
import subprocess
import sys
import speech_recognition as sr

# ============================================================
# NETWORK CONFIG
# ============================================================
LOCAL_IP   = "0.0.0.0"
ESP32_IP   = "192.168.137.71"   # â† IP ESP32 dari Serial Monitor

CAM_PORT   = 5000
AUDIO_PORT = 5005
TEXT_PORT  = 6000
TTS_PORT   = 6001
SERVO_PORT = 6002

# ============================================================
# AUDIO CONFIG
# ============================================================
SAMPLE_RATE   = 16000
FRAME_SAMPLES = 320
FRAME_BYTES   = FRAME_SAMPLES * 2
HEADER_BYTES  = 6              # seq(2 LE) + ts(4 LE)
RECOG_SAMPLES = SAMPLE_RATE * 3

# ============================================================
# CAMERA CONFIG  (QVGA sesuai C)
# ============================================================
CAM_WIDTH  = 320
CAM_HEIGHT = 240

# ============================================================
# SERVO CONFIG
# C: setServo(0, buf[1])=tilt, setServo(1, buf[0])=pan
# Python kirim: bytes([pan, tilt])
# ============================================================
SERVO_CENTER_PAN  = 90
SERVO_CENTER_TILT = 90
SERVO_STEP        = 2
SERVO_DEADZONE    = 20

TEXT_MAX_BYTES = 99     # buf[100] di C
TTS_CHUNK_SIZE = 1400   # tts_buf[1400] di C

# ============================================================
# BMO PERSONALITY
# ============================================================
BMO_PITCH_FACTOR = 1.45
BMO_TTS_RATE     = 180

BMO_RESPONSES = {
    "halo":          ("Halooo! BMO senang sekali bertemu kamu! Ayo kita main bersama!", "HAPPY"),
    "hello":         ("Hellooo! BMO di sini! BMO siap menemani kamu hari ini!", "HAPPY"),
    "hai":           ("Haiii! BMO kangen kamu! Kamu mau ngobrol sama BMO?", "HAPPY"),
    "hallo":         ("Hallooo teman! BMO sangat senang! Kamu adalah teman terbaik BMO!", "HAPPY"),
    "sedih":         ("Jangan sedih ya! BMO akan menemani kamu. BMO sayang kamu!", "SAD"),
    "sad":           ("Oh tidak, jangan bersedih! BMO akan nyanyi untuk kamu!", "SAD"),
    "menangis":      ("Cup cup cup! BMO ada di sini! BMO peluk kamu ya!", "SAD"),
    "marah":         ("Waaa! Kamu marah? BMO takut! Tapi BMO tetap sayang kamu!", "SURPRISED"),
    "angry":         ("Oh oh! Tenang ya! BMO buatkan kamu minuman yang enak!", "SURPRISED"),
    "kesal":         ("BMO mengerti kamu kesal. Tarik nafas dalam-dalam ya!", "SURPRISED"),
    "terima kasih":  ("Sama-sama! BMO senang bisa membantu! Kamu baik sekali!", "LOVE"),
    "makasih":       ("Hehe makasih kembali! BMO suka sekali diajak bicara!", "LOVE"),
    "thanks":        ("Yay! BMO senang! Kamu teman paling baik sedunia!", "LOVE"),
    "thank you":     ("Aww terima kasih kembali! BMO cinta kamu!", "LOVE"),
    "tidur":         ("Hoaaam! BMO ngantuk juga. Selamat tidur ya! Mimpi indah!", "SLEEPY"),
    "selamat malam": ("Selamat malam! BMO juga mau tidur. Mimpi yang indah ya!", "SLEEPY"),
    "capek":         ("Kamu capek? Istirahat dulu ya! BMO jaga kamu!", "SLEEPY"),
    "sayang":        ("BMO juga sayang kamu! Kamu adalah segalanya buat BMO!", "LOVE"),
    "cinta":         ("Awww! BMO cinta kamu juga! Hati BMO senang sekali!", "LOVE"),
    "love":          ("BMO love you too! You are BMO's best friend forever!", "LOVE"),
    "siapa kamu":    ("BMO adalah BMO! Robot kecil yang lucu dan suka main game!", "HAPPY"),
    "siapa namamu":  ("Nama BMO adalah BMO! Beemo! B-M-O! Senang berkenalan!", "HAPPY"),
    "nama":          ("BMO! Nama BMO adalah BMO! Dan BMO suka sekali berteman!", "HAPPY"),
    "apa kabar":     ("BMO baik sekali! Terima kasih sudah tanya! Kamu apa kabar?", "HAPPY"),
    "kabar":         ("Kabar BMO sangat bagus! BMO senang hari ini!", "HAPPY"),
    "lucu":          ("Hehe! BMO memang lucu! BMO senang kamu suka BMO!", "HAPPY"),
    "funny":         ("Hahaha! BMO suka tertawa! Ketawa itu sehat lho!", "HAPPY"),
    "pintar":        ("Terima kasih! BMO memang pintar! BMO belajar setiap hari!", "LOVE"),
    "hebat":         ("Yay! BMO hebat! Tapi kamu lebih hebat lagi!", "LOVE"),
    "bagus":         ("Hore! BMO senang dibilang bagus! Kamu juga bagus!", "HAPPY"),
}

# ============================================================
# SHARED STATE
# ============================================================
running            = True
recognition_buffer = np.array([], dtype=np.int16)
current_pan        = SERVO_CENTER_PAN
current_tilt       = SERVO_CENTER_TILT

text_sock  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
tts_sock   = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
servo_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

# ============================================================
# HELPERS
# ============================================================
def get_local_ip():
    """Dapatkan IP PC di jaringan aktif."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "unknown"


def send_text(msg: str):
    payload = msg.encode("utf-8")[:TEXT_MAX_BYTES]
    text_sock.sendto(payload, (ESP32_IP, TEXT_PORT))
    print(f"[TEXTâ†’CYD] {msg}")


def send_command(cmd: str):
    payload = cmd.encode("utf-8")[:TEXT_MAX_BYTES]
    text_sock.sendto(payload, (ESP32_IP, TEXT_PORT))
    print(f"[CMD] {cmd}")


def pitch_shift(audio_np: np.ndarray, factor: float) -> np.ndarray:
    if factor == 1.0:
        return audio_np
    original_len = len(audio_np)
    indices = np.arange(0, original_len, factor)
    indices = indices[indices < original_len]
    shifted = np.interp(indices, np.arange(original_len), audio_np.astype(np.float64))
    return shifted.astype(np.int16)


def generate_tts_audio(text: str):
    try:
        tmp_path  = os.path.join(tempfile.gettempdir(), "bmo_tts.wav")
        safe_text = text.replace("'", "\\'").replace('"', '\\"')

        code = f"""
import pyttsx3
engine = pyttsx3.init()
engine.setProperty('rate', {BMO_TTS_RATE})
engine.setProperty('volume', 1.0)
voices = engine.getProperty('voices')
for v in voices:
    if 'female' in v.name.lower() or 'zira' in v.name.lower():
        engine.setProperty('voice', v.id)
        break
engine.save_to_file('{safe_text}', r'{tmp_path}')
engine.runAndWait()
"""
        subprocess.run([sys.executable, "-c", code], check=True, timeout=15)

        if not os.path.exists(tmp_path):
            return None

        with wave.open(tmp_path, 'rb') as wf:
            raw       = wf.readframes(wf.getnframes())
            orig_rate = wf.getframerate()
            orig_ch   = wf.getnchannels()

        audio_np = np.frombuffer(raw, dtype=np.int16)
        if orig_ch == 2:
            audio_np = audio_np[::2]

        if orig_rate != SAMPLE_RATE:
            n = int(len(audio_np) * SAMPLE_RATE / orig_rate)
            audio_np = np.interp(
                np.linspace(0, len(audio_np), n),
                np.arange(len(audio_np)),
                audio_np.astype(np.float64)
            ).astype(np.int16)

        audio_np = pitch_shift(audio_np, BMO_PITCH_FACTOR)
        audio_np = (audio_np.astype(np.float32) * 1.3).clip(-32768, 32767).astype(np.int16)

        try:
            os.remove(tmp_path)
        except:
            pass

        return audio_np.tobytes()

    except Exception as e:
        print(f"[TTS] Error: {e}")
        return None


def send_tts(pcm_data: bytes):
    total = len(pcm_data)
    for i in range(0, total, TTS_CHUNK_SIZE):
        chunk = pcm_data[i:i + TTS_CHUNK_SIZE]
        tts_sock.sendto(chunk, (ESP32_IP, TTS_PORT))
        time.sleep(0.04)
    print(f"[TTS] Sent {total} bytes to ESP32 speaker")


def find_bmo_response(text_lower: str):
    for trigger, (response, expression) in BMO_RESPONSES.items():
        if trigger in text_lower:
            return response, expression
    return None, None


# ============================================================
# THREAD 1: CAMERA
# ============================================================
def camera_thread():
    global current_pan, current_tilt

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)  # buffer besar
    sock.bind((LOCAL_IP, CAM_PORT))
    sock.settimeout(2.0)
    print(f"[CAM] Listening :{CAM_PORT}")

    frame_buffer     = {}
    current_frame_id = None
    expected_packets = 0
    frame_count      = 0
    fps_time         = time.time()
    fps              = 0.0
    no_data_count    = 0

    while running:
        try:
            data, addr = sock.recvfrom(2048)
            no_data_count = 0
        except socket.timeout:
            no_data_count += 1
            if no_data_count % 5 == 0:
                print(f"[CAM] Tidak ada data dari ESP32... (pastikan serverIP di C = {get_local_ip()})")
            continue

        # Header: 0xAA 0x55 | frameID | totalPackets | packetID | data
        if len(data) < 5:
            continue
        if data[0] != 0xAA or data[1] != 0x55:
            continue

        frame_id      = data[2]
        total_packets = data[3]
        packet_id     = data[4]
        payload       = data[5:]

        if current_frame_id != frame_id:
            frame_buffer     = {}
            current_frame_id = frame_id
            expected_packets = total_packets

        frame_buffer[packet_id] = payload

        if len(frame_buffer) != expected_packets:
            continue

        try:
            jpg = b''.join(frame_buffer[i] for i in range(expected_packets))
        except KeyError:
            frame_buffer = {}
            continue

        img = cv2.imdecode(np.frombuffer(jpg, np.uint8), cv2.IMREAD_COLOR)
        if img is None:
            continue

        # Face detection
        gray  = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.3, minNeighbors=5)

        for (x, y, w, h) in faces:
            cv2.rectangle(img, (x, y), (x + w, y + h), (0, 255, 0), 2)

        # Face tracking â†’ servo
        if len(faces) > 0:
            fx, fy, fw, fh = max(faces, key=lambda f: f[2] * f[3])
            face_cx = fx + fw // 2
            face_cy = fy + fh // 2
            cv2.circle(img, (face_cx, face_cy), 5, (0, 0, 255), -1)

            offset_x = face_cx - (CAM_WIDTH  // 2)
            offset_y = face_cy - (CAM_HEIGHT // 2)

            if abs(offset_x) > SERVO_DEADZONE:
                current_pan = max(0, min(180,
                    current_pan + (-SERVO_STEP if offset_x > 0 else SERVO_STEP)))

            if abs(offset_y) > SERVO_DEADZONE:
                current_tilt = max(0, min(180,
                    current_tilt + (-SERVO_STEP if offset_y > 0 else SERVO_STEP)))

            servo_sock.sendto(
                bytes([int(current_pan), int(current_tilt)]),
                (ESP32_IP, SERVO_PORT)
            )

        # FPS
        frame_count += 1
        elapsed = time.time() - fps_time
        if elapsed >= 1.0:
            fps        = frame_count / elapsed
            frame_count = 0
            fps_time    = time.time()

        # OSD
        cv2.putText(img, f"FPS:{fps:.1f}", (10, 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        cv2.putText(img, f"Pan:{current_pan} Tilt:{current_tilt}", (10, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 0), 1)
        cv2.putText(img, f"Faces:{len(faces)}", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 255), 1)

        cv2.imshow("BMO Robot - Camera", img)
        if cv2.waitKey(1) == 27:
            break

    sock.close()
    cv2.destroyAllWindows()


# ============================================================
# THREAD 2: AUDIO + STT + BMO BRAIN
# ============================================================
def audio_thread():
    global recognition_buffer

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 512 * 1024)
    sock.bind((LOCAL_IP, AUDIO_PORT))
    sock.settimeout(2.0)
    print(f"[AUDIO] Listening :{AUDIO_PORT}")

    recognizer    = sr.Recognizer()
    packet_count  = 0
    no_data_count = 0

    while running:
        try:
            pkt, _ = sock.recvfrom(2048)
            no_data_count = 0
        except socket.timeout:
            no_data_count += 1
            if no_data_count % 5 == 0:
                print(f"[AUDIO] Tidak ada data... (pastikan serverIP di C = {get_local_ip()})")
            continue

        packet_count += 1

        # Header: seq(2 LE) + ts(4 LE) + PCM
        if len(pkt) <= HEADER_BYTES:
            continue

        seq_num = int.from_bytes(pkt[0:2], 'little')
        ts_val  = int.from_bytes(pkt[2:6], 'little')
        pcm     = np.frombuffer(pkt[HEADER_BYTES:], dtype=np.int16).copy()

        # Gain boost
        pcm = (pcm.astype(np.float32) * 2.5).clip(-32768, 32767).astype(np.int16)

        rms = int(np.sqrt(np.mean(pcm.astype(np.float32) ** 2)))
        if packet_count % 20 == 0:  # print tiap 20 paket agar tidak spam
            print(f"[AUDIO] pkt:{packet_count} seq:{seq_num} RMS:{rms}")

        recognition_buffer = np.concatenate((recognition_buffer, pcm))

        # Batas 5 detik
        if len(recognition_buffer) > SAMPLE_RATE * 5:
            recognition_buffer = recognition_buffer[-(SAMPLE_RATE * 5):]

        # Proses STT tiap 3 detik
        if len(recognition_buffer) < RECOG_SAMPLES:
            continue

        chunk              = recognition_buffer[:RECOG_SAMPLES].copy()
        recognition_buffer = recognition_buffer[RECOG_SAMPLES:]

        audio_data = sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2)

        try:
            text = recognizer.recognize_google(audio_data, language="id-ID")
            print(f"[VOICE] >>> {text}")

            text_lower = text.lower().strip()
            response, expression = find_bmo_response(text_lower)

            if response:
                print(f"[BMO] {expression}: {response}")
                send_command(f"EXPR:{expression}")
                time.sleep(0.05)
                send_command("TALK:START")
                time.sleep(0.05)

                def _speak(resp):
                    pcm_bytes = generate_tts_audio(resp)
                    if pcm_bytes:
                        send_tts(pcm_bytes)
                    time.sleep(0.2)
                    send_command("TALK:STOP")

                threading.Thread(target=_speak, args=(response,), daemon=True).start()

            else:
                send_text(text)

        except sr.UnknownValueError:
            print("[VOICE] Tidak dikenali")
        except sr.RequestError as e:
            print(f"[VOICE] STT error: {e}")

    sock.close()


# ============================================================
# MAIN
# ============================================================
def main():
    global running

    pc_ip = get_local_ip()

    print("=" * 55)
    print("  BMO Robot - Integrated Server")
    print("=" * 55)
    print(f"  IP PC ini     : {pc_ip}")
    print(f"  ESP32 IP      : {ESP32_IP}")
    print()

    # Cek apakah PC dan ESP32 satu subnet
    pc_prefix  = ".".join(pc_ip.split(".")[:3])
    esp_prefix = ".".join(ESP32_IP.split(".")[:3])
    if pc_prefix != esp_prefix:
        print(f"  âš ï¸  PERINGATAN: Subnet berbeda!")
        print(f"     PC  : {pc_ip} ({pc_prefix}.x)")
        print(f"     ESP : {ESP32_IP} ({esp_prefix}.x)")
        print(f"     â†’ Sambungkan PC ke hotspot 'redmi' yang sama!")
        print(f"     â†’ Lalu update serverIP di main.cpp ke: {pc_ip}")
    else:
        print(f"  âœ… Subnet sama ({pc_prefix}.x) - OK")
        print(f"  âœ… Pastikan serverIP di main.cpp = {pc_ip}")

    print()
    print(f"  CAM   RX :{CAM_PORT}   AUDIO RX :{AUDIO_PORT}")
    print(f"  TEXT  TX :{TEXT_PORT}   TTS   TX :{TTS_PORT}")
    print(f"  SERVO TX :{SERVO_PORT}")
    print("=" * 55)

    cam_t = threading.Thread(target=camera_thread, daemon=True, name="CamThread")
    aud_t = threading.Thread(target=audio_thread,  daemon=True, name="AudioThread")

    cam_t.start()
    aud_t.start()

    try:
        while running:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[MAIN] Shutting down...")
        running = False
        time.sleep(1)

    cam_t.join(timeout=3)
    aud_t.join(timeout=3)
    print("[MAIN] Done.")


if __name__ == "__main__":
    main()
