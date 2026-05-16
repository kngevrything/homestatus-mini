void handleSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processCommand(String command) {
  command.trim();

  if (command == "help") {
    printHelp();
    return;
  }

  if (command == "ok" || command == "clear") {
    setDefaultOk();
    return;
  }

  if (command == "warning") {
    setDefaultWarning();
    return;
  }

  if (command == "alert") {
    setDefaultAlert();
    return;
  }

  if (command == "info") {
    setDefaultInfo();
    return;
  }

  if (command.startsWith("set|")) {
    processSetCommand(command);
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
}

void processSetCommand(String command) {
  String parts[5];

  int partIndex = 0;
  int startIndex = 0;

  for (int i = 0; i < command.length(); i++) {
    if (command.charAt(i) == '|') {
      if (partIndex < 5) {
        parts[partIndex] = command.substring(startIndex, i);
        partIndex++;
        startIndex = i + 1;
      }
    }
  }

  if (partIndex < 4) {
    Serial.println("Invalid set command.");
    Serial.println("Use: set|alert|GARAGE|Open too long|Alert active");
    return;
  }

  parts[partIndex] = command.substring(startIndex);

  String levelText = parts[1];
  String title = parts[2];
  String mainText = parts[3];
  String footer = parts[4];

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    Serial.print("Invalid level: ");
    Serial.println(levelText);
    return;
  }

  setStatus(level, title, mainText, footer);
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help");
  Serial.println("  ok");
  Serial.println("  clear");
  Serial.println("  warning");
  Serial.println("  alert");
  Serial.println("  info");
  Serial.println("  set|alert|GARAGE|Open too long|Alert active");
  Serial.println();
  Serial.println("HTTP routes:");
  Serial.println("  GET /");
  Serial.println("  GET /status");
  Serial.println("  GET /health");
  Serial.println("  GET /reboot");
  Serial.println("  GET /ok");
  Serial.println("  GET /warning");
  Serial.println("  GET /alert");
  Serial.println("  GET /info");
  Serial.println("  GET /clear");
  Serial.println("  GET /set?level=alert&title=GARAGE&main=Open%20too%20long&footer=Alert%20active");
  Serial.println("  GET /factory-reset?key=YOUR_API_KEY");
  Serial.println("  GET /config?key=YOUR_API_KEY");
 
  Serial.println("Button:");
  Serial.println("  Short press: acknowledge or clear");
  Serial.println("  Long press 8s: factory reset and reboot");
  
}