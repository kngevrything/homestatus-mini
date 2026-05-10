#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int OLED_SDA = 21;
const int OLED_SCL = 22;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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

  showStatus();

  Serial.println();
  Serial.println("HomeStatus Mini ready.");
  Serial.println("Commands:");
  Serial.println("  ok");
  Serial.println("  warning");
  Serial.println("  alert");
  Serial.println("  info");
  Serial.println("  clear");
  Serial.println("  set|alert|GARAGE|Open too long|Alert active");
}

void loop() {
  handleButton();
  handleSerial();
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
  // Expected:
  // set|level|title|mainText|footer

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

  if (levelText == "ok") {
    level = STATUS_OK;
  } else if (levelText == "warning") {
    level = STATUS_WARNING;
  } else if (levelText == "alert") {
    level = STATUS_ALERT;
  } else if (levelText == "info") {
    level = STATUS_INFO;
  } else {
    Serial.print("Invalid level: ");
    Serial.println(levelText);
    return;
  }

  setStatus(level, title, mainText, footer);
}

void setStatus(StatusLevel level, String title, String mainText, String footer) {
  currentStatus.level = level;
  currentStatus.title = title;
  currentStatus.mainText = mainText;
  currentStatus.footer = footer;

  showStatus();

  Serial.print("Status updated: ");
  Serial.print(title);
  Serial.print(" / ");
  Serial.print(mainText);
  Serial.print(" / ");
  Serial.println(footer);
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
      // Acknowledged but not fully cleared.
      // Yellow keeps it visible without saying urgent.
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