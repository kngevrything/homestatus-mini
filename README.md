# HomeStatus Mini

HomeStatus Mini is an ESP32-based local status display prototype with an OLED screen, RGB status LED, and acknowledgement button.

The current prototype accepts serial commands and updates the OLED and RGB LED to represent household status states such as OK, warning, alert, and info.

## Current Hardware

- ESP32 DevKit V1
- SSD1306 128x64 OLED display
- ELEGOO RGB LED module
- ELEGOO button module

## Confirmed Pin Mapping

| Component | ESP32 Pin |
|---|---|
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |
| Button | GPIO18 |
| RGB Green | GPIO25 |
| RGB Red | GPIO26 |
| RGB Blue | GPIO27 |

## Current Commands

```text
ok
clear
warning
alert
info
set|alert|GARAGE|Open too long|Alert active
set|warning|FREEZER|Temp rising|Check door
```

## Local Wi-Fi Configuration

Copy the example config file:

```bash
cp firmware/homestatus-mini/wifi_config.example.h firmware/homestatus-mini/wifi_config.h

```Windows PowerShell
Copy-Item firmware/homestatus-mini/wifi_config.example.h firmware/homestatus-mini/wifi_config.h
