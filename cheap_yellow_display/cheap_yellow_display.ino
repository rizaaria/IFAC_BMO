#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
HardwareSerial SerialMain(2);

void setup() {
  Serial.begin(115200);
  SerialMain.begin(115200, SERIAL_8N1, 27, 22);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 20);
  tft.println("Waiting...");
}

void loop() {

  if (SerialMain.available()) {

    String text = SerialMain.readStringUntil('\n');

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 20);
    tft.println("Recognized:");
    tft.println("");
    tft.println(text);
  }
}