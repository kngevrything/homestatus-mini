// Status state management.
//
// HomeStatus Mini tracks active statuses by source. Each source owns at most
// one active status, so a new message from the same source replaces that
// source's previous message.
//
// The selected status is the one currently shown on the OLED. The RGB LED
// uses the same level as the selected status so the light and screen describe
// the same item.
//
// Priority is used to choose which status should be selected automatically
// when new messages arrive or when the selected status is cleared. It does not
// prevent lower-priority statuses from being stored or browsed.
//
// All runtime status changes should flow through applyIncomingStatus() so HTTP,
// MQTT, Serial, Home Assistant, and button actions share the same behavior.

// -----------------------------------------------------------------------------
// Hardware setup
// -----------------------------------------------------------------------------

void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
}

// -----------------------------------------------------------------------------
// Active status state
// -----------------------------------------------------------------------------

SourceStatus sourceStatuses[MAX_STATUS_SOURCES];

// selectedStatusIndex is the active source slot currently shown on the OLED.
// -1 means no active status is selected, so the display should show default OK.
int selectedStatusIndex = -1;

// -----------------------------------------------------------------------------
// Text normalization
// -----------------------------------------------------------------------------

void normalizeStatusText(String& source, String& title, String& mainText, String& footer) {
  source = limitText(source, MAX_SOURCE_CHARS);
  title = limitText(title, MAX_TITLE_CHARS);
  mainText = limitText(mainText, MAX_MAIN_CHARS);
  footer = limitText(footer, MAX_FOOTER_CHARS);

  source.trim();
  title.trim();
  mainText.trim();
  footer.trim();

  source.toLowerCase();
}

// -----------------------------------------------------------------------------
// Status parsing and formatting
// -----------------------------------------------------------------------------

bool tryParseStatusLevel(String levelText, StatusLevel& level) {
  levelText.trim();
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

// Priority is used for automatic selection and fallback selection.
//
// It does not decide whether an incoming status is stored. Valid non-OK
// statuses are stored by source even when a higher-priority status is active.
//
// Current order:
//        alert > warning > acked > info > ok

int statusPriority(StatusLevel level) {
  switch (level) {
    case STATUS_ALERT:
      return 4;

    case STATUS_WARNING:
      return 3;

    case STATUS_ACKED:
      return 2;

    case STATUS_INFO:
      return 1;

    case STATUS_OK:
    default:
      return 0;
  }
}

// -----------------------------------------------------------------------------
// Active-list accessors
// -----------------------------------------------------------------------------

int getActiveStatusCount() {
  int activeCount = 0;

  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    if (sourceStatuses[statusIndex].active) {
      activeCount++;
    }
  }

  return activeCount;
}

bool hasActiveStatuses() {
  return getActiveStatusCount() > 0;
}

const SourceStatus* getSelectedStatus() {
  if (selectedStatusIndex < 0 || selectedStatusIndex >= MAX_STATUS_SOURCES) {
    return nullptr;
  }

  if (!sourceStatuses[selectedStatusIndex].active) {
    return nullptr;
  }

  return &sourceStatuses[selectedStatusIndex];
}

StatusLevel getSelectedStatusLevel() {
  const SourceStatus* selectedStatus = getSelectedStatus();

  if (selectedStatus == nullptr) {
    return STATUS_OK;
  }

  return selectedStatus->level;
}

int getSelectedStatusPosition() {
  if (selectedStatusIndex < 0 || selectedStatusIndex >= MAX_STATUS_SOURCES) {
    return 0;
  }

  int selectedPosition = 0;

  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    if (!sourceStatuses[statusIndex].active) {
      continue;
    }

    selectedPosition++;

    if (statusIndex == selectedStatusIndex) {
      return selectedPosition;
    }
  }

  return 0;
}

int findHighestPriorityStatusIndex() {
  int bestStatusIndex = -1;
  int bestPriority = -1;
  unsigned long bestUpdatedAt = 0;

  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    if (!sourceStatuses[statusIndex].active) {
      continue;
    }

    int candidatePriority = statusPriority(sourceStatuses[statusIndex].level);

    if (bestStatusIndex < 0 || candidatePriority > bestPriority ||
        (candidatePriority == bestPriority &&
         sourceStatuses[statusIndex].updatedAt >= bestUpdatedAt)) {
      bestStatusIndex = statusIndex;
      bestPriority = candidatePriority;
      bestUpdatedAt = sourceStatuses[statusIndex].updatedAt;
    }
  }

  return bestStatusIndex;
}

