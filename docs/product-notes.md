# Product Notes

HomeStatus Mini is currently a personal prototype and local-first automation display.

These notes describe the current product direction, design decisions, and known tradeoffs.

## Product Intent

HomeStatus Mini is intended to be a small physical status display for local automations.

It is not meant to replace Home Assistant dashboards, phone notifications, or full smart displays. It is meant to provide a glanceable physical indicator for important household or homelab states.

Examples:

- Garage left open
- Washer done
- Freezer temperature warning
- Internet down
- Server offline
- 3D printer finished
- General Home Assistant alert state

## Current Audience

The current version is best suited for:

- Home Assistant users
- Homelab users
- MQTT users
- Makers and tinkerers
- Developers who want a simple physical status endpoint

This is not currently a mainstream consumer product. It requires some understanding of local networks, MQTT, HTTP, or Home Assistant.

## Design Principles

### Local First

The device is designed to run on a trusted local network without a cloud service.

### Simple Output Surface

The device is primarily an output surface. It shows status provided by another system, such as Home Assistant, MQTT automations, scripts, or local services.

### Glanceable Status

The OLED and RGB LED should communicate state quickly.

The display is optimized for short status text:

- Title: category or source
- Main: important state
- Footer: action or detail

Example:

```text
GARAGE
Open too long
Check door
```

### Multiple Control Paths

The device supports several ways to control status:

- Serial commands
- HTTP API
- MQTT
- Home Assistant discovery controls
- Physical button

All runtime status updates should flow through shared status logic so priority, source handling, display updates, and MQTT publishing remain consistent.

## Status Model

The current firmware stores one active displayed status.

Supported levels:

```text
ok
warning
alert
info
acked
```

Priority order:

```text
alert / acked > warning > info > ok
```

Higher-priority messages can block lower-priority messages.

## Source-Aware Clearing

Messages can include a `source`.

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

A clear message with the same source clears that source:

```json
{
  "level": "ok",
  "source": "garage"
}
```

A clear message without a source clears everything:

```json
{
  "level": "ok"
}
```

## Current Limitation: No Status Registry Yet

The current firmware does not maintain a source-keyed registry or queue of active statuses.

Example limitation:

1. Garage sends an alert.
2. Washer sends an info message.
3. Washer info may be blocked by the garage alert.
4. If the garage clears, the washer info is not restored.

A future version may add a source-keyed status registry:

```text
garage -> alert
washer -> info
freezer -> warning
network -> alert
```

The device would then display the highest-priority active source and fall back to the next active source when one clears.

## Setup Model

On first boot, or after factory reset, the device starts a temporary setup access point.

```text
SSID: HomeStatus-Setup
Password: homestatus
Setup URL: http://192.168.99.1
```

The setup page collects:

- Wi-Fi SSID and password
- Device name
- API key
- Optional MQTT broker settings

Settings are stored in ESP32 Preferences.

## Security Model

HomeStatus Mini is intended for trusted local networks.

Current protections:

- State-changing HTTP routes require an API key.
- `/config` requires an API key and does not expose secrets.
- MQTT authentication is delegated to the MQTT broker.
- The setup AP is only intended for provisioning.

Known tradeoffs:

- Wi-Fi and MQTT credentials are stored in ESP32 Preferences.
- Stored credentials are not encrypted.
- The setup AP uses a shared default password.
- There is no TLS support yet.
- The device should not be exposed directly to the internet.

## Possible Future Work

- Source-keyed status registry
- Alert expiration / stale message timeout
- Optional alert queue behavior
- OTA firmware updates
- Cleaner logging controls with a debug flag
- Better setup portal styling
- Optional Wi-Fi scan during setup
- Enclosure design
- Additional Home Assistant controls
- Optional TLS support
- More polished display layouts
