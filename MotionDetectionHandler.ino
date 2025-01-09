#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h> // Include the secure client


// Wi-Fi Credentials
const char* ssid = "Suhas alcatel";
const char* password = "suhamirza";

// ESP32-CAM API Endpoint
const char* captureImageURL = "http://192.168.235.173/activate"; // URL to trigger image capture on ESP32 CAM

// Telegram Bot Token and Chat ID
const char* botToken = "7398468940:AAFPPGkExlsMESYVfauZXVuiF4XT6qLlfN0"; // Your bot token
const char* chatID = "1785699902"; // Your chat ID

// PIR Motion Sensor Pin
#define PIR_PIN D6 // GPIO12 for NodeMCU

// Buzzer Pin
#define BUZZER_PIN D0 // GPIO16 for NodeMCU

ESP8266WebServer server(80);
WiFiClient wifiClient; // Create a WiFi client
bool systemActivated = true; // System activation state

void setup() {
  Serial.begin(115200);

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("ESP8266 IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize PIR motion sensor
  pinMode(PIR_PIN, INPUT);

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially

  // Set up server routes
  server.on("/activate", []() {
    systemActivated = true;
    server.send(200, "text/plain", "System Activated");
    Serial.println("System Activated: PIR sensor enabled.");
  });

  server.on("/deactivate", []() {
    systemActivated = false;
    server.send(200, "text/plain", "System Deactivated");
    Serial.println("System Deactivated: PIR sensor disabled.");
  });

  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient(); // Handle incoming HTTP requests

  // Check motion only if system is activated
  if (systemActivated) {
    int motionState = digitalRead(PIR_PIN);
    if (motionState == HIGH) {
      Serial.println("Motion Detected! Sending capture request...");
      beepBuzzer(); // Trigger the buzzer
      sendCaptureRequest(); // Send request to capture an image
      sendTelegramMessage("Motion detected by your ESP8266 device!"); // Send Telegram notification
      delay(5000); // Prevent repeated requests
    }
  }
}

// Function to send HTTP request to ESP32-CAM
void sendCaptureRequest() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(wifiClient, captureImageURL); // Updated: Use WiFiClient with URL
    int httpCode = http.GET();  // Send GET request

    if (httpCode > 0) {
      Serial.println("Request sent successfully. Response code: " + String(httpCode));
    } else {
      Serial.println("Request failed. Error: " + String(http.errorToString(httpCode).c_str()));
    }

    http.end(); // Close connection
  } else {
    Serial.println("WiFi not connected. Cannot send request.");
  }
}

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client; // Use WiFiClientSecure for HTTPS
    HTTPClient http;

    client.setInsecure(); // Disable certificate verification (use for testing; consider adding proper certificates for production)

    String telegramURL = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;

    http.begin(client, telegramURL); // Use secure client with the URL
    int httpCode = http.GET(); // Send GET request

    if (httpCode > 0) {
      Serial.println("Telegram notification sent! HTTP code: " + String(httpCode));
      String response = http.getString();
      Serial.println("Response: " + response); // Debug response from Telegram
    } else {
      Serial.println("Telegram notification failed! Error: " + String(http.errorToString(httpCode).c_str()));
    }

    http.end(); // Close connection
  } else {
    Serial.println("WiFi not connected. Cannot send Telegram message.");
  }
}

// Function to beep the buzzer for 2-3 seconds
void beepBuzzer() {
  for (int i = 0; i < 5; i++) { // Adjust the loop count for desired beep duration
    digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
    delay(200);                     // Beep duration (200ms)
    digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
    delay(200);                     // Pause between beeps (200ms)
  }
}
