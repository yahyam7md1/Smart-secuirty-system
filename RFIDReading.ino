#include <WiFi.h>
#include <HTTPClient.h>
#include <MFRC522.h>

// Wi-Fi Credentials
const char* ssid = "Suhas alcatel";
const char* password = "suhamirza";

// RFID Pins
#define RST_PIN 22
#define SDA_PIN 5
MFRC522 rfid(SDA_PIN, RST_PIN);

// ESP32-CAM API Endpoints
const char* motionActivateURL = "http://192.168.235.114/activate";  // Replace with ESP8266 IP
const char* motionDeactivateURL = "http://192.168.235.114/deactivate"; // Replace with ESP8266 IP

// Predefined RFID Tag IDs in byte format
byte activateTag[] = {0x37, 0xCD, 0xCD, 0xCF}; // Replace with actual Activate tag ID
byte deactivateTag[] = {0x83, 0x01, 0x64, 0xA6}; // Replace with actual Deactivate tag ID

void setup() {
  Serial.begin(115200);

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID initialized");
}

void loop() {
  // Check for a new RFID card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (compareRFID(rfid.uid.uidByte, rfid.uid.size, activateTag, sizeof(activateTag))) {
      Serial.println("Activate tag scanned: Activating system...");
      sendCameraRequest(motionActivateURL);
    } else if (compareRFID(rfid.uid.uidByte, rfid.uid.size, deactivateTag, sizeof(deactivateTag))) {
      Serial.println("Deactivate tag scanned: Deactivating system...");
      sendCameraRequest(motionDeactivateURL);
    } else {
      Serial.println("Unknown tag scanned");
    }
    rfid.PICC_HaltA();
  }
}

// Function to compare RFID tags
bool compareRFID(byte* scannedTag, byte scannedSize, byte* knownTag, byte knownSize) {
  if (scannedSize != knownSize) return false;
  for (byte i = 0; i < scannedSize; i++) {
    if (scannedTag[i] != knownTag[i]) return false;
  }
  return true;
}

// Function to send HTTP requests to the ESP32-CAM
void sendCameraRequest(const char* url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Request sent: " + String(httpCode));
    } else {
      Serial.println("Request failed");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}