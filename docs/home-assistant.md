# Home Assistant Integration

HomeStatus Mini integrates with Home Assistant through MQTT.

The device publishes MQTT discovery messages so Home Assistant can automatically create sensors and controls.

## Requirements

- Home Assistant
- MQTT integration enabled
- MQTT broker configured, such as the Mosquitto broker add-on
- HomeStatus Mini configured with the MQTT broker host, port, username, password, and base topic

## Default Topics

If the MQTT base topic is:

```text
homestatus-mini
```

The device uses:

| Topic                          | Direction                | Purpose                                      |
| ------------------------------ | ------------------------ | -------------------------------------------- |
| `homestatus-mini/set`          | Home Assistant to device | Set a custom status message                  |
| `homestatus-mini/level/set`    | Home Assistant to device | Set a default status level                   |
| `homestatus-mini/action`       | Home Assistant to device | Trigger actions such as acknowledge or clear |
| `homestatus-mini/status`       | Device to Home Assistant | Publish current state                        |
| `homestatus-mini/availability` | Device to Home Assistant | Publish online/offline availability          |

## Home Assistant Discovery

HomeStatus Mini publishes discovery messages for:

### Sensors

- Level
- Source
- Title
- Main
- Footer

### Controls

- Acknowledge / Clear button
- Status Level select

If the device does not appear, verify that MQTT is connected and that Home Assistant MQTT discovery is enabled.

## Status Levels

| Level     | LED    | Meaning                                           |
| --------- | ------ | ------------------------------------------------- |
| `ok`      | Green  | No active alert                                   |
| `warning` | Yellow | Needs attention                                   |
| `alert`   | Red    | Action needed                                     |
| `info`    | Blue   | Informational or task complete                    |
| `acked`   | Yellow | Alert has been acknowledged but not fully cleared |

## Publish a Custom Status

Use the `mqtt.publish` action.

```yaml
action: mqtt.publish
data:
  topic: homestatus-mini/set
  payload: >
    {
      "level": "alert",
      "source": "garage",
      "title": "GARAGE",
      "main": "Open too long",
      "footer": "Check door"
    }
```

Expected result:

- OLED shows `GARAGE`
- OLED main text shows `Open too long`
- OLED footer shows `Check door`
- LED turns red
- Home Assistant status sensors update

## Source-Aware Clearing

Status messages can include an optional `source`.

Source-specific clear:

```yaml
action: mqtt.publish
data:
  topic: homestatus-mini/set
  payload: >
    {
      "level": "ok",
      "source": "garage"
    }
```

This only clears the display if the current status is owned by `garage`.

Global clear:

```yaml
action: mqtt.publish
data:
  topic: homestatus-mini/set
  payload: >
    {
      "level": "ok"
    }
```

This clears the display regardless of source.

## Acknowledge or Clear

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

Behavior matches the physical button:

| Current State | Result                         |
| ------------- | ------------------------------ |
| `warning`     | Changes to `acked`             |
| `alert`       | Changes to `acked`             |
| `info`        | Clears to `ok`                 |
| `acked`       | Clears to `ok`                 |
| `ok`          | No active alert to acknowledge |

## Status Level Select

The Home Assistant `Status Level` select publishes simple values to:

```text
homestatus-mini/level/set
```

Supported payloads:

```text
ok
warning
alert
info
```

These apply the device's built-in default messages.

## Example Scripts

Add these under `scripts.yaml`, or create them through the Home Assistant UI.

### Garage Alert

```yaml
homestatus_garage_alert:
  alias: HomeStatus Garage Alert
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "alert",
            "source": "garage",
            "title": "GARAGE",
            "main": "Open too long",
            "footer": "Check door"
          }
```

### Garage Clear

```yaml
homestatus_garage_clear:
  alias: HomeStatus Garage Clear
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "ok",
            "source": "garage"
          }
```

### Washer Done

```yaml
homestatus_washer_done:
  alias: HomeStatus Washer Done
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "info",
            "source": "washer",
            "title": "WASHER",
            "main": "Done",
            "footer": "Move laundry"
          }
```

### Freezer Warning

```yaml
homestatus_freezer_warning:
  alias: HomeStatus Freezer Warning
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "warning",
            "source": "freezer",
            "title": "FREEZER",
            "main": "Temp rising",
            "footer": "Check door"
          }
```

### Internet Down

```yaml
homestatus_internet_down:
  alias: HomeStatus Internet Down
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "alert",
            "source": "network",
            "title": "NETWORK",
            "main": "Internet down",
            "footer": "Check router"
          }
```

### Global Clear

```yaml
homestatus_clear_all:
  alias: HomeStatus Clear All
  sequence:
    - action: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "ok"
          }
```

## Example Automations

### Garage Open Too Long

Replace `binary_sensor.garage_door` with your actual entity.

```yaml
alias: HomeStatus Garage Open Too Long
trigger:
  - platform: state
    entity_id: binary_sensor.garage_door
    to: "on"
    for:
      minutes: 15
action:
  - action: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "alert",
          "source": "garage",
          "title": "GARAGE",
          "main": "Open 15 min",
          "footer": "Check door"
        }
mode: single
```

### Garage Closed

```yaml
alias: HomeStatus Garage Closed
trigger:
  - platform: state
    entity_id: binary_sensor.garage_door
    to: "off"
action:
  - action: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "ok",
          "source": "garage"
        }
mode: single
```

### Washer Done

This example assumes a washer power sensor where low power means the washer has finished.

Replace `sensor.washer_power` with your actual entity and adjust the threshold.

```yaml
alias: HomeStatus Washer Done
trigger:
  - platform: numeric_state
    entity_id: sensor.washer_power
    below: 5
    for:
      minutes: 3
action:
  - action: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "info",
          "source": "washer",
          "title": "WASHER",
          "main": "Done",
          "footer": "Move laundry"
        }
mode: single
```

### Freezer Temperature Warning

Replace `sensor.freezer_temperature` with your actual entity.

```yaml
alias: HomeStatus Freezer Temp Warning
trigger:
  - platform: numeric_state
    entity_id: sensor.freezer_temperature
    above: 10
    for:
      minutes: 5
action:
  - action: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "warning",
          "source": "freezer",
          "title": "FREEZER",
          "main": "Temp rising",
          "footer": "Check door"
        }
mode: single
```

## Testing with Developer Tools

In Home Assistant:

1. Go to **Developer Tools**
2. Open the **Actions** tab
3. Select `mqtt.publish`
4. Use a topic and payload from the examples above

Example:

```yaml
topic: homestatus-mini/set
payload: >
  {
    "level": "alert",
    "source": "test",
    "title": "TEST",
    "main": "MQTT works",
    "footer": "Home Assistant"
  }
```

## Notes

- Do not retain command messages on `homestatus-mini/set`, `homestatus-mini/action`, or `homestatus-mini/level/set`.
- The device publishes retained status and availability messages.
- The MQTT base topic can be changed during device setup.
- If the base topic changes, update the examples to match the new topic.
- Use stable source names such as `garage`, `washer`, `freezer`, or `network`.
- The current firmware displays one active status at a time. Lower-priority messages are not queued for later display yet.
