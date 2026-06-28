#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h> // Pastikan install library ArduinoJson
#include "LoRa_E22.h"

#include <ESPmDNS.h>

// --- PIN LORA PADA ESP32-S3 ---
#define LORA_M0  4
#define LORA_M1  5
#define LORA_AUX 6
#define LORA_RX  18   // ESP32 terima dari TXD modul
#define LORA_TX  17   // ESP32 kirim ke RXD modul

LoRa_E22 e22ttl(&Serial2, LORA_AUX, LORA_M0, LORA_M1);

// Konfigurasi WiFi Hotspot HP
const char *WIFI_SSID = "Xiaomi 14T"; // Ganti dengan nama hotspot HP Anda
const char *WIFI_PASS = "12345678"; // Ganti dengan password hotspot HP Anda

// Inisialisasi WebSocket Server di port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// State untuk menunggu ACK dari LoRa
bool waitingForAck = false;
unsigned long ackStartTime = 0;
const unsigned long ackTimeout = 5000; // 5 detik

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
      
    case WStype_TEXT:
      Serial.printf("[%u] Received Text: %s\n", num, payload);
      
      // Parse JSON payload dari App
      StaticJsonDocument<2048> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      
      const char* msgType = doc["type"];
      if (strcmp(msgType, "path") == 0) {
        // Build LoRa payload string
        String loraPayload = "PATH:";
        JsonArray waypoints = doc["waypoints"];
        for (JsonObject wp : waypoints) {
          float lat = wp["lat"];
          float lng = wp["lng"];
          loraPayload += String(lat, 6) + "," + String(lng, 6) + ";";
        }
        
        Serial.print("Mengirim Path Planning ke LoRa: ");
        Serial.println(loraPayload);
        
        // Kirim via LoRa
        e22ttl.sendMessage(loraPayload);
        
        // Set state untuk tunggu ACK
        waitingForAck = true;
        ackStartTime = millis();
      }
      break;
  }
}

void setup() {
  Serial.begin(9600);
  delay(3000); // Berikan delay lebih lama (3 detik) agar USB-CDC siap stabil
  
  Serial.println("=== GATEWAY WIFI + LORA (ESP32-S3) ===");

  // Aktifkan Serial2 untuk komunikasi ke LoRa E22
  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e22ttl.begin();

  // Setup WiFi Station (Konek ke Hotspot HP)
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Gagal terhubung ke WiFi Hotspot!");
  } else {
    Serial.println("WiFi terhubung!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Inisialisasi mDNS
    if (!MDNS.begin("rover-gateway")) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started: rover-gateway.local");
    }
  }

  // Mulai WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("Gateway Siap. Menunggu koneksi WebSocket dan koordinat...");
}

void loop() {
  webSocket.loop();
  
  // Cek pesan dari LoRa
  if (e22ttl.available() > 1) {
    ResponseContainer rc = e22ttl.receiveMessage();
    if (rc.status.code != 1) {
      Serial.print("Error terima LoRa: ");
      Serial.println(rc.status.getResponseDescription());
    } else {
      String dataDiterima = rc.data;
      Serial.print("Mentah dari LoRa: ");
      Serial.println(dataDiterima);
      
      // Jika menunggu ACK
      if (waitingForAck && dataDiterima.startsWith("ACK:")) {
        waitingForAck = false;
        String status = dataDiterima.substring(4); // Ambil "OK" atau "ERR"
        
        // Broadcast ke semua ws client
        String wsMessage = "{\"type\":\"ack\",\"status\":\"" + status + "\"}";
        webSocket.broadcastTXT(wsMessage);
        Serial.print("Forward ACK to App: ");
        Serial.println(wsMessage);
      } 
      // Jika itu koordinat GPS
      else {
        int indexKoma = dataDiterima.indexOf(',');
        if (indexKoma != -1) {
          String strLat = dataDiterima.substring(0, indexKoma);
          String strLng = dataDiterima.substring(indexKoma + 1);
          
          float latitudeResult = strLat.toFloat();
          float longitudeResult = strLng.toFloat();
          
          // Broadcast ke WS Client sebagai JSON
          String wsMessage = "{\"type\":\"gps\",\"lat\":" + String(latitudeResult, 6) + ",\"lng\":" + String(longitudeResult, 6) + "}";
          webSocket.broadcastTXT(wsMessage);
        }
      }
    }
  }
  
  // Cek timeout ACK
  if (waitingForAck && (millis() - ackStartTime > ackTimeout)) {
    waitingForAck = false;
    Serial.println("Timeout menunggu ACK dari Rover.");
    // Broadcast timeout sebagai ERR
    webSocket.broadcastTXT("{\"type\":\"ack\",\"status\":\"ERR_TIMEOUT\"}");
  }
}