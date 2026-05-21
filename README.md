# HomeStatus Mini

HomeStatus Mini is a local-first ESP32 status display for home automation, homelab monitoring, and Home Assistant / MQTT workflows.

It uses a small OLED screen, RGB status LED, and physical acknowledge button to show simple household or system states such as OK, warning, alert, info, and acknowledged.

The project is currently prototype firmware, but the core flow works:

- First-time Wi-Fi setup mode
- OLED status display
- RGB status LED
- Physical acknowledge / clear button
- Local HTTP API
- API-key-protected control routes
- MQTT status and command topics
- Home Assistant MQTT discovery
- Source-aware status clearing
- Message priority handling
- Long-press factory reset

## Current Hardware

Tested with:

- ESP32 DevKit V1
- SSD1306 128x64 I2C OLED display
- ELEGOO RGB LED module
- ELEGOO button module

Other ESP32 boards should work, but pin mappings may need to change.

## Confirmed Pin Mapping

| Component | ESP32 Pin |
| --------- | --------- |
| OLED SDA  | GPIO21    |
| OLED SCL  | GPIO22    |
| Button    | GPIO18    |
| RGB Green | GPIO25    |
| RGB Red   | GPIO26    |
| RGB Blue  | GPIO27    |

## Arduino Requirements

Install the ESP32 board package:

- ESP32 by Espressif Systems

Install these Arduino libraries through the Arduino Library Manager:

- Adafruit SSD1306
- Adafruit GFX Library
- PubSubClient
- ArduinoJson

## First-Time Setup

No firmware secrets are required before compiling.

When no saved Wi-Fi configuration exists, HomeStatus Mini starts a temporary setup access point:

```text
SSID: HomeStatus-Setup
Password: homestatus
Setup URL: http://192.168.99.1
```

Setup flow:

1. Flash the firmware to the ESP32.
2. Power on the device.
3. Connect your phone or computer to `HomeStatus-Setup`.
4. Open `http://192.168.99.1`.
5. Enter Wi-Fi, device name, API key, and optional MQTT settings.
6. Save the form.
7. The device reboots and connects to your Wi-Fi network.

Settings are stored in ESP32 Preferences.

## Physical Button

| Action                | Behavior                                                     |
| --------------------- | ------------------------------------------------------------ |
| Short press           | Acknowledge alert, clear info, or clear acknowledged state   |
| Long press, 8 seconds | Factory reset saved configuration and reboot into setup mode |

## Status Levels

| Level     | LED    | Meaning                                |
| --------- | ------ | -------------------------------------- |
| `ok`      | Green  | No active alert                        |
| `warning` | Yellow | Needs attention                        |
| `alert`   | Red    | Action needed                          |
| `info`    | Blue   | Informational state                    |
| `acked`   | Yellow | Alert was acknowledged but not cleared |

## Message Priority

HomeStatus Mini protects higher-priority messages from being overwritten by lower-priority automation updates.

Priority order:

```text
alert / acked > warning > info > ok
```

`ok` is treated as an explicit clear.

## Source-Aware Clearing

Status messages can include an optional `source`.

Example:

```json
{
  "level": "alert",
  "source": "garage",
  "title": "GARAGE",
  "main": "Open too long",
  "footer": "Check door"
}
```

A source-specific clear only clears the display if the current status has the same source:

```json
{
  "level": "ok",
  "source": "garage"
}
```

An `ok` message without a source clears everything:

```json
{
  "level": "ok"
}
```

This helps prevent one automation from accidentally clearing another automation's active alert.

## Serial Commands

Serial commands are available at `115200` baud.

```text
help
ok
clear
warning
alert
info
set|alert|GARAGE|Open too long|Alert active
set|alert|garage|GARAGE|Open too long|Alert active
set|warning|freezer|FREEZER|Temp rising|Check door
```

The source-aware format is:

```text
set|level|source|title|main|footer
```

## HTTP API

Read-only routes:

```text
GET /
GET /status
GET /health
```

