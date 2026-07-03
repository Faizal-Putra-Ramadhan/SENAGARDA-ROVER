#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// ====================== PIN CONFIGURATION ======================
#define SDA_PIN 8
#define SCL_PIN 9
#define EN_ALL  21

// Motor Kiri (M1 & M3)
#define M1_RPWM 47
#define M1_LPWM 48
#define M3_RPWM 13
#define M3_LPWM 12

// Motor Kanan (M2 & M4)
#define M2_RPWM 35
#define M2_LPWM 36
#define M4_RPWM 10
#define M4_LPWM 11

// ====================== SENSOR & PID ======================
Adafruit_BNO055 bno(55, 0x28);

float Kp = 4.00; 
float Ki = 1.00;
float Kd = 1.00;

float setPoint = 0;
float error = 0, lastError = 0, integral = 0;

int minSpeed = 50;          // Batas bawah kecepatan agar motor tidak mendengung/stall
int baseForwardSpeed = 60; // Kecepatan dasar (cukup besar agar kuat jalan)

unsigned long lastTime;
bool isReady = false;

// ====================== FUNGSI MOTOR MAJU KOREKSI ======================
void jalanMajuKoreksi(float correction) {
  // Hitung kecepatan roda kiri dan kanan berdasarkan koreksi PID
  // Tanda plus (+) dan minus (-) DITUKAR
  int leftPWM = baseForwardSpeed - (int)correction;
  int rightPWM = baseForwardSpeed + (int)correction;

  // Pastikan nilai PWM tidak kurang dari minSpeed dan tidak lebih dari 255
  leftPWM = constrain(leftPWM, minSpeed, 255);
  rightPWM = constrain(rightPWM, minSpeed, 255);

  // Terapkan ke Motor Kiri (M1 & M3) -> Logika Majumu: LPWM yang diisi, RPWM = 0
  ledcWrite(M1_RPWM, 0); ledcWrite(M1_LPWM, leftPWM);
  ledcWrite(M3_RPWM, 0); ledcWrite(M3_LPWM, leftPWM);

  // Terapkan ke Motor Kanan (M2 & M4) -> Logika Majumu: RPWM yang diisi, LPWM = 0
  ledcWrite(M2_RPWM, rightPWM); ledcWrite(M2_LPWM, 0);
  ledcWrite(M4_RPWM, rightPWM); ledcWrite(M4_LPWM, 0);
}

// ====================== FUNGSI BANTUAN SUDUT ======================
float normalizeAngle(float angle) {
  angle = fmod(angle, 360.0);
  if (angle < 0) angle += 360;
  return angle;
}

float angleError(float target, float current) {
  float err = target - current;
  if (err > 180) err -= 360;
  if (err < -180) err += 360;
  return err;
}

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);

  // Aktifkan Driver Motor (Wajib nyala agar motor bisa berputar)
  pinMode(EN_ALL, OUTPUT);
  digitalWrite(EN_ALL, HIGH); 

  // Setup PWM Motor (Core 3.x)
  int pins[] = {M1_RPWM, M1_LPWM, M2_RPWM, M2_LPWM, M3_RPWM, M3_LPWM, M4_RPWM, M4_LPWM};
  for(int p : pins) {
    ledcAttach(p, 20000, 8);
  }

  // Setup Sensor BNO055
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bno.begin()) {
    Serial.println("ERROR: Sensor BNO055 tidak terbaca. Periksa kabel SDA (8) & SCL (9)!");
    while (1) { delay(10); } // Jika masuk sini, robot akan diam selamanya
  }
  delay(2000); // Beri waktu sensor untuk kalibrasi awal

  // Tetapkan posisi awal sebagai target lurus
  setPoint = normalizeAngle(bno.getVector(Adafruit_BNO055::VECTOR_EULER).x());
  
  lastTime = millis();
  isReady = true;
  Serial.println("ROBOT SIAP JALAN LURUS TERUS!");
}

// ====================== LOOP ======================
void loop() {
  if (!isReady) return;

  // 1. Baca arah hadap robot saat ini
  float heading = normalizeAngle(bno.getVector(Adafruit_BNO055::VECTOR_EULER).x());
  
  // 2. Hitung jarak waktu (dt)
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  if (dt <= 0.001) return;
  lastTime = now;

  // 3. Hitung Koreksi PID
  error = angleError(setPoint, heading);
  
  integral += error * dt;
  integral = constrain(integral, -20.0, 20.0); // Batasi integral
  
  float derivative = (error - lastError) / dt;
  float correction = (Kp * error) + (Ki * integral) + (Kd * derivative);
  lastError = error;

  // 4. Perintahkan motor maju dengan koreksi
  jalanMajuKoreksi(correction);

  // 5. Cetak ke Serial Monitor untuk mantau kondisi
  Serial.print("Target: "); Serial.print(setPoint);
  Serial.print(" | Saat Ini: "); Serial.print(heading);
  Serial.print(" | Error: "); Serial.print(error);
  Serial.print(" | Nilai Koreksi: "); Serial.println(correction);

  delay(20);
}