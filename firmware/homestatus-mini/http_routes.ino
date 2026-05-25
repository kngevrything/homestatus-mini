// Local HTTP API.
//
// Provides browser-friendly status pages, read-only status/health/config
// endpoints, and API-key-protected control routes for local automation.
// API key is configured through the setup page and saved in ESP32 Preferences. It is not
// encrypted, so do not expose this device to untrusted physical access.

// State-changing HTTP routes require an API key. Read-only routes such as
// /status and /health are intentionally left open for local diagnostics.
bool requireApiKey() {
  if (hasValidApiKey()) {
    return true;
  }

  server.send(401, "text/plain", "Unauthorized. Missing or invalid API key.");
  return false;
}

bool hasValidApiKey() {
  if (!server.hasArg("key")) {
    return false;
  }

  String providedKey = server.arg("key");

  if (providedKey.length() == 0) {
    return false;
  }

  return providedKey == getApiKey();
}

String limitText(String value, int maxLength) {
  value.trim();

  if (value.length() <= maxLength) {
    return value;
  }

  return value.substring(0, maxLength);
}

void setupHttpRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatusJson);
  server.on("/config", HTTP_GET, handleConfigJson);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/reboot", HTTP_GET, handleReboot);

  server.on("/ok", HTTP_GET, []() {
    if (!requireApiKey()) {
      return;
    }

    setDefaultOk();
    sendPlain("OK state set");
  });

  server.on("/clear", HTTP_GET, []() {
    if (!requireApiKey()) {
      return;
    }

    setDefaultOk();
    sendPlain("Status cleared");
  });

  server.on("/warning", HTTP_GET, []() {
    if (!requireApiKey()) {
      return;
    }

    setDefaultWarning();
    sendPlain("Warning state set");
  });

  server.on("/alert", HTTP_GET, []() {
    if (!requireApiKey()) {
      return;
    }

    setDefaultAlert();
    sendPlain("Alert state set");
  });

  server.on("/info", HTTP_GET, []() {
    if (!requireApiKey()) {
      return;
    }

    setDefaultInfo();
    sendPlain("Info state set");
  });

  server.on("/set", HTTP_GET, handleSetFromHttp);

  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

  server.on("/factory-reset", HTTP_GET, handleFactoryReset);

  server.begin();
  Serial.println("HTTP server started.");
}

