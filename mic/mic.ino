// Mic + Without CYD
// #include <Arduino.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <Wire.h>

// #include "AudioTools.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"
// #include "ESP32S3AISmartSpeaker.h"

// using namespace audio_tools;
// using namespace audio_driver;

// AudioInfo info(16000, 1, 16);
// AudioBoardStream audio(ESP32S3AISmartSpeaker);

// WiFiUDP udp;

// // ====== set these ======
// // const char* WIFI_SSID = "";
// // const char* WIFI_PASS = "";
// const char* WIFI_SSID = "";
// const char* WIFI_PASS = "";

// // Wireless LAN adapter Wi-Fi:
// // IPv4 Address. . . . . . . . . . . : 10.250.32.130
// IPAddress PC_IP(192,168,0,106);     // your PC IP
// const uint16_t PC_PORT = 5005;      // python listens here
// // =======================

// static uint16_t seq = 0;
// static uint32_t ts  = 0; // in samples

// // 20 ms @ 16kHz = 320 samples = 640 bytes (16-bit)
// static const int FRAME_SAMPLES = 320;
// static const int FRAME_BYTES   = FRAME_SAMPLES * 2;
// static uint8_t frame[FRAME_BYTES];

// // Optional tiny header (not full RTP, but enough to order packets)
// // [0..1]=seq, [2..5]=timestamp_samples, then PCM payload
// static uint8_t pkt[2 + 4 + FRAME_BYTES];

// void wifi_connect() {
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(200);
//     Serial.print(".");
//   }
//   Serial.println();
//   Serial.print("WiFi OK, IP=");
//   Serial.println(WiFi.localIP());
// }

// void setup() {
//   Serial.begin(115200);
//   delay(300);

//   // codec I2C pins for your board
//   Wire.begin(11, 10);

//   wifi_connect();
//   udp.begin(0); // any local port

//   auto cfg = audio.defaultConfig(RXTX_MODE);
//   cfg.copyFrom(info);

//   if (!audio.begin(cfg)) {
//     Serial.println("audio.begin FAILED");
//     while (true) delay(1000);
//   }
//   audio.setVolume(0.3f); // speaker volume to 30%
//   audio.setInputVolume(0.9f);   // mic gain to 80%
//   Serial.println("Streaming mic PCM over UDP...");
// }

// static bool read_exact(Stream &s, uint8_t *dst, size_t n) {
//   size_t got = 0;
//   uint32_t t0 = millis();
//   while (got < n) {
//     int r = s.readBytes(dst + got, n - got);
//     if (r > 0) got += (size_t)r;
//     if (millis() - t0 > 2000) return false; // safety timeout
//     delay(0);
//   }
//   return true;
// }

// void loop() {
//   if (!read_exact(audio, frame, FRAME_BYTES)) return;

//   // build packet
//   pkt[0] = (uint8_t)(seq & 0xFF);
//   pkt[1] = (uint8_t)((seq >> 8) & 0xFF);
//   pkt[2] = (uint8_t)(ts & 0xFF);
//   pkt[3] = (uint8_t)((ts >> 8) & 0xFF);
//   pkt[4] = (uint8_t)((ts >> 16) & 0xFF);
//   pkt[5] = (uint8_t)((ts >> 24) & 0xFF);
//   memcpy(pkt + 6, frame, FRAME_BYTES);

//   udp.beginPacket(PC_IP, PC_PORT);
//   udp.write(pkt, sizeof(pkt));
//   udp.endPacket();

//   seq++;
//   ts += FRAME_SAMPLES; // advance by samples per frame
// }

// Mic + With CYD
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

using namespace audio_tools;
using namespace audio_driver;

AudioInfo info(16000, 1, 16);
AudioBoardStream audio(ESP32S3AISmartSpeaker);

WiFiUDP udp_audio;
WiFiUDP udp_text;

const char *WIFI_SSID = "";
const char *WIFI_PASS = "";

IPAddress PC_IP(10, 250, 32, 130); // Python IP
const uint16_t PC_PORT = 5005;
const uint16_t TEXT_PORT = 6000;

HardwareSerial SerialCYD(2);

