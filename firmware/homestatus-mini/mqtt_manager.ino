// MQTT runtime and Home Assistant integration.
//
// Connects to the configured MQTT broker, receives status/action commands,
// publishes retained status and availability, and publishes Home Assistant
// MQTT discovery messages for sensors and controls.
//
// MQTT messages are published under the base topic {mqttBaseTopic}, which defaults to the device
// name. The following subtopics are used:
// - {baseTopic}/status: Retained JSON status document with device, level, source, title, mainText,
//                       footer, and uptimeSeconds fields.
// - {baseTopic}/availability: Retained string topic with "online" or "offline" value representing
//                             the device's availability.
// - {baseTopic}/set: JSON topic to set the status with optional source, title, mainText, and footer
//                    fields. Example payload:
//                    {"level":"alert","source":"garage","title":"GARAGE","main":"Open too
//                    long","footer":"Alert active"}
// - {baseTopic}/action: JSON topic to trigger actions with an "action" field. Example payload:
//                       {"action":"clear"} or {"action":"acknowledge"}
// - {baseTopic}/level/set: String topic to set the status level with priority handling. Example
//                          payload: "warning"
//
// MQTT configuration is set through the setup page and saved in ESP32 Preferences. It is not
// encrypted, so do not expose this device to untrusted physical access. MQTT is optional and will
// be skipped if not configured.

constexpr size_t MQTT_STATUS_JSON_CAPACITY = 384;
constexpr size_t MQTT_STATUS_BUFFER_SIZE = 384;

void setupMqtt() {
  if (!hasMqttConfig()) {
    Serial.println("MQTT not configured. Skipping MQTT setup.");
    return;
  }

  mqttClient.setServer(deviceConfig.mqttHost.c_str(), deviceConfig.mqttPort);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(1024);

  DEBUG_PRINTLN("MQTT configured.");
  DEBUG_PRINT("  Host: ");
  DEBUG_PRINTLN(deviceConfig.mqttHost);
  DEBUG_PRINT("  Port: ");
  DEBUG_PRINTLN(deviceConfig.mqttPort);
  DEBUG_PRINT("  Base topic: ");
  DEBUG_PRINTLN(deviceConfig.mqttBaseTopic);
}

void handleMqtt() {
  if (!hasMqttConfig()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  reconnectMqttIfNeeded();

  if (mqttClient.connected()) {
    mqttClient.loop();
  }
}

// MQTT reconnect is non-blocking and throttled so the device remains responsive
// when the broker is unavailable.
void reconnectMqttIfNeeded() {
  if (mqttClient.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMqttReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastMqttReconnectAttemptMs = now;

  String clientId = getDeviceName();
  clientId += "-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  String availabilityTopic = mqttTopic("availability");
  String setTopic = mqttTopic("set");
  String actionTopic = mqttTopic("action");
  String levelSetTopic = mqttTopic("level/set");

  DEBUG_PRINT("Connecting to MQTT broker: ");
  DEBUG_PRINTLN(deviceConfig.mqttHost);

  bool connected;

  if (deviceConfig.mqttUsername.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), deviceConfig.mqttUsername.c_str(),
                                   deviceConfig.mqttPassword.c_str(), availabilityTopic.c_str(), 0,
                                   true, "offline");
  } else {
    connected = mqttClient.connect(clientId.c_str(), availabilityTopic.c_str(), 0, true, "offline");
  }

  if (!connected) {
    int state = mqttClient.state();

    Serial.print("MQTT connection failed. State: ");
    Serial.print(state);
    Serial.print(" ");
    Serial.println(mqttStateToString(state));

    return;
  }

  DEBUG_PRINTLN("MQTT connected.");

  publishMqttAvailability("online");
  publishHomeAssistantDiscovery();

  subscribeMqttTopic(setTopic);
  subscribeMqttTopic(actionTopic);
  subscribeMqttTopic(levelSetTopic);

  publishMqttStatus();
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicText = String(topic);

  DEBUG_PRINT("MQTT message received on ");
  DEBUG_PRINTLN(topicText);

  if (topicText == mqttTopic("level/set")) {
    String levelText = "";

    for (unsigned int i = 0; i < length; i++) {
      levelText += (char)payload[i];
    }

    levelText = limitText(levelText, MAX_LEVEL_CHARS);
    handleMqttLevelSet(levelText);
    return;
  }

  if (topicText == mqttTopic("action")) {
    StaticJsonDocument<128> actionDoc;

    DeserializationError actionError = deserializeJson(actionDoc, payload, length);

    if (actionError) {
      Serial.print("MQTT action JSON parse failed: ");
      Serial.println(actionError.c_str());
      return;
    }

    String action = actionDoc["action"] | "";
    action = limitText(action, MAX_LEVEL_CHARS);

    handleMqttAction(action);
    return;
  }

  if (topicText != mqttTopic("set")) {
    DEBUG_PRINTLN("MQTT topic ignored.");
    return;
  }

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("MQTT JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  String levelText = doc["level"] | "";
  String source = doc["source"] | "";
  String title = doc["title"] | "HOME";
  String mainText = doc["main"] | "Updated";
  String footer = doc["footer"] | "MQTT";

  levelText = limitText(levelText, MAX_LEVEL_CHARS);

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    Serial.print("Invalid MQTT level: ");
    Serial.println(levelText);
    return;
  }

  if (level == STATUS_OK && !doc["title"].is<const char*>() && !doc["main"].is<const char*>() &&
      !doc["footer"].is<const char*>()) {
    clearStatusWithSource(source);
    return;
  }

  setStatusWithPriority(level, source, title, mainText, footer);
}