void handleRoot() {
  const SourceStatus* selectedStatus = getSelectedStatus();

  int activeCount = getActiveStatusCount();
  int selectedPosition = getSelectedStatusPosition();

  String visibleLevel = "ok";
  String visibleSource = "";
  String visibleTitle = "HOME";
  String visibleMain = "All Good";
  String visibleFooter = "No alerts";

  if (selectedStatus != nullptr) {
    visibleLevel = statusLevelToString(selectedStatus->level);
    visibleSource = selectedStatus->source;
    visibleTitle = selectedStatus->title;
    visibleMain = selectedStatus->mainText;
    visibleFooter = selectedStatus->footer;
  }

  String html = "";

  html += "<!doctype html>";
  html += "<html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>HomeStatus Mini</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:24px;line-height:1.4;}";
  html += "code{background:#eee;padding:2px 4px;border-radius:4px;word-break:break-all;}";
  html +=
      "a{display:inline-block;margin:4px 8px 4px 0;padding:8px "
      "12px;background:#eee;border-radius:8px;text-decoration:none;color:#111;}";
  html += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;max-width:560px;}";
  html += ".muted{color:#555;font-size:14px;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"card\">";

  html += "<h1>HomeStatus Mini</h1>";

  html += "<h2>Device</h2>";
  html += "<p><strong>Device:</strong> " + String(getDeviceName()) + "</p>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";

  html += "<h2>Status</h2>";
  html += "<p><strong>Active Statuses:</strong> " + String(activeCount) + "</p>";

  if (activeCount == 0) {
    html += "<p><strong>Selected Position:</strong> none</p>";
  } else {
    html += "<p><strong>Selected Position:</strong> " + String(selectedPosition) + "/" +
            String(activeCount) + "</p>";
  }

  html += "<p><strong>Worst Level:</strong> " + statusLevelToString(getWorstActiveLevel()) + "</p>";

  html +=
      "<p class=\"muted\">Visible status is what the OLED is showing. Worst level is what the RGB "
      "LED is showing.</p>";

  html += "<h2>Visible Status</h2>";
  html += "<p><strong>Level:</strong> " + escapeHtml(visibleLevel) + "</p>";
  html += "<p><strong>Source:</strong> " + escapeHtml(visibleSource) + "</p>";
  html += "<p><strong>Title:</strong> " + escapeHtml(visibleTitle) + "</p>";
  html += "<p><strong>Main:</strong> " + escapeHtml(visibleMain) + "</p>";
  html += "<p><strong>Footer:</strong> " + escapeHtml(visibleFooter) + "</p>";

  html += "<h2>Read-Only Routes</h2>";
  html += "<a href=\"/status\">Status JSON</a>";
  html += "<a href=\"/health\">Health</a>";

  html += "<h2>Protected Actions</h2>";
  html += "<p>State-changing routes require <code>?key=YOUR_API_KEY</code>.</p>";
  html += "<code>/ok?key=YOUR_API_KEY</code><br>";
  html += "<code>/warning?key=YOUR_API_KEY</code><br>";
  html += "<code>/alert?key=YOUR_API_KEY</code><br>";
  html += "<code>/info?key=YOUR_API_KEY</code><br>";
  html += "<code>/clear?key=YOUR_API_KEY</code><br>";
  html += "<code>/reboot?key=YOUR_API_KEY</code><br>";
  html += "<code>/config?key=YOUR_API_KEY</code><br>";

  html += "<h2>Custom Set</h2>";
  html += "<p>Example with source:</p>";
  html +=
      "<code>/"
      "set?key=YOUR_API_KEY&level=alert&source=garage&title=GARAGE&main=Open%20too%20long&footer="
      "Check%20door</code>";

  html += "<p>Source-specific clear:</p>";
  html += "<code>/set?key=YOUR_API_KEY&level=ok&source=garage</code>";

  html += "<p>Global clear:</p>";
  html += "<code>/set?key=YOUR_API_KEY&level=ok</code>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatusJson() {
  server.send(200, "application/json", buildStatusJson());
}

// /config reports configuration state without exposing secrets.
// Passwords and API keys should never be returned by this endpoint.
void handleConfigJson() {
  if (!requireApiKey()) {
    return;
  }

  String json = "";

  json += "{";

  json += "\"device\":\"" + escapeJson(getDeviceName()) + "\",";

  json += "\"wifiConfigured\":";
  json += hasSavedWifiConfig() ? "true" : "false";
  json += ",";

  json += "\"wifiSSID\":\"" + escapeJson(deviceConfig.wifiSSID) + "\",";

  json += "\"apiKeyConfigured\":";
  json += hasSavedApiKey() ? "true" : "false";
  json += ",";

  json += "\"setupModeActive\":";
  json += setupModeActive ? "true" : "false";
  json += ",";

  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";

  json += "\"wifiStatus\":\"" + wifiStatusToString(WiFi.status()) + "\",";

  json += "\"uptimeMs\":";
  json += String(millis());
  json += ",";

  json += "\"mqttEnabled\":";
  json += deviceConfig.mqttEnabled ? "true" : "false";
  json += ",";

  json += "\"mqttConfigured\":";
  json += hasMqttConfig() ? "true" : "false";
  json += ",";

  json += "\"mqttHost\":\"" + escapeJson(deviceConfig.mqttHost) + "\",";

  json += "\"mqttPort\":";
  json += String(deviceConfig.mqttPort);
  json += ",";

  json += "\"mqttUsernameConfigured\":";
  json += deviceConfig.mqttUsername.length() > 0 ? "true" : "false";
  json += ",";

  json += "\"mqttPasswordConfigured\":";
  json += deviceConfig.mqttPassword.length() > 0 ? "true" : "false";
  json += ",";

  json += "\"mqttBaseTopic\":\"" + escapeJson(deviceConfig.mqttBaseTopic) + "\"";

  json += "}";
  server.send(200, "application/json", json);
}

