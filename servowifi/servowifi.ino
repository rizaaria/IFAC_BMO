// #include <Arduino.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <Wire.h>
// #include "esp_camera.h"
// #include "img_converters.h"
// #include <Adafruit_PWMServoDriver.h>

// // ================= WIFI =================
// const char *ssid = "";
// const char *password = "";

// IPAddress serverIP(10,60,56,96);

// // ================= PORT =================
// const uint16_t CAM_PORT   = 5000;
// const uint16_t SERVO_PORT = 6002;
// const uint16_t TEXT_PORT  = 6000;

// // ================= UDP =================
// WiFiUDP udp_cam;
// WiFiUDP udp_servo;
// WiFiUDP udp_text;

// // ================= UART CYD =================
// #define RXD2 8   // dari CYD TX (IO22)
// #define TXD2 9   // ke CYD RX (IO27)

// // ================= CAMERA =================
// #define CAM_PIN_PWDN -1
// #define CAM_PIN_RESET -1
// #define CAM_PIN_XCLK 43
// #define CAM_PIN_SIOD 11
// #define CAM_PIN_SIOC 10
// #define CAM_PIN_D7 48
// #define CAM_PIN_D6 47
// #define CAM_PIN_D5 46
// #define CAM_PIN_D4 45
// #define CAM_PIN_D3 39
// #define CAM_PIN_D2 18
// #define CAM_PIN_D1 17
// #define CAM_PIN_D0 2
// #define CAM_PIN_VSYNC 21
// #define CAM_PIN_HREF 1
// #define CAM_PIN_PCLK 44

// #define UDP_CHUNK 1400
// #define FRAME_HEADER_0 0xAA
// #define FRAME_HEADER_1 0x55

// uint8_t frameID = 0;

// // ================= CAMERA INIT =================
// bool camera_init() {
//   camera_config_t config;

//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer   = LEDC_TIMER_0;

//   config.pin_d0 = CAM_PIN_D0;
//   config.pin_d1 = CAM_PIN_D1;
//   config.pin_d2 = CAM_PIN_D2;
//   config.pin_d3 = CAM_PIN_D3;
//   config.pin_d4 = CAM_PIN_D4;
//   config.pin_d5 = CAM_PIN_D5;
//   config.pin_d6 = CAM_PIN_D6;
//   config.pin_d7 = CAM_PIN_D7;

//   config.pin_xclk = CAM_PIN_XCLK;
//   config.pin_pclk = CAM_PIN_PCLK;
//   config.pin_vsync = CAM_PIN_VSYNC;
//   config.pin_href = CAM_PIN_HREF;

//   config.pin_sccb_sda = CAM_PIN_SIOD;
//   config.pin_sccb_scl = CAM_PIN_SIOC;

//   config.pin_pwdn = CAM_PIN_PWDN;
//   config.pin_reset = CAM_PIN_RESET;

//   config.xclk_freq_hz = 20000000;
//   config.pixel_format = PIXFORMAT_RGB565;

//   config.frame_size = FRAMESIZE_QVGA;
//   config.fb_count = 1;
//   config.fb_location = CAMERA_FB_IN_PSRAM;
//   config.grab_mode = CAMERA_GRAB_LATEST;

//   if (esp_camera_init(&config) != ESP_OK) {
//     Serial.println("Camera init failed");
//     return false;
//   }

//   Serial.println("[Camera] OK");
//   return true;
// }

// // ================= SERVO PCA =================
// #define PCA_SDA 6
// #define PCA_SCL 7

// Adafruit_PWMServoDriver pca9685 = Adafruit_PWMServoDriver(0x40, Wire);

// #define SERVOMIN 150
// #define SERVOMAX 600

// uint16_t angleToPulse(int angle) {
//   return map(angle, 0, 180, SERVOMIN, SERVOMAX);
// }

// void setServo(uint8_t ch, int angle) {
//   angle = constrain(angle, 0, 180);
//   pca9685.setPWM(ch, 0, angleToPulse(angle));
// }

// void servo_init() {
//   Wire.begin(PCA_SDA, PCA_SCL);
//   pca9685.begin();
//   pca9685.setPWMFreq(50);

//   setServo(0, 90); // atas (tilt)
//   setServo(1, 90); // bawah (pan)

//   Serial.println("[Servo] OK");
// }

