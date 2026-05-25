// Physical button handling.
//
// Short press browses active statuses.
// Double press acts on the status currently shown on the OLED.
// Long press performs a factory reset and reboots into setup mode.
//
// Short press is intentionally non-destructive. Acknowledge and clear actions
// require a double press and apply only to the selected visible status.

const unsigned long BUTTON_DOUBLE_PRESS_MS = 350;

int buttonClickCount = 0;
unsigned long lastButtonReleaseMs = 0;

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
        registerButtonClick(now);
      }
    }
  }

  if (isPressed && !longPressHandled) {
    unsigned long heldMs = now - buttonPressedAtMs;

    // Long press is intentionally handled before release so the user gets immediate
    // feedback once the factory-reset threshold is reached.
    if (heldMs >= FACTORY_RESET_HOLD_MS) {
      longPressHandled = true;
      buttonClickCount = 0;

      Serial.println("Factory reset long press detected.");

      drawScreen("RESET", "Factory", "Rebooting...");
      updateLed(STATUS_ALERT);

      delay(1000);
      factoryResetAndReboot();
    }
  }

  handlePendingButtonClicks(now);
}

void registerButtonClick(unsigned long now) {
  buttonClickCount++;
  lastButtonReleaseMs = now;

  if (buttonClickCount >= 2) {
    buttonClickCount = 0;
    onButtonDoublePressed();
  }
}

void handlePendingButtonClicks(unsigned long now) {
  if (buttonClickCount == 0) {
    return;
  }

  if (now - lastButtonReleaseMs < BUTTON_DOUBLE_PRESS_MS) {
    return;
  }

  if (buttonClickCount == 1) {
    buttonClickCount = 0;
    onButtonPressed();
    return;
  }

  buttonClickCount = 0;
}

void onButtonPressed() {
  if (selectNextActiveStatus()) {
    DEBUG_PRINTLN("Selected next active status");
    return;
  }

  DEBUG_PRINTLN("No active status to browse");
}

void onButtonDoublePressed() {
  if (actOnSelectedStatus()) {
    DEBUG_PRINTLN("Acted on selected status");
    return;
  }

  DEBUG_PRINTLN("No selected status action available");
}
