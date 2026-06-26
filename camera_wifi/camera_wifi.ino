#include "esp_camera.h"
#include <WiFi.h>

const char *ssid = "SHIZUDELTA";
const char *password = "rizaaria12";

IPAddress pcIP(192, 168, 137, 1); // GANTI IP PC
const uint16_t udpPort = 5000;

WiFiUDP udp;

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

// Header framing bytes
#define UDP_PAYLOAD 1400
#define FRAME_HEADER_0 0xAA
#define FRAME_HEADER_1 0x55

uint8_t frameID = 0;

void setup() {
  Serial.begin(2000000);

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
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.fb_count = 1;
  config.fb_location =
      CAMERA_FB_IN_PSRAM; // <<< GUNAKAN PSRAM untuk frame buffer
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while (true)
      delay(1000);
  }

  Serial.println("Camera OK");
}

void loop() {

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
    return;

  uint32_t totalSize = fb->len;
  uint16_t totalPackets = (totalSize + UDP_PAYLOAD - 1) / UDP_PAYLOAD;

  for (uint16_t i = 0; i < totalPackets; i++) {
    uint32_t offset = i * UDP_PAYLOAD;
    uint32_t remaining = totalSize - offset;
    uint16_t chunkSize = (remaining > UDP_PAYLOAD) ? UDP_PAYLOAD : remaining;

    udp.beginPacket(pcIP, udpPort);

    udp.write(FRAME_HEADER_0);
    udp.write(FRAME_HEADER_1);
    udp.write(frameID);
    udp.write(totalPackets);
    udp.write(i);

    udp.write(fb->buf + offset, chunkSize);

    udp.endPacket();
  }

  frameID++;
  esp_camera_fb_return(fb);
}