// // ================= WIFI =================
// void wifi_connect() {
//   WiFi.begin(ssid, password);

//   Serial.print("Connecting WiFi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }

//   Serial.println("\nWiFi Connected");
//   Serial.println(WiFi.localIP());
// }

// // ================= SETUP =================
// void setup() {
//   Serial.begin(115200);

//   wifi_connect();

//   udp_cam.begin(0);
//   udp_servo.begin(SERVO_PORT);
//   udp_text.begin(TEXT_PORT);

//   Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

//   camera_init();
//   servo_init();

//   Serial.println("=== READY ===");
// }

// // ================= LOOP =================
// void loop() {

//   // ===== CAMERA STREAM =====
//   camera_fb_t *fb = esp_camera_fb_get();

//   if (fb) {
//     uint8_t *jpg_buf = NULL;
//     size_t jpg_len = 0;

//     if (frame2jpg(fb, 70, &jpg_buf, &jpg_len)) {

//       uint16_t totalPackets = (jpg_len + UDP_CHUNK - 1) / UDP_CHUNK;

//       for (uint16_t i = 0; i < totalPackets; i++) {

//         uint32_t offset = i * UDP_CHUNK;
//         uint16_t size = min((uint32_t)UDP_CHUNK, jpg_len - offset);

//         udp_cam.beginPacket(serverIP, CAM_PORT);
//         udp_cam.write(FRAME_HEADER_0);
//         udp_cam.write(FRAME_HEADER_1);
//         udp_cam.write(frameID);
//         udp_cam.write(totalPackets);
//         udp_cam.write(i);
//         udp_cam.write(jpg_buf + offset, size);
//         udp_cam.endPacket();
//       }

//       frameID++;
//       free(jpg_buf);
//     }

//     esp_camera_fb_return(fb);
//   }

//   // ===== SERVO CONTROL =====
//   int packetSize = udp_servo.parsePacket();
//   if (packetSize >= 2) {
//     uint8_t buf[2];
//     udp_servo.read(buf, 2);

//     int pan  = buf[0];
//     int tilt = buf[1];

//     setServo(0, tilt); // atas
//     setServo(1, pan);  // bawah
//   }

//   // ===== TEXT COMMAND → CYD =====
//   int textSize = udp_text.parsePacket();
//   if (textSize) {
//     char buf[100];
//     int len = udp_text.read(buf, 100);
//     if (len > 0) {
//       buf[len] = '\0';

//       Serial.println(buf);

//       Serial2.print(buf);
//       Serial2.print("\n");
//     }
//   }

//   delay(5);
// }

#include "esp_camera.h"
#include "img_converters.h"
#include <Adafruit_PWMServoDriver.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>


// AUDIO
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

using namespace audio_tools;
using namespace audio_driver;

// ================= WIFI =================
const char *ssid = "";
const char *password = "";
IPAddress serverIP(10, 60, 56, 96);

// ================= PORT =================
const uint16_t CAM_PORT = 5000;
const uint16_t AUDIO_PORT = 5005;
const uint16_t SERVO_PORT = 6002;
const uint16_t TEXT_PORT = 6000;
const uint16_t TTS_PORT = 6001;

// ================= UDP =================
WiFiUDP udp_cam, udp_audio, udp_servo, udp_text, udp_tts;

// ================= AUDIO =================
AudioInfo info(16000, 1, 16);
AudioBoardStream audio(ESP32S3AISmartSpeaker);

#define FRAME_SAMPLES 320
#define FRAME_BYTES (FRAME_SAMPLES * 2)

uint8_t frame[FRAME_BYTES];
uint8_t pkt[6 + FRAME_BYTES];

uint16_t seq = 0;
uint32_t ts = 0;
size_t mic_got = 0;

uint8_t tts_buf[1400];

// ================= UART CYD =================
#define RXD2 8
#define TXD2 9

// ================= CAMERA =================
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 43
#define CAM_PIN_SIOD 11
#define CAM_PIN_SIOC 10
#define CAM_PIN_D7 48
#define CAM_PIN_D6 47
#define CAM_PIN_D5 46
#define CAM_PIN_D4 45
#define CAM_PIN_D3 39
#define CAM_PIN_D2 18
#define CAM_PIN_D1 17
#define CAM_PIN_D0 2
#define CAM_PIN_VSYNC 21
#define CAM_PIN_HREF 1
#define CAM_PIN_PCLK 44