static uint16_t seq = 0;
static uint32_t ts = 0;

static const int FRAME_SAMPLES = 320;
static const int FRAME_BYTES = FRAME_SAMPLES * 2;
static uint8_t frame[FRAME_BYTES];
static uint8_t pkt[6 + FRAME_BYTES];

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK, IP=");
  Serial.println(WiFi.localIP());
}

static bool read_exact(Stream &s, uint8_t *dst, size_t n) {
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < n) {
    int r = s.readBytes(dst + got, n - got);
    if (r > 0)
      got += (size_t)r;
    if (millis() - t0 > 2000)
      return false;
    delay(0);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(11, 10);

  wifi_connect();

  udp_audio.begin(0);
  udp_text.begin(TEXT_PORT);

  SerialCYD.begin(115200, SERIAL_8N1, 8, 9);

  auto cfg = audio.defaultConfig(RXTX_MODE);
  cfg.copyFrom(info);

  if (!audio.begin(cfg)) {
    Serial.println("audio.begin FAILED");
    while (true)
      ;
  }

  audio.setVolume(0.3f);
  audio.setInputVolume(0.9f);

  Serial.println("System Ready");
}

void loop() {

  // ===== AUDIO STREAMING =====
  if (read_exact(audio, frame, FRAME_BYTES)) {

    pkt[0] = seq & 0xFF;
    pkt[1] = (seq >> 8) & 0xFF;
    pkt[2] = ts & 0xFF;
    pkt[3] = (ts >> 8) & 0xFF;
    pkt[4] = (ts >> 16) & 0xFF;
    pkt[5] = (ts >> 24) & 0xFF;

    memcpy(pkt + 6, frame, FRAME_BYTES);

    udp_audio.beginPacket(PC_IP, PC_PORT);
    udp_audio.write(pkt, sizeof(pkt));
    udp_audio.endPacket();

    seq++;
    ts += FRAME_SAMPLES;
  }

  // ===== AUDIO STREAMING =====
  // if (read_exact(audio, frame, FRAME_BYTES)) {

  //   int16_t *sample = (int16_t*)frame;
  //   int16_t max_val = 0;

  //   for (int i = 0; i < FRAME_SAMPLES; i++) {
  //     if (abs(sample[i]) > max_val) {
  //       max_val = abs(sample[i]);
  //     }
  //   }

  //   Serial.print("Audio Frame OK | Max Amplitude: ");
  //   Serial.println(max_val);

  //   pkt[0] = seq & 0xFF;
  //   pkt[1] = (seq >> 8) & 0xFF;
  //   pkt[2] = ts & 0xFF;
  //   pkt[3] = (ts >> 8) & 0xFF;
  //   pkt[4] = (ts >> 16) & 0xFF;
  //   pkt[5] = (ts >> 24) & 0xFF;

  //   memcpy(pkt + 6, frame, FRAME_BYTES);

  //   udp_audio.beginPacket(PC_IP, PC_PORT);
  //   udp_audio.write(pkt, sizeof(pkt));
  //   udp_audio.endPacket();

  //   seq++;
  //   ts += FRAME_SAMPLES;
  // }

  // ===== RECEIVE TEXT (DEBUG VERSION) =====
  int packetSize = udp_text.parsePacket();

  if (packetSize > 0) {

    Serial.println("========== TEXT PACKET RECEIVED ==========");
    Serial.print("From IP: ");
    Serial.println(udp_text.remoteIP());

    Serial.print("From Port: ");
    Serial.println(udp_text.remotePort());

    Serial.print("Packet size: ");
    Serial.println(packetSize);

    char incoming[256];
    int len = udp_text.read(incoming, sizeof(incoming) - 1);

    if (len > 0) {
      incoming[len] = 0;

      Serial.print("Text length: ");
      Serial.println(len);

      Serial.print("Recognized text: ");
      Serial.println(incoming);

      Serial.println("Forwarding to CYD via UART...");
      SerialCYD.println(incoming);

      Serial.println("========== END PACKET ==========\n");
    } else {
      Serial.println("ERROR: Packet received but no data read!");
    }
  }
}