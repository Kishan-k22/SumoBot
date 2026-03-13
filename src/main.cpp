#include <Arduino.h>

// --- Motor Driver Pins (TB6612FNG) ---
#define PWMA 11   // Motor A speed (PWM)
#define AIN1 9   // Motor A direction
#define AIN2 10   // Motor A direction

#define PWMB 6   // Motor B speed (PWM)
#define BIN1 4   // Motor B direction
#define BIN2 5  // Motor B direction
#define STBY 7   // Standby — must be HIGH for motors to run

// --- Ultrasonic Sensor Pins (HC-SR04) ---
#define TRIG 2
#define ECHO 3

// --- Tuning ---
#define MOTOR_SPEED     255   // 0-255, adjust for your motors
#define SEARCH_SPEED    150   // slower speed while spinning to search
#define ATTACK_DISTANCE 10    // cm — charge when opponent is closer than this
#define SEARCH_TURN_MS  300   // ms to spin before re-scanning

// --- Motor helpers ---
void motorLeft(int speed) {
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

void motorRight(int speed) {
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
  if (duration == 0) return 999;               // no echo → nothing nearby
  return duration / 58;                        // convert to cm
}

// --- Main ---
void setup() {
  // Motor driver pins
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);  // Take driver out of standby

  // Ultrasonic pins
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  Serial.begin(9600);

  stopMotors();

  // 5-second start delay (rules: no movement, but sensors allowed)
  unsigned long startTime = millis();
  long initialDist = 999;
  while (millis() - startTime < 5000) {
    initialDist = readDistanceCm();
    delay(50);
  }
}

void loop() {
  long dist = readDistanceCm();

  if (dist < ATTACK_DISTANCE) {
    // Opponent detected — charge!
    Serial.print("Opponent detected at ");
    Serial.print(dist);
    Serial.println(" cm — charging!");
    driveForward(MOTOR_SPEED);
  } else if (dist < 999) {
    // Opponent detected but too far — stop
    Serial.print("Opponent detected at ");
    Serial.print(dist);
    Serial.println(" cm — not charging");
    stopMotors();
  } 
}
