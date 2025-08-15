#include <NewPing.h>

// Pin definitions
#define TRIG_PIN 12    // Ultrasonic TRIG pin
#define ECHO_PIN 14    // Ultrasonic ECHO pin
#define BUZZER_PIN 27  // Buzzer pin
#define ENA 4          // PWM-enabled pin for left motors
#define IN1 16         // Input 1 for left motors
#define IN2 17         // Input 2 for left motors
#define ENB 5          // PWM-enabled pin for right motors
#define IN3 18         // Input 3 for right motors
#define IN4 19         // Input 4 for right motors

// Constants
#define MAX_DISTANCE 200       // Max distance for ultrasonic (cm)
#define OBSTACLE_DISTANCE 15   // Obstacle detection threshold (cm)
#define MOTOR_SPEED 150        // Motor speed (0-255)

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

void setup() {
  // Initialize pins
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Test components
  digitalWrite(BUZZER_PIN, HIGH); // Buzzer test
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(IN1, HIGH); // Left motors test
  digitalWrite(IN2, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  delay(500);
  moveStop();
  digitalWrite(IN3, HIGH); // Right motors test
  digitalWrite(IN4, LOW);
  analogWrite(ENB, MOTOR_SPEED);
  delay(500);
  moveStop();
  readPing(); // Sensor test
}

void loop() {
  int distance = readPing(); // Check forward
  if (distance <= OBSTACLE_DISTANCE && distance > 0) {
    digitalWrite(BUZZER_PIN, HIGH); // Beep
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    moveStop();
    delay(50);

    // Try to find clear path
    bool clearPath = false;
    while (!clearPath) {
      // Try left turn
      turnLeft();
      delay(400);
      moveStop();
      delay(50);
      distance = readPing();
      if (distance > OBSTACLE_DISTANCE) {
        clearPath = true;
      } else {
        // Try right turn (undo left + turn right)
        turnRight();
        delay(800);
        moveStop();
        delay(50);
        distance = readPing();
        if (distance > OBSTACLE_DISTANCE) {
          clearPath = true;
        } else {
          // Move backward
          moveBackward();
          delay(300);
          moveStop();
          delay(50);
          distance = readPing();
          if (distance > OBSTACLE_DISTANCE) {
            clearPath = true;
          }
        }
      }
    }
  } else {
    moveForward();
  }
  delay(20);
}

int readPing() {
  int cm = sonar.ping_cm();
  return cm > 0 ? cm : MAX_DISTANCE;
}

void moveStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}