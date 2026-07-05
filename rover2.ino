#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <math.h>

// ====================== PIN CONFIGURATION ======================

#define SDA_PIN 8
#define SCL_PIN 9

#define EN_ALL  21

// Motor Kiri
#define M1_RPWM 47
#define M1_LPWM 48

#define M3_RPWM 13
#define M3_LPWM 12

// Motor Kanan
#define M2_RPWM 35
#define M2_LPWM 36

#define M4_RPWM 10
#define M4_LPWM 11

// ====================== PWM CHANNEL ESP32 CORE 2.x ======================

#define CH_M1_RPWM 0
#define CH_M1_LPWM 1

#define CH_M2_RPWM 2
#define CH_M2_LPWM 3

#define CH_M3_RPWM 4
#define CH_M3_LPWM 5

#define CH_M4_RPWM 6
#define CH_M4_LPWM 7

const int pwmFreq = 20000;
const int pwmResolution = 8;

// ====================== SENSOR BNO055 ======================

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

// ====================== PID MAJU ======================

float Kp = 4.00;
float Ki = 1.00;
float Kd = 1.00;

float setPoint = 0;
float error = 0;
float lastError = 0;
float integral = 0;

int minSpeed = 50;
int baseForwardSpeed = 60;

unsigned long lastTime = 0;
bool isReady = false;

// ====================== MODE ROBOT ======================

enum RobotMode {
  MODE_MAJU_PID,
  MODE_BELOK_KIRI,
  MODE_STOP_SEBENTAR
};

RobotMode modeRobot = MODE_MAJU_PID;

// ====================== PARAMETER OTOMATIS ======================

unsigned long durasiMaju = 2000;

unsigned long durasiStop = 500;

unsigned long modeStartTime = 0;

// ====================== PARAMETER BELOK KIRI ======================

float targetTurnHeading = 0;
float turnTolerance = 3.0;
int turnPWM = 70;

// ====================== FUNGSI SUDUT ======================

float normalizeAngle(float angle) {
  angle = fmod(angle, 360.0);

  if (angle < 0) {
    angle += 360.0;
  }

  return angle;
}

float angleError(float target, float current) {
  float err = target - current;

  if (err > 180.0) {
    err -= 360.0;
  }

  if (err < -180.0) {
    err += 360.0;
  }

  return err;
}

float getHeading() {
  sensors_event_t event;
  bno.getEvent(&event);

  float heading = event.orientation.x;
  return normalizeAngle(heading);
}

void resetPID() {
  error = 0;
  lastError = 0;
  integral = 0;
  lastTime = millis();
}

// ====================== SETUP PWM ======================

void setupPWM() {
  ledcSetup(CH_M1_RPWM, pwmFreq, pwmResolution);
  ledcSetup(CH_M1_LPWM, pwmFreq, pwmResolution);

  ledcSetup(CH_M2_RPWM, pwmFreq, pwmResolution);
  ledcSetup(CH_M2_LPWM, pwmFreq, pwmResolution);

  ledcSetup(CH_M3_RPWM, pwmFreq, pwmResolution);
  ledcSetup(CH_M3_LPWM, pwmFreq, pwmResolution);

  ledcSetup(CH_M4_RPWM, pwmFreq, pwmResolution);
  ledcSetup(CH_M4_LPWM, pwmFreq, pwmResolution);

  ledcAttachPin(M1_RPWM, CH_M1_RPWM);
  ledcAttachPin(M1_LPWM, CH_M1_LPWM);

  ledcAttachPin(M2_RPWM, CH_M2_RPWM);
  ledcAttachPin(M2_LPWM, CH_M2_LPWM);

  ledcAttachPin(M3_RPWM, CH_M3_RPWM);
  ledcAttachPin(M3_LPWM, CH_M3_LPWM);

  ledcAttachPin(M4_RPWM, CH_M4_RPWM);
  ledcAttachPin(M4_LPWM, CH_M4_LPWM);
}

// ====================== MOTOR STOP ======================

void motorStop() {
  ledcWrite(CH_M1_RPWM, 0);
  ledcWrite(CH_M1_LPWM, 0);

  ledcWrite(CH_M2_RPWM, 0);
  ledcWrite(CH_M2_LPWM, 0);

  ledcWrite(CH_M3_RPWM, 0);
  ledcWrite(CH_M3_LPWM, 0);

  ledcWrite(CH_M4_RPWM, 0);
  ledcWrite(CH_M4_LPWM, 0);
}

// ====================== MOTOR MAJU DENGAN PID ======================

void jalanMajuKoreksi(float correction) {
  int leftPWM = baseForwardSpeed - (int)correction;
  int rightPWM = baseForwardSpeed + (int)correction;

  leftPWM = constrain(leftPWM, minSpeed, 255);
  rightPWM = constrain(rightPWM, minSpeed, 255);

  // Motor kiri maju
  ledcWrite(CH_M1_RPWM, 0);
  ledcWrite(CH_M1_LPWM, leftPWM);

  ledcWrite(CH_M3_RPWM, 0);
  ledcWrite(CH_M3_LPWM, leftPWM);

  // Motor kanan maju
  ledcWrite(CH_M2_RPWM, rightPWM);
  ledcWrite(CH_M2_LPWM, 0);

  ledcWrite(CH_M4_RPWM, rightPWM);
  ledcWrite(CH_M4_LPWM, 0);
}

