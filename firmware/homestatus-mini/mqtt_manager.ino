void setupMqtt() {
  if (!hasMqttConfig()) {
    Serial.println("MQTT not configured. Skipping MQTT setup.");
    return;
  }

  mqttClient.setServer(deviceConfig.mqttHost.c_str(), deviceConfig.mqttPort);
  mqttClient.setCallback(onMqttMessage);

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

  if (mqttClient.subscribe(setTopic.c_str())) {
    Serial.print("MQTT subscribed: ");
    Serial.println(setTopic);
  } else {
    Serial.print("MQTT subscribe failed: ");
    Serial.println(setTopic);
  }

  publishMqttStatus();
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicText = String(topic);

  Serial.print("MQTT message received on ");
  Serial.println(topicText);

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

  setStatus(level, title, mainText, footer);
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
  doc["uptimeMs"] = millis();

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