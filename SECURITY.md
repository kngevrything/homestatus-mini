# Security Policy

HomeStatus Mini is prototype firmware intended for trusted local networks.

It is not designed to be exposed directly to the public internet.

## Current Security Model

HomeStatus Mini currently uses the following protections:

- State-changing HTTP routes require an API key.
- `/status` and `/health` are readable without authentication.
- `/config` requires an API key and does not expose stored secrets.
- MQTT authentication is handled by the configured MQTT broker.
- Wi-Fi and MQTT credentials are stored locally on the ESP32 using Preferences.
- The setup access point is intended only for first-time setup and recovery.

## Stored Credentials

The device stores configuration in ESP32 Preferences, including:

- Wi-Fi SSID
- Wi-Fi password
- Device name
- API key
- MQTT host
- MQTT username
- MQTT password
- MQTT base topic

Stored credentials are not encrypted.

Anyone with sufficient physical access to the device may be able to extract secrets from flash storage. Factory reset the device before giving it to someone else.

## Recommendations

Use HomeStatus Mini only on trusted local networks.

Recommended precautions:

- Do not expose the device directly to the internet.
- Use a unique API key.
- Use a dedicated MQTT user for this device.
- Limit MQTT permissions if your broker supports ACLs.
- Do not reuse your main Home Assistant user credentials for MQTT.
- Factory reset the device before transferring ownership.
- Keep the setup access point disabled during normal operation.
- Do not publish command messages as retained MQTT messages.

## Known Limitations

Current prototype limitations:

- Stored credentials are not encrypted.
- HTTP traffic is not encrypted.
- MQTT traffic is not encrypted unless handled externally by the broker/network.
- No secure boot support is currently configured.
- No ESP32 flash encryption support is currently configured.
- Setup mode uses a shared default access point password.
- No brute-force protection exists for the local HTTP API key.

## Supported Versions

| Version | Supported              |
| ------- | ---------------------- |
| v0.1.x  | Prototype support only |

## Reporting a Vulnerability

This project is currently a prototype.

If you find a security issue, please open a GitHub issue with a clear description of the problem and steps to reproduce it.

Do not include real Wi-Fi passwords, MQTT passwords, API keys, or other secrets in public issues.

## Scope

Security reports are most useful when they involve:

- Exposure of stored secrets through HTTP, MQTT, Serial, or logs
- Authentication bypass on protected HTTP routes
- Unsafe setup mode behavior
- MQTT behavior that allows unintended command execution
- Documentation that encourages unsafe deployment

The following are currently known limitations and do not need separate reports unless you have a specific improvement proposal:

- Lack of credential encryption in ESP32 Preferences
- Lack of TLS
- Lack of secure boot
- Lack of flash encryption
