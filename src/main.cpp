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

// --- IR Edge Sensor Pins ---
#define IR_LEFT  A2   // Left edge sensor
#define IR_RIGHT A1   // Right edge sensor
// IR sensors read LOW when they see the white edge line

// --- Tuning ---
#define MOTOR_SPEED      255   // Full power for attack — maximum torque
#define SEARCH_SPEED     120   // Curved arc speed (outer wheel)
#define SEARCH_INNER     60    // Curved arc speed (inner wheel) — drives forward while turning
#define ATTACK_DISTANCE  40    // cm — charge when opponent is closer than this
#define EDGE_REVERSE_MS  100   // ms to reverse when edge detected (shorter = less blind time)
#define EDGE_PIVOT_MS    120   // ms to pivot away from edge
#define BURST_MS         150   // ms for initial forward burst after start
#define REATTACK_MS      200   // ms to charge forward after edge escape (re-engage fast)

// --- State tracking ---
bool justEscapedEdge = false;        // Flag: did we just escape an edge?
unsigned long escapeCount = 0;       // Varies escape direction to be less predictable
unsigned long searchCount = 0;       // Alternates arc direction during search

// --- IR debounce helper ---
// Confirms edge reading with a second read after 2ms to filter false positives
bool edgeConfirmed(int pin) {
  if (digitalRead(pin) != LOW) return false;
  delay(2);
  return digitalRead(pin) == LOW;
}

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

void driveBackward(int speed) {
  motorLeft(-speed);
  motorRight(-speed);
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

  long duration = pulseIn(ECHO, HIGH, 10000); // 10ms timeout — covers ~1.7m, faster loop
  if (duration == 0) return 999;
  return duration / 58;
}

// --- Edge Detection ---
// Returns true if edge was detected and escape maneuver was performed.
// Highest priority — always checked first. Uses debounce to prevent false triggers.
bool checkEdge() {
  bool leftEdge  = edgeConfirmed(IR_LEFT);
  bool rightEdge = edgeConfirmed(IR_RIGHT);

  if (!leftEdge && !rightEdge) return false;

  // Reverse at full power with early exit once sensors clear the edge
  driveBackward(MOTOR_SPEED);
  unsigned long t = millis();
  while (millis() - t < EDGE_REVERSE_MS) {
    if (digitalRead(IR_LEFT) != LOW && digitalRead(IR_RIGHT) != LOW) break;
  }

  // Pivot direction: alternate when both triggered, otherwise pivot away from detected side
  if (leftEdge && rightEdge) {
    if (escapeCount % 2 == 0) spinRight(MOTOR_SPEED);
    else                       spinLeft(MOTOR_SPEED);
  } else if (leftEdge) {
    spinRight(MOTOR_SPEED);
  } else {
    spinLeft(MOTOR_SPEED);
  }

  // Slight timing variation (+0/20/40ms) to be less predictable
  delay(EDGE_PIVOT_MS + (escapeCount % 3) * 20);

  escapeCount++;
  justEscapedEdge = true;
  return true;
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

  // IR edge sensor pins (INPUT_PULLUP = safer if modules have open-collector output)
  pinMode(IR_LEFT,  INPUT);
  pinMode(IR_RIGHT, INPUT);

  Serial.begin(9600);
  stopMotors();

  // --- 5-second start delay (competition rule) ---
  // Motors off; sensors pre-read during wait
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    readDistanceCm();
    delay(50);
  }

  // --- Start strategy: short forward burst to close the gap ---
  driveForward(MOTOR_SPEED);
  delay(BURST_MS);
}

void loop() {
  // EDGE CHECK — highest priority, always runs first
  if (checkEdge()) return;

  // RE-ENGAGE — after escaping edge, charge forward briefly before searching
  if (justEscapedEdge) {
    justEscapedEdge = false;
    driveForward(MOTOR_SPEED);
    unsigned long t = millis();
    while (millis() - t < REATTACK_MS) {
      if (digitalRead(IR_LEFT) == LOW || digitalRead(IR_RIGHT) == LOW) {
        stopMotors();  // Stop before handing off to checkEdge next iteration
        return;
      }
    }
    return;
  }

  // Read ultrasonic distance
  long dist = readDistanceCm();

  if (dist <= ATTACK_DISTANCE) {
    // ATTACK — opponent detected, full power charge
    driveForward(MOTOR_SPEED);
  } else {
    // SEARCH — alternate arc direction each cycle to be less predictable
    if (searchCount % 2 == 0) {
      motorLeft(SEARCH_SPEED);
      motorRight(SEARCH_INNER);
    } else {
      motorLeft(SEARCH_INNER);
      motorRight(SEARCH_SPEED);
    }
    searchCount++;
  }
}