void publishMqttStatus() {
  if (!isMqttReady()) {
    return;
  }

  StaticJsonDocument<MQTT_STATUS_JSON_CAPACITY> doc;

  writeStatusJson(doc);
  doc["ip"] = WiFi.localIP().toString();

  char buffer[MQTT_STATUS_BUFFER_SIZE];

  size_t needed = measureJson(doc);
  if (needed >= sizeof(buffer)) {
    Serial.print("MQTT status JSON too large. Needed: ");
    Serial.println(needed + 1);
    return;
  }

  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  String topic = mqttTopic("status");

  bool published =
      mqttClient.publish(topic.c_str(), reinterpret_cast<const uint8_t*>(buffer), length, true);

  if (!published) {
    Serial.println("MQTT status publish failed.");
  }
}

bool subscribeMqttTopic(String topic) {
  bool subscribed = mqttClient.subscribe(topic.c_str());

  if (subscribed) {
    DEBUG_PRINT("MQTT subscribed: ");
    DEBUG_PRINTLN(topic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(topic);
  }

  return subscribed;
}

void publishMqttAvailability(const char* availability) {
  if (!mqttClient.connected()) {
    return;
  }

  String topic = mqttTopic("availability");
  mqttClient.publish(topic.c_str(), availability, true);
}

String mqttTopic(String suffix) {
  String baseTopic = deviceConfig.mqttBaseTopic;

  baseTopic.trim();

  while (baseTopic.endsWith("/")) {
    baseTopic.remove(baseTopic.length() - 1);
  }

  if (baseTopic.length() == 0) {
    baseTopic = getDeviceName();
  }

  return baseTopic + "/" + suffix;
}

bool isMqttReady() {
  return hasMqttConfig() && mqttClient.connected();
}

String mqttStateToString(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT:
      return "MQTT_CONNECTION_TIMEOUT";
    case MQTT_CONNECTION_LOST:
      return "MQTT_CONNECTION_LOST";
    case MQTT_CONNECT_FAILED:
      return "MQTT_CONNECT_FAILED";
    case MQTT_DISCONNECTED:
      return "MQTT_DISCONNECTED";
    case MQTT_CONNECTED:
      return "MQTT_CONNECTED";
    case MQTT_CONNECT_BAD_PROTOCOL:
      return "MQTT_CONNECT_BAD_PROTOCOL";
    case MQTT_CONNECT_BAD_CLIENT_ID:
      return "MQTT_CONNECT_BAD_CLIENT_ID";
    case MQTT_CONNECT_UNAVAILABLE:
      return "MQTT_CONNECT_UNAVAILABLE";
    case MQTT_CONNECT_BAD_CREDENTIALS:
      return "MQTT_CONNECT_BAD_CREDENTIALS";
    case MQTT_CONNECT_UNAUTHORIZED:
      return "MQTT_CONNECT_UNAUTHORIZED";
    default:
      return "MQTT_UNKNOWN";
  }
}

