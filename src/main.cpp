#include <Arduino.h>

// --- Motor Driver Pins (TB6612FNG) ---
#define PWMA 11
#define AIN1 9
#define AIN2 10

#define PWMB 6
#define BIN1 4
#define BIN2 5
#define STBY 7

// --- Ultrasonic Sensor Pins (HC-SR04) ---
#define TRIG 2
#define ECHO 3

// --- IR Edge Sensor Pins ---
#define IR_LEFT  A2
#define IR_RIGHT A1
// LOW = white edge, HIGH = black arena floor (IR cannot detect opponent body)

// --- Tuning ---
#define MOTOR_SPEED      255
#define SEARCH_SPEED     150
#define SEARCH_TURN_MS   300
#define ATTACK_DISTANCE  40   // cm — charge when opponent closer than this
#define EDGE_REVERSE_MS  100
#define EDGE_PIVOT_MS    120
#define BURST_MS         150
#define REATTACK_MS      200

// --- Motor helpers ---
void motorRight(int speed) {
  if (speed > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else if (speed < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    speed = -speed;
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }
  analogWrite(PWMA, speed);
}

void motorLeft(int speed) {
  if (speed > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  } else if (speed < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    speed = -speed;
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }
  analogWrite(PWMB, speed);
}

void stopMotors() {
  motorLeft(0);
  motorRight(0);
}

void driveForward(int speed) {
  motorLeft(speed);
  motorRight(speed);
}

void spinRight(int speed) {
  motorLeft(speed);
  motorRight(-speed);
}

void spinLeft(int speed) {
  motorLeft(-speed);
  motorRight(speed);
}

void driveBackward(int speed) {
  motorLeft(-speed);
  motorRight(-speed);
}

// --- Edge Detection ---
// Highest priority — reverses and pivots away from the white border.
void checkEdge() {
  bool leftEdge  = (digitalRead(IR_LEFT)  == LOW);
  bool rightEdge = (digitalRead(IR_RIGHT) == LOW);

  if (!leftEdge && !rightEdge) return;

  // Reverse away from edge
  driveBackward(MOTOR_SPEED);
  delay(150);

  // Pivot away from the side that triggered
  if (leftEdge && !rightEdge) {
    spinRight(MOTOR_SPEED);
  } else if (rightEdge && !leftEdge) {
    spinLeft(MOTOR_SPEED);
  } else {
    spinRight(MOTOR_SPEED); // both triggered — reverse was enough, now turn
  }
  delay(150);
}

// --- Ultrasonic ---
long readDistanceCm() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000); // 30 ms timeout (~5 m max)
  if (duration == 0) return 999;              // no echo → nothing nearby
  return duration / 58;                       // convert to cm
}

// --- Main ---
void setup() {
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);  // Take driver out of standby

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(IR_LEFT,  INPUT);
  pinMode(IR_RIGHT, INPUT);

  Serial.begin(9600);
  stopMotors();

  // --- 5-second start delay (Rule 12) ---
  // Bot is stationary; sensor readings are allowed during this period.
  unsigned long startTime = millis();
  while (millis() - startTime < 3300) {
    readDistanceCm();
    delay(50);
  }
  Serial.println("--- 5-Second Delay Complete! ---");
}

void loop() {
  // EDGE CHECK — highest priority, always runs first
  checkEdge();

  // Stop briefly before every read — clears motor PWM noise
  // from the power lines so ECHO signal is clean.
  stopMotors();
  delayMicroseconds(250);
  long dist = readDistanceCm();

  if (dist <= ATTACK_DISTANCE) {
    // -----------------------------------------------
    // STATE 1: ATTACK
    // -----------------------------------------------
    Serial.print("Opponent at ");
    Serial.print(dist);
    Serial.println(" cm — CHARGING!");
    driveForward(MOTOR_SPEED);
    delay(100); // Charge for 100ms, then loop() re-evaluates

  } else if (dist < 999) {
    // -----------------------------------------------
    // STATE 2: OPPONENT TOO FAR — HOLD
    // Motors already stopped from top of loop()
    // -----------------------------------------------
    Serial.print("Opponent at ");
    Serial.print(dist);
    Serial.println(" cm — holding...");

  } else {
    // -----------------------------------------------
    // STATE 3: SEARCH
    // Spin one arc, then loop() restarts and re-scans.
    // Motors already stopped — spin begins after read.
    // -----------------------------------------------
    Serial.println("No opponent — searching...");
    spinLeft(SEARCH_SPEED);
    delay(SEARCH_TURN_MS);
  }
}