void handleHealth() {
  const SourceStatus* selectedStatus = getSelectedStatus();

  String visibleLevel = "ok";
  String visibleSource = "";

  if (selectedStatus != nullptr) {
    visibleLevel = statusLevelToString(selectedStatus->level);
    visibleSource = selectedStatus->source;
  }

  String json = "";

  json += "{";
  json += "\"device\":\"" + String(getDeviceName()) + "\",";
  json += "\"status\":\"ok\",";
  json += "\"wifiConnected\":";
  json += WiFi.status() == WL_CONNECTED ? "true" : "false";
  json += ",";

  json += "\"mqttEnabled\":";
  json += deviceConfig.mqttEnabled ? "true" : "false";
  json += ",";

  json += "\"mqttConfigured\":";
  json += hasMqttConfig() ? "true" : "false";
  json += ",";

  json += "\"mqttConnected\":";
  json += mqttClient.connected() ? "true" : "false";
  json += ",";

  json += "\"homeAssistantDiscovery\":\"enabled\",";
  json += "\"wifiStatus\":\"" + wifiStatusToString(WiFi.status()) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";

  json += "\"level\":\"" + visibleLevel + "\",";
  json += "\"source\":\"" + escapeJson(visibleSource) + "\",";
  json += "\"activeCount\":";
  json += String(getActiveStatusCount());
  json += ",";
  json += "\"selectedPosition\":";
  json += String(getSelectedStatusPosition());
  json += ",";
  json += "\"worstLevel\":\"" + statusLevelToString(getWorstActiveLevel()) + "\",";

  json += "\"uptimeMs\":";
  json += String(millis());
  json += "}";

  server.send(200, "application/json", json);
}

void handleReboot() {
  if (!requireApiKey()) {
    return;
  }

  server.send(200, "text/plain", "Rebooting HomeStatus Mini...");
  delay(500);
  ESP.restart();
}

void handleFactoryReset() {
  if (!requireApiKey()) {
    return;
  }

  server.send(200, "text/plain", "Factory reset complete. Rebooting into setup mode...");
  delay(500);

  factoryResetAndReboot();
}

void handleSetFromHttp() {
  if (!requireApiKey()) {
    return;
  }

  if (!server.hasArg("level")) {
    server.send(400, "text/plain", "Missing required query parameter: level");
    return;
  }

  String levelText = limitText(server.arg("level"), MAX_LEVEL_CHARS);
  String source = server.hasArg("source") ? server.arg("source") : "";
  String title = server.hasArg("title") ? server.arg("title") : "HOME";
  String mainText = server.hasArg("main") ? server.arg("main") : "Updated";
  String footer = server.hasArg("footer") ? server.arg("footer") : "HTTP";

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    server.send(400, "text/plain", "Invalid level. Use ok, warning, alert, info, or acked.");
    return;
  }

  bool accepted = setStatusWithPriority(level, source, title, mainText, footer);

  String response = accepted ? "Status updated: " : "Status ignored: ";
  response += statusLevelToString(level);
  response += " / source=";
  response += source;
  response += " / ";
  response += title;
  response += " / ";
  response += mainText;
  response += " / ";
  response += footer;

  server.send(accepted ? 200 : 409, "text/plain", response);
}

void sendPlain(String message) {
  server.send(200, "text/plain", message);
}

String escapeJson(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "\\r");
  return value;
}

String escapeHtml(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}
