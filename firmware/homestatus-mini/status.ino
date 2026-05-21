// Status state management.
//
// Centralizes status updates, text normalization, priority handling,
// source-aware clearing, LED updates, OLED refresh, and MQTT status publishing.
// All runtime status changes should flow through setStatus() or
// setStatusWithPriority() so behavior stays consistent across HTTP, MQTT,
// Serial, Home Assistant, and button input.

void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
}

void normalizeStatusText(String& source, String& title, String& mainText, String& footer) {
  source = limitText(source, MAX_SOURCE_CHARS);
  title = limitText(title, MAX_TITLE_CHARS);
  mainText = limitText(mainText, MAX_MAIN_CHARS);
  footer = limitText(footer, MAX_FOOTER_CHARS);

  source.toLowerCase();
}

// -----------------------------------------------------------------------------
// Default statuses
// -----------------------------------------------------------------------------
void setDefaultOk() {
  setStatus(STATUS_OK, "", "HOME", "All Good", "No alerts");
}

void setDefaultOkQuiet() {
  setStatus(STATUS_OK, "", "HOME", "All Good", "No alerts", false);
}

void setDefaultWarning() {
  setStatus(STATUS_WARNING, "default", "GARAGE", "Open 29 min", "Warning");
}

void setDefaultAlert() {
  setStatus(STATUS_ALERT, "default", "GARAGE", "Open too long", "Alert active");
}

void setDefaultInfo() {
  setStatus(STATUS_INFO, "default", "WASHER", "Done", "Move laundry");
}

bool setDefaultWarningWithPriority() {
  return setStatusWithPriority(STATUS_WARNING, "default", "GARAGE", "Open 29 min", "Warning");
}

bool setDefaultAlertWithPriority() {
  return setStatusWithPriority(STATUS_ALERT, "default", "GARAGE", "Open too long", "Alert active");
}

bool setDefaultInfoWithPriority() {
  return setStatusWithPriority(STATUS_INFO, "default", "WASHER", "Done", "Move laundry");
}

// -----------------------------------------------------------------------------
// Status setters
// -----------------------------------------------------------------------------

void setStatus(StatusLevel level, String title, String mainText, String footer) {
  setStatus(level, "", title, mainText, footer, true);
}

void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate) {
  setStatus(level, "", title, mainText, footer, logUpdate);
}

void setStatus(StatusLevel level, String source, String title, String mainText, String footer) {
  setStatus(level, source, title, mainText, footer, true);
}

void setStatus(StatusLevel level, String source, String title, String mainText, String footer,
               bool logUpdate) {
  normalizeStatusText(source, title, mainText, footer);

  currentStatus.level = level;
  currentStatus.source = source;
  currentStatus.title = title;
  currentStatus.mainText = mainText;
  currentStatus.footer = footer;

  if (level == STATUS_OK) {
    currentStatus.source = "";
  }

  showStatus();

  publishMqttStatus();

  if (logUpdate) {
    Serial.print("Status updated: ");
    Serial.print(statusLevelToString(level));
    Serial.print(" / source=");
    Serial.print(currentStatus.source);
    Serial.print(" / ");
    Serial.print(title);
    Serial.print(" / ");
    Serial.print(mainText);
    Serial.print(" / ");
    Serial.println(footer);
  }
}

void writeStatusJson(JsonDocument& doc) {
  doc["device"] = getDeviceName();
  doc["level"] = statusLevelToString(currentStatus.level);
  doc["source"] = currentStatus.source;
  doc["title"] = currentStatus.title;
  doc["mainText"] = currentStatus.mainText;
  doc["footer"] = currentStatus.footer;
  doc["uptimeSeconds"] = millis() / 1000;
}

String buildStatusJson() {
  StaticJsonDocument<256> doc;

  writeStatusJson(doc);

  String json;
  serializeJson(doc, json);

  return json;
}

// -----------------------------------------------------------------------------
// Priority-aware status setters
// -----------------------------------------------------------------------------

bool setStatusWithPriority(StatusLevel level, String source, String title, String mainText,
                           String footer) {
  source.trim();

  if (level == STATUS_OK) {
    if (!shouldAcceptClear(source)) {
      Serial.print("Clear ignored due to source mismatch. Incoming source: ");
      Serial.print(source);
      Serial.print(", current source: ");
      Serial.println(currentStatus.source);
      return false;
    }

    setStatus(STATUS_OK, "", title, mainText, footer);
    return true;
  }

  if (!shouldAcceptStatusUpdate(level, currentStatus.level)) {
    Serial.print("Status update ignored due to priority. Incoming: ");
    Serial.print(statusLevelToString(level));
    Serial.print(", current: ");
    Serial.println(statusLevelToString(currentStatus.level));
    return false;
  }

  setStatus(level, source, title, mainText, footer);
  return true;
}

bool setStatusWithPriority(StatusLevel level, String title, String mainText, String footer) {
  return setStatusWithPriority(level, "", title, mainText, footer);
}

// -----------------------------------------------------------------------------
// Status rendering
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Status parsing and formatting
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Priority and source rules
// -----------------------------------------------------------------------------

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

// Source-aware clear rules:
//
// - Empty incoming source means intentional global clear.
// - Non-empty incoming source only clears if it matches currentStatus.source.
// - This prevents one automation from accidentally clearing another automation's
//   active alert.
bool shouldAcceptClear(String incomingSource) {
  incomingSource.trim();

  // Empty source means intentional global clear.
  if (incomingSource.length() == 0) {
    return true;
  }

  // If current state has no source, allow clear.
  if (currentStatus.source.length() == 0) {
    return true;
  }

  return incomingSource == currentStatus.source;
}

// Source-aware clear logic is integrated into setStatusWithPriority() since OK is a status level
// with priority rules, not a separate clear command. Clear attempts with a source that does not
// match the current status's source will be rejected to prevent unintended clears of active alerts.
// Status updates with levels other than OK will continue to be evaluated against priority rules
// without regard to source, since they are intended to be able to update each other based on
// priority regardless of source.
bool clearStatusWithSource(String source) {
  return setStatusWithPriority(STATUS_OK, source, "HOME", "All Good", "No alerts");
}

// Priority rules protect important alerts from lower-priority updates.
//
// alert / acked > warning > info > ok
//
// OK is handled separately by source-aware clear logic.
bool shouldAcceptStatusUpdate(StatusLevel incomingLevel, StatusLevel currentLevel) {
  // OK is handled separately by source-aware clear logic.
  if (incomingLevel == STATUS_OK) {
    return true;
  }

  // Alert acknowledged should not be overwritten by lower-priority messages.
  if (currentLevel == STATUS_ACKED && incomingLevel != STATUS_ALERT) {
    return false;
  }

  return statusPriority(incomingLevel) >= statusPriority(currentLevel);
}
