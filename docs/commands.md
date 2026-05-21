# Commands and API Reference

HomeStatus Mini can be controlled through Serial, HTTP, and MQTT.

## Status Levels

| Level     | Meaning                                     |
| --------- | ------------------------------------------- |
| `ok`      | No active alert                             |
| `warning` | Needs attention                             |
| `alert`   | Action needed                               |
| `info`    | Informational state                         |
| `acked`   | Alert has been acknowledged but not cleared |

## Serial Commands

Serial commands are available at `115200` baud.

```text
help
ok
clear
warning
alert
info
```

Set a custom status:

```text
set|alert|GARAGE|Open too long|Alert active
```

Set a custom status with a source:

```text
set|alert|garage|GARAGE|Open too long|Alert active
```

Format:

```text
set|level|title|main|footer
set|level|source|title|main|footer
```

## HTTP API

### Read-Only Routes

These routes do not require an API key:

```text
GET /
GET /status
GET /health
```

### Protected Routes

State-changing routes require an API key:

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

### Custom Status

```text
GET /set?key=YOUR_API_KEY&level=alert&source=garage&title=GARAGE&main=Open%20too%20long&footer=Check%20door
```

### Source-Specific Clear

```text
GET /set?key=YOUR_API_KEY&level=ok&source=garage
```

This clears the display only if the current status source is `garage`.

### Global Clear

```text
GET /set?key=YOUR_API_KEY&level=ok
```

This clears the display regardless of source.

## MQTT Topics

Assuming the MQTT base topic is:

```text
homestatus-mini
```

The device uses:

| Topic                          | Direction        | Purpose                      |
| ------------------------------ | ---------------- | ---------------------------- |
| `homestatus-mini/set`          | Broker to device | Set custom status JSON       |
| `homestatus-mini/level/set`    | Broker to device | Set default status level     |
| `homestatus-mini/action`       | Broker to device | Trigger acknowledge / clear  |
| `homestatus-mini/status`       | Device to broker | Publish current state        |
| `homestatus-mini/availability` | Device to broker | Publish `online` / `offline` |

## MQTT Custom Status

Topic:

```text
homestatus-mini/set
```

Payload:

```json
{
  "level": "alert",
  "source": "garage",
  "title": "GARAGE",
  "main": "Open too long",
  "footer": "Check door"
}
```

## MQTT Source-Specific Clear

Topic:

```text
homestatus-mini/set
```

Payload:

```json
{
  "level": "ok",
  "source": "garage"
}
```

## MQTT Global Clear

Topic:

```text
homestatus-mini/set
```

Payload:

```json
{
  "level": "ok"
}
```

## MQTT Action Topic

Topic:

```text
homestatus-mini/action
```

Payload:

```json
{
  "action": "ack"
}
```

Supported actions:

```text
ack
acknowledge
clear
```

The action behavior mirrors the physical button:

| Current State | Result                         |
| ------------- | ------------------------------ |
| `alert`       | Changes to `acked`             |
| `warning`     | Changes to `acked`             |
| `info`        | Clears to `ok`                 |
| `acked`       | Clears to `ok`                 |
| `ok`          | No active alert to acknowledge |

## MQTT Status Level Topic

Topic:

```text
homestatus-mini/level/set
```

Payload examples:

```text
ok
warning
alert
info
```

These apply the built-in default status messages.

## Retained Messages

The device publishes retained messages for:

```text
homestatus-mini/status
homestatus-mini/availability
```

Do not retain command messages on:

```text
homestatus-mini/set
homestatus-mini/action
homestatus-mini/level/set
```

Retained command messages can replay unexpectedly after reconnect.
