#include <ESP8266_ISR_Servo.h>

#define MIN_MICROS 1000
#define MAX_MICROS 2000

// Finger order: 0=thumb, 1=index, 2=middle, 3=ring, 4=pinky
int pins[5]     = {D1, D2, D5, D6, D7};
int servoIndex[5];

// Neutral positions (resting)
int neutral[5]  = {90, 90, 90, 90, 90};

// Inversion map: ring & pinky are inverted
bool invert[5]  = {false, false, false, true, true};

// Inward targets (safe inward angles)
int inTarget[5] = {120, 120, 120, 60, 60};

// Deadband tracking
int lastCmd[5] = {90,90,90,90,90};
const int DEADBAND = 4; // only update if change >= 4°

void setup() {
  Serial.begin(115200);
  delay(50);

  // Attach index first and assert neutral immediately
  servoIndex[1] = ISR_Servo.setupServo(pins[1], MIN_MICROS, MAX_MICROS);
  if (servoIndex[1] != -1) {
    setFingerAngle(1, neutral[1]);
    Serial.println("Index attached and set to neutral");
  }

  // Attach the rest
  for (int i = 0; i < 5; i++) {
    if (i == 1) continue;
    servoIndex[i] = ISR_Servo.setupServo(pins[i], MIN_MICROS, MAX_MICROS);
    if (servoIndex[i] != -1) {
      setFingerAngle(i, neutral[i]);
      Serial.printf("Finger %d attached and set to neutral\n", i);
    }
  }
}

// Deadband‑aware setter
void setFingerAngle(int i, int angle) {
  if (servoIndex[i] == -1) return;
  if (abs(angle - lastCmd[i]) >= DEADBAND) {
    ISR_Servo.setPosition(servoIndex[i], angle);
    lastCmd[i] = angle;
  }
}

// Smooth ramp with neutral‑first option
void smoothMove(int idx, int target, int step = 5, int dwellMs = 40, bool forceNeutral = false) {
  if (servoIndex[idx] == -1) return;

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

// Safe inward move with clamps
void fingerInSafe(int i) {
  if (servoIndex[i] == -1) return;
  int target = inTarget[i];
  if (!invert[i]) target = constrain(target, neutral[i], 180); // non‑inverted: ≥ neutral
  else            target = constrain(target, 0, neutral[i]);   // inverted: ≤ neutral
  smoothMove(i, target, 5, 40, true); // always start from neutral
}

// Return finger to neutral smoothly
void fingerNeutral(int i) {
  if (servoIndex[i] == -1) return;
  smoothMove(i, neutral[i], 5, 30, true);
}

// Gesture: fist with staggered, ramped motion
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

// Gesture: open hand smoothly
void openHand() {
  for (int i = 0; i < 5; i++) fingerNeutral(i);
}

// Health check: move one finger at a time
void healthCheck() {
  for (int i = 0; i < 5; i++) {
    Serial.printf("Testing finger %d...\n", i);
    fingerNeutral(i);   // park at neutral
    delay(500);
    fingerInSafe(i);    // move inward safely
    delay(1000);
    fingerNeutral(i);   // return to neutral
    delay(1000);
  }
}

void loop() {
  // Demo mode: fist then open
  makeFistStaggered();
  delay(1500);
  openHand();
  delay(2000);

  // Health check mode: uncomment to test fingers individually
  // healthCheck();
  // delay(5000);
}