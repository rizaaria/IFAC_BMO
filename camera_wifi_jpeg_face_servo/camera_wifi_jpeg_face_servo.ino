#include "esp_camera.h"
#include "img_converters.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ================= WIFI =================
const char *ssid = "SHIZUDELTA";
const char *password = "rizaaria12";

IPAddress serverIP(192, 168, 137, 1);

// ================= PORT =================
const uint16_t CAM_PORT   = 5000;
const uint16_t SERVO_PORT = 6002;

// ================= UDP =================
WiFiUDP udp_cam;
WiFiUDP udp_servo;

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

#define UDP_CHUNK 1200
#define FRAME_HEADER_0 0xAA
#define FRAME_HEADER_1 0x55

uint8_t frameID = 0;

// ================= SERVO =================
#define SDA_PIN 5
#define SCL_PIN 6

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
  while (WiFi.status() != WL_CONNECTED) delay(300);

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// ================= CAMERA =================
bool camera_init() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

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

  config.frame_size = FRAMESIZE_QQVGA; // 🔥 lebih ringan
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return false;
  }

  return true;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  wifi_connect();

  udp_cam.begin(0);
  udp_servo.begin(SERVO_PORT);

  camera_init();

  // 🔥 FIX I2C (pakai Wire, bukan Wire1)
  Wire.begin(SDA_PIN, SCL_PIN);

  pca9685.begin();
  pca9685.setPWMFreq(50);

  setServo(0, 90); // tilt
  setServo(1, 90); // pan
}

// ================= LOOP =================
void loop() {

  // ===== CAMERA =====
  camera_fb_t *fb = esp_camera_fb_get();

  if (fb) {
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    if (frame2jpg(fb, 50, &jpg_buf, &jpg_len)) {

      uint16_t totalPackets = (jpg_len + UDP_CHUNK - 1) / UDP_CHUNK;

      for (uint16_t i = 0; i < totalPackets; i++) {
        uint32_t offset = i * UDP_CHUNK;
        uint32_t remaining = jpg_len - offset;
        uint16_t size = (remaining > UDP_CHUNK) ? UDP_CHUNK : remaining;

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

  // ===== SERVO =====
  int packetSize = udp_servo.parsePacket();
  if (packetSize >= 2) {
    uint8_t buf[2];
    udp_servo.read(buf, 2);

    setServo(0, buf[1]); // tilt
    setServo(1, buf[0]); // pan
  }
}
