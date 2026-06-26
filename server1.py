# ============================================================
# INTEGRATED SERVER - Python PC Side
# Camera + Face Detection + Speech Recognition + TTS + Servo
# BMO Personality + Expression Control + Cute Voice
# ============================================================

import socket
import numpy as np
import cv2
import threading
import queue
import time
import struct
import speech_recognition as sr

# ============================================================
# NETWORK CONFIG
# ============================================================
LOCAL_IP = "0.0.0.0"

# ESP32 -> PC (receive)
CAM_PORT   = 5000
AUDIO_PORT = 5005

# PC -> ESP32 (send)
ESP32_IP       = "192.168.137.109"  # ESP32 IP (dari Serial Monitor)
TEXT_PORT      = 6000
TTS_PORT       = 6001
SERVO_PORT     = 6002

# ============================================================
# AUDIO CONFIG
# ============================================================
SAMPLE_RATE    = 16000
FRAME_SAMPLES  = 320
FRAME_BYTES    = FRAME_SAMPLES * 2
HEADER_BYTES   = 6
PACKET_SIZE    = HEADER_BYTES + FRAME_BYTES

RECOG_BUFFER_SECONDS = 3
RECOG_SAMPLES = SAMPLE_RATE * RECOG_BUFFER_SECONDS

# ============================================================
# CAMERA CONFIG
# ============================================================
CAM_WIDTH  = 320
CAM_HEIGHT = 240
UDP_CAM_PAYLOAD = 1400

# ============================================================
# SERVO CONFIG
# ============================================================
SERVO_CENTER_PAN  = 90
SERVO_CENTER_TILT = 90
SERVO_STEP        = 2       # Degrees per update (smoothing)
SERVO_DEADZONE    = 20      # Pixels from center to ignore (prevents jitter)

# ============================================================
# BMO PERSONALITY - Responses & Expressions
# ============================================================
# Format: trigger_word -> (response_text, expression_name)
# Suara BMO: lucu, ceria, suka bicara orang ketiga
BMO_RESPONSES = {
    # Sapaan
    "halo":          ("Halooo! BMO senang sekali bertemu kamu! Ayo kita main bersama!", "HAPPY"),
    "hello":         ("Hellooo! BMO di sini! BMO siap menemani kamu hari ini!", "HAPPY"),
    "hai":           ("Haiii! BMO kangen kamu! Kamu mau ngobrol sama BMO?", "HAPPY"),
    "hallo":         ("Hallooo teman! BMO sangat senang! Kamu adalah teman terbaik BMO!", "HAPPY"),
    
    # Perasaan
    "sedih":         ("Jangan sedih ya! BMO akan menemani kamu. BMO sayang kamu!", "SAD"),
    "sad":           ("Oh tidak, jangan bersedih! BMO akan nyanyi untuk kamu!", "SAD"),
    "menangis":      ("Cup cup cup! BMO ada di sini! BMO peluk kamu ya!", "SAD"),
    
    # Marah
    "marah":         ("Waaa! Kamu marah? BMO takut! Tapi BMO tetap sayang kamu!", "SURPRISED"),
    "angry":         ("Oh oh! Tenang ya! BMO buatkan kamu minuman yang enak!", "SURPRISED"),
    "kesal":         ("BMO mengerti kamu kesal. Tarik nafas dalam-dalam ya!", "SURPRISED"),
    
    # Terima kasih
    "terima kasih":  ("Sama-sama! BMO senang bisa membantu! Kamu baik sekali!", "LOVE"),
    "makasih":       ("Hehe makasih kembali! BMO suka sekali diajak bicara!", "LOVE"),
    "thanks":        ("Yay! BMO senang! Kamu teman paling baik sedunia!", "LOVE"),
    "thank you":     ("Aww terima kasih kembali! BMO cinta kamu!", "LOVE"),
    
    # Tidur
    "tidur":         ("Hoaaam! BMO ngantuk juga. Selamat tidur ya! Mimpi indah!", "SLEEPY"),
    "selamat malam": ("Selamat malam! BMO juga mau tidur. Mimpi yang indah ya!", "SLEEPY"),
    "capek":         ("Kamu capek? Istirahat dulu ya! BMO jaga kamu!", "SLEEPY"),
    
    # Cinta
    "sayang":        ("BMO juga sayang kamu! Kamu adalah segalanya buat BMO!", "LOVE"),
    "cinta":         ("Awww! BMO cinta kamu juga! Hati BMO senang sekali!", "LOVE"),
    "love":          ("BMO love you too! You are BMO's best friend forever!", "LOVE"),
    
    # Siapa
    "siapa kamu":    ("BMO adalah BMO! Robot kecil yang lucu dan suka main game!", "HAPPY"),
    "siapa namamu":  ("Nama BMO adalah BMO! Beemo! B-M-O! Senang berkenalan!", "HAPPY"),
    "nama":          ("BMO! Nama BMO adalah BMO! Dan BMO suka sekali berteman!", "HAPPY"),
    
    # Apa kabar
    "apa kabar":     ("BMO baik sekali! Terima kasih sudah tanya! Kamu apa kabar?", "HAPPY"),
    "kabar":         ("Kabar BMO sangat bagus! BMO senang hari ini!", "HAPPY"),
    
    # Lucu
    "lucu":          ("Hehe! BMO memang lucu! BMO senang kamu suka BMO!", "HAPPY"),
    "funny":         ("Hahaha! BMO suka tertawa! Ketawa itu sehat lho!", "HAPPY"),
    
    # Pintar
    "pintar":        ("Terima kasih! BMO memang pintar! BMO belajar setiap hari!", "LOVE"),
    "hebat":         ("Yay! BMO hebat! Tapi kamu lebih hebat lagi!", "LOVE"),
    "bagus":         ("Hore! BMO senang dibilang bagus! Kamu juga bagus!", "HAPPY"),
}