StatusLevel getWorstActiveLevel() {
  int worstStatusIndex = findHighestPriorityStatusIndex();

  if (worstStatusIndex < 0) {
    return STATUS_OK;
  }

  return sourceStatuses[worstStatusIndex].level;
}

void normalizeSelectedStatus() {
  if (selectedStatusIndex >= 0 && selectedStatusIndex < MAX_STATUS_SOURCES &&
      sourceStatuses[selectedStatusIndex].active) {
    return;
  }

  selectedStatusIndex = findHighestPriorityStatusIndex();
}

bool selectStatusIndex(int statusIndex) {
  if (statusIndex < 0 || statusIndex >= MAX_STATUS_SOURCES) {
    return false;
  }

  if (!sourceStatuses[statusIndex].active) {
    return false;
  }

  selectedStatusIndex = statusIndex;
  return true;
}

bool selectHighestPriorityStatus() {
  int highestPriorityStatusIndex = findHighestPriorityStatusIndex();

  if (highestPriorityStatusIndex < 0) {
    selectedStatusIndex = -1;
    return false;
  }

  selectedStatusIndex = highestPriorityStatusIndex;
  return true;
}

// Used by the physical button short press.
//
// This is intentionally non-destructive. It only changes which active status is
// shown on the OLED.
bool selectNextActiveStatus() {
  int activeCount = getActiveStatusCount();

  if (activeCount == 0) {
    selectedStatusIndex = -1;
    showStatus();
    publishMqttStatus();
    return false;
  }

  int startStatusIndex = selectedStatusIndex;

  if (startStatusIndex < 0 || startStatusIndex >= MAX_STATUS_SOURCES) {
    startStatusIndex = -1;
  }

  for (int offset = 1; offset <= MAX_STATUS_SOURCES; offset++) {
    int candidateStatusIndex =
        (startStatusIndex + offset + MAX_STATUS_SOURCES) % MAX_STATUS_SOURCES;

    if (sourceStatuses[candidateStatusIndex].active) {
      selectedStatusIndex = candidateStatusIndex;
      showStatus();
      publishMqttStatus();
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// Source registry helpers
// -----------------------------------------------------------------------------

int findSourceStatusIndex(const String& source) {
  if (source.length() == 0) {
    return -1;
  }

  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    if (sourceStatuses[statusIndex].active && sourceStatuses[statusIndex].source == source) {
      return statusIndex;
    }
  }

  return -1;
}

int findEmptySourceStatusIndex() {
  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    if (!sourceStatuses[statusIndex].active) {
      return statusIndex;
    }
  }

  return -1;
}

void clearSourceStatus(const String& source) {
  int sourceStatusIndex = findSourceStatusIndex(source);

  if (sourceStatusIndex < 0) {
    return;
  }

  sourceStatuses[sourceStatusIndex].active = false;
  sourceStatuses[sourceStatusIndex].level = STATUS_OK;
  sourceStatuses[sourceStatusIndex].source = "";
  sourceStatuses[sourceStatusIndex].title = "";
  sourceStatuses[sourceStatusIndex].mainText = "";
  sourceStatuses[sourceStatusIndex].footer = "";
  sourceStatuses[sourceStatusIndex].updatedAt = 0;

  if (selectedStatusIndex == sourceStatusIndex) {
    selectedStatusIndex = -1;
  }

  normalizeSelectedStatus();
}

void clearAllSourceStatuses() {
  for (int statusIndex = 0; statusIndex < MAX_STATUS_SOURCES; statusIndex++) {
    sourceStatuses[statusIndex].active = false;
    sourceStatuses[statusIndex].level = STATUS_OK;
    sourceStatuses[statusIndex].source = "";
    sourceStatuses[statusIndex].title = "";
    sourceStatuses[statusIndex].mainText = "";
    sourceStatuses[statusIndex].footer = "";
    sourceStatuses[statusIndex].updatedAt = 0;
  }

  selectedStatusIndex = -1;
}

bool upsertSourceStatus(StatusLevel level, const String& source, const String& title,
                        const String& mainText, const String& footer) {
  if (source.length() == 0) {
    return false;
  }

  int sourceStatusIndex = findSourceStatusIndex(source);

  if (sourceStatusIndex < 0) {
    sourceStatusIndex = findEmptySourceStatusIndex();
  }

  if (sourceStatusIndex < 0) {
    return false;
  }

  sourceStatuses[sourceStatusIndex].active = true;
  sourceStatuses[sourceStatusIndex].level = level;
  sourceStatuses[sourceStatusIndex].source = source;
  sourceStatuses[sourceStatusIndex].title = title;
  sourceStatuses[sourceStatusIndex].mainText = mainText;
  sourceStatuses[sourceStatusIndex].footer = footer;
  sourceStatuses[sourceStatusIndex].updatedAt = millis();

  return true;
}

// -----------------------------------------------------------------------------
// Display helpers
// -----------------------------------------------------------------------------

String getActionHintForStatus(StatusLevel level) {
  switch (level) {
    case STATUS_ALERT:
    case STATUS_WARNING:
      return "Dbl: ack";

    case STATUS_ACKED:
    case STATUS_INFO:
      return "Dbl: clear";

    case STATUS_OK:
    default:
      return "";
  }
}

String buildDisplayFooter(const SourceStatus& status) {
  String actionHint = getActionHintForStatus(status.level);

  if (actionHint.length() == 0) {
    actionHint = status.footer;
  }

  int activeCount = getActiveStatusCount();

  if (activeCount <= 1) {
    return actionHint;
  }

  int selectedPosition = getSelectedStatusPosition();

  return String(selectedPosition) + "/" + String(activeCount) + " " + actionHint;
}

// Render the selected status. If no status is active, show the default OK
// screen. The LED follows the selected status level so the physical light
// matches the OLED.
void showStatus() {
  const SourceStatus* selectedStatus = getSelectedStatus();

  if (selectedStatus == nullptr) {
    updateLed(STATUS_OK);
    drawScreen("HOME", "All Good", "No alerts");
    return;
  }

  updateLed(selectedStatus->level);

  drawScreen(selectedStatus->title, selectedStatus->mainText, buildDisplayFooter(*selectedStatus));
}

void writeStatusJson(JsonDocument& doc) {
  const SourceStatus* selectedStatus = getSelectedStatus();

  doc["device"] = getDeviceName();
  doc["uptimeSeconds"] = millis() / 1000;
  doc["activeCount"] = getActiveStatusCount();
  doc["selectedPosition"] = getSelectedStatusPosition();
  doc["worstLevel"] = statusLevelToString(getWorstActiveLevel());

  if (selectedStatus == nullptr) {
    doc["level"] = statusLevelToString(STATUS_OK);
    doc["source"] = "";
    doc["title"] = "HOME";
    doc["mainText"] = "All Good";
    doc["footer"] = "No alerts";
    return;
  }

  doc["level"] = statusLevelToString(selectedStatus->level);
  doc["source"] = selectedStatus->source;
  doc["title"] = selectedStatus->title;
  doc["mainText"] = selectedStatus->mainText;
  doc["footer"] = selectedStatus->footer;
}

String buildStatusJson() {
  StaticJsonDocument<384> doc;

  writeStatusJson(doc);

  String json;
  serializeJson(doc, json);

  return json;
}

// -----------------------------------------------------------------------------
// Central status application
// -----------------------------------------------------------------------------

bool applyIncomingStatus(StatusLevel level, String source, String title, String mainText,
                         String footer, bool logUpdate) {
  normalizeStatusText(source, title, mainText, footer);

  if (level == STATUS_OK) {
    if (source.length() == 0) {
      clearAllSourceStatuses();
    } else {
      clearSourceStatus(source);
    }

    normalizeSelectedStatus();
    showStatus();
    publishMqttStatus();

    if (logUpdate) {
      Serial.print("Status cleared: source=");
      Serial.println(source.length() == 0 ? "all" : source);
    }

    return true;
  }

  if (source.length() == 0) {
    source = MANUAL_SOURCE;
  }

  StatusLevel previouslyVisibleLevel = getSelectedStatusLevel();
  int previousSourceStatusIndex = findSourceStatusIndex(source);

  if (!upsertSourceStatus(level, source, title, mainText, footer)) {
    Serial.print("Status update ignored because source registry is full. Incoming source: ");
    Serial.println(source);
    return false;
  }

  int updatedSourceStatusIndex = findSourceStatusIndex(source);

  if (selectedStatusIndex < 0) {
    selectedStatusIndex = updatedSourceStatusIndex;
  } else if (selectedStatusIndex == previousSourceStatusIndex) {
    selectedStatusIndex = updatedSourceStatusIndex;
  } else if (statusPriority(level) > statusPriority(previouslyVisibleLevel)) {
    selectedStatusIndex = updatedSourceStatusIndex;
  }

  normalizeSelectedStatus();
  showStatus();
  publishMqttStatus();

  if (logUpdate) {
    Serial.print("Status updated: ");
    Serial.print(statusLevelToString(level));
    Serial.print(" / source=");
    Serial.print(source);
    Serial.print(" / ");
    Serial.print(title);
    Serial.print(" / ");
    Serial.print(mainText);
    Serial.print(" / ");
    Serial.println(footer);
  }

  return true;
}

// -----------------------------------------------------------------------------
// Default statuses
// -----------------------------------------------------------------------------

void setDefaultOk() {
  applyIncomingStatus(STATUS_OK, "", "HOME", "All Good", "No alerts", true);
}

void setDefaultOkQuiet() {
  applyIncomingStatus(STATUS_OK, "", "HOME", "All Good", "No alerts", false);
}

void setDefaultWarning() {
  applyIncomingStatus(STATUS_WARNING, MANUAL_SOURCE, "GARAGE", "Open 29 min", "Warning", true);
}

void setDefaultAlert() {
  applyIncomingStatus(STATUS_ALERT, MANUAL_SOURCE, "GARAGE", "Open too long", "Alert active", true);
}

void setDefaultInfo() {
  applyIncomingStatus(STATUS_INFO, MANUAL_SOURCE, "WASHER", "Done", "Move laundry", true);
}

bool setDefaultWarningWithPriority() {
  return applyIncomingStatus(STATUS_WARNING, MANUAL_SOURCE, "GARAGE", "Open 29 min", "Warning",
                             true);
}

bool setDefaultAlertWithPriority() {
  return applyIncomingStatus(STATUS_ALERT, MANUAL_SOURCE, "GARAGE", "Open too long", "Alert active",
                             true);
}

bool setDefaultInfoWithPriority() {
  return applyIncomingStatus(STATUS_INFO, MANUAL_SOURCE, "WASHER", "Done", "Move laundry", true);
}

// -----------------------------------------------------------------------------
// Compatibility wrappers for existing call sites
// -----------------------------------------------------------------------------

void setStatus(StatusLevel level, String title, String mainText, String footer) {
  applyIncomingStatus(level, "", title, mainText, footer, true);
}

void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate) {
  applyIncomingStatus(level, "", title, mainText, footer, logUpdate);
}

