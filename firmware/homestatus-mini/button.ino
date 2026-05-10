void handleButton() {
  bool buttonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && buttonState == LOW) {
    onButtonPressed();
    delay(250);
  }

  lastButtonState = buttonState;
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