#define UDP_CHUNK 1400
#define FRAME_HEADER_0 0xAA
#define FRAME_HEADER_1 0x55

uint8_t frameID = 0;

// ================= SERVO (I2C1) =================
#define PCA_SDA 5
#define PCA_SCL 6

Adafruit_PWMServoDriver pca9685 = Adafruit_PWMServoDriver(0x40, Wire1);

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
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\n[WiFi] Connected");
  Serial.println(WiFi.localIP());
}

// ================= CAMERA =================
bool camera_init() {
  Serial.println("[Camera] Init...");

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;

  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;

  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;

  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;

  config.frame_size = FRAMESIZE_QVGA;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("[Camera] FAILED ❌");
    return false;
  }

  Serial.println("[Camera] OK ✅");
  return true;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  wifi_connect();

  // 🔥 1. AUDIO dulu (dia klaim I2C)
  auto cfg = audio.defaultConfig(RXTX_MODE);
  cfg.copyFrom(info);

  if (!audio.begin(cfg)) {
    Serial.println("[Audio] FAIL ❌");
    while (1)
      ;
  }
  Serial.println("[Audio] OK ✅");

  // 🔥 2. CAMERA setelah audio
  if (!camera_init()) {
    Serial.println("STOP: Camera error");
    while (1)
      ;
  }

  // 🔥 3. BARU SERVO (Wire1 beda bus)
  Wire1.begin(PCA_SDA, PCA_SCL);

  pca9685.begin();
  pca9685.setPWMFreq(50);

  setServo(0, 90);
  setServo(1, 90);

  Serial.println("========== SYSTEM READY ==========");
}

// ================= LOOP =================
void loop() {

  // ===== MIC STREAM =====
  if (audio.available() > 0) {
    int r = audio.readBytes(frame + mic_got, FRAME_BYTES - mic_got);
    if (r > 0)
      mic_got += r;
  }

  if (mic_got >= FRAME_BYTES) {
    pkt[0] = seq & 0xFF;
    pkt[1] = seq >> 8;
    pkt[2] = ts & 0xFF;
    pkt[3] = ts >> 8;
    pkt[4] = ts >> 16;
    pkt[5] = ts >> 24;

    memcpy(pkt + 6, frame, FRAME_BYTES);

    udp_audio.beginPacket(serverIP, AUDIO_PORT);
    udp_audio.write(pkt, sizeof(pkt));
    udp_audio.endPacket();

    seq++;
    ts += FRAME_SAMPLES;
    mic_got = 0;
  }

  // ===== CAMERA STREAM =====
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    if (frame2jpg(fb, 70, &jpg_buf, &jpg_len)) {
      uint16_t totalPackets = (jpg_len + UDP_CHUNK - 1) / UDP_CHUNK;

      for (uint16_t i = 0; i < totalPackets; i++) {
        uint32_t offset = i * UDP_CHUNK;
        uint16_t size = min((uint32_t)UDP_CHUNK, jpg_len - offset);

        udp_cam.beginPacket(serverIP, CAM_PORT);
        udp_cam.write(FRAME_HEADER_0);
        udp_cam.write(FRAME_HEADER_1);
        udp_cam.write(frameID);
        udp_cam.write(totalPackets);
        udp_cam.write(i);
        udp_cam.write(jpg_buf + offset, size);
        udp_cam.endPacket();
      }

      frameID++;
      free(jpg_buf);
    }

    esp_camera_fb_return(fb);
  }

  // ===== TTS PLAYBACK =====
  int ttsSize = udp_tts.parsePacket();
  if (ttsSize > 0) {
    int len = udp_tts.read(tts_buf, sizeof(tts_buf));
    if (len > 0) {
      audio.write(tts_buf, len);
    }
  }

  // ===== SERVO =====
  int packetSize = udp_servo.parsePacket();
  if (packetSize >= 2) {
    uint8_t buf[2];
    udp_servo.read(buf, 2);

    setServo(0, buf[1]); // tilt
    setServo(1, buf[0]); // pan
  }

  // ===== TEXT → CYD =====
  int textSize = udp_text.parsePacket();
  if (textSize) {
    char buf[100];
    int len = udp_text.read(buf, 100);

    if (len > 0) {
      buf[len] = '\0';
      Serial.println("[TEXT] " + String(buf));
      Serial2.println(buf);
    }
  }
}
