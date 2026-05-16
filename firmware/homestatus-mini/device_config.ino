const char* PREFERENCES_NAMESPACE = "homestatus";

void loadDeviceConfig() {
    preferences.begin(PREFERENCES_NAMESPACE, true);

    deviceConfig.wifiSSID = preferences.getString("wifiSSID", "");
    deviceConfig.wifiPassword = preferences.getString("wifiPassword", "");
    deviceConfig.deviceName = preferences.getString("deviceName", "homestatus-mini");
    deviceConfig.apiKey = preferences.getString("apiKey", "");

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

String getDeviceName() {
  if (deviceConfig.deviceName.length() > 0) {
    return deviceConfig.deviceName;
  }

  return "homestatus-mini";
}

String getApiKey() {
  if (deviceConfig.apiKey.length() > 0) {
    return deviceConfig.apiKey;
  }

  return String(API_KEY);
}