void setStatus(StatusLevel level, String source, String title, String mainText, String footer) {
  applyIncomingStatus(level, source, title, mainText, footer, true);
}

void setStatus(StatusLevel level, String source, String title, String mainText, String footer,
               bool logUpdate) {
  applyIncomingStatus(level, source, title, mainText, footer, logUpdate);
}

bool setStatusWithPriority(StatusLevel level, String source, String title, String mainText,
                           String footer) {
  return applyIncomingStatus(level, source, title, mainText, footer, true);
}

bool setStatusWithPriority(StatusLevel level, String title, String mainText, String footer) {
  return applyIncomingStatus(level, "", title, mainText, footer, true);
}

bool clearStatusWithSource(String source) {
  return applyIncomingStatus(STATUS_OK, source, "HOME", "All Good", "No alerts", true);
}

// Older code may still call this name.
// New code should call actOnSelectedStatus().
bool acknowledgeOrClearCurrentStatus() {
  return actOnSelectedStatus();
}

// -----------------------------------------------------------------------------
// Selected-status action
// -----------------------------------------------------------------------------

bool actOnSelectedStatus() {
  const SourceStatus* selectedStatus = getSelectedStatus();

  if (selectedStatus == nullptr) {
    Serial.println("No active status to acknowledge or clear.");
    return false;
  }

  String source = selectedStatus->source;
  String title = selectedStatus->title;
  String mainText = selectedStatus->mainText;

  if (selectedStatus->level == STATUS_ALERT || selectedStatus->level == STATUS_WARNING) {
    upsertSourceStatus(STATUS_ACKED, source, title, mainText, "Acknowledged");
    selectedStatusIndex = findSourceStatusIndex(source);

    normalizeSelectedStatus();
    showStatus();
    publishMqttStatus();

    Serial.print("Status acknowledged: source=");
    Serial.println(source);

    return true;
  }

  if (selectedStatus->level == STATUS_INFO || selectedStatus->level == STATUS_ACKED) {
    clearSourceStatus(source);

    normalizeSelectedStatus();
    showStatus();
    publishMqttStatus();

    Serial.print("Status cleared by action: source=");
    Serial.println(source);

    return true;
  }

  Serial.println("Selected status has no action.");
  return false;
}

// -----------------------------------------------------------------------------
// LED helpers
// -----------------------------------------------------------------------------

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
