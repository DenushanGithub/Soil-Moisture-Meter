#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <ESP_Google_Sheet_Client.h>
#include <Discord_WebHook.h>

// secrets.h

// WiFi credentials
#define WIFI_SSID "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Google Sheets/Drive API credentials
#define PROJECT_ID "Your_Project_ID"
#define CLIENT_EMAIL "Your_Client_Email"
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nqwertyuiopasdfghjklzxcvbnm=\n-----END PRIVATE KEY-----\n";

// Google Sheets spreadsheet ID
const char spreadsheetId[] = "Your_Spreadsheet_ID";

// Discord Webhook URL
String DISCORD_WEBHOOK = "Your_Discord_Webhook_URL";


// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

// Token Callback function
void tokenStatusCallback(TokenInfo info);

// NTP server to request epoch time
const char * ntpServer = "pool.ntp.org";

// Variable to save current epoch time
String currentTime;

// Soil moisture thresholds
const int wetSoilThreshold = 1200;
const int drySoilThreshold = 2000;

// Soil moisture sensor configuration
const int sensorPin = 35;
int moisture;

// WiFi connection timeout
const int wifiTimeout = 30000; // 30 seconds

bool connectWiFi(int timeout) {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int elapsed = 0;
  while (WiFi.status() != WL_CONNECTED && elapsed < timeout) {
    delay(1000);
    Serial.print(".");
    elapsed += 1000;
  }
  return WiFi.status() == WL_CONNECTED;
}

void setup() {
  Serial.begin(9600);

  // Configure time
  configTime(0, 0, ntpServer);

  // Connect to Wi-Fi with timeout
  Serial.print("Connecting to Wi-Fi");
  if (connectWiFi(wifiTimeout)) {
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi");
    return; // Exit setup if WiFi connection fails
  }

  // Set the callback for Google API access token generation status (for debug only)
  GSheet.setTokenCallback(tokenStatusCallback);

  // Set the seconds to refresh the auth token before expire (60 to 3540, default is 300 seconds)
  GSheet.setPrerefreshSeconds(10 * 60);

  // Begin the access token generation for Google API authentication
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

  // Discord setup
  discord.begin(DISCORD_WEBHOOK);
  discord.addWiFi(WIFI_SSID, WIFI_PASSWORD);
  discord.connectWiFi();

  delay(5000);
}

// Function that gets current epoch time
String getDateTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return ("Failed to obtain time");
  }
  char datetime[20]; // Adjust the size as needed
  strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(datetime);
}

void sendDiscordNotification(int moisture) {
  discord.send("------------------------------------------");
  discord.send("Soil Moisture Reading: " + String(moisture));
  if (moisture < wetSoilThreshold) {
    discord.send("Status: Soil is too wet");
  } else if (moisture >= wetSoilThreshold && moisture < drySoilThreshold) {
    discord.send("Status: Soil moisture is perfect");
  } else {
    discord.send("Status: Soil is too dry - time to water!");
  }
}

void appendToSheet() {
  FirebaseJson response;
  FirebaseJson valueRange;

  // Read the soil moisture level
  moisture = analogRead(sensorPin);

  // Get timestamp
  currentTime = getDateTime();
  valueRange.add("majorDimension", "COLUMNS");
  valueRange.set("values/[0]/[0]", currentTime);
  valueRange.set("values/[1]/[0]", moisture);

  // Append values to the spreadsheet
  bool success = GSheet.values.append(&response, spreadsheetId, "Sheet1!A1", &valueRange);
  if (success) {
    response.toString(Serial, true);
    valueRange.clear();
  } else {
    Serial.println(GSheet.errorReason());
  }
  Serial.println();
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  bool ready = GSheet.ready();
  unsigned long currentMillis = millis();

  if (ready && currentMillis - lastTime > timerDelay) {
    lastTime = currentMillis;
    appendToSheet();
    sendDiscordNotification(moisture);
  }
}

// Callback function for token status
void tokenStatusCallback(TokenInfo info) {
  GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
  if (info.status == token_status_error) {
    GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
  }
}
