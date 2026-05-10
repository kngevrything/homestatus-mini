void setupDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at address 0x3C");
    while (true) {
      delay(1000);
    }
  }
}

void drawScreen(String title, String mainText, String footer) {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 22);
  display.println(mainText);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.println(footer);

  display.display();
}