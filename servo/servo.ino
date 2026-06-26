#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>


// Inisialisasi I2C custom pin
TwoWire I2Cbus = TwoWire(0);

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, I2Cbus);

// Kalibrasi servo
#define SERVOMIN 150
#define SERVOMAX 600

int angleToPulse(int angle) { return map(angle, 0, 180, SERVOMIN, SERVOMAX); }

void setup() {
  // Inisialisasi I2C (SDA, SCL)
  I2Cbus.begin(6, 7);

  pwm.begin();
  pwm.setPWMFreq(50); // 50Hz servo
}

void loop() {
  // 0 derajat
  int p0 = angleToPulse(0);
  pwm.setPWM(0, 0, p0);
  pwm.setPWM(1, 0, p0);
  delay(2000);

  // 90 derajat
  int p90 = angleToPulse(90);
  pwm.setPWM(0, 0, p90);
  pwm.setPWM(1, 0, p90);
  delay(2000);

  // 180 derajat
  int p180 = angleToPulse(180);
  pwm.setPWM(0, 0, p180);
  pwm.setPWM(1, 0, p180);
  delay(2000);
}
