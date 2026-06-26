#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

using namespace audio_tools;
using namespace audio_driver;

// ================= WIFI =================
const char *ssid = "";
const char *password = "";

IPAddress serverIP(192, 168, 137, 1);

// ================= PORT =================
const uint16_t AUDIO_PORT = 5005;
const uint16_t TTS_PORT = 6001;
const uint16_t SERVO_PORT = 6002;
const uint16_t CAM_PORT = 5000;

// ================= UDP =================
WiFiUDP udp_audio;
WiFiUDP udp_tts;
WiFiUDP udp_servo;

// ================= AUDIO =================
AudioInfo info(16000, 1, 16);
AudioBoardStream audio(ESP32S3AISmartSpeaker);

#define FRAME_SAMPLES 320
#define FRAME_BYTES (FRAME_SAMPLES * 2)

uint8_t frame[FRAME_BYTES];
uint16_t mic_got = 0;

uint8_t tts_buf[1400];

// ================= SERVO =================
Adafruit_PWMServoDriver pca9685 = Adafruit_PWMServoDriver(0x40, Wire);

#define SERVOMIN 150
#define SERVOMAX 600

uint16_t angleToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

void setServo(uint8_t ch, int angle) {
  angle = constrain(angle, 0, 180);
  pca9685.setPWM(ch, 0, angleToPulse(angle));
}

// ================= WIFI =================
void wifi_connect() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  Serial.println(WiFi.localIP());
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  wifi_connect();

  udp_audio.begin(0);
  udp_tts.begin(TTS_PORT);
  udp_servo.begin(SERVO_PORT);

  // AUDIO
  auto cfg = audio.defaultConfig(RXTX_MODE);
  cfg.copyFrom(info);

  audio.begin(cfg);
  audio.setVolume(0.4);

  // SERVO
  Wire.begin(5, 6); // sesuai setup kamu yang working
  pca9685.begin();
  pca9685.setPWMFreq(50);

  setServo(0, 90);
  setServo(1, 90);
}

// ================= LOOP =================
void loop() {

  // ===== MIC =====
  if (audio.available()) {
    int r = audio.readBytes(frame + mic_got, FRAME_BYTES - mic_got);
    if (r > 0)
      mic_got += r;
  }

  if (mic_got >= FRAME_BYTES) {

    int16_t *samples = (int16_t *)frame;

    for (int i = 0; i < FRAME_SAMPLES; i++) {
      samples[i] *= 5;
    }

    udp_audio.beginPacket(serverIP, AUDIO_PORT);
    udp_audio.write(frame, FRAME_BYTES);
    udp_audio.endPacket();

    mic_got = 0;
  }

  // ===== TTS =====
  int ttsSize = udp_tts.parsePacket();
  if (ttsSize > 0) {
    int len = udp_tts.read(tts_buf, sizeof(tts_buf));
    if (len > 0)
      audio.write(tts_buf, len);
  }

  // ===== SERVO =====
  int packetSize = udp_servo.parsePacket();
  if (packetSize >= 2) {
    uint8_t buf[2];
    udp_servo.read(buf, 2);

    setServo(0, buf[1]);
    setServo(1, buf[0]);
  }
}
