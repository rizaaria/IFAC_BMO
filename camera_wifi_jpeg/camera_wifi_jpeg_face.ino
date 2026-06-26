#include "esp_camera.h"
#include "img_converters.h"
#include <WiFi.h>
#include <WiFiUdp.h>

const char *ssid = "";
const char *password = "";

IPAddress pcIP(192, 168, 137, 1);
const uint16_t udpPort = 5000;

WiFiUDP udp;

// ==== CAMERA PINS (ESP32-S3 AUDIO BOARD) ====
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

// ==== UDP CONFIG ====
#define UDP_PAYLOAD 1200
#define FRAME_HEADER_0 0xAA
#define FRAME_HEADER_1 0x55

uint8_t frameID = 0;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  udp.begin(udpPort);

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

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

  // 🔥 IMPORTANT
  config.pixel_format = PIXFORMAT_RGB565;

  config.frame_size = FRAMESIZE_QVGA; // jangan VGA dulu
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1)
      ;
  }

  Serial.println("Camera OK");
}

void loop() {

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
    return;

  // ===== CONVERT TO JPEG =====
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;

  bool ok = frame2jpg(fb, 20, &jpg_buf, &jpg_len);

  if (!ok) {
    Serial.println("JPEG conversion failed");
    esp_camera_fb_return(fb);
    return;
  }

  // ===== SEND VIA UDP =====
  uint16_t totalPackets = (jpg_len + UDP_PAYLOAD - 1) / UDP_PAYLOAD;

  for (uint16_t i = 0; i < totalPackets; i++) {

    uint32_t offset = i * UDP_PAYLOAD;
    uint32_t remaining = jpg_len - offset;

    uint16_t chunkSize = (remaining > UDP_PAYLOAD) ? UDP_PAYLOAD : remaining;

    udp.beginPacket(pcIP, udpPort);

    udp.write(FRAME_HEADER_0);
    udp.write(FRAME_HEADER_1);
    udp.write(frameID);
    udp.write(totalPackets);
    udp.write(i);

    udp.write(jpg_buf + offset, chunkSize);

    udp.endPacket();
  }

  frameID++;

  free(jpg_buf); // WAJIB
  esp_camera_fb_return(fb);

  delay(100); // throttle biar stabil
}
