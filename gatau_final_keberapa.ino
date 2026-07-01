#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

#define SDA_PIN 21
#define SCL_PIN 22

// Motor Kiri
#define ENA 33
#define IN1 25
#define IN2 26

// Motor Kanan
#define ENB 13
#define IN3 27
#define IN4 14

#define LED_BUILTIN 2

Adafruit_BNO055 bno(55, 0x28);

// ===== PID TUNING REKOMENDASI (SMOOTH) =====
float Kp = 4.00;      // Turunkan dari sebelumnya  
float Ki = 1.00;      // Kecil untuk menghindari windup
float Kd = 1.00;      // Optimal untuk smoothing

// ===== PARAMETER GERAKAN =====
float setPoint = 0;
float error;
float lastError = 0;
float integral = 0;
float lastCorrection = 0;  // Untuk smoothing tambahan

// Kecepatan
int minSpeed = 60;
int baseForwardSpeed = 140;
int maxRotationSpeed = 120;
float maxCorrection = 60;

// Filter untuk smoothing
float alpha = 0.3;  // Filter factor (0-1), semakin kecil semakin smooth
float filteredError = 0;

unsigned long lastTime;
bool isReady = false;

// Fungsi normalisasi
float normalizeAngle(float angle)
{
  angle = fmod(angle, 360.0);
  if (angle < 0) angle += 360;
  return angle;
}

// Fungsi error minimal
float angleError(float target, float current)
{
  float err = target - current;
  if (err > 180) err -= 360;
  if (err < -180) err += 360;
  return err;
}

// Fungsi smooth dengan rate limiting
float smoothCorrection(float newCorrection, float maxChange)
{
  float diff = newCorrection - lastCorrection;
  if (abs(diff) > maxChange) {
    newCorrection = lastCorrection + (diff > 0 ? maxChange : -maxChange);
  }
  return newCorrection;
}

// Rotasi halus
void rotateRobot(float correction)
{
  // Rate limiting untuk menghindari sentakan
  correction = smoothCorrection(correction, 15);
  lastCorrection = correction;
  
  int pwmValue = abs((int)correction);
  pwmValue = constrain(pwmValue, minSpeed, maxRotationSpeed);
  
  // Tambahkan ramp up untuk start halus
  static int lastPWM = 0;
  if (pwmValue > lastPWM + 10) {
    pwmValue = lastPWM + 10;
  }
  lastPWM = pwmValue;
  
  if (correction < -2) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else if (correction > 2) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    stopRobot();
    return;
  }
  
  ledcWrite(ENA, pwmValue);
  ledcWrite(ENB, pwmValue);
}

// Gerakan maju smooth
void moveForward(float correction)
{
  // Rate limiting untuk koreksi
  correction = smoothCorrection(correction, 8);
  lastCorrection = correction;
  
  correction = constrain(correction, -maxCorrection, maxCorrection);
  
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  int leftPWM = baseForwardSpeed + (int)correction;
  int rightPWM = baseForwardSpeed - (int)correction;
  
  // Smooth PWM change
  static int lastLeftPWM = baseForwardSpeed;
  static int lastRightPWM = baseForwardSpeed;
  
  leftPWM = constrain(leftPWM, minSpeed, 255);
  rightPWM = constrain(rightPWM, minSpeed, 255);
  
  // Rate limiting untuk PWM
  if (abs(leftPWM - lastLeftPWM) > 10) {
    leftPWM = lastLeftPWM + (leftPWM > lastLeftPWM ? 10 : -10);
  }
  if (abs(rightPWM - lastRightPWM) > 10) {
    rightPWM = lastRightPWM + (rightPWM > lastRightPWM ? 10 : -10);
  }
  
  lastLeftPWM = leftPWM;
  lastRightPWM = rightPWM;
  
  ledcWrite(ENA, leftPWM);
  ledcWrite(ENB, rightPWM);
}

void stopRobot()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  lastCorrection = 0;
}

void setup()
{
  Serial.begin(115200);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  
  if (!bno.begin()) {
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
  }
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);
  
  delay(2000);
  
  setPoint = normalizeAngle(bno.getVector(Adafruit_BNO055::VECTOR_EULER).x());
  
  lastTime = millis();
  isReady = true;
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.println("=== SMOOTH PID CONTROL ===");
  Serial.print("Kp: "); Serial.println(Kp);
  Serial.print("Ki: "); Serial.println(Ki);
  Serial.print("Kd: "); Serial.println(Kd);
  Serial.print("SetPoint: "); Serial.println(setPoint);
  Serial.println("==========================\n");
  
  delay(1000);
}

void loop()
{
  if (!isReady) return;
  
  // Baca heading
  float heading = normalizeAngle(bno.getVector(Adafruit_BNO055::VECTOR_EULER).x());
  
  // Hitung dt
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  if (dt <= 0.001) {
    delay(5);
    return;
  }
  lastTime = now;
  
  // Filter error untuk smoothing
  error = angleError(setPoint, heading);
  filteredError = (alpha * error) + ((1 - alpha) * filteredError);
  
  // PID dengan filtered error
  integral += filteredError * dt;
  integral = constrain(integral, -20.0, 20.0);
  
  float derivative = (filteredError - lastError) / dt;
  float correction = (Kp * filteredError) + (Ki * integral) + (Kd * derivative);
  
  // Anti-windup untuk integral
  if (abs(correction) > 100) {
    integral *= 0.95;
  }
  
  // Logika kontrol
  if (abs(error) > 25.0) {
    rotateRobot(correction);
  } else {
    moveForward(correction);
  }
  
  // Debug output
  Serial.print("H:"); Serial.print(heading, 1);
  Serial.print(" E:"); Serial.print(error, 1);
  Serial.print(" C:"); Serial.print(correction, 0);
  Serial.print(" I:"); Serial.print(integral, 1);
  if (abs(error) > 25.0) {
    Serial.print(" [ROTASI]");
  } else {
    Serial.print(" [MAJU]");
  }
  Serial.println();
  
  lastError = filteredError;
  
  delay(20);  // 50Hz update rate
}