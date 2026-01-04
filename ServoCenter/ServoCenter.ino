// Using M5Stack AtomU GPIO

#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

Servo myServo;

const int servoPin   = 25;   // Servo signal (white wire)
const int buttonPin  = 39;   // AtomU front button (active LOW)
#define NEOPIXEL_PIN 27

Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Attach servo with calibrated pulse range
  myServo.attach(servoPin, 500, 2500);
  myServo.write(90); // start at 0°

  pinMode(buttonPin, INPUT); // AtomU button is active LOW

  pixel.begin();
  pixel.clear();
  pixel.show();

  Serial.begin(115200);
  Serial.println("Servo zero-to-90 demo using AtomU button");
}

void loop() {
  // Check AtomU button
  if (digitalRead(buttonPin) == LOW) {
    myServo.write(90); // reset to 90°

    // Blink NeoPixel green to confirm
    pixel.setPixelColor(0, pixel.Color(0, 255, 0));
    pixel.show();
    delay(200);
    pixel.clear();
    pixel.show();

    Serial.println("Button pressed → Servo reset to 90°");
    delay(300); // debounce
  }
}
