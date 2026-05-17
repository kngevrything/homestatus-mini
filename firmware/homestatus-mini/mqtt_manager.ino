void setupMqtt() {
  if (!hasMqttConfig()) {
    Serial.println("MQTT not configured. Skipping MQTT setup.");
    return;
  }

  mqttClient.setServer(deviceConfig.mqttHost.c_str(), deviceConfig.mqttPort);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(1024);

  Serial.println("MQTT configured.");
  Serial.print("  Host: ");
  Serial.println(deviceConfig.mqttHost);
  Serial.print("  Port: ");
  Serial.println(deviceConfig.mqttPort);
  Serial.print("  Base topic: ");
  Serial.println(deviceConfig.mqttBaseTopic);
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

  Serial.print("Connecting to MQTT broker: ");
  Serial.println(deviceConfig.mqttHost);

  bool connected;

  if (deviceConfig.mqttUsername.length() > 0) {
    connected = mqttClient.connect(
      clientId.c_str(),
      deviceConfig.mqttUsername.c_str(),
      deviceConfig.mqttPassword.c_str(),
      availabilityTopic.c_str(),
      0,
      true,
      "offline"
    );
  } else {
    connected = mqttClient.connect(
      clientId.c_str(),
      availabilityTopic.c_str(),
      0,
      true,
      "offline"
    );
  }

  if (!connected) {
    int state = mqttClient.state();

    Serial.print("MQTT connection failed. State: ");
    Serial.print(state);
    Serial.print(" ");
    Serial.println(mqttStateToString(state));
    
    return;
  }

  Serial.println("MQTT connected.");

  publishMqttAvailability("online");
  publishHomeAssistantDiscovery();
  
  if (mqttClient.subscribe(setTopic.c_str())) {
    Serial.print("MQTT subscribed: ");
    Serial.println(setTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(setTopic);
  }

  if (mqttClient.subscribe(actionTopic.c_str())) {
    Serial.print("MQTT subscribed: ");
    Serial.println(actionTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(actionTopic);
  }

  if (mqttClient.subscribe(levelSetTopic.c_str())) {
    Serial.print("MQTT subscribed: ");
    Serial.println(levelSetTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(levelSetTopic);
  }

  publishMqttStatus();
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicText = String(topic);

  Serial.print("MQTT message received on ");
  Serial.println(topicText);

  if (topicText == mqttTopic("level/set")) {
    String levelText = "";

    for (unsigned int i = 0; i < length; i++) {
      levelText += (char)payload[i];
    }

    levelText = limitText(levelText, 16);
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
    action = limitText(action, 16);

    handleMqttAction(action);
    return;
  }

  if (topicText != mqttTopic("set")) {
    Serial.println("MQTT topic ignored.");
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
  String title = doc["title"] | "HOME";
  String mainText = doc["main"] | "Updated";
  String footer = doc["footer"] | "MQTT";

  levelText = limitText(levelText, 16);
  title = limitText(title, 12);
  mainText = limitText(mainText, 18);
  footer = limitText(footer, 18);

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    Serial.print("Invalid MQTT level: ");
    Serial.println(levelText);
    return;
  }

  setStatusWithPriority(level, title, mainText, footer);
}

void publishMqttStatus() {
  if (!isMqttReady()) {
    return;
  }

  StaticJsonDocument<256> doc;

  doc["device"] = getDeviceName();
  doc["level"] = statusLevelToString(currentStatus.level);
  doc["title"] = currentStatus.title;
  doc["mainText"] = currentStatus.mainText;
  doc["footer"] = currentStatus.footer;
  doc["uptimeSeconds"] = millis() / 1000;

  char buffer[256];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  String topic = mqttTopic("status");

  bool published = mqttClient.publish(
    topic.c_str(),
    reinterpret_cast<const uint8_t*>(buffer),
    length,
    true
  );

  if (!published) {
    Serial.println("MQTT status publish failed.");
  }
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

void publishHomeAssistantDiscovery() {
  if (!isMqttReady()) {
    return;
  }

  String baseObjectId = haSafeObjectId(getDeviceName());

  publishHaSensorDiscovery(
    baseObjectId + "_level",
    "Level",
    "{{ value_json.level }}",
    "mdi:alert-circle-outline",
    "",
    ""
  );

  publishHaSensorDiscovery(
    baseObjectId + "_title",
    "Title",
    "{{ value_json.title }}",
    "mdi:format-title",
    "",
    ""
  );

  publishHaSensorDiscovery(
    baseObjectId + "_main",
    "Main",
    "{{ value_json.mainText }}",
    "mdi:text-short",
    "",
    ""
  );

  publishHaSensorDiscovery(
    baseObjectId + "_footer",
    "Footer",
    "{{ value_json.footer }}",
    "mdi:text",
    "",
    ""
  );

  publishHaButtonDiscovery(
    baseObjectId + "_acknowledge",
    "Acknowledge / Clear",
    mqttTopic("action"),
    "{\"action\":\"ack\"}",
    "mdi:check-circle-outline"
  );

  publishHaSelectDiscovery(
    baseObjectId + "_status_level",
    "Status Level",
    mqttTopic("level/set"),
    mqttTopic("status"),
    "{{ value_json.level }}",
    "mdi:format-list-bulleted"
  );

  Serial.println("Home Assistant discovery publish attempt complete.");
}

void publishHaButtonDiscovery(
  String objectId,
  String name,
  String commandTopic,
  String payloadPress,
  String icon
) {
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
  device["identifiers"][0] = haSafeObjectId(getDeviceName());
  device["name"] = getDeviceName();
  device["manufacturer"] = "HomeStatus";
  device["model"] = "HomeStatus Mini";

  char buffer[768];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(
    discoveryTopic.c_str(),
    reinterpret_cast<const uint8_t*>(buffer),
    length,
    true
  );

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
    Serial.print("HA button discovery published: ");
    Serial.println(discoveryTopic);
    Serial.print("HA button discovery payload size: ");
    Serial.println(length);
  }
}

void publishHaSensorDiscovery(
  String objectId,
  String name,
  String valueTemplate,
  String icon,
  String unitOfMeasurement,
  String deviceClass
) {
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

  JsonObject device = doc.createNestedObject("device");
  device["identifiers"][0] = haSafeObjectId(getDeviceName());
  device["name"] = getDeviceName();
  device["manufacturer"] = "HomeStatus";
  device["model"] = "HomeStatus Mini";

  char buffer[768];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(
    discoveryTopic.c_str(),
    reinterpret_cast<const uint8_t*>(buffer),
    length,
    true
  );

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
    Serial.print("HA discovery published: ");
    Serial.println(discoveryTopic);
    Serial.print("HA discovery payload size: ");
    Serial.println(length);
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

  Serial.print("MQTT level set received: ");
  Serial.println(levelText);

  if (levelText == "ok") {
    setDefaultOk();
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

void publishHaSelectDiscovery(
  String objectId,
  String name,
  String commandTopic,
  String stateTopic,
  String valueTemplate,
  String icon
) {
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
  device["identifiers"][0] = haSafeObjectId(getDeviceName());
  device["name"] = getDeviceName();
  device["manufacturer"] = "HomeStatus";
  device["model"] = "HomeStatus Mini";

  char buffer[1024];
  size_t length = serializeJson(doc, buffer, sizeof(buffer));

  bool published = mqttClient.publish(
    discoveryTopic.c_str(),
    reinterpret_cast<const uint8_t*>(buffer),
    length,
    true
  );

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
    Serial.print("HA select discovery published: ");
    Serial.println(discoveryTopic);
    Serial.print("HA select discovery payload size: ");
    Serial.println(length);
  }
}