// Discovery messages are retained so Home Assistant can recreate entities
// after restart without waiting for a firmware reboot.
void publishHomeAssistantDiscovery() {
  if (!isMqttReady()) {
    return;
  }

  String baseObjectId = haSafeObjectId(getDeviceName());

  publishHaSensorDiscovery(baseObjectId + "_ip", "IP Address", "{{ value_json.ip }}",
                           "mdi:ip-network", "", "", "diagnostic");

  publishHaSensorDiscovery(baseObjectId + "_level", "Level", "{{ value_json.level }}",
                           "mdi:alert-circle-outline", "", "", "");

  publishHaSensorDiscovery(baseObjectId + "_title", "Title", "{{ value_json.title }}",
                           "mdi:format-title", "", "", "");

  publishHaSensorDiscovery(baseObjectId + "_source", "Source", "{{ value_json.source }}",
                           "mdi:source-branch", "", "", "");

  publishHaSensorDiscovery(baseObjectId + "_main", "Main", "{{ value_json.mainText }}",
                           "mdi:text-short", "", "", "");

  publishHaSensorDiscovery(baseObjectId + "_footer", "Footer", "{{ value_json.footer }}",
                           "mdi:text", "", "", "");

  publishHaButtonDiscovery(baseObjectId + "_acknowledge", "Acknowledge / Clear",
                           mqttTopic("action"), "{\"action\":\"ack\"}", "mdi:check-circle-outline");

  publishHaSelectDiscovery(baseObjectId + "_status_level", "Status Level", mqttTopic("level/set"),
                           mqttTopic("status"), "{{ value_json.level }}",
                           "mdi:format-list-bulleted");

  DEBUG_PRINTLN("Home Assistant discovery publish attempt complete.");
}

void publishHaButtonDiscovery(String objectId, String name, String commandTopic,
                              String payloadPress, String icon) {
  if (!isMqttReady()) {
    return;
  }

  String discoveryTopic = "homeassistant/button/";
  discoveryTopic += objectId;
  discoveryTopic += "/config";

  String availabilityTopic = mqttTopic("availability");

  StaticJsonDocument<768> doc;

  doc["name"] = name;
  doc["unique_id"] = objectId;
  doc["command_topic"] = commandTopic;
  doc["payload_press"] = payloadPress;
  doc["availability_topic"] = availabilityTopic;
  doc["icon"] = icon;

  JsonObject device = doc.createNestedObject("device");
  addHaDeviceInfo(device);

  char buffer[768];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(discoveryTopic.c_str(),
                                      reinterpret_cast<const uint8_t*>(buffer), length, true);

  if (!published) {
    Serial.print("HA button discovery publish failed: ");
    Serial.println(discoveryTopic);
    Serial.print("Payload length: ");
    Serial.println(length);
    Serial.print("MQTT state: ");
    Serial.print(mqttClient.state());
    Serial.print(" ");
    Serial.println(mqttStateToString(mqttClient.state()));
  } else {
    DEBUG_PRINT("HA button discovery published: ");
    DEBUG_PRINTLN(discoveryTopic);
    DEBUG_PRINT("HA button discovery payload size: ");
    DEBUG_PRINTLN(length);
  }
}
void publishHaSensorDiscovery(String objectId, String name, String valueTemplate, String icon,
                              String unitOfMeasurement, String deviceClass, String entityCategory) {
  if (!isMqttReady()) {
    return;
  }

  String discoveryTopic = "homeassistant/sensor/";
  discoveryTopic += objectId;
  discoveryTopic += "/config";

  String statusTopic = mqttTopic("status");
  String availabilityTopic = mqttTopic("availability");

  StaticJsonDocument<768> doc;

  doc["name"] = name;
  doc["unique_id"] = objectId;
  doc["state_topic"] = statusTopic;
  doc["availability_topic"] = availabilityTopic;
  doc["value_template"] = valueTemplate;
  doc["icon"] = icon;

  if (unitOfMeasurement.length() > 0) {
    doc["unit_of_measurement"] = unitOfMeasurement;
  }

  if (deviceClass.length() > 0) {
    doc["device_class"] = deviceClass;
  }

  if (entityCategory.length() > 0) {
    doc["entity_category"] = entityCategory;
  }

  JsonObject device = doc.createNestedObject("device");
  addHaDeviceInfo(device);

  char buffer[768];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(discoveryTopic.c_str(),
                                      reinterpret_cast<const uint8_t*>(buffer), length, true);

  if (!published) {
    Serial.print("HA discovery publish failed: ");
    Serial.println(discoveryTopic);
    Serial.print("Payload length: ");
    Serial.println(length);
    Serial.print("MQTT state: ");
    Serial.print(mqttClient.state());
    Serial.print(" ");
    Serial.println(mqttStateToString(mqttClient.state()));
  } else {
    DEBUG_PRINT("HA discovery published: ");
    DEBUG_PRINTLN(discoveryTopic);
    DEBUG_PRINT("HA discovery payload size: ");
    DEBUG_PRINTLN(length);
  }
}

