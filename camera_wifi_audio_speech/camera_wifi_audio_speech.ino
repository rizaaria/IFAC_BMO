#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

using namespace audio_tools;
using namespace audio_driver;

// ================= WIFI =================
const char *ssid = "";
const char *password = "";

IPAddress serverIP(192, 168, 137, 1);

const uint16_t AUDIO_PORT = 5005;
const uint16_t TTS_PORT = 6001;

// ================= UDP =================
WiFiUDP udp_audio;
WiFiUDP udp_tts;

// ================= AUDIO =================
AudioInfo info(16000, 1, 16);
AudioBoardStream audio(ESP32S3AISmartSpeaker);

#define FRAME_SAMPLES 320
#define FRAME_BYTES (FRAME_SAMPLES * 2)

uint8_t frame[FRAME_BYTES];
uint16_t mic_got = 0;

uint8_t tts_buf[1400];

// ================= WIFI =================
void wifi_connect() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nESP32 IP:");
  Serial.println(WiFi.localIP());
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  wifi_connect();

  udp_audio.begin(0);
  udp_tts.begin(TTS_PORT);

  auto cfg = audio.defaultConfig(RXTX_MODE);
  cfg.copyFrom(info);

  if (!audio.begin(cfg)) {
    Serial.println("Audio FAIL");
    while (1)
      ;
  }

  audio.setVolume(0.4); // 🔥 speaker volume

  Serial.println("Audio OK");
}

// ================= LOOP =================
void loop() {

  // ===== MIC READ =====
  if (audio.available()) {
    int r = audio.readBytes(frame + mic_got, FRAME_BYTES - mic_got);
    if (r > 0)
      mic_got += r;
  }

  if (mic_got >= FRAME_BYTES) {

    int16_t *samples = (int16_t *)frame;

    // 🔥 GAIN DI ESP32 (PENTING)
    for (int i = 0; i < FRAME_SAMPLES; i++) {
      int32_t s = samples[i];

      s = s * 5.0; // 🔥 ini kunci utama

      if (s > 32767)
        s = 32767;
      if (s < -32768)
        s = -32768;

      samples[i] = (int16_t)s;
    }

    udp_audio.beginPacket(serverIP, AUDIO_PORT);
    udp_audio.write(frame, FRAME_BYTES);
    udp_audio.endPacket();

    mic_got = 0;
  }

  // ===== PLAYBACK DARI PYTHON =====
  int ttsSize = udp_tts.parsePacket();
  if (ttsSize > 0) {
    int len = udp_tts.read(tts_buf, sizeof(tts_buf));

    if (len > 0) {
      audio.write(tts_buf, len);
    }
  }
}
