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

// LED Internal
#define LED_BUILTIN 2

Adafruit_BNO055 bno(55, 0x28);

// PID TUNING (Sempurna)
float Kp = 7.00;     
float Ki = 2.00;    
float Kd = 0.00;    

float setPoint = 0;  
float error;
float lastError = 0;
float integral = 0;

int minSpeed = 70;          // Dinaikkan sedikit agar tidak stall
int rotationSpeed = 110;  
int baseForwardSpeed = 160; // DITINGKATKAN DRASTIS agar torsi maju kuat

unsigned long lastTime;
bool isReady = false;  

float normalizeAngle(float angle)
{
  angle = fmod(angle, 360.0);
  if (angle < 0) angle += 360;
  return angle;
}

float angleError(float target, float current)
{
  float err = target - current;
  if (err > 180) err -= 360;
  if (err < -180) err += 360;
  return err;
}

void rotateRobot(float correction)
{
  int pwmValue = abs((int)correction);
  
  if (pwmValue > 0 && pwmValue < minSpeed) {
    pwmValue = minSpeed;
  }
  
  pwmValue = constrain(pwmValue, 0, rotationSpeed);
  
  if (correction < 0)  
  {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
  else if (correction > 0)  
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }
  else  
  {
    stopRobot();
    return;
  }
  
  ledcWrite(ENA, pwmValue);
  ledcWrite(ENB, pwmValue);
}

void moveForward(float correction)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  int leftPWM = baseForwardSpeed + (int)correction;
  int rightPWM = baseForwardSpeed - (int)correction;
  
  leftPWM = constrain(leftPWM, minSpeed, 255);
  rightPWM = constrain(rightPWM, minSpeed, 255);
  
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
}

void setup()
{
  Serial.begin(115200);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  
  if (!bno.begin())
  {
    while (1)
    {
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
  
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  setPoint = 0;  
  
  lastTime = millis();
  isReady = true;
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(1000);
}

void loop()
{
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  float heading = normalizeAngle(euler.x());
  
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  
  if (dt <= 0.001) 
  {
    delay(5);
    return;
  }
  
  lastTime = now;
  
  error = angleError(setPoint, heading);
  
  integral += error * dt;
  integral = constrain(integral, -20.0, 20.0);
  
  float derivative = (error - lastError) / dt;
  float correction = (Kp * error) + (Ki * integral) + (Kd * derivative);
  
  if (abs(error) > 15.0) 
  {
    rotateRobot(correction);
    Serial.print("Mode: ROTASI | ");
  }
  else 
  {
    moveForward(correction);
    Serial.print("Mode: JALAN MAJU | ");
  }
  
  lastError = error;
  
  Serial.print("Heading: ");
  Serial.print(heading);
  Serial.print("° | Error: ");
  Serial.println(error);
  
  delay(20); 
}