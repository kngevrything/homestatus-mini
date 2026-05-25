// HomeStatus Mini firmware entry point.
//
// Owns global configuration, shared state, setup(), and loop().
// Feature code is split into focused Arduino tabs for display, Wi-Fi,
// provisioning, HTTP, MQTT, status handling, and button behavior.

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>

// -----------------------------------------------------------------------------
// Logging and debugging
// -----------------------------------------------------------------------------
#define DEBUG_LOGS 0

#if DEBUG_LOGS
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// -----------------------------------------------------------------------------
// Hardware configuration
// -----------------------------------------------------------------------------

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

const int OLED_SDA = 21;
const int OLED_SCL = 22;

const int BUTTON_PIN = 18;

// Confirmed RGB channel mapping
const int GREEN_PIN = 25;
const int RED_PIN = 26;
const int BLUE_PIN = 27;

const int MAX_LEVEL_CHARS = 16;
const int MAX_SOURCE_CHARS = 24;
const int MAX_TITLE_CHARS = 21;
const int MAX_MAIN_CHARS = 42;
const int MAX_FOOTER_CHARS = 21;

void normalizeStatusText(String& source, String& title, String& mainText, String& footer);

const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long FACTORY_RESET_HOLD_MS = 8000;

const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;
unsigned long lastMqttReconnectAttemptMs = 0;

// -----------------------------------------------------------------------------
// Device configuration
// -----------------------------------------------------------------------------

const char* DEVICE_NAME = "homestatus-mini";

// -----------------------------------------------------------------------------
// Wi-Fi runtime configuration
// -----------------------------------------------------------------------------

const unsigned long WIFI_CHECK_INTERVAL_MS = 10000;
unsigned long lastWifiCheckMs = 0;

// -----------------------------------------------------------------------------
// Status types
// -----------------------------------------------------------------------------

enum StatusLevel { STATUS_OK, STATUS_WARNING, STATUS_ALERT, STATUS_INFO, STATUS_ACKED };

const int MAX_STATUS_SOURCES = 8;

#define MANUAL_SOURCE "manual"

struct SourceStatus {
  bool active;
  StatusLevel level;
  String source;
  String title;
  String mainText;
  String footer;
  unsigned long updatedAt;
};

struct StatusScreen {
  StatusLevel level;
  String source;
  String title;
  String mainText;
  String footer;
};

// -----------------------------------------------------------------------------
// Device config model
// -----------------------------------------------------------------------------

struct DeviceConfig {
  String wifiSSID;
  String wifiPassword;
  String deviceName;
  String apiKey;

  bool mqttEnabled;
  String mqttHost;
  int mqttPort;
  String mqttUsername;
  String mqttPassword;
  String mqttBaseTopic;

  bool hasWifiConfig() const {
    return wifiSSID.length() > 0;
  }

  bool hasApiKey() const {
    return apiKey.length() > 0;
  }

  bool hasConfig() const {
    return hasWifiConfig() && hasApiKey();
  }

  bool hasMqttConfig() const {
    return mqttEnabled && mqttHost.length() > 0 && mqttBaseTopic.length() > 0;
  }
};

// -----------------------------------------------------------------------------
// Global objects
// -----------------------------------------------------------------------------

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);
Preferences preferences;

DeviceConfig deviceConfig = {"", "", "homestatus-mini", "", false, "", 1883,
                             "", "", "homestatus-mini"};

bool setupModeActive = false;
bool lastButtonState = HIGH;

bool buttonWasPressed = false;
bool longPressHandled = false;
unsigned long buttonPressedAtMs = 0;
unsigned long lastButtonChangeMs = 0;

String serialBuffer = "";

// -----------------------------------------------------------------------------
// Hardware setup
// -----------------------------------------------------------------------------

void setupPins();

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------

void setupDisplay();
void drawScreen(String title, String mainText, String footer);

// -----------------------------------------------------------------------------
// Device config persistence
// -----------------------------------------------------------------------------

void loadDeviceConfig();
void saveDeviceConfig(String ssid, String password, String deviceName, String apiKey);
void clearDeviceConfig();

bool hasSavedWifiConfig();
bool hasSavedApiKey();
bool hasCompleteDeviceConfig();

String getDeviceName();
String getApiKey();

// -----------------------------------------------------------------------------
// Wi-Fi management
// -----------------------------------------------------------------------------

void connectToWifi();
void checkWifiConnection();
void switchToStationMode();

String wifiStatusToString(wl_status_t status);

// -----------------------------------------------------------------------------
// Provisioning / setup mode
// -----------------------------------------------------------------------------

void startSetupMode();
void setupProvisioningRoutes();

