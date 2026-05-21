// Wi-Fi connection management.
//
// Connects to saved Wi-Fi credentials from Preferences, switches cleanly out of
// setup AP mode, and periodically attempts reconnects during normal operation.

// Ensure the setup access point is fully disabled before joining the user's
// Wi-Fi network. Without this, the setup SSID can linger after provisioning.
void switchToStationMode() {
  WiFi.softAPdisconnect(true);
  delay(100);

  WiFi.mode(WIFI_STA);
  delay(100);
}

void connectToWifi() {
  String ssid = deviceConfig.wifiSSID;
  String password = deviceConfig.wifiPassword;

  if (ssid.length() == 0) {
    Serial.println("No saved Wi-Fi SSID configured.");
    drawScreen("WIFI", "No Config", "Setup needed");
    return;
  }

  drawScreen("WIFI", "Connecting", ssid);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  switchToStationMode();

  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 80) {
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
    delay(2500);
  } else {
    Serial.print("Wi-Fi connection failed. Final status: ");
    Serial.print(WiFi.status());
    Serial.print(" ");
    Serial.println(wifiStatusToString(WiFi.status()));

    drawScreen("WIFI", "Failed", "Serial still works");
    delay(2000);
  }

  showStatus();
}

void checkWifiConnection() {
  String ssid = deviceConfig.wifiSSID;
  String password = deviceConfig.wifiPassword;

  if (ssid.length() == 0) {
    Serial.println("No saved Wi-Fi SSID configured. Reconnect skipped.");
    return;
  }

  unsigned long now = millis();

  if (now - lastWifiCheckMs < WIFI_CHECK_INTERVAL_MS) {
    return;
  }

  lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  // Reconnect attempts are throttled so MQTT, HTTP, Serial, and button handling
  // remain responsive if the network is down.
  Serial.println("Wi-Fi disconnected. Attempting reconnect...");

  WiFi.disconnect();
  delay(100);

  WiFi.mode(WIFI_STA);
  delay(100);

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(250);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi reconnected. IP address: ");
    Serial.println(WiFi.localIP());

    publishMqttStatus();

  } else {
    Serial.print("Wi-Fi reconnect failed. Status: ");
    Serial.print(WiFi.status());
    Serial.print(" ");
    Serial.println(wifiStatusToString(WiFi.status()));
  }
}

String wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "WL_IDLE_STATUS";

    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";

    case WL_SCAN_COMPLETED:
      return "WL_SCAN_COMPLETED";

    case WL_CONNECTED:
      return "WL_CONNECTED";

    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";

    case WL_CONNECTION_LOST:
      return "WL_CONNECTION_LOST";

    case WL_DISCONNECTED:
      return "WL_DISCONNECTED";

    default:
      return "UNKNOWN";
  }
}
