// OLED display rendering.
//
// Converts the current status title, main text, and footer into a predictable
// 128x64 SSD1306 layout. Display formatting is intentionally handled here only;
// stored status text is normalized elsewhere and is not modified by rendering.

const int OLED_WIDTH_PX = 128;

// Layout constants for the default Adafruit GFX font.
// Small text is approximately 6 px wide per character.
// Large text uses text size 2, so it is approximately 12 px per character.
const int TEXT_SIZE_SMALL = 1;
const int TEXT_SIZE_LARGE = 2;

const int SMALL_TEXT_CHAR_WIDTH_PX = 6;
const int LARGE_TEXT_CHAR_WIDTH_PX = SMALL_TEXT_CHAR_WIDTH_PX * TEXT_SIZE_LARGE;

const int TITLE_Y = 0;
const int TITLE_SEPARATOR_Y = 12;

const int MAIN_LARGE_Y = 23;
const int MAIN_SMALL_LINE_1_Y = 22;
const int MAIN_SMALL_LINE_2_Y = 34;

const int FOOTER_SEPARATOR_Y = 50;
const int FOOTER_Y = 55;

const int TITLE_MAX_CHARS = OLED_WIDTH_PX / SMALL_TEXT_CHAR_WIDTH_PX;
const int MAIN_LARGE_MAX_CHARS = OLED_WIDTH_PX / LARGE_TEXT_CHAR_WIDTH_PX;
const int MAIN_SMALL_MAX_CHARS = OLED_WIDTH_PX / SMALL_TEXT_CHAR_WIDTH_PX;
const int FOOTER_MAX_CHARS = OLED_WIDTH_PX / SMALL_TEXT_CHAR_WIDTH_PX;

void setupDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at address 0x3C");
    while (true) {
      delay(1000);
    }
  }
}

String fitText(String value, int maxChars) {
  value.trim();

  if (value.length() <= maxChars) {
    return value;
  }

  if (maxChars <= 1) {
    return value.substring(0, maxChars);
  }

  return value.substring(0, maxChars - 1) + ".";
}

// Splits long main text into two small-text lines using a word boundary when
// possible. If the text still does not fit, line 2 is truncated.
void splitTextTwoLines(String value, int maxCharsPerLine, String& line1, String& line2) {
  value.trim();

  if (value.length() <= maxCharsPerLine) {
    line1 = value;
    line2 = "";
    return;
  }

  int splitIndex = -1;
  int searchLimit = min(maxCharsPerLine, (int)value.length() - 1);

  for (int i = searchLimit; i >= 0; i--) {
    if (value.charAt(i) == ' ') {
      splitIndex = i;
      break;
    }
  }

  if (splitIndex <= 0) {
    line1 = fitText(value, maxCharsPerLine);
    line2 = "";
    return;
  }

  line1 = value.substring(0, splitIndex);
  line1.trim();

  line2 = value.substring(splitIndex + 1);
  line2.trim();
  line2 = fitText(line2, maxCharsPerLine);
}

void drawScreen(String title, String mainText, String footer) {
  title = fitText(title, TITLE_MAX_CHARS);
  footer = fitText(footer, FOOTER_MAX_CHARS);
  mainText.trim();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  // Title row
  display.setTextSize(TEXT_SIZE_SMALL);
  display.setCursor(0, TITLE_Y);
  display.print(title);

  // Title separator
  display.drawLine(0, TITLE_SEPARATOR_Y, OLED_WIDTH_PX - 1, TITLE_SEPARATOR_Y, SSD1306_WHITE);

  // Main text
  if (mainText.length() <= MAIN_LARGE_MAX_CHARS) {
    display.setTextSize(TEXT_SIZE_LARGE);
    display.setCursor(0, MAIN_LARGE_Y);
    display.print(mainText);
  } else {
    String line1;
    String line2;

    splitTextTwoLines(mainText, MAIN_SMALL_MAX_CHARS, line1, line2);

    display.setTextSize(TEXT_SIZE_SMALL);
    display.setCursor(0, MAIN_SMALL_LINE_1_Y);
    display.print(line1);

    if (line2.length() > 0) {
      display.setCursor(0, MAIN_SMALL_LINE_2_Y);
      display.print(line2);
    }
  }

  // Footer separator
  display.drawLine(0, FOOTER_SEPARATOR_Y, OLED_WIDTH_PX - 1, FOOTER_SEPARATOR_Y, SSD1306_WHITE);

  // Footer row
  display.setTextSize(TEXT_SIZE_SMALL);
  display.setCursor(0, FOOTER_Y);
  display.print(footer);

  display.display();
}