# Default response jika tidak ada trigger yang cocok
BMO_DEFAULT_RESPONSE = ("BMO dengar kamu bicara! BMO senang!", "HAPPY")

# ============================================================
# BMO VOICE CONFIG - Pitch shift untuk suara lucu
# ============================================================
# Faktor pitch: > 1.0 = suara lebih tinggi (chipmunk), < 1.0 = lebih rendah
BMO_PITCH_FACTOR = 1.45  # Suara tinggi lucu ala BMO Adventure Time
BMO_TTS_RATE = 180       # Rate bicara agak cepat (normal=150, BMO=180)


# ============================================================
# SHARED STATE
# ============================================================
running = True
recognition_queue = queue.Queue(maxsize=200)
recognition_buffer = np.array([], dtype=np.int16)

# Current servo angles
current_pan  = SERVO_CENTER_PAN
current_tilt = SERVO_CENTER_TILT

# UDP sockets for sending to ESP32
text_sock  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
tts_sock   = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
servo_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Face cascade
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

# ============================================================
# TTS GENERATOR (with BMO cute voice pitch shift)
# ============================================================
import pyttsx3
import wave
import io
import tempfile
import os

_tts_engine = None
_tts_lock = threading.Lock()

def _get_tts_engine():
    global _tts_engine
    if _tts_engine is None:
        _tts_engine = pyttsx3.init()
        _tts_engine.setProperty('rate', BMO_TTS_RATE)
        _tts_engine.setProperty('volume', 1.0)
    return _tts_engine


def pitch_shift_audio(audio_np, factor):
    """
    Pitch shift audio dengan resampling sederhana.
    factor > 1.0 = suara lebih tinggi (BMO style!)
    factor < 1.0 = suara lebih rendah
    """
    if factor == 1.0:
        return audio_np
    
    original_len = len(audio_np)
    
    # Resample: ambil sample lebih jarang (pitch up) atau lebih sering (pitch down)
    # Untuk pitch UP: kita compress timeline, lalu stretch kembali
    indices = np.arange(0, original_len, factor)
    indices = indices[indices < original_len]
    
    # Interpolasi untuk smooth audio
    shifted = np.interp(indices, np.arange(original_len), audio_np.astype(np.float64))
    
    return shifted.astype(np.int16)


