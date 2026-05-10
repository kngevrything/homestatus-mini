void setupHttpRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatusJson);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/reboot", HTTP_GET, handleReboot);

  server.on("/ok", HTTP_GET, []() {
    setDefaultOk();
    sendPlain("OK state set");
  });

  server.on("/clear", HTTP_GET, []() {
    setDefaultOk();
    sendPlain("Status cleared");
  });

  server.on("/warning", HTTP_GET, []() {
    setDefaultWarning();
    sendPlain("Warning state set");
  });

  server.on("/alert", HTTP_GET, []() {
    setDefaultAlert();
    sendPlain("Alert state set");
  });

  server.on("/info", HTTP_GET, []() {
    setDefaultInfo();
    sendPlain("Info state set");
  });

  server.on("/set", HTTP_GET, handleSetFromHttp);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

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
  html += "<p><strong>Device:</strong> " + String(DEVICE_NAME) + "</p>";
  html += "<p><strong>Level:</strong> " + statusLevelToString(currentStatus.level) + "</p>";
  html += "<p><strong>Title:</strong> " + escapeHtml(currentStatus.title) + "</p>";
  html += "<p><strong>Main:</strong> " + escapeHtml(currentStatus.mainText) + "</p>";
  html += "<p><strong>Footer:</strong> " + escapeHtml(currentStatus.footer) + "</p>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";

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

  html += "<h2>Custom Set</h2>";
  html += "<p>Example:</p>";
  html += "<code>/set?level=alert&title=GARAGE&main=Open%20too%20long&footer=Alert%20active</code>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleStatusJson() {
  String json = "";

  json += "{";
  json += "\"device\":\"" + String(DEVICE_NAME) + "\",";
  json += "\"level\":\"" + statusLevelToString(currentStatus.level) + "\",";
  json += "\"title\":\"" + escapeJson(currentStatus.title) + "\",";
  json += "\"mainText\":\"" + escapeJson(currentStatus.mainText) + "\",";
  json += "\"footer\":\"" + escapeJson(currentStatus.footer) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleHealth() {
  String json = "";

  json += "{";
  json += "\"device\":\"" + String(DEVICE_NAME) + "\",";
  json += "\"status\":\"ok\",";
  json += "\"wifiConnected\":";
  json += WiFi.status() == WL_CONNECTED ? "true" : "false";
  json += ",";
  json += "\"wifiStatus\":\"" + wifiStatusToString(WiFi.status()) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"level\":\"" + statusLevelToString(currentStatus.level) + "\",";
  json += "\"uptimeMs\":";
  json += String(millis());
  json += "}";

  server.send(200, "application/json", json);
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting HomeStatus Mini...");
  delay(500);
  ESP.restart();
}

void handleSetFromHttp() {
  if (!server.hasArg("level")) {
    server.send(400, "text/plain", "Missing required query parameter: level");
    return;
  }

  String levelText = server.arg("level");
  String title = server.hasArg("title") ? server.arg("title") : "HOME";
  String mainText = server.hasArg("main") ? server.arg("main") : "Updated";
  String footer = server.hasArg("footer") ? server.arg("footer") : "HTTP";

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    server.send(400, "text/plain", "Invalid level. Use ok, warning, alert, info, or acked.");
    return;
  }

  setStatus(level, title, mainText, footer);

  String response = "Status updated: ";
  response += statusLevelToString(level);
  response += " / ";
  response += title;
  response += " / ";
  response += mainText;
  response += " / ";
  response += footer;

  server.send(200, "text/plain", response);
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