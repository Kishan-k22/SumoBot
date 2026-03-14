#include <Arduino.h>

// --- Motor Driver Pins (TB6612FNG) ---
#define PWMA 11   // Motor A speed (PWM)
#define AIN1 9    // Motor A direction
#define AIN2 10   // Motor A direction

#define PWMB 6    // Motor B speed (PWM)
#define BIN1 4    // Motor B direction
#define BIN2 5    // Motor B direction
#define STBY 7    // Standby — must be HIGH for motors to run

// --- Ultrasonic Sensor Pins (HC-SR04) ---
#define TRIG 2
#define ECHO 3

// --- Tuning ---
#define MOTOR_SPEED      150  // 0–255, adjust for your motors
#define SEARCH_SPEED     100  // slower speed while spinning to search
#define ATTACK_DISTANCE  40   // cm — charge when opponent is closer than this
#define SEARCH_TURN_MS   200  // ms to spin before re-scanning

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

  Serial.begin(9600);
  stopMotors();

  // --- 5-second start delay (Rule 12) ---
  // Bot is stationary; sensor readings are allowed during this period.
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    readDistanceCm();
    delay(50);
  }
  Serial.println("--- 5-Second Delay Complete! ---");
}

void loop() {
  long dist = readDistanceCm();

  // Debug: always print raw distance
  Serial.print("Distance: ");
  Serial.print(dist);
  Serial.println(" cm");

  if (dist <= ATTACK_DISTANCE) {
    // Opponent in range — charge without stopping
    Serial.print("Opponent at ");
    Serial.print(dist);
    Serial.println(" cm — CHARGING!");
    driveForward(MOTOR_SPEED);

  } else {
    // No opponent in range — spin to search
    Serial.println("No opponent — searching...");
    spinRight(SEARCH_SPEED);
  }
}
