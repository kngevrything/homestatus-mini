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

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.on("/factory-reset", HTTP_GET, handleFactoryReset);

  server.begin();
  Serial.println("HTTP server started.");
}

void handleRoot() {
  String html = "";

  html += "<!doctype html>";
  html += "<html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>HomeStatus Mini</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:24px;line-height:1.4;}";
  html += "code{background:#eee;padding:2px 4px;border-radius:4px;}";
  html += "a{display:inline-block;margin:4px 8px 4px 0;padding:8px 12px;background:#eee;border-radius:8px;text-decoration:none;color:#111;}";
  html += ".card{border:1px solid #ddd;border-radius:12px;padding:16px;max-width:520px;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"card\">";
  html += "<h1>HomeStatus Mini</h1>";
  html += "<p><strong>Device:</strong> " + String(getDeviceName()) + "</p>";
  html += "<p><strong>Level:</strong> " + statusLevelToString(currentStatus.level) + "</p>";
  html += "<p><strong>Title:</strong> " + escapeHtml(currentStatus.title) + "</p>";
  html += "<p><strong>Main:</strong> " + escapeHtml(currentStatus.mainText) + "</p>";
  html += "<p><strong>Footer:</strong> " + escapeHtml(currentStatus.footer) + "</p>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
 
  html += "<h2>Quick Actions</h2>";
  html += "<p>State-changing routes require <code>?key=YOUR_API_KEY</code>.</p>";
  html += "<code>/alert?key=YOUR_API_KEY</code><br>";
  html += "<code>/clear?key=YOUR_API_KEY</code><br>";

  html += "<h2>Read-Only Routes</h2>";
  html += "<a href=\"/status\">Status JSON</a>";
  html += "<a href=\"/health\">Health</a>";

  html += "<h2>Quick Actions</h2>";
  html += "<a href=\"/ok\">OK</a>";
  html += "<a href=\"/warning\">Warning</a>";
  html += "<a href=\"/alert\">Alert</a>";
  html += "<a href=\"/info\">Info</a>";
  html += "<a href=\"/clear\">Clear</a>";

  html += "<h2>Device</h2>";
  html += "<a href=\"/status\">Status JSON</a>";
  html += "<a href=\"/health\">Health</a>";
  html += "<a href=\"/reboot\">Reboot</a>";
  html += "<a href=\"/config?key=YOUR_API_KEY\">Config</a>";
  html += "<h2>Custom Set</h2>";
  html += "<p>Example:</p>";
  html += "<code>/set?level=alert&title=GARAGE&main=Open%20too%20long&footer=Alert%20active</code>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatusJson() {
  server.send(200, "application/json", buildStatusJson());
}

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
  json += "\"level\":\"" + statusLevelToString(currentStatus.level) + "\",";
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