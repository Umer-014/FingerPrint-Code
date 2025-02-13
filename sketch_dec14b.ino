#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  
#include <time.h> 


#define RX_PIN 16  // GPIO for RX (to TX of the sensor)
#define TX_PIN 17  // GPIO for TX (to RX of the sensor)

// Power saving settings
#define WAKE_TIME_MS 60000      
#define SLEEP_TIME_SECONDS 60 

HardwareSerial mySerial(2);  // Use Serial2 for ESP32
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

int finger_status = -1;
uint8_t id;

const char* ssid = "iPhone";  // Wi-Fi SSID
const char* password = "123456789";  // Wi-Fi Password


// Timezone configuration
const char* ntpServer = "pool.ntp.org";  // NTP server
const long gmtOffset_sec = 5 * 3600;    // GMT+5 for Pakistan Standard Time
const int daylightOffset_sec = 0;      // No daylight saving time in Pakistan

// API Endpoints
const String scanEndpoint = "http://16.171.32.161:4000/api/fingerprint/scan";
const String enrollEndpoint = "http://16.171.32.161:4000/api/fingerprint/enroll";
const String countEndpoint = "http://16.171.32.161:4000/api/fingerprint/count";


void setup() {
  Serial.begin(9600);  // Initialize the serial monitor
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);  // Configure Serial2 for ESP32
  finger.begin(57600);  // Initialize the fingerprint sensor
  

  WiFi.begin(ssid, password);  // Connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");


  Serial.println("Fingerprint Sensor Menu:");
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found!");
  } else {
    Serial.println("Fingerprint sensor not found!");
    while (1) delay(1);  // Stop if sensor is not found
  }

  // Display the current time when the program starts
  String currentTime = getCurrentTime();
  if (currentTime != "") {
    Serial.print("Current time: ");
    Serial.println(currentTime);  // Print the formatted time to the serial monitor
  }

  showMainMenu();


}

void showMainMenu() {
  Serial.println("\nSelect an option:");
  Serial.println("1. Enroll Fingerprint");
  Serial.println("2. Scan Fingerprint");
  Serial.println("3. Count Fingerprints");
  Serial.println("4. Exit");

  while (!Serial.available());  // Wait for user input
  int choice = Serial.parseInt();
  processMenuChoice(choice);
}

void processMenuChoice(int choice) {
  switch (choice) {
    case 1:
      enrollFingerprint();
      break;
    case 2:
      scanFingerprint();
      break;
    case 3:
      countFingerprints();
      break;
    case 4:
      Serial.println("Exiting...");
      delay(2000);
      return;  // Exit program
    default:
      Serial.println("Invalid option. Try again.");
      delay(2000);
      showMainMenu();
      break;
  }
}

void enrollFingerprint() {
  Serial.print("Enter ID for the fingerprint to be enrolled (1-127): ");
  delay(5000);
  while (!Serial.available());
  id = Serial.parseInt();
  
  if (id == 0) {
    Serial.println("ID #0 is not allowed. Try again.");
    return;
  }

  finger_status = getFingerprintIDez();
  if (finger_status != -1) {
    delay(1000);
    Serial.println("Finger already in the database");
    Serial.println("Use the another finger");
  } 
  else {
    Serial.println("Fingerprint not found..");
    Serial.println("Trying to save Finger..");
    int p = getFingerprintEnroll();
    if (p == FINGERPRINT_OK) {
      Serial.println("Fingerprint enrollment successful!");
      sendFingerprintData(id);  // Send fingerprint ID and timestamp for enrollment
    } else {
      Serial.println("Fingerprint enrollment failed!");
    }
  }

  delay(3000);
  showMainMenu();
}


uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Enrolling Fingerprint as ID #");
  Serial.println(id);
  
  // Capture first fingerprint image
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }
  
  // Convert image to characteristics
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return p;
  Serial.println("Image converted");
  
  // Ask user to remove and re-place finger
  Serial.println("Remove finger");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  
  Serial.println("Place same finger again");
  while ((p = finger.getImage()) != FINGERPRINT_OK) ;
  Serial.println("Image taken");
  
  // Convert second image to characteristics
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return p;
  
  // Create fingerprint model
  p = finger.createModel();
  if (p != FINGERPRINT_OK) return p;
  
  // Store fingerprint model
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint stored!");
  } else {
    return p;
  }
  
  return FINGERPRINT_OK;
}


void scanFingerprint() {
  Serial.println("Scanning for a matching fingerprint...");
  Serial.println("Place your finger...");
  delay(5000);
  finger_status = getFingerprintIDez();
  if (finger_status != -1) {
    Serial.print("Fingerprint match found! ID: ");
    Serial.println(finger_status);
    SendscanFingerprint(finger_status, "success");  // Send matched fingerprint ID, timestamp, and status
  } else {
    Serial.println("No matching fingerprint found.");
    SendscanFingerprint(finger_status, "Not Found");  // Send 0 if no match
  }

  delay(3000);
  showMainMenu();
}

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;
  
  return finger.fingerID;
}