String haSafeObjectId(String value) {
  value.toLowerCase();
  value.trim();

  String result = "";

  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);

    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      result += c;
    } else {
      result += "_";
    }
  }

  while (result.indexOf("__") >= 0) {
    result.replace("__", "_");
  }

  while (result.startsWith("_")) {
    result.remove(0, 1);
  }

  while (result.endsWith("_")) {
    result.remove(result.length() - 1);
  }

  if (result.length() == 0) {
    result = "homestatus_mini";
  }

  return result;
}

void handleMqttAction(String action) {
  action.toLowerCase();
  action.trim();

  if (action == "ack" || action == "acknowledge" || action == "clear") {
    Serial.print("MQTT action received: ");
    Serial.println(action);

    onButtonPressed();
    return;
  }

  Serial.print("Unknown MQTT action: ");
  Serial.println(action);
}

void handleMqttLevelSet(String levelText) {
  levelText.toLowerCase();
  levelText.trim();

  DEBUG_PRINT("MQTT level set received: ");
  DEBUG_PRINTLN(levelText);

  if (levelText == "ok") {
    clearStatusWithSource("");
    return;
  }

  if (levelText == "warning") {
    setDefaultWarningWithPriority();
    return;
  }

  if (levelText == "alert") {
    setDefaultAlertWithPriority();
    return;
  }

  if (levelText == "info") {
    setDefaultInfoWithPriority();
    return;
  }

  Serial.print("Unknown MQTT level set value: ");
  Serial.println(levelText);
}

void addHaDeviceInfo(JsonObject device) {
  device["identifiers"][0] = haSafeObjectId(getDeviceName());
  device["name"] = getDeviceName();
  device["manufacturer"] = "HomeStatus";
  device["model"] = "HomeStatus Mini";
}

void publishHaSelectDiscovery(String objectId, String name, String commandTopic, String stateTopic,
                              String valueTemplate, String icon) {
  if (!isMqttReady()) {
    return;
  }

  String discoveryTopic = "homeassistant/select/";
  discoveryTopic += objectId;
  discoveryTopic += "/config";

  String availabilityTopic = mqttTopic("availability");

  StaticJsonDocument<1024> doc;

  doc["name"] = name;
  doc["unique_id"] = objectId;
  doc["command_topic"] = commandTopic;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = valueTemplate;
  doc["availability_topic"] = availabilityTopic;
  doc["icon"] = icon;

  JsonArray options = doc.createNestedArray("options");
  options.add("ok");
  options.add("warning");
  options.add("alert");
  options.add("info");

  JsonObject device = doc.createNestedObject("device");
  addHaDeviceInfo(device);

  char buffer[1024];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(discoveryTopic.c_str(),
                                      reinterpret_cast<const uint8_t*>(buffer), length, true);

  if (!published) {
    Serial.print("HA select discovery publish failed: ");
    Serial.println(discoveryTopic);
    Serial.print("Payload length: ");
    Serial.println(length);
    Serial.print("MQTT state: ");
    Serial.print(mqttClient.state());
    Serial.print(" ");
    Serial.println(mqttStateToString(mqttClient.state()));
  } else {
    DEBUG_PRINT("HA select discovery published: ");
    DEBUG_PRINTLN(discoveryTopic);
    DEBUG_PRINT("HA select discovery payload size: ");
    DEBUG_PRINTLN(length);
  }
}
