#include "Arduino.h"
#include "WiFi.h"
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Replace with your network credentials
const char* ssid = "";
const char* password = "";

// Insert Firebase project API Key
#define API_KEY ""

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL ""
#define USER_PASSWORD ""

#define STORAGE_BUCKET_ID ""

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"

#define FLASH_GPIO_NUM 4 // Use GPIO4 (D4) for the flash

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

boolean takeNewPhoto = true;

// Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

bool taskCompleted = false;
bool isActive = false; // Activation flag

WiFiServer server(80); // HTTP server on port 80

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info) {
  if (info.status == firebase_fcs_upload_status_init) {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  } else if (info.status == firebase_fcs_upload_status_upload) {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  } else if (info.status == firebase_fcs_upload_status_complete) {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
    Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
  } else if (info.status == firebase_fcs_upload_status_error) {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}
void capturePhotoAndUpload() {
    // Turn on the flash and allow it to stabilize
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    delay(500); // Allow the flash to stabilize

    // Attempt to capture a photo
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb || fb->len == 0) {
        Serial.println("Failed to capture photo or photo is empty. Reinitializing camera...");
        reinitializeCamera(); // Reinitialize the camera if capture fails
        digitalWrite(FLASH_GPIO_NUM, LOW);
        return;
    }

    // Generate unique file names for Firebase
    String timestamp = String(millis());
    String bucketPhotoPath = "/data/photo_" + timestamp + ".jpg";

    // Upload the photo to Firebase directly from the frame buffer
    if (Firebase.ready()) {
        Serial.println("Uploading photo to Firebase...");
        if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, fb->buf, fb->len, bucketPhotoPath.c_str(), "image/jpeg", fcsUploadCallback)) {
            Serial.println("Photo uploaded successfully");
        } else {
            Serial.printf("Photo upload failed: %s\n", fbdo.errorReason().c_str());
        }
    }

    // Release the frame buffer to free PSRAM
    esp_camera_fb_return(fb);
    Serial.println("Frame buffer memory released.");

    // Turn off the flash
    digitalWrite(FLASH_GPIO_NUM, LOW);
}


// Handle HTTP requests
void handleHTTPRequest() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    Serial.println("HTTP Request: " + request);

    if (request.indexOf("GET /activate") >= 0) {
      isActive = true;
      Serial.println("System Activated");
      client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nActivated");

      // Take a photo and upload it immediately when activated
      Serial.println("Capturing and uploading photo on activation...");
      capturePhotoAndUpload();
    } else if (request.indexOf("GET /deactivate") >= 0) {
      isActive = false;
      Serial.println("System Deactivated");
      client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDeactivated");
    }
    client.stop();
  }
}

void initWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error occurred while mounting LittleFS");
    ESP.restart();
  }
  Serial.println("LittleFS mounted successfully");
}

void initCamera() {
    // OV2640 camera module
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA; // High resolution
        config.jpeg_quality = 10;          // Lower quality for larger files
        config.fb_count = 2;               // Double buffering
    } else {
        config.frame_size = FRAMESIZE_SVGA; // Lower resolution
        config.jpeg_quality = 12;           // Higher quality for smaller files
        config.fb_count = 1;                // Single buffering
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x. Reinitializing...\n", err);
        reinitializeCamera(); // Reinitialize camera on failure
    }
}


void reinitializeCamera() {
    esp_camera_deinit(); // Deinitialize the camera to free resources
    initCamera();        // Reinitialize the camera
    Serial.println("Camera reinitialized.");
}


void setup() {
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  initCamera();
  server.begin();
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW); 
  // Firebase
  configF.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.token_status_callback = tokenStatusCallback;
  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Setup complete");
}

void loop() {
  handleHTTPRequest();
}
