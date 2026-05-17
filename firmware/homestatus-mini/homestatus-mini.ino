#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "wifi_config.h"

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
const int RED_PIN   = 26;
const int BLUE_PIN  = 27;

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

enum StatusLevel {
  STATUS_OK,
  STATUS_WARNING,
  STATUS_ALERT,
  STATUS_INFO,
  STATUS_ACKED
};

struct StatusScreen {
  StatusLevel level;
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

DeviceConfig deviceConfig = {
  "",
  "",
  "homestatus-mini",
  "",
  false,
  "",
  1883,
  "",
  "",
  "homestatus-mini"
};

StatusScreen currentStatus = {
  STATUS_OK,
  "HOME",
  "All Good",
  "No alerts"
};

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

// -----------------------------------------------------------------------------
// HTTP security and input validation
// -----------------------------------------------------------------------------

bool requireApiKey();
bool hasValidApiKey();

String limitedTextArg(const String& name, const String& fallback, int maxLength);
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

void saveMqttConfig(bool enabled, String host, int port, String username, String password, String baseTopic);
bool hasMqttConfig();
void setupMqtt();
void handleMqtt();
void reconnectMqttIfNeeded();
void onMqttMessage(char* topic, byte* payload, unsigned int length);

void publishMqttStatus();
void publishMqttAvailability(const char* availability);

String mqttStateToString(int state);

String mqttTopic(String suffix);
bool isMqttReady();

WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

void publishHomeAssistantDiscovery();
void publishHaSensorDiscovery(String objectId, String name, String valueTemplate, String icon, String unitOfMeasurement);
String haSafeObjectId(String value);

void publishHaButtonDiscovery(
  String objectId,
  String name,
  String commandTopic,
  String payloadPress,
  String icon
);

void handleMqttAction(String action);


// -----------------------------------------------------------------------------
// Status state
// -----------------------------------------------------------------------------

bool tryParseStatusLevel(String levelText, StatusLevel& level);

void setStatus(StatusLevel level, String title, String mainText, String footer);
void setStatus(StatusLevel level, String title, String mainText, String footer, bool logUpdate);

void setDefaultOk();
void setDefaultWarning();
void setDefaultAlert();
void setDefaultInfo();

void showStatus();
void updateLed(StatusLevel level);
void allOff();

String statusLevelToString(StatusLevel level);

// -----------------------------------------------------------------------------
// Encoding helpers
// -----------------------------------------------------------------------------

String escapeJson(String value);
String escapeHtml(String value);


void setup() {
    Serial.begin(115200);

    setupPins();
    setupDisplay();

    setStatus(STATUS_OK, "HOME", "All Good", "No alerts", false);

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