void countFingerprints() {
  uint16_t templateCount = getFingerprintCount();

  if (templateCount > 0) {
    Serial.print("Total fingerprint models stored: ");
    Serial.println(templateCount);
    sendFingerprintCount(templateCount);  // Send the count to backend
  } else {
    Serial.println("No fingerprints stored.");
  }

  delay(3000);
  showMainMenu();
}

uint16_t getFingerprintCount() {
  int p = finger.getTemplateCount();
  if (p == FINGERPRINT_OK) {
    return finger.templateCount;
  } else {
    Serial.print("Error retrieving template count: ");
    switch (p) {
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    return 0;
  }
}

// Function to get the current time in formatted string
String getCurrentTime() {
    // Set time zone to Pakistan (UTC+5)
    configTime(5 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Failed to obtain time.");
        return "";
    }
    
    // Format the time as a string (ISO 8601 format: YYYY-MM-DD HH:MM:SS)
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);

    return String(timeStr);  // Return the formatted time as a string
}


void sendFingerprintData(uint8_t fingerprintID) {
  HTTPClient http;
  String endpoint = enrollEndpoint;  // Enroll endpoint URL
  
  http.begin(endpoint);  // Start HTTP request
  http.addHeader("Content-Type", "application/json");  // Set header
  
  String timestamp = getCurrentTime();  // Fetch the current timestamp as a string
  
  // Check if timestamp is empty
  if (timestamp == "") {
    Serial.println("Timestamp is empty! Check time retrieval.");
    return;  // Exit the function if time could not be retrieved
  }
  
  // Append the timezone offset to the timestamp (UTC +5:00)
  timestamp += "+05:00";
  
  // Create the JSON payload
  String payload = "{\"fingerId\": " + String(fingerprintID) + 
                   ", \"timestamp\": \"" + timestamp + "\"}";  // Fixed the payload creation

  // Send HTTP POST request
  int httpResponseCode = http.POST(payload);  // Capture the response code after sending the POST request
  Serial.println("Payload: " + payload);

  if (httpResponseCode > 0) {
    String response = http.getString();  // Read the response body (if any)
    Serial.println("Data sent successfully");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + response);  // Print the server's response
    uint16_t templateCount = getFingerprintCount();
    sendFingerprintCount(templateCount);  // Send the count to backend
    Serial.println(templateCount);
  } else {
    Serial.println("Error sending data");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));  // Print error code
  }

  http.end();  // Close the connection
  goToSleep();
}




void SendscanFingerprint(uint8_t fingerprintID, String status) {
  HTTPClient http;
  String endpoint = scanEndpoint;  // Scan endpoint URL
  
  http.begin(endpoint);  // Start HTTP request
  http.addHeader("Content-Type", "application/json");  // Set header
  
    String timestamp = getCurrentTime();  // Fetch the current timestamp as a string
  
  // Check if timestamp is empty
  if (timestamp == "") {
    Serial.println("Timestamp is empty! Check time retrieval.");
    return;  // Exit the function if time could not be retrieved
  }
  
  // Append the timezone offset to the timestamp (UTC +5:00)
  timestamp += "+05:00";
  
  // Create the JSON payload for scanning
  String payload = "{\"fingerId\": " + String(fingerprintID) + 
                   ", \"timestamp\": \"" + timestamp + "\", \"status\": \"" + status + "\"}";
  
  // Send HTTP POST request
  int httpResponseCode = http.POST(payload);  
  if (httpResponseCode > 0) {
    String response = http.getString();  // Read the response body (if any)
    Serial.println("Data sent successfully");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + response);  // Print the server's response
  } else {
    Serial.println("Error scanning fingerprint");
    Serial.println(httpResponseCode);  // HTTP response code
  }

  http.end();  // Close the connection
  goToSleep();
}

void sendFingerprintCount(uint16_t count) {
  HTTPClient http;
  http.begin(countEndpoint);  // Use the count endpoint
  http.addHeader("Content-Type", "application/json");  // Specify content type

  String payload = "{\"count\": " + String(count) + "}";
  Serial.println("Payload: " + payload);  // Log the payload to confirm the data being sent


  int httpResponseCode = http.POST(payload);  // Send POST request with data
  if (httpResponseCode > 0) {
     String response = http.getString(); 
    Serial.println("Data sent successfully");
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + response);  // Print the server's response
  } else {
    Serial.println("Error sending count data");
    Serial.println(httpResponseCode);  // Print HTTP response code
  }

  http.end();  // Close the connection
  goToSleep();
}


void goToSleep() {
  Serial.println("Going to deep sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}


void loop() {

}


