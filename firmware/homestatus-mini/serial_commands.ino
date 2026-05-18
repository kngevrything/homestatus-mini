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
  String parts[6];

  int partIndex = 0;
  int startIndex = 0;

  for (int i = 0; i < command.length(); i++) {
    if (command.charAt(i) == '|') {
      if (partIndex >= 6) {
        Serial.println("Invalid set command. Too many fields.");
        Serial.println("Use: set|alert|GARAGE|Open too long|Alert active");
        Serial.println("Or:  set|alert|garage|GARAGE|Open too long|Alert active");
        return;
      }

      parts[partIndex] = command.substring(startIndex, i);
      partIndex++;
      startIndex = i + 1;
    }
  }

  if (partIndex >= 6) {
    Serial.println("Invalid set command. Too many fields.");
    Serial.println("Use: set|alert|GARAGE|Open too long|Alert active");
    Serial.println("Or:  set|alert|garage|GARAGE|Open too long|Alert active");
    return;
  }

  parts[partIndex] = command.substring(startIndex);

  int partCount = partIndex + 1;

  String levelText;
  String source;
  String title;
  String mainText;
  String footer;

  // Existing format:
  // set|level|title|main|footer
  if (partCount == 5) {
    levelText = parts[1];
    source = "";
    title = parts[2];
    mainText = parts[3];
    footer = parts[4];
  }
  // New source-aware format:
  // set|level|source|title|main|footer
  else if (partCount == 6) {
    levelText = parts[1];
    source = parts[2];
    title = parts[3];
    mainText = parts[4];
    footer = parts[5];
  }
  else {
    Serial.println("Invalid set command.");
    Serial.println("Use: set|alert|GARAGE|Open too long|Alert active");
    Serial.println("Or:  set|alert|garage|GARAGE|Open too long|Alert active");
    return;
  }

  levelText = limitText(levelText, 16);
  source = limitText(source, 24);
  title = limitText(title, 12);
  mainText = limitText(mainText, 18);
  footer = limitText(footer, 18);

  StatusLevel level;

  if (!tryParseStatusLevel(levelText, level)) {
    Serial.print("Invalid level: ");
    Serial.println(levelText);
    return;
  }

  bool accepted = setStatusWithPriority(level, source, title, mainText, footer);

  if (!accepted) {
    Serial.println("Serial set command ignored.");
  }
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
  Serial.println("  set|alert|garage|GARAGE|Open too long|Alert active");
  Serial.println();
  Serial.println("HTTP routes:");
  Serial.println("  GET /");
  Serial.println("  GET /status");
  Serial.println("  GET /health");
  Serial.println("  GET /reboot?key=YOUR_API_KEY");
  Serial.println("  GET /ok?key=YOUR_API_KEY");
  Serial.println("  GET /warning?key=YOUR_API_KEY");
  Serial.println("  GET /alert?key=YOUR_API_KEY");
  Serial.println("  GET /info?key=YOUR_API_KEY");
  Serial.println("  GET /clear?key=YOUR_API_KEY");
  Serial.println("  GET /set?key=YOUR_API_KEY&level=alert&source=garage&title=GARAGE&main=Open%20too%20long&footer=Alert%20active");
  Serial.println("  GET /factory-reset?key=YOUR_API_KEY");
  Serial.println("  GET /config?key=YOUR_API_KEY");
  Serial.println();
  Serial.println("Button:");
  Serial.println("  Short press: acknowledge or clear");
  Serial.println("  Long press 8s: factory reset and reboot");
}