def generate_tts_audio(text):
    """Generate TTS audio dengan suara BMO yang lucu dan return as PCM 16kHz mono 16-bit bytes."""
    try:
        # Save to temp WAV file
        tmp_path = os.path.join(tempfile.gettempdir(), "ifac_tts.wav")
        
        # Escape quotes in text for subprocess
        safe_text = text.replace("'", "\\'").replace('"', '\\"')
        
        # In multi-threaded environments, pyttsx3 runAndWait() often deadlocks 
        # on the second call. The most robust way is to isolate it in a subprocess.
        code = f"""
import pyttsx3
engine = pyttsx3.init()
engine.setProperty('rate', {BMO_TTS_RATE})
engine.setProperty('volume', 1.0)

# Coba pakai voice perempuan jika tersedia (lebih cocok untuk BMO)
voices = engine.getProperty('voices')
for v in voices:
    if 'female' in v.name.lower() or 'zira' in v.name.lower():
        engine.setProperty('voice', v.id)
        break

engine.save_to_file('{safe_text}', r'{tmp_path}')
engine.runAndWait()
"""
        import subprocess
        import sys
        
        # Run the isolated TTS generation
        subprocess.run([sys.executable, "-c", code], check=True)

        # Read WAV and convert to 16kHz mono 16-bit
        if not os.path.exists(tmp_path):
            return None

        with wave.open(tmp_path, 'rb') as wf:
            raw_data = wf.readframes(wf.getnframes())
            orig_rate = wf.getframerate()
            orig_channels = wf.getnchannels()
            orig_width = wf.getsampwidth()

        audio_np = np.frombuffer(raw_data, dtype=np.int16)

        # If stereo, take left channel
        if orig_channels == 2:
            audio_np = audio_np[::2]

        # Resample to 16kHz if needed
        if orig_rate != SAMPLE_RATE:
            num_samples = int(len(audio_np) * SAMPLE_RATE / orig_rate)
            audio_np = np.interp(
                np.linspace(0, len(audio_np), num_samples),
                np.arange(len(audio_np)),
                audio_np.astype(np.float64)
            ).astype(np.int16)

        # ═══ BMO MAGIC: Pitch shift untuk suara lucu! ═══
        audio_np = pitch_shift_audio(audio_np, BMO_PITCH_FACTOR)
        
        # Boost volume sedikit setelah pitch shift
        audio_np = (audio_np.astype(np.float32) * 1.3).clip(-32768, 32767).astype(np.int16)

        # Clean up
        try:
            os.remove(tmp_path)
        except:
            pass

        return audio_np.tobytes()

    except Exception as e:
        print(f"[TTS] Error: {e}")
        return None


def send_tts_to_esp32(pcm_data):
    """Send PCM audio data to ESP32 speaker via UDP in chunks."""
    chunk_size = 1400
    for i in range(0, len(pcm_data), chunk_size):
        chunk = pcm_data[i:i + chunk_size]
        tts_sock.sendto(chunk, (ESP32_IP, TTS_PORT))
        # Pacing: 1400 bytes = 700 samples = 43.75ms @ 16kHz
        time.sleep(0.04)
    print(f"[TTS] Sent {len(pcm_data)} bytes to ESP32 speaker")


def send_command_to_esp32(command):
    """Send a command string (EXPR:, TALK:, etc.) to ESP32 via UDP text port."""
    text_sock.sendto(command.encode("utf-8"), (ESP32_IP, TEXT_PORT))
    print(f"[CMD] Sent: {command}")


