void connectToWifi() {
  drawScreen("WIFI", "Connecting", WIFI_SSID);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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
  unsigned long now = millis();

  if (now - lastWifiCheckMs < WIFI_CHECK_INTERVAL_MS) {
    return;
  }

  lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Wi-Fi disconnected. Attempting reconnect...");

  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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