void handleSetupRoot();
void handleSetupSave();

void factoryResetAndReboot();

// -----------------------------------------------------------------------------
// HTTP routes
// -----------------------------------------------------------------------------

void setupHttpRoutes();

void handleRoot();
void handleStatusJson();
void handleHealth();
void handleReboot();
void handleFactoryReset();
void handleSetFromHttp();
void handleConfigJson();

void sendPlain(String message);

void writeStatusJson(JsonDocument& doc);
String buildStatusJson();

// -----------------------------------------------------------------------------
// HTTP security and input validation
// -----------------------------------------------------------------------------

bool requireApiKey();
bool hasValidApiKey();

String limitText(String value, int maxLength);

// -----------------------------------------------------------------------------
// Button
// -----------------------------------------------------------------------------

void handleButton();
void onButtonPressed();

// -----------------------------------------------------------------------------
// Serial commands
// -----------------------------------------------------------------------------

void handleSerial();
void processCommand(String command);
void processSetCommand(String command);
void printHelp();

// -----------------------------------------------------------------------------
// MQTT
// -----------------------------------------------------------------------------

void saveMqttConfig(bool enabled, String host, int port, String username, String password,
                    String baseTopic);
bool hasMqttConfig();
void setupMqtt();
void handleMqtt();
void reconnectMqttIfNeeded();
void onMqttMessage(char* topic, byte* payload, unsigned int length);

void publishMqttStatus();
void publishMqttAvailability(const char* availability);

String mqttStateToString(int state);

String mqttTopic(String suffix);
bool subscribeMqttTopic(String topic);
bool isMqttReady();

WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

void handleMqttAction(String action);
void handleMqttLevelSet(String levelText);

// -----------------------------------------------------------------------------
// Home Assistant MQTT discovery
// See https://www.home-assistant.io/docs/mqtt/discovery/
// For select entities: https://www.home-assistant.io/integrations/select.mqtt/
// For buttons: https://www.home-assistant.io/integrations/button.mqtt/
// Note: Home Assistant's MQTT discovery does not support sensors with multiple states, so we use a
// select entity to represent the status level. The select entity's options represent the possible
// status levels. The state is updated to match the current status level, and commands to change the
// select's state are used to set the status level with priority handling.
// ------------------------------------------------------------------------------
void publishHomeAssistantDiscovery();
void publishHaSensorDiscovery(String objectId, String name, String valueTemplate, String icon,
                              String unitOfMeasurement);

void addHaDeviceInfo(JsonObject device);

String haSafeObjectId(String value);

void publishHaButtonDiscovery(String objectId, String name, String commandTopic,
                              String payloadPress, String icon);

void publishHaSelectDiscovery(String objectId, String name, String commandTopic, String stateTopic,
                              String valueTemplate, String icon);

bool tryParseStatusLevel(String levelText, StatusLevel& level);

void setStatus(StatusLevel level, String title, String mainText, String footer);
void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate);

void setStatus(StatusLevel level, String source, String title, String mainText, String footer);

void setStatus(StatusLevel level, String source, String title, String mainText, String footer,
               bool logUpdate);

bool setStatusWithPriority(StatusLevel level, String source, String title, String mainText,
                           String footer);

int getStatusPriority(const String& level);

bool clearStatusWithSource(String source);

void setDefaultOk();
void setDefaultOkQuiet();

void setDefaultWarning();
bool setDefaultWarningWithPriority();

void setDefaultAlert();
bool setDefaultAlertWithPriority();

void setDefaultInfo();
bool setDefaultInfoWithPriority();

void showStatus();
void updateLed(StatusLevel level);
void allOff();

String statusLevelToString(StatusLevel level);

bool setStatusWithPriority(StatusLevel level, String title, String mainText, String footer);
int statusPriority(StatusLevel level);

// -----------------------------------------------------------------------------
// Encoding helpers
// -----------------------------------------------------------------------------

String escapeJson(String value);
String escapeHtml(String value);

void setup() {
  Serial.begin(115200);

  setupPins();
  setupDisplay();

  setDefaultOkQuiet();

  loadDeviceConfig();

  drawScreen(getDeviceName(), "Booting", "HomeStatus Mini");
  delay(1500);

  connectToWifi();

  if (WiFi.status() != WL_CONNECTED) {
    startSetupMode();
  } else {
    setupHttpRoutes();
    setupMqtt();
  }

  Serial.println();
  Serial.println("HomeStatus Mini ready.");
  printHelp();
}

void loop() {
  handleButton();
  handleSerial();
  server.handleClient();

  if (!setupModeActive) {
    checkWifiConnection();
    handleMqtt();
  }
}