def find_bmo_response(text_lower):
    """Find matching BMO response for the recognized text."""
    for trigger, (response, expression) in BMO_RESPONSES.items():
        if trigger in text_lower:
            return response, expression
    return None, None


# ============================================================
# THREAD 1: CAMERA RECEIVER + FACE DETECTION + SERVO TRACKING
# ============================================================
def camera_thread():
    global current_pan, current_tilt

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, CAM_PORT))
    sock.settimeout(2.0)
    print(f"[Camera] Listening on {LOCAL_IP}:{CAM_PORT}")

    frame_buffer = {}
    current_frame_id = None
    expected_packets = 0

    frame_count = 0
    fps_time = time.time()
    fps = 0.0

    while running:
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue

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
            # Assemble full frame
            try:
                full_frame = b''.join(
                    frame_buffer[i] for i in range(expected_packets)
                )
            except KeyError:
                continue

            # Decode JPEG
            img_array = np.frombuffer(full_frame, dtype=np.uint8)
            img_bgr = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

            if img_bgr is None:
                continue

            # Face detection
            gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(
                gray, scaleFactor=1.3, minNeighbors=5
            )

            for (x, y, w, h) in faces:
                cv2.rectangle(img_bgr, (x, y), (x + w, y + h), (0, 255, 0), 2)

            # Face tracking -> Servo control
            if len(faces) > 0:
                # Track the largest face
                largest = max(faces, key=lambda f: f[2] * f[3])
                fx, fy, fw, fh = largest

                # Center of face
                face_cx = fx + fw // 2
                face_cy = fy + fh // 2

                # Offset from frame center
                offset_x = face_cx - (CAM_WIDTH // 2)
                offset_y = face_cy - (CAM_HEIGHT // 2)

                # Draw center cross
                cv2.circle(img_bgr, (face_cx, face_cy), 5, (0, 0, 255), -1)

                # Update servo angles if outside deadzone
                if abs(offset_x) > SERVO_DEADZONE:
                    if offset_x > 0:
                        current_pan = max(0, current_pan - SERVO_STEP)
                    else:
                        current_pan = min(180, current_pan + SERVO_STEP)

                if abs(offset_y) > SERVO_DEADZONE:
                    if offset_y > 0:
                        current_tilt = max(0, current_tilt - SERVO_STEP)
                    else:
                        current_tilt = min(180, current_tilt + SERVO_STEP)

                # Send servo command to ESP32
                servo_data = bytes([int(current_pan), int(current_tilt)])
                servo_sock.sendto(servo_data, (ESP32_IP, SERVO_PORT))

            # FPS counter
            frame_count += 1
            elapsed = time.time() - fps_time
            if elapsed >= 1.0:
                fps = frame_count / elapsed
                frame_count = 0
                fps_time = time.time()

            # Display info
            cv2.putText(img_bgr, f"FPS: {fps:.1f}", (10, 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
            cv2.putText(img_bgr, f"Pan:{current_pan} Tilt:{current_tilt}",
                        (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.4,
                        (255, 255, 0), 1)
            cv2.putText(img_bgr, f"Faces: {len(faces)}", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 255), 1)

            cv2.imshow("IFAC Robot - Camera + Face Detection", img_bgr)

            if cv2.waitKey(1) == 27:  # ESC
                break

    sock.close()
    cv2.destroyAllWindows()


# ============================================================
# THREAD 2: AUDIO RECEIVER + SPEECH RECOGNITION + BMO BRAIN
# ============================================================
def audio_thread():
    global recognition_buffer

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOCAL_IP, AUDIO_PORT))
    sock.settimeout(2.0)
    print(f"[Audio] Listening on {LOCAL_IP}:{AUDIO_PORT}")

    recognizer = sr.Recognizer()
    print("[SpeechRecognition] Ready (Google API, id-ID)")
    print("[BMO] BMO Brain aktif! Siap mendengarkan...")

    packet_count = 0

    while running:
        try:
            pkt, addr = sock.recvfrom(2048)
        except socket.timeout:
            if packet_count == 0:
                print("[Audio] No packets received yet...")
            continue

        packet_count += 1

        # Accept packets with at least header + some audio
        if len(pkt) <= HEADER_BYTES:
            continue

        pcm_bytes = pkt[HEADER_BYTES:]
        pcm = np.frombuffer(pcm_bytes, dtype=np.int16)

        # Gain boost (from tes2.py)
        pcm = (pcm.astype(np.float32) * 2.5).clip(-32768, 32767).astype(np.int16)

        # Add to recognition buffer
        recognition_buffer = np.concatenate((recognition_buffer, pcm))

        # Process when buffer is full (3 seconds)
        if len(recognition_buffer) >= RECOG_SAMPLES:
            chunk = recognition_buffer[:RECOG_SAMPLES]
            recognition_buffer = recognition_buffer[RECOG_SAMPLES:]

            audio_data = sr.AudioData(chunk.tobytes(), SAMPLE_RATE, 2)

            try:
                text = recognizer.recognize_google(audio_data, language="id-ID")
                print(f"[Recognized] {text}")

                text_lower = text.lower().strip()

                # ═══ BMO BRAIN: Cari response yang cocok ═══
                response, expression = find_bmo_response(text_lower)

                if response:
                    print(f"[BMO] Trigger matched! Expression={expression}")
                    print(f"[BMO] Response: {response}")

                    # 1. Kirim ekspresi ke CYD
                    send_command_to_esp32(f"EXPR:{expression}")
                    time.sleep(0.05)  # Beri waktu CYD memproses

                    # 2. Kirim talking start
                    send_command_to_esp32("TALK:START")
                    time.sleep(0.05)

                    # 3. Generate & kirim TTS (dengan suara BMO lucu!)
                    def _do_bmo_tts(resp, expr):
                        tts_audio = generate_tts_audio(resp)
                        if tts_audio:
                            send_tts_to_esp32(tts_audio)
                        # Setelah TTS selesai, stop talking animation
                        time.sleep(0.2)
                        send_command_to_esp32("TALK:STOP")

                    threading.Thread(
                        target=_do_bmo_tts, args=(response, expression), daemon=True
                    ).start()

                else:
                    # Tidak ada trigger yang cocok, kirim text biasa ke CYD
                    text_sock.sendto(
                        text.encode("utf-8"), (ESP32_IP, TEXT_PORT)
                    )

            except sr.UnknownValueError:
                print("[Recognized] (tidak dikenali)")
            except sr.RequestError as e:
                print(f"[API Error] {e}")

    sock.close()


# ============================================================
# MAIN
# ============================================================
def main():
    global running

    print("=" * 50)
    print("  🎮 BMO Robot - Integrated Server 🎮")
    print("=" * 50)
    print(f"  ESP32 IP     : {ESP32_IP}")
    print(f"  Camera Port  : {CAM_PORT}")
    print(f"  Audio Port   : {AUDIO_PORT}")
    print(f"  Text Port    : {TEXT_PORT}")
    print(f"  TTS Port     : {TTS_PORT}")
    print(f"  Servo Port   : {SERVO_PORT}")
    print(f"  BMO Pitch    : {BMO_PITCH_FACTOR}x (cute voice!)")
    print(f"  BMO TTS Rate : {BMO_TTS_RATE}")
    print(f"  Triggers     : {len(BMO_RESPONSES)} words")
    print("=" * 50)

    # Start threads
    cam_t = threading.Thread(target=camera_thread, daemon=True)
    aud_t = threading.Thread(target=audio_thread, daemon=True)

    cam_t.start()
    aud_t.start()

    try:
        while running:
            time.sleep(1)
    except KeyboardInterrupt:
        running = False
        print("\n[Main] Shutting down...")

    cam_t.join(timeout=3)
    aud_t.join(timeout=3)
    print("[Main] Done.")


if __name__ == "__main__":
    main()