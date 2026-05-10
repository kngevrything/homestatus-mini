#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "wifi_config.h"

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int OLED_SDA = 21;
const int OLED_SCL = 22;

const int BUTTON_PIN = 18;

// Confirmed RGB mapping
const int GREEN_PIN = 25;
const int RED_PIN   = 26;
const int BLUE_PIN  = 27;

const char* DEVICE_NAME = "homestatus-mini";

const unsigned long WIFI_CHECK_INTERVAL_MS = 10000;
unsigned long lastWifiCheckMs = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

enum StatusLevel {
  STATUS_OK,
  STATUS_WARNING,
  STATUS_ALERT,
  STATUS_INFO,
  STATUS_ACKED
};

struct StatusScreen {
  StatusLevel level;
  String title;
  String mainText;
  String footer;
};

StatusScreen currentStatus = {
  STATUS_OK,
  "HOME",
  "All Good",
  "No alerts"
};

bool lastButtonState = HIGH;
String serialBuffer = "";

// Forward declarations
void setupPins();

void setupDisplay();
void drawScreen(String title, String mainText, String footer);

void connectToWifi();
void checkWifiConnection();
String wifiStatusToString(wl_status_t status);

void setupHttpRoutes();
void handleRoot();
void handleStatusJson();
void handleHealth();
void handleReboot();
void handleSetFromHttp();
void sendPlain(String message);

void handleButton();
void onButtonPressed();

void handleSerial();
void processCommand(String command);
void processSetCommand(String command);
void printHelp();

bool tryParseStatusLevel(String levelText, StatusLevel& level);

void setStatus(StatusLevel level, String title, String mainText, String footer);
void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate);

void setDefaultOk();
void setDefaultWarning();
void setDefaultAlert();
void setDefaultInfo();

void showStatus();
void updateLed(StatusLevel level);
void allOff();

String statusLevelToString(StatusLevel level);
String escapeJson(String value);
String escapeHtml(String value);

void setup() {
  Serial.begin(115200);

  setupPins();
  setupDisplay();

  setStatus(STATUS_OK, "HOME", "All Good", "No alerts", false);

  connectToWifi();
  setupHttpRoutes();

  Serial.println();
  Serial.println("HomeStatus Mini ready.");
  printHelp();
}

void loop() {
  handleButton();
  handleSerial();
  server.handleClient();
  checkWifiConnection();
}