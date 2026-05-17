void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
}

void setDefaultOk() {
  setStatus(STATUS_OK, "HOME", "All Good", "No alerts");
}

void setDefaultWarning() {
  setStatus(STATUS_WARNING, "GARAGE", "Open 29 min", "Warning");
}

void setDefaultAlert() {
  setStatus(STATUS_ALERT, "GARAGE", "Open too long", "Alert active");
}

void setDefaultInfo() {
  setStatus(STATUS_INFO, "WASHER", "Done", "Move laundry");
}

void setStatus(StatusLevel level, String title, String mainText, String footer) {
  setStatus(level, title, mainText, footer, true);
}

bool setDefaultWarningWithPriority() {
  return setStatusWithPriority(STATUS_WARNING, "GARAGE", "Open 29 min", "Warning");
}

bool setDefaultAlertWithPriority() {
  return setStatusWithPriority(STATUS_ALERT, "GARAGE", "Open too long", "Alert active");
}

bool setDefaultInfoWithPriority() {
  return setStatusWithPriority(STATUS_INFO, "WASHER", "Done", "Move laundry");
}

void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate) {
  currentStatus.level = level;
  currentStatus.title = title;
  currentStatus.mainText = mainText;
  currentStatus.footer = footer;

  showStatus();

  publishMqttStatus();

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

void allOff() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
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

int statusPriority(StatusLevel level) {
  switch (level) {
    case STATUS_ALERT:
      return 4;

    case STATUS_ACKED:
      return 4;

    case STATUS_WARNING:
      return 3;

    case STATUS_INFO:
      return 2;

    case STATUS_OK:
      return 1;
  }

  return 0;
}

bool shouldAcceptStatusUpdate(StatusLevel incomingLevel, StatusLevel currentLevel) {
  // OK is an explicit clear. Always accept it.
  if (incomingLevel == STATUS_OK) {
    return true;
  }

  // Alert acknowledged should not be overwritten by lower priority messages.
  if (currentLevel == STATUS_ACKED && incomingLevel != STATUS_ALERT) {
    return false;
  }

  return statusPriority(incomingLevel) >= statusPriority(currentLevel);
}

bool setStatusWithPriority(StatusLevel level, String title, String mainText, String footer) {
  if (!shouldAcceptStatusUpdate(level, currentStatus.level)) {
    Serial.print("Status update ignored due to priority. Incoming: ");
    Serial.print(statusLevelToString(level));
    Serial.print(", current: ");
    Serial.println(statusLevelToString(currentStatus.level));
    return false;
  }

  setStatus(level, title, mainText, footer);
  return true;
}