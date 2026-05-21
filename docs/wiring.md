# Wiring

This document describes the confirmed wiring for the current HomeStatus Mini prototype.

## Tested Hardware

- ESP32 DevKit V1
- SSD1306 128x64 I2C OLED display
- ELEGOO RGB LED module
- ELEGOO button module

## Confirmed Pin Mapping

| Component | ESP32 Pin |
| --------- | --------- |
| OLED SDA  | GPIO21    |
| OLED SCL  | GPIO22    |
| Button    | GPIO18    |
| RGB Green | GPIO25    |
| RGB Red   | GPIO26    |
| RGB Blue  | GPIO27    |

## OLED Display

The SSD1306 OLED uses I2C.

| OLED Pin | ESP32 Pin |
| -------- | --------- |
| VCC      | 3.3V      |
| GND      | GND       |
| SDA      | GPIO21    |
| SCL      | GPIO22    |

The firmware initializes the OLED at I2C address:

```text
0x3C
```

## Button Module

| Button Pin | ESP32 Pin |
| ---------- | --------- |
| Signal     | GPIO18    |
| VCC        | 3.3V      |
| GND        | GND       |

The firmware uses:

```cpp
pinMode(BUTTON_PIN, INPUT_PULLUP);
```

The button is treated as active-low:

```text
Pressed = LOW
Released = HIGH
```

## RGB LED Module

Confirmed RGB channel mapping:

| LED Channel | ESP32 Pin |
| ----------- | --------- |
| Green       | GPIO25    |
| Red         | GPIO26    |
| Blue        | GPIO27    |

The confirmed mapping is intentionally not listed in RGB order because the tested module wiring did not match the intuitive order.

## Status Colors

| Status    | LED Behavior                |
| --------- | --------------------------- |
| `ok`      | Green                       |
| `warning` | Red + Green, appears yellow |
| `alert`   | Red                         |
| `info`    | Blue                        |
| `acked`   | Red + Green, appears yellow |

## Notes

- This prototype uses simple digital outputs for the RGB LED module.
- The current firmware does not use PWM brightness control.
- If your LED module is common-anode instead of common-cathode, the LED logic may need to be inverted.
- If colors appear wrong, run a simple channel test and update the pin mapping in firmware.
- Keep all grounds common between the ESP32 and connected modules.

## ESP32 DevKit V1 Notes

Pin labels can vary slightly across ESP32 DevKit V1 boards.

Double-check the printed labels on your board before wiring.

Avoid using boot-sensitive pins for new peripherals unless you know the implications. The current confirmed wiring uses GPIO18, GPIO21, GPIO22, GPIO25, GPIO26, and GPIO27.
