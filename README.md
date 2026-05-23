# HomeStatus Mini

HomeStatus Mini is a local-first ESP32 status display for home automation, homelab monitoring, and Home Assistant / MQTT workflows.

It uses a small OLED screen, RGB status LED, and physical acknowledge button to show simple household or system states such as OK, warning, alert, info, and acknowledged.

This is prototype firmware, but the core flow works.

## Features

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

## Pin Summary

| Component | ESP32 Pin |
| --------- | --------- |
| OLED SDA  | GPIO21    |
| OLED SCL  | GPIO22    |
| Button    | GPIO18    |
| RGB Green | GPIO25    |
| RGB Red   | GPIO26    |
| RGB Blue  | GPIO27    |

See [`docs/wiring.md`](docs/wiring.md) for wiring details.

## Arduino Requirements

Install the ESP32 board package:

- ESP32 by Espressif Systems

Install these Arduino libraries through the Arduino Library Manager:

- Adafruit SSD1306
- Adafruit GFX Library
- PubSubClient
- ArduinoJson

## Quick Start

1. Clone the repository.
2. Open `firmware/homestatus-mini/homestatus-mini.ino` in the Arduino IDE.
3. Select your ESP32 board and port.
4. Install the required libraries.
5. Flash the firmware.
6. Connect to the setup Wi-Fi network.
7. Configure Wi-Fi, API key, device name, and optional MQTT settings.
8. Use Serial, HTTP, MQTT, or Home Assistant to update the display.

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

## Status Model

Supported levels:

| Level     | LED    | Meaning                                |
| --------- | ------ | -------------------------------------- |
| `ok`      | Green  | No active alert                        |
| `warning` | Yellow | Needs attention                        |
| `alert`   | Red    | Action needed                          |
| `info`    | Blue   | Informational state                    |
| `acked`   | Yellow | Alert was acknowledged but not cleared |

Priority order:

```text
alert / acked > warning > info > ok
```

Messages can include an optional `source`. A source-specific clear only clears the display if the current status has the same source.

Example alert:

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

## Control Options

HomeStatus Mini can be controlled through:

- Serial commands
- Local HTTP routes
- MQTT topics
- Home Assistant MQTT discovery controls
- Physical button

See [`docs/commands.md`](docs/commands.md) for the full Serial, HTTP, and MQTT command reference.

## MQTT

When MQTT is enabled, the device uses the configured base topic.

Default base topic:

```text
homestatus-mini
```

Core topics:

| Topic                          | Direction        | Purpose                      |
| ------------------------------ | ---------------- | ---------------------------- |
| `homestatus-mini/set`          | Broker to device | Set custom status JSON       |
| `homestatus-mini/level/set`    | Broker to device | Set default status level     |
| `homestatus-mini/action`       | Broker to device | Trigger acknowledge / clear  |
| `homestatus-mini/status`       | Device to broker | Publish current state        |
| `homestatus-mini/availability` | Device to broker | Publish `online` / `offline` |

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

See [`docs/home-assistant.md`](docs/home-assistant.md) for setup notes, scripts, and automation examples.

## Documentation

- [`docs/wiring.md`](docs/wiring.md), hardware wiring and pin mapping
- [`docs/commands.md`](docs/commands.md), Serial, HTTP, and MQTT command reference
- [`docs/home-assistant.md`](docs/home-assistant.md), Home Assistant MQTT integration examples
- [`docs/product-notes.md`](docs/product-notes.md), product direction, design notes, tradeoffs, and roadmap

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
  commands.md
  home-assistant.md
  product-notes.md
  wiring.md
```

## Security Notes

HomeStatus Mini is intended for trusted local networks.

See [SECURITY.md](SECURITY.md) for the prototype security model, local-network assumptions, and credential storage notes.

- State-changing HTTP routes require an API key.
- `/status` and `/health` are readable without an API key.
- `/config` requires an API key and does not expose secrets.
- MQTT authentication is handled by the configured MQTT broker.
- Wi-Fi and MQTT credentials are stored in ESP32 Preferences.
- Stored credentials are not encrypted.
- Do not expose the device directly to the internet.
- Use a dedicated MQTT user with limited permissions when possible.
- Factory reset the device before giving it to someone else.

## Official Builds and Forks

The firmware is licensed under GPLv3.

Forks and modifications are allowed under the license, but unofficial builds are not endorsed by this project. Users should only flash firmware from sources they trust. Official releases are published from this repository.

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

## Roadmap

Possible future work:

- Source-keyed status registry for multiple active sources
- Alert expiration / stale message timeout
- Optional alert queue behavior
- OTA firmware updates
- Enclosure design
- Cleaner logging controls with a debug flag
- Optional TLS support
- Better setup portal styling
- Optional Wi-Fi scan during setup

## License

Licensed under the GNU General Public License v3.0.
