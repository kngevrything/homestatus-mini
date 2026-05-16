void handleButton() {
  bool isPressed = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (isPressed != buttonWasPressed) {
    if (now - lastButtonChangeMs < BUTTON_DEBOUNCE_MS) {
      return;
    }

    lastButtonChangeMs = now;
    buttonWasPressed = isPressed;

    if (isPressed) {
      buttonPressedAtMs = now;
      longPressHandled = false;
      Serial.println("Button pressed");
    } else {
      unsigned long heldMs = now - buttonPressedAtMs;

      Serial.print("Button released after ");
      Serial.print(heldMs);
      Serial.println(" ms");

      if (!longPressHandled) {
        onButtonPressed();
      }
    }
  }

  if (isPressed && !longPressHandled) {
    unsigned long heldMs = now - buttonPressedAtMs;

    if (heldMs >= FACTORY_RESET_HOLD_MS) {
      longPressHandled = true;

      Serial.println("Factory reset long press detected.");

      drawScreen("RESET", "Factory", "Rebooting...");
      updateLed(STATUS_ALERT);

      delay(1000);
      factoryResetAndReboot();
    }
  }
}

void onButtonPressed() {
  if (currentStatus.level == STATUS_WARNING || currentStatus.level == STATUS_ALERT) {
    currentStatus.level = STATUS_ACKED;
    currentStatus.footer = "Acknowledged";
    showStatus();

    Serial.println("Alert acknowledged");
    return;
  }

  if (currentStatus.level == STATUS_INFO) {
    setDefaultOk();
    Serial.println("Info cleared");
    return;
  }

  if (currentStatus.level == STATUS_ACKED) {
    setDefaultOk();
    Serial.println("Acknowledged alert cleared");
    return;
  }

  Serial.println("No active alert to acknowledge");
}