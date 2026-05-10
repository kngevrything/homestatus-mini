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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WebServer server(80);

const int BUTTON_PIN = 18;

// Confirmed RGB mapping
const int GREEN_PIN = 25;
const int RED_PIN   = 26;
const int BLUE_PIN  = 27;

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
void connectToWifi();
void setupHttpRoutes();

void handleRoot();
void handleStatusJson();
void handleSetFromHttp();
void sendPlain(String message);

void handleButton();
void onButtonPressed();

void handleSerial();
void processCommand(String command);
void processSetCommand(String command);

bool tryParseStatusLevel(String levelText, StatusLevel& level);

void setStatus(StatusLevel level, String title, String mainText, String footer);
void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate);

void showStatus();
void updateLed(StatusLevel level);
void drawScreen(String title, String mainText, String footer);
void allOff();

String statusLevelToString(StatusLevel level);
String escapeJson(String value);
String escapeHtml(String value);

void printHelp();

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at address 0x3C");
    while (true) {
      delay(1000);
    }
  }

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
}

void connectToWifi() {
  drawScreen("WIFI", "Connecting", WIFI_SSID);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();

    Serial.println("Wi-Fi connected.");
    Serial.print("IP address: ");
    Serial.println(ip);

    drawScreen("WIFI", "Connected", ip);
    delay(1500);
  } else {
    Serial.println("Wi-Fi connection failed.");
    drawScreen("WIFI", "Failed", "Serial still works");
    delay(2000);
  }

  showStatus();
}

void setupHttpRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatusJson);

  server.on("/ok", HTTP_GET, []() {
    setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
    sendPlain("OK state set");
  });

  server.on("/clear", HTTP_GET, []() {
    setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
    sendPlain("Status cleared");
  });

  server.on("/warning", HTTP_GET, []() {
    setStatus(STATUS_WARNING, "GARAGE", "Open 29 min", "Warning");
    sendPlain("Warning state set");
  });

  server.on("/alert", HTTP_GET, []() {
    setStatus(STATUS_ALERT, "GARAGE", "Open too long", "Alert active");
    sendPlain("Alert state set");
  });

  server.on("/info", HTTP_GET, []() {
    setStatus(STATUS_INFO, "WASHER", "Done", "Move laundry");
    sendPlain("Info state set");
  });

  server.on("/set", HTTP_GET, handleSetFromHttp);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void handleRoot() {
  String html = "";

  html += "<!doctype html>";
  html += "<html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>HomeStatus Mini</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:24px;line-height:1.4;}";
  html += "code{background:#eee;padding:2px 4px;border-radius:4px;}";
  html += "a{display:inline-block;margin:4px 8px 4px 0;padding:8px 12px;background:#eee;border-radius:8px;text-decoration:none;color:#111;}";
  html += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;max-width:520px;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"card\">";
  html += "<h1>HomeStatus Mini</h1>";
  html += "<p><strong>Level:</strong> " + statusLevelToString(currentStatus.level) + "</p>";
  html += "<p><strong>Title:</strong> " + escapeHtml(currentStatus.title) + "</p>";
  html += "<p><strong>Main:</strong> " + escapeHtml(currentStatus.mainText) + "</p>";
  html += "<p><strong>Footer:</strong> " + escapeHtml(currentStatus.footer) + "</p>";

  html += "<h2>Quick Actions</h2>";
  html += "<a href=\"/ok\">OK</a>";
  html += "<a href=\"/warning\">Warning</a>";
  html += "<a href=\"/alert\">Alert</a>";
  html += "<a href=\"/info\">Info</a>";
  html += "<a href=\"/clear\">Clear</a>";

  html += "<h2>Custom Set</h2>";
  html += "<p>Example:</p>";
  html += "<code>/set?level=alert&title=GARAGE&main=Open%20too%20long&footer=Alert%20active</code>";

  html += "<h2>Status JSON</h2>";
  html += "<p><a href=\"/status\">/status</a></p>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatusJson() {
  String json = "";

  json += "{";
  json += "\"level\":\"" + statusLevelToString(currentStatus.level) + "\",";
  json += "\"title\":\"" + escapeJson(currentStatus.title) + "\",";
  json += "\"mainText\":\"" + escapeJson(currentStatus.mainText) + "\",";
  json += "\"footer\":\"" + escapeJson(currentStatus.footer) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleSetFromHttp() {
  if (!server.hasArg("level")) {
    server.send(400, "text/plain", "Missing required query parameter: level");
    return;
  }

  String levelText = server.arg("level");
  String title = server.hasArg("title") ? server.arg("title") : "HOME";
  String mainText = server.hasArg("main") ? server.arg("main") : "Updated";
  String footer = server.hasArg("footer") ? server.arg("footer") : "HTTP";

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    server.send(400, "text/plain", "Invalid level. Use ok, warning, alert, info, or acked.");
    return;
  }

  setStatus(level, title, mainText, footer);

  String response = "Status updated: ";
  response += statusLevelToString(level);
  response += " / ";
  response += title;
  response += " / ";
  response += mainText;
  response += " / ";
  response += footer;

  server.send(200, "text/plain", response);
}

void sendPlain(String message) {
  server.send(200, "text/plain", message);
}

void handleButton() {
  bool buttonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && buttonState == LOW) {
    onButtonPressed();
    delay(250);
  }

  lastButtonState = buttonState;
}

