#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// PCA9685 driver instance
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo pulse range (ticks at 50 Hz)
#define SERVO_MIN  205   // ~1000 µs
#define SERVO_MAX  410   // ~2000 µs

// Finger order: 0=thumb, 1=index, 2=middle, 3=ring, 4=pinky
int channels[5]   = {0, 1, 2, 3, 4};   // PCA9685 channels
int neutral[5]    = {90, 90, 90, 90, 90};
bool invert[5]    = {false, false, false, true, true};
int inTarget[5]   = {120, 120, 120, 60, 60};
int lastCmd[5]    = {90, 90, 90, 90, 90};
const int DEADBAND = 4;

// --- Helper: map angle to PCA9685 ticks ---
int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}

// --- Deadband‑aware setter ---
void setFingerAngle(int i, int angle) {
  if (abs(angle - lastCmd[i]) >= DEADBAND) {
    int pulse = angleToPulse(angle);
    pwm.setPWM(channels[i], 0, pulse);
    lastCmd[i] = angle;
  }
}

// --- Smooth ramp with neutral‑first option ---
void smoothMove(int idx, int target, int step = 5, int dwellMs = 40, bool forceNeutral = false) {
  int current = lastCmd[idx];
  if (forceNeutral) {
    current = neutral[idx];
    setFingerAngle(idx, current);
    delay(200); // settle at neutral before ramp
  }

  int dir = (target > current) ? 1 : -1;
  for (int a = current; dir > 0 ? a <= target : a >= target; a += dir * step) {
    setFingerAngle(idx, a);
    delay(dwellMs);
  }
}

// --- Safe inward move with clamps ---
void fingerInSafe(int i) {
  int target = inTarget[i];
  if (!invert[i]) target = constrain(target, neutral[i], 180);
  else            target = constrain(target, 0, neutral[i]);
  smoothMove(i, target, 5, 40, true);
}

// --- Return finger to neutral smoothly ---
void fingerNeutral(int i) {
  smoothMove(i, neutral[i], 5, 30, true);
}

// --- Gesture: fist with staggered, ramped motion ---
void makeFistStaggered() {
  int groups[][2] = {{0,2}, {1,3}}; // thumb+middle, index+ring
  for (auto &g : groups) {
    fingerInSafe(g[0]);
    delay(30);
    fingerInSafe(g[1]);
    delay(40);
  }
  fingerInSafe(4); // pinky last
}

// --- Gesture: open hand smoothly ---
void openHand() {
  for (int i = 0; i < 5; i++) fingerNeutral(i);
}

// --- Health check: move one finger at a time ---
void healthCheck() {
  for (int i = 0; i < 5; i++) {
    Serial.printf("Testing finger %d...\n", i);
    fingerNeutral(i);
    delay(500);
    fingerInSafe(i);
    delay(1000);
    fingerNeutral(i);
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();          // ESP32‑C3 default SDA/SCL pins
  pwm.begin();
  pwm.setPWMFreq(50);    // 50 Hz for servos

  // Initialize all fingers to neutral
  for (int i = 0; i < 5; i++) {
    setFingerAngle(i, neutral[i]);
    Serial.printf("Finger %d set to neutral\n", i);
  }
}

void loop() {
  makeFistStaggered();
  delay(1500);
  openHand();
  delay(2000);

  // Uncomment to test fingers individually
  // healthCheck();
  // delay(5000);
}
