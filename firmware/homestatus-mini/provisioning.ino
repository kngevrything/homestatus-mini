// First-time setup and provisioning mode.
//
// When no saved Wi-Fi configuration exists, or connection fails, the ESP32
// starts a temporary setup access point and serves a local setup page for Wi-Fi,
// device, API key, and MQTT settings.

void startSetupMode() {
  setupModeActive = true;

  server.stop();
  delay(100);

  const char* setupSsid = "HomeStatus-Setup";
  const char* setupPassword = "homestatus";

  // Use a less common private subnet for setup mode to avoid conflicts with
  // typical home router addresses.
  Serial.println("Starting setup mode.");
  Serial.print("Setup SSID: ");
  Serial.println(setupSsid);
  Serial.println("Setup URL: http://192.168.99.1");

  WiFi.disconnect(true);
  delay(250);

  WiFi.mode(WIFI_AP);
  delay(100);

  IPAddress localIp(192, 168, 99, 1);
  IPAddress gateway(192, 168, 99, 1);
  IPAddress subnet(255, 255, 255, 0);

  bool configOk = WiFi.softAPConfig(localIp, gateway, subnet);

  Serial.print("softAPConfig OK: ");
  Serial.println(configOk ? "yes" : "no");

  bool apStarted = WiFi.softAP(setupSsid, setupPassword);

  Serial.print("softAP started: ");
  Serial.println(apStarted ? "yes" : "no");

  IPAddress ip = WiFi.softAPIP();

  Serial.print("softAP IP: ");
  Serial.println(ip);

  drawScreen("SETUP MODE", setupSsid, ip.toString());

  setupProvisioningRoutes();

  server.begin();
  Serial.println("Setup HTTP server started.");
}

void setupProvisioningRoutes() {
  server.on("/", HTTP_GET, handleSetupRoot);
  server.on("/setup", HTTP_GET, handleSetupRoot);
  server.on("/save", HTTP_POST, handleSetupSave);

  server.onNotFound([]() { server.send(404, "text/plain", "Setup route not found"); });
}

void handleSetupRoot() {
  String html = "";

  html += "<!doctype html>";
  html += "<html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>HomeStatus Mini Setup</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:24px;line-height:1.4;}";
  html += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;max-width:520px;}";
  html += "label{display:block;margin-top:12px;font-weight:bold;}";
  html += "input{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;}";
  html +=
      "button{margin-top:16px;padding:10px "
      "14px;border:0;border-radius:8px;background:#111;color:#fff;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"card\">";
  html += "<h1>HomeStatus Mini Setup</h1>";
  html += "<p>Enter your local Wi-Fi settings and a local API key.</p>";
  html += "<form method=\"POST\" action=\"/save\">";

  html += "<label>Wi-Fi SSID</label>";
  html += "<input name=\"wifiSSID\" required>";

  html += "<label>Wi-Fi Password</label>";
  html += "<input name=\"wifiPassword\" type=\"password\">";

  html += "<label>Device Name</label>";
  html += "<input name=\"deviceName\" value=\"homestatus-mini\">";

  html += "<label>API Key</label>";
  html += "<input name=\"apiKey\" value=\"change-me\" required>";
  html += "<p style=\"font-size:13px;color:#555;\">";
  html += "Required for local HTTP control routes. Change this from the default.";
  html += "</p>";

  html += "<h2>MQTT Settings</h2>";
  html += "<p style=\"font-size:13px;color:#555;\">";
  html +=
      "Optional. Required for Home Assistant discovery and MQTT control. "
      "Leave disabled if you only want to use Serial or local HTTP control.";

  html += "</p>";

  html += "<label>";
  html += "<input name=\"mqttEnabled\" type=\"checkbox\" value=\"1\"> Enable MQTT";
  html += "</label>";

  html += "<label>MQTT Host</label>";
  html += "<input name=\"mqttHost\" placeholder=\"192.168.1.10 or mqtt.local\">";

  html += "<label>MQTT Port</label>";
  html += "<input name=\"mqttPort\" type=\"number\" value=\"1883\">";

  html += "<label>MQTT Username</label>";
  html += "<input name=\"mqttUsername\">";

  html += "<label>MQTT Password</label>";
  html += "<input name=\"mqttPassword\" type=\"password\">";

  html += "<label>MQTT Base Topic</label>";
  html += "<input name=\"mqttBaseTopic\" value=\"homestatus-mini\">";

  html += "<button type=\"submit\">Save and Reboot</button>";
  html += "</form>";

  html += "<p style=\"margin-top:16px;font-size:13px;color:#555;\">";
  html += "After saving, reconnect your phone or computer to your normal Wi-Fi network.";
  html += "</p>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

// Setup credentials are stored in ESP32 Preferences. They are not encrypted,
// so do not expose this device to untrusted physical access.
void handleSetupSave() {
  if (!server.hasArg("wifiSSID") || !server.hasArg("apiKey")) {
    server.send(400, "text/plain", "Missing required setup fields.");
    return;
  }

  String ssid = limitText(server.arg("wifiSSID"), 64);
  String password = limitText(server.arg("wifiPassword"), 64);
  String deviceName = limitText(server.arg("deviceName"), 32);
  String apiKey = limitText(server.arg("apiKey"), 64);

  if (ssid.length() == 0 || apiKey.length() < 8) {
    server.send(400, "text/plain", "SSID is required and API key must be at least 8 characters.");
    return;
  }

  saveDeviceConfig(ssid, password, deviceName, apiKey);

  bool mqttEnabled = server.hasArg("mqttEnabled") && server.arg("mqttEnabled") == "1";

  String mqttHost = server.hasArg("mqttHost") ? limitText(server.arg("mqttHost"), 64) : "";
  int mqttPort = server.hasArg("mqttPort") ? server.arg("mqttPort").toInt() : 1883;
  String mqttUsername =
      server.hasArg("mqttUsername") ? limitText(server.arg("mqttUsername"), 64) : "";
  String mqttPassword =
      server.hasArg("mqttPassword") ? limitText(server.arg("mqttPassword"), 64) : "";
  String mqttBaseTopic =
      server.hasArg("mqttBaseTopic") ? limitText(server.arg("mqttBaseTopic"), 64) : "";

  if (mqttPort <= 0) {
    mqttPort = 1883;
  }

  if (mqttBaseTopic.length() == 0) {
    mqttBaseTopic = deviceName;
  }

  saveMqttConfig(mqttEnabled, mqttHost, mqttPort, mqttUsername, mqttPassword, mqttBaseTopic);

  // Setup save logging
  Serial.println("Saved setup values:");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Device name: ");
  Serial.println(deviceName);
  Serial.print("API key length: ");
  Serial.println(apiKey.length());

  Serial.print("MQTT enabled: ");
  Serial.println(mqttEnabled ? "Yes" : "No");
  Serial.print("MQTT host: ");
  Serial.println(mqttHost);
  Serial.print("MQTT port: ");
  Serial.println(mqttPort);
  Serial.print("MQTT username configured: ");
  Serial.println(mqttUsername.length() > 0 ? "Yes" : "No");
  Serial.print("MQTT password configured: ");
  Serial.println(mqttPassword.length() > 0 ? "Yes" : "No");
  Serial.print("MQTT base topic: ");
  Serial.println(mqttBaseTopic);

  String html = "";
  html += "<!doctype html><html><body>";
  html += "<h1>Saved</h1>";
  html += "<p>HomeStatus Mini is rebooting and will try to join your Wi-Fi network.</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  delay(1000);
  ESP.restart();
}