void onButtonPressed() {
  if (currentStatus.level == STATUS_WARNING || currentStatus.level == STATUS_ALERT) {
    currentStatus.level = STATUS_ACKED;
    currentStatus.footer = "Acknowledged";
    showStatus();

    Serial.println("Alert acknowledged");
    return;
  }

  if (currentStatus.level == STATUS_INFO) {
    setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
    Serial.println("Info cleared");
    return;
  }

  if (currentStatus.level == STATUS_ACKED) {
    setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
    Serial.println("Acknowledged alert cleared");
    return;
  }

  Serial.println("No active alert to acknowledge");
}

void handleSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processCommand(String command) {
  command.trim();

  if (command == "help") {
    printHelp();
    return;
  }

  if (command == "ok" || command == "clear") {
    setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
    return;
  }

  if (command == "warning") {
    setStatus(STATUS_WARNING, "GARAGE", "Open 29 min", "Warning");
    return;
  }

  if (command == "alert") {
    setStatus(STATUS_ALERT, "GARAGE", "Open too long", "Alert active");
    return;
  }

  if (command == "info") {
    setStatus(STATUS_INFO, "WASHER", "Done", "Move laundry");
    return;
  }

  if (command.startsWith("set|")) {
    processSetCommand(command);
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
}

void processSetCommand(String command) {
  String parts[5];

  int partIndex = 0;
  int startIndex = 0;

  for (int i = 0; i < command.length(); i++) {
    if (command.charAt(i) == '|') {
      if (partIndex < 5) {
        parts[partIndex] = command.substring(startIndex, i);
        partIndex++;
        startIndex = i + 1;
      }
    }
  }

  if (partIndex < 4) {
    Serial.println("Invalid set command.");
    Serial.println("Use: set|alert|GARAGE|Open too long|Alert active");
    return;
  }

  parts[partIndex] = command.substring(startIndex);

  String levelText = parts[1];
  String title = parts[2];
  String mainText = parts[3];
  String footer = parts[4];

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    Serial.print("Invalid level: ");
    Serial.println(levelText);
    return;
  }

  setStatus(level, title, mainText, footer);
}

bool tryParseStatusLevel(String levelText, StatusLevel& level) {
  levelText.toLowerCase();

  if (levelText == "ok") {
    level = STATUS_OK;
    return true;
  }

  if (levelText == "warning") {
    level = STATUS_WARNING;
    return true;
  }

  if (levelText == "alert") {
    level = STATUS_ALERT;
    return true;
  }

  if (levelText == "info") {
    level = STATUS_INFO;
    return true;
  }

  if (levelText == "acked" || levelText == "acknowledged") {
    level = STATUS_ACKED;
    return true;
  }

  return false;
}

void setStatus(StatusLevel level, String title, String mainText, String footer) {
  setStatus(level, title, mainText, footer, true);
}

void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate) {
  currentStatus.level = level;
  currentStatus.title = title;
  currentStatus.mainText = mainText;
  currentStatus.footer = footer;

  showStatus();

  if (logUpdate) {
    Serial.print("Status updated: ");
    Serial.print(statusLevelToString(level));
    Serial.print(" / ");
    Serial.print(title);
    Serial.print(" / ");
    Serial.print(mainText);
    Serial.print(" / ");
    Serial.println(footer);
  }
}

void showStatus() {
  updateLed(currentStatus.level);
  drawScreen(currentStatus.title, currentStatus.mainText, currentStatus.footer);
}

void updateLed(StatusLevel level) {
  allOff();

  switch (level) {
    case STATUS_OK:
      digitalWrite(GREEN_PIN, HIGH);
      break;

    case STATUS_WARNING:
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(GREEN_PIN, HIGH);
      break;

    case STATUS_ALERT:
      digitalWrite(RED_PIN, HIGH);
      break;

    case STATUS_INFO:
      digitalWrite(BLUE_PIN, HIGH);
      break;

    case STATUS_ACKED:
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(GREEN_PIN, HIGH);
      break;
  }
}

void drawScreen(String title, String mainText, String footer) {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 22);
  display.println(mainText);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.println(footer);

  display.display();
}

void allOff() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

String statusLevelToString(StatusLevel level) {
  switch (level) {
    case STATUS_OK:
      return "ok";

    case STATUS_WARNING:
      return "warning";

    case STATUS_ALERT:
      return "alert";

    case STATUS_INFO:
      return "info";

    case STATUS_ACKED:
      return "acked";
  }

  return "unknown";
}

String escapeJson(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "\\r");
  return value;
}

String escapeHtml(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help");
  Serial.println("  ok");
  Serial.println("  clear");
  Serial.println("  warning");
  Serial.println("  alert");
  Serial.println("  info");
  Serial.println("  set|alert|GARAGE|Open too long|Alert active");
  Serial.println();
  Serial.println("HTTP routes:");
  Serial.println("  GET /");
  Serial.println("  GET /status");
  Serial.println("  GET /ok");
  Serial.println("  GET /warning");
  Serial.println("  GET /alert");
  Serial.println("  GET /info");
  Serial.println("  GET /clear");
  Serial.println("  GET /set?level=alert&title=GARAGE&main=Open%20too%20long&footer=Alert%20active");
}