void motorBelokKiri(int pwm) {
  // Kecepatan roda kanan maju
  int pwmKananMaju = pwm;

  // Kecepatan roda kiri mundur dibuat lebih pelan
  int pwmKiriMundur = pwm * 0.80; 

  pwmKananMaju = constrain(pwmKananMaju, 0, 255);
  pwmKiriMundur = constrain(pwmKiriMundur, 0, 255);

  // RODA KANAN MAJU
  ledcWrite(CH_M1_RPWM, 0);
  ledcWrite(CH_M1_LPWM, pwmKananMaju);

  ledcWrite(CH_M3_RPWM, 0);
  ledcWrite(CH_M3_LPWM, pwmKananMaju);

  // RODA KIRI MUNDUR PELAN
  ledcWrite(CH_M2_RPWM, 0);
  ledcWrite(CH_M2_LPWM, pwmKiriMundur);

  ledcWrite(CH_M4_RPWM, 0);
  ledcWrite(CH_M4_LPWM, pwmKiriMundur);
}

// ====================== MULAI BELOK KIRI ======================

void mulaiBelokKiri(float derajat) {
  float headingSekarang = getHeading();

  // Untuk belok kiri target heading dikurangi 90 derajat.
  targetTurnHeading = normalizeAngle(headingSekarang - derajat);

  modeRobot = MODE_BELOK_KIRI;
  resetPID();

  Serial.println("================================");
  Serial.println("MODE: BELOK KIRI OTOMATIS");
  Serial.print("Heading sekarang : ");
  Serial.println(headingSekarang);
  Serial.print("Target heading   : ");
  Serial.println(targetTurnHeading);
  Serial.println("================================");
}

// ====================== UPDATE BELOK KIRI ======================

void updateBelokKiri() {
  float heading = getHeading();
  float errTurn = angleError(targetTurnHeading, heading);

  Serial.print("BELOK KIRI | Target: ");
  Serial.print(targetTurnHeading);
  Serial.print(" | Heading: ");
  Serial.print(heading);
  Serial.print(" | Error: ");
  Serial.println(errTurn);

  if (abs(errTurn) <= turnTolerance) {
    motorStop();

    setPoint = targetTurnHeading;
    resetPID();

    modeRobot = MODE_STOP_SEBENTAR;
    modeStartTime = millis();

    Serial.println("================================");
    Serial.println("BELOK KIRI SELESAI");
    Serial.print("SetPoint baru: ");
    Serial.println(setPoint);
    Serial.println("Robot berhenti sebentar.");
    Serial.println("================================");
  } else {
    motorBelokKiri(turnPWM);
  }
}

// ====================== UPDATE MAJU PID ======================

void updateMajuPID() {
  float heading = getHeading();

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;

  if (dt <= 0.001) {
    return;
  }

  lastTime = now;

  error = angleError(setPoint, heading);

  integral += error * dt;
  integral = constrain(integral, -20.0, 20.0);

  float derivative = (error - lastError) / dt;

  float correction = (Kp * error) + (Ki * integral) + (Kd * derivative);

  lastError = error;

  jalanMajuKoreksi(correction);

  Serial.print("MAJU PID | Target: ");
  Serial.print(setPoint);
  Serial.print(" | Heading: ");
  Serial.print(heading);
  Serial.print(" | Error: ");
  Serial.print(error);
  Serial.print(" | Koreksi: ");
  Serial.println(correction);

  // Kalau sudah maju selama durasiMaju, mulai belok kiri
  if (millis() - modeStartTime >= durasiMaju) {
    motorStop();
    delay(300);
    mulaiBelokKiri(90.0);
  }
}

// ====================== UPDATE STOP SEBENTAR ======================

void updateStopSebentar() {
  motorStop();

  if (millis() - modeStartTime >= durasiStop) {
    modeRobot = MODE_MAJU_PID;
    modeStartTime = millis();
    resetPID();

    Serial.println("================================");
    Serial.println("LANJUT MAJU PID");
    Serial.print("SetPoint: ");
    Serial.println(setPoint);
    Serial.println("================================");
  }
}

// ====================== SETUP ======================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("SENAGARDA ROVER PID + BELOK KIRI DELAY");
  Serial.println("ESP32-S3 Core 2.x");
  Serial.println("================================");

  pinMode(EN_ALL, OUTPUT);
  digitalWrite(EN_ALL, HIGH);

  setupPWM();
  motorStop();

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bno.begin()) {
    Serial.println("ERROR: Sensor BNO055 tidak terbaca!");
    Serial.println("Periksa kabel SDA, SCL, VCC, dan GND.");

    while (1) {
      motorStop();
      delay(10);
    }
  }

  delay(2000);

  setPoint = getHeading();
  lastTime = millis();
  modeStartTime = millis();
  isReady = true;

  Serial.println("================================");
  Serial.println("ROBOT SIAP");
  Serial.println("Mode awal: MAJU PID");
  Serial.print("SetPoint awal: ");
  Serial.println(setPoint);
  Serial.print("Durasi maju sebelum belok: ");
  Serial.print(durasiMaju / 1000);
  Serial.println(" detik");
  Serial.println("================================");
}

// ====================== LOOP ======================

void loop() {
  if (!isReady) {
    return;
  }

  if (modeRobot == MODE_MAJU_PID) {
    updateMajuPID();
    delay(20);
    return;
  }

  if (modeRobot == MODE_BELOK_KIRI) {
    updateBelokKiri();
    delay(20);
    return;
  }

  if (modeRobot == MODE_STOP_SEBENTAR) {
    updateStopSebentar();
    delay(20);
    return;
  }
}