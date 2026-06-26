// ============================================================
// INTEGRATED FIRMWARE - ESP32-S3 Audio Board (Waveshare)
// Camera + Mic + Speaker + CYD Display
// (Servo disabled for debugging)
// ============================================================

#include "esp_camera.h"
#include "img_converters.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

using namespace audio_tools;
using namespace audio_driver;

// ============================================================
// WIFI CONFIG
// ============================================================
const char *WIFI_SSID = "Shizu";
const char *WIFI_PASS = "rizaaria12";

// ============================================================
// NETWORK CONFIG
// ============================================================
IPAddress PC_IP(192, 168, 137, 1); // Hotspot laptop IP

// ESP32 -> PC
const uint16_t CAM_PORT = 5000;
const uint16_t AUDIO_PORT = 5005;

// PC -> ESP32
const uint16_t TEXT_PORT = 6000;
const uint16_t TTS_PORT = 6001;

// ============================================================
// UDP SOCKETS
// ============================================================
WiFiUDP udp_cam;
WiFiUDP udp_audio;
WiFiUDP udp_text;
WiFiUDP udp_tts;

// ============================================================
// AUDIO CONFIG
// ============================================================
AudioInfo info(16000, 1, 16);
AudioBoardStream audio(ESP32S3AISmartSpeaker);

static uint16_t seq = 0;
static uint32_t ts = 0;
static const int FRAME_SAMPLES = 320;
static const int FRAME_BYTES = FRAME_SAMPLES * 2;
static uint8_t frame[FRAME_BYTES];
static uint8_t pkt[6 + FRAME_BYTES];

static uint8_t tts_buf[1400];

// ============================================================
// CAMERA CONFIG (DVP - OV2640)
// ============================================================
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

#define UDP_CAM_PAYLOAD 1400
#define FRAME_HEADER_0 0xAA
#define FRAME_HEADER_1 0x55

uint8_t frameID = 0;

// ============================================================
// CYD UART
// ============================================================
HardwareSerial SerialCYD(2);

// ============================================================
// DEBUG COUNTERS
// ============================================================
static uint32_t audioSent = 0;
static uint32_t camSent = 0;
static unsigned long lastDebugMs = 0;

// ============================================================
// WIFI CONNECT
// ============================================================
void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK, IP=");
  Serial.println(WiFi.localIP());
  Serial.print("Target PC_IP=");
  Serial.println(PC_IP);
}

// ============================================================
// CAMERA INIT
// ============================================================
bool camera_init() {
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

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  Serial.println("Camera OK");
  return true;
}

// ============================================================
static size_t mic_got = 0;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(11, 10);
  wifi_connect();

  udp_cam.begin(0);
  udp_audio.begin(0);
  udp_text.begin(TEXT_PORT);
  udp_tts.begin(TTS_PORT);

  SerialCYD.begin(115200, SERIAL_8N1, 8, 9);

  auto cfg = audio.defaultConfig(RXTX_MODE);
  cfg.copyFrom(info);
  if (!audio.begin(cfg)) {
    Serial.println("Audio init FAILED!");
    while (true)
      delay(1000);
  }
  audio.setVolume(0.3f);
  audio.setInputVolume(0.9f);
  Serial.println("Audio OK");

  if (!camera_init()) {
    Serial.println("WARNING: Camera init failed");
  }

  Serial.println("========== SYSTEM READY ==========");
  lastDebugMs = millis();
}

// ============================================================
static unsigned long lastCamMs = 0;
const unsigned long CAM_INTERVAL_MS = 100;

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  // ===== DEBUG: print stats every 3s =====
  if (now - lastDebugMs >= 3000) {
    lastDebugMs = now;
    Serial.printf("[DBG] audio_pkts=%lu cam_frames=%lu mic_avail=%d\n",
                  audioSent, camSent, audio.available());
  }

  // ===== 1. MIC -> UDP =====
  if (audio.available() > 0) {
    int r = audio.readBytes(frame + mic_got, FRAME_BYTES - mic_got);
    if (r > 0)
      mic_got += r;
  }
  if (mic_got >= FRAME_BYTES) {
    pkt[0] = seq & 0xFF;
    pkt[1] = (seq >> 8) & 0xFF;
    pkt[2] = ts & 0xFF;
    pkt[3] = (ts >> 8) & 0xFF;
    pkt[4] = (ts >> 16) & 0xFF;
    pkt[5] = (ts >> 24) & 0xFF;
    memcpy(pkt + 6, frame, FRAME_BYTES);
    udp_audio.beginPacket(PC_IP, AUDIO_PORT);
    udp_audio.write(pkt, sizeof(pkt));
    udp_audio.endPacket();
    seq++;
    ts += FRAME_SAMPLES;
    mic_got = 0;
    audioSent++;
  }

  // ===== 2. CAMERA -> UDP (JPEG) =====
  if (now - lastCamMs >= CAM_INTERVAL_MS) {
    lastCamMs = now;
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      uint8_t *jpg_buf = nullptr;
      size_t jpg_len = 0;
      if (frame2jpg(fb, 60, &jpg_buf, &jpg_len) && jpg_buf && jpg_len > 0) {
        uint16_t totalPackets =
            (jpg_len + UDP_CAM_PAYLOAD - 1) / UDP_CAM_PAYLOAD;
        for (uint16_t i = 0; i < totalPackets; i++) {
          uint32_t offset = i * UDP_CAM_PAYLOAD;
          uint32_t remaining = jpg_len - offset;
          uint16_t chunkSize =
              (remaining > UDP_CAM_PAYLOAD) ? UDP_CAM_PAYLOAD : remaining;
          udp_cam.beginPacket(PC_IP, CAM_PORT);
          udp_cam.write(FRAME_HEADER_0);
          udp_cam.write(FRAME_HEADER_1);
          udp_cam.write(frameID);
          udp_cam.write((uint8_t)totalPackets);
          udp_cam.write((uint8_t)i);
          udp_cam.write(jpg_buf + offset, chunkSize);
          udp_cam.endPacket();
        }
        frameID++;
        camSent++;
        free(jpg_buf);
      }
      esp_camera_fb_return(fb);
    }
  }

  // ===== 3. TTS AUDIO FROM PC -> SPEAKER =====
  int ttsSize = udp_tts.parsePacket();
  if (ttsSize > 0) {
    int len = udp_tts.read(tts_buf, sizeof(tts_buf));
    if (len > 0) {
      audio.write(tts_buf, len);
    }
  }

  // ===== 4. TEXT FROM PC -> CYD =====
  int textSize = udp_text.parsePacket();
  if (textSize > 0) {
    char incoming[256];
    int len = udp_text.read(incoming, sizeof(incoming) - 1);
    if (len > 0) {
      incoming[len] = 0;
      Serial.print("[Text] ");
      Serial.println(incoming);
      SerialCYD.println(incoming);
      SerialCYD.flush();
    }
  }

  // ===== 5. CYD -> Serial Monitor =====
  while (SerialCYD.available()) {
    Serial.write(SerialCYD.read());
  }
}