Protected routes require:

```text
?key=YOUR_API_KEY
```

Protected routes:

```text
GET /ok?key=YOUR_API_KEY
GET /warning?key=YOUR_API_KEY
GET /alert?key=YOUR_API_KEY
GET /info?key=YOUR_API_KEY
GET /clear?key=YOUR_API_KEY
GET /reboot?key=YOUR_API_KEY
GET /factory-reset?key=YOUR_API_KEY
GET /config?key=YOUR_API_KEY
```

Set a custom status:

```text
GET /set?key=YOUR_API_KEY&level=alert&source=garage&title=GARAGE&main=Open%20too%20long&footer=Check%20door
```

Source-specific clear:

```text
GET /set?key=YOUR_API_KEY&level=ok&source=garage
```

Global clear:

```text
GET /set?key=YOUR_API_KEY&level=ok
```

## MQTT

When MQTT is enabled, the device uses the configured base topic.

Default base topic:

```text
homestatus-mini
```

Topics:

| Topic                          | Direction        | Purpose                      |
| ------------------------------ | ---------------- | ---------------------------- |
| `homestatus-mini/set`          | Broker to device | Set custom status JSON       |
| `homestatus-mini/level/set`    | Broker to device | Set default status level     |
| `homestatus-mini/action`       | Broker to device | Trigger acknowledge / clear  |
| `homestatus-mini/status`       | Device to broker | Publish current state        |
| `homestatus-mini/availability` | Device to broker | Publish `online` / `offline` |

Custom status payload:

```json
{
  "level": "alert",
  "source": "garage",
  "title": "GARAGE",
  "main": "Open too long",
  "footer": "Check door"
}
```

Source-specific clear:

```json
{
  "level": "ok",
  "source": "garage"
}
```

Global clear:

```json
{
  "level": "ok"
}
```

Action payload:

```json
{
  "action": "ack"
}
```

Level set payload:

```text
alert
```

Supported level values:

```text
ok
warning
alert
info
```

Do not retain command messages on `set`, `level/set`, or `action`.

## Home Assistant

HomeStatus Mini publishes MQTT discovery messages for Home Assistant.

Discovered sensors:

- Level
- Source
- Title
- Main
- Footer

Discovered controls:

- Acknowledge / Clear button
- Status Level select

See:

```text
docs/home-assistant.md
```

## Security Notes

HomeStatus Mini is intended for trusted local networks.

- State-changing HTTP routes require an API key.
- `/status` and `/health` are readable without an API key.
- `/config` requires an API key and does not expose secrets.
- MQTT authentication is handled by the configured MQTT broker.
- Wi-Fi and MQTT credentials are stored in ESP32 Preferences.
- Stored credentials are not encrypted.
- Do not expose the device directly to the internet.
- Use a dedicated MQTT user with limited permissions when possible.
- Factory reset the device before giving it to someone else.

## Known Limitations

- Prototype firmware.
- Credentials are stored in ESP32 Preferences and are not encrypted.
- No OTA firmware update support yet.
- No TLS support for HTTP or MQTT yet.
- Setup AP uses a shared default password.
- Display text is optimized for short status messages.
- The device currently displays one active status at a time.
- Lower-priority messages can be blocked by higher-priority messages and are not queued for later display.
- A future version may add a source-keyed status registry for multiple active sources.
- Hardware enclosure is not finalized.

## Project Structure

```text
firmware/homestatus-mini/
  homestatus-mini.ino
  status.ino
  display.ino
  button.ino
  wifi_manager.ino
  provisioning.ino
  device_config.ino
  http_routes.ino
  mqtt_manager.ino
  serial_commands.ino

docs/
  home-assistant.md
```

## Roadmap

Possible future work:

- Source-keyed status registry for multiple active sources
- Alert expiration / stale message handling
- Optional alert queue behavior
- OTA firmware updates
- Enclosure design
- Cleaner logging controls with a debug flag
- Optional TLS support

## License

Licensed under the Apache License, Version 2.0.
