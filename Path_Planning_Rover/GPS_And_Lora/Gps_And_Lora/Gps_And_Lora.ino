#include <Arduino.h>

// WAJIB diletakkan SEBELUM #include "LoRa_E22.h"
#define E22_DISABLE_SOFTWARE_SERIAL 

#include <TinyGPS++.h>
#include "LoRa_E22.h"

// --- KONFIGURASI PIN ESP32-S3 ---
// Pin Kontrol LoRa E22
#define LORA_M0  5
#define LORA_M1  6
#define LORA_AUX 4

// Pin Serial LoRa E22 (Menggunakan UART 1)
#define LORA_RX  15
#define LORA_TX  16

// Pin Serial GPS (Menggunakan UART 2)
#define GPS_RX   18
#define GPS_TX   17

// Deklarasi HardwareSerial untuk ESP32
HardwareSerial SerialLoRa(1); // UART1
HardwareSerial SerialGPS(2);  // UART2

// Inisialisasi LoRa menggunakan SerialLoRa
LoRa_E22 e22ttl(&SerialLoRa, LORA_AUX, LORA_M0, LORA_M1);
TinyGPSPlus gps;

float latitude = 0.0;
float longitude = 0.0;

unsigned long lastGpsSendTime = 0;
const unsigned long gpsSendInterval = 2000; // 2 detik

// Struktur Waypoint
struct Waypoint {
  float lat;
  float lng;
};

// Array untuk menyimpan path planning
const int MAX_WAYPOINTS = 50;
Waypoint path[MAX_WAYPOINTS];
int waypointCount = 0;
int currentWaypointIndex = 0;

unsigned long lastMoveSimTime = 0;

void parsePathPlanning(String pathData) {
  // Format: "PATH:lat1,lng1;lat2,lng2;"
  if (!pathData.startsWith("PATH:")) {
    return;
  }
  
  String dataString = pathData.substring(5); // Hilangkan "PATH:"
  waypointCount = 0;
  currentWaypointIndex = 0;
  
  int startIdx = 0;
  int semicolonIdx = dataString.indexOf(';');
  
  while (semicolonIdx != -1 && waypointCount < MAX_WAYPOINTS) {
    String wpString = dataString.substring(startIdx, semicolonIdx);
    int commaIdx = wpString.indexOf(',');
    
    if (commaIdx != -1) {
      String latStr = wpString.substring(0, commaIdx);
      String lngStr = wpString.substring(commaIdx + 1);
      
      path[waypointCount].lat = latStr.toFloat();
      path[waypointCount].lng = lngStr.toFloat();
      waypointCount++;
    }
    
    startIdx = semicolonIdx + 1;
    semicolonIdx = dataString.indexOf(';', startIdx);
  }
  
  if (waypointCount > 0) {
    Serial.print("Berhasil menerima ");
    Serial.print(waypointCount);
    Serial.println(" waypoint!");
    e22ttl.sendMessage("ACK:OK");
  } else {
    Serial.println("Gagal parse waypoint!");
    e22ttl.sendMessage("ACK:ERR");
  }
}

void checkIncoming() {
  if (e22ttl.available() > 1) {
    ResponseContainer rc = e22ttl.receiveMessage();
    if (rc.status.code == 1) {
      String received = rc.data;
      Serial.print("Pesan masuk LoRa: ");
      Serial.println(received);
      
      if (received.startsWith("PATH:")) {
        parsePathPlanning(received);
      }
    } else {
      Serial.print("Error terima LoRa: ");
      Serial.println(rc.status.getResponseDescription());
    }
  }
}

void setup() {
  Serial.begin(115200); // Disarankan 115200 untuk ESP32
  delay(2000);
  Serial.println("=== LoRa PENGIRIM + GPS (ESP32-S3) ===");

  // Jalur LoRa: Mengaktifkan SerialLoRa (UART1) dengan kustomisasi PIN
  // Format: .begin(baudrate, konfigurasi, RX, TX)
  SerialLoRa.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e22ttl.begin();

  // Jalur GPS: Mengaktifkan SerialGPS (UART2) dengan kustomisasi PIN
  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  Serial.println("Sistem Siap. Mencari data GPS dan menunggu Path Planning...");
}

void loop() {
  // 1. Cek Pesan Masuk LoRa (Non-blocking)
  checkIncoming();
  
  // 2. Baca GPS (Non-blocking)
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }
  
  // 3. Kirim Koordinat GPS tiap interval tertentu
  if (millis() - lastGpsSendTime >= gpsSendInterval) {
    lastGpsSendTime = millis();
    
    // Hanya kirim jika koordinat valid
    if (gps.location.isValid()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      
      String dataKirim = String(latitude, 6) + "," + String(longitude, 6);
      ResponseStatus rs = e22ttl.sendMessage(dataKirim);
      
      Serial.print("Mengirim GPS: ");
      Serial.print(dataKirim);
      Serial.print(" -> Status: ");
      Serial.println(rs.getResponseDescription());
    } else {
      Serial.println("Menunggu sinyal GPS (invalid)...");
    }
  }
  
  // 4. Simulasi pergerakan rover (Print ke Serial Monitor)
  if (waypointCount > 0 && currentWaypointIndex < waypointCount) {
    if (millis() - lastMoveSimTime >= 3000) { // Simulasi gerak tiap 3 detik
      lastMoveSimTime = millis();
      Serial.print("Menuju waypoint ke-");
      Serial.print(currentWaypointIndex + 1);
      Serial.print(": lat ");
      Serial.print(path[currentWaypointIndex].lat, 6);
      Serial.print(", lng ");
      Serial.println(path[currentWaypointIndex].lng, 6);
      
      currentWaypointIndex++;
      if (currentWaypointIndex >= waypointCount) {
        Serial.println("Tujuan akhir (semua waypoint) telah dicapai!");
      }
    }
  }
}