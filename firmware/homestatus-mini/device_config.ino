// Persistent device configuration.
//
// Loads, saves, and clears device settings from ESP32 Preferences, including
// Wi-Fi, device name, API key, and MQTT configuration. Secrets are intentionally
// not printed to Serial or exposed through /config.

// Preferences namespace. Changing this value will make previously saved device
// configuration unreadable unless migration logic is added.
const char* PREFERENCES_NAMESPACE = "homestatus";

void loadDeviceConfig() {
  preferences.begin(PREFERENCES_NAMESPACE, true);

  deviceConfig.wifiSSID = preferences.getString("wifiSSID", "");
  deviceConfig.wifiPassword = preferences.getString("wifiPassword", "");
  deviceConfig.deviceName = preferences.getString("deviceName", "homestatus-mini");
  deviceConfig.apiKey = preferences.getString("apiKey", "");

  deviceConfig.mqttEnabled = preferences.getBool("mqttEnabled", false);
  deviceConfig.mqttHost = preferences.getString("mqttHost", "");
  deviceConfig.mqttPort = preferences.getInt("mqttPort", 1883);
  deviceConfig.mqttUsername = preferences.getString("mqttUser", "");
  deviceConfig.mqttPassword = preferences.getString("mqttPass", "");
  deviceConfig.mqttBaseTopic = preferences.getString("mqttTopic", "homestatus-mini");

  preferences.end();

  Serial.println("Loaded device config:");
  Serial.print("  Wi-Fi configured: ");
  Serial.println(hasSavedWifiConfig() ? "Yes" : "No");
  Serial.print("  API key configured: ");
  Serial.println(hasSavedApiKey() ? "Yes" : "No");
  Serial.print("  Device Name: ");
  Serial.println(deviceConfig.deviceName);
  Serial.print("  WiFi SSID: ");
  Serial.println(deviceConfig.wifiSSID);
  Serial.print("  Has complete config: ");
  Serial.println(hasCompleteDeviceConfig() ? "Yes" : "No");
  Serial.print("  MQTT enabled: ");
  Serial.println(deviceConfig.mqttEnabled ? "Yes" : "No");
  Serial.print("  MQTT configured: ");
  Serial.println(hasMqttConfig() ? "Yes" : "No");
  Serial.print("  MQTT host: ");
  Serial.println(deviceConfig.mqttHost);
  Serial.print("  MQTT port: ");
  Serial.println(deviceConfig.mqttPort);
  Serial.print("  MQTT base topic: ");
  Serial.println(deviceConfig.mqttBaseTopic);
}

void saveDeviceConfig(String ssid, String password, String deviceName, String apiKey) {
  ssid.trim();
  password.trim();
  deviceName.trim();
  apiKey.trim();

  if (deviceName.length() == 0) {
    deviceName = "homestatus-mini";
  }

  preferences.begin(PREFERENCES_NAMESPACE, false);

  preferences.putString("wifiSSID", ssid);
  preferences.putString("wifiPassword", password);
  preferences.putString("deviceName", deviceName);
  preferences.putString("apiKey", apiKey);

  preferences.end();

  deviceConfig.wifiSSID = ssid;
  deviceConfig.wifiPassword = password;
  deviceConfig.deviceName = deviceName;
  deviceConfig.apiKey = apiKey;

  Serial.println("Device config saved.");
  Serial.print("  Wi-Fi configured: ");
  Serial.println(hasSavedWifiConfig() ? "Yes" : "No");
  Serial.print("  API key configured: ");
  Serial.println(hasSavedApiKey() ? "Yes" : "No");
  Serial.print("  Device Name: ");
  Serial.println(deviceConfig.deviceName);
  Serial.print("  WiFi SSID: ");
  Serial.println(deviceConfig.wifiSSID);
}

void clearDeviceConfig() {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.clear();
  preferences.end();

  deviceConfig.wifiSSID = "";
  deviceConfig.wifiPassword = "";
  deviceConfig.deviceName = "homestatus-mini";
  deviceConfig.apiKey = "";

  deviceConfig.mqttEnabled = false;
  deviceConfig.mqttHost = "";
  deviceConfig.mqttPort = 1883;
  deviceConfig.mqttUsername = "";
  deviceConfig.mqttPassword = "";
  deviceConfig.mqttBaseTopic = "homestatus-mini";

  Serial.println("Device config cleared.");
}

void factoryResetAndReboot() {
  clearDeviceConfig();

  Serial.println("Factory reset complete. Rebooting into setup mode...");
  delay(1000);

  ESP.restart();
}

bool hasSavedWifiConfig() {
  return deviceConfig.wifiSSID.length() > 0;
}

bool hasSavedApiKey() {
  return deviceConfig.apiKey.length() > 0;
}

bool hasCompleteDeviceConfig() {
  return hasSavedWifiConfig() && hasSavedApiKey();
}

bool hasMqttConfig() {
  return deviceConfig.hasMqttConfig();
}

String getDeviceName() {
  if (deviceConfig.deviceName.length() > 0) {
    return deviceConfig.deviceName;
  }

  return "homestatus-mini";
}

// API key is saved during setup mode. If the user did not complete setup, there is no API key and
// state-changing HTTP routes will be blocked until setup is complete.
String getApiKey() {
  return deviceConfig.apiKey;
}

void saveMqttConfig(bool enabled, String host, int port, String username, String password,
                    String baseTopic) {
  host.trim();
  username.trim();
  password.trim();
  baseTopic.trim();

  if (port <= 0) {
    port = 1883;
  }

  if (baseTopic.length() == 0) {
    baseTopic = getDeviceName();
  }

  preferences.begin(PREFERENCES_NAMESPACE, false);

  preferences.putBool("mqttEnabled", enabled);
  preferences.putString("mqttHost", host);
  preferences.putInt("mqttPort", port);
  preferences.putString("mqttUser", username);
  preferences.putString("mqttPass", password);
  preferences.putString("mqttTopic", baseTopic);

  preferences.end();

  deviceConfig.mqttEnabled = enabled;
  deviceConfig.mqttHost = host;
  deviceConfig.mqttPort = port;
  deviceConfig.mqttUsername = username;
  deviceConfig.mqttPassword = password;
  deviceConfig.mqttBaseTopic = baseTopic;

  Serial.println("MQTT config saved.");
  Serial.print("  MQTT enabled: ");
  Serial.println(deviceConfig.mqttEnabled ? "Yes" : "No");
  Serial.print("  MQTT configured: ");
  Serial.println(hasMqttConfig() ? "Yes" : "No");
  Serial.print("  MQTT host: ");
  Serial.println(deviceConfig.mqttHost);
  Serial.print("  MQTT port: ");
  Serial.println(deviceConfig.mqttPort);
  Serial.print("  MQTT base topic: ");
  Serial.println(deviceConfig.mqttBaseTopic);
}
