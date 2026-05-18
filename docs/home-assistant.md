# Home Assistant Integration

HomeStatus Mini integrates with Home Assistant through MQTT.

The device can be controlled by publishing MQTT messages, and it also publishes its current status so Home Assistant can display the active state.

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

The device uses these topics:

| Topic                          | Direction                | Purpose                                      |
| ------------------------------ | ------------------------ | -------------------------------------------- |
| `homestatus-mini/set`          | Home Assistant to device | Set a custom status message                  |
| `homestatus-mini/level/set`    | Home Assistant to device | Set a default status level                   |
| `homestatus-mini/action`       | Home Assistant to device | Trigger actions such as acknowledge or clear |
| `homestatus-mini/status`       | Device to Home Assistant | Publish current state                        |
| `homestatus-mini/availability` | Device to Home Assistant | Publish online/offline availability          |

## Home Assistant Discovery

When MQTT is enabled, HomeStatus Mini publishes Home Assistant discovery messages.

Home Assistant should create:

### Sensors

- Level
- Title
- Main
- Footer

### Controls

- Acknowledge / Clear button
- Status Level select

## Status Levels

| Level     | LED    | Meaning                                           |
| --------- | ------ | ------------------------------------------------- |
| `ok`      | Green  | No active alert                                   |
| `warning` | Yellow | Needs attention                                   |
| `alert`   | Red    | Action needed                                     |
| `info`    | Blue   | Informational or task complete                    |
| `acked`   | Yellow | Alert has been acknowledged but not fully cleared |

## Message Priority

HomeStatus Mini protects higher-priority messages from being overwritten by lower-priority automation updates.

Priority order:

````text
alert > warning > info > ok

## Publish a Custom Status

Use the `mqtt.publish` service.

```yaml
service: mqtt.publish
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
````

Expected result:

- OLED shows `GARAGE`
- OLED main text shows `Open too long`
- OLED footer shows `Check door`
- LED turns red
- Home Assistant status sensors update

## Clear the Display

```yaml
service: mqtt.publish
data:
  topic: homestatus-mini/level/set
  payload: ok
```

This applies the default OK state:

```text
HOME
All Good
No alerts
```

## Acknowledge or Clear

```yaml
service: mqtt.publish
data:
  topic: homestatus-mini/action
  payload: >
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

## Set a Default Status Level

Use the simple status level topic:

```yaml
service: mqtt.publish
data:
  topic: homestatus-mini/level/set
  payload: alert
```

Supported payloads:

```text
ok
warning
alert
info
```

These apply the device’s built-in default messages.

## Example Scripts

Add these under `scripts.yaml`, or create them through the Home Assistant UI.

### Garage Alert

```yaml
homestatus_garage_alert:
  alias: HomeStatus Garage Alert
  sequence:
    - service: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "alert",
            "title": "GARAGE",
            "main": "Open too long",
            "footer": "Check door"
          }
```

### Washer Done

```yaml
homestatus_washer_done:
  alias: HomeStatus Washer Done
  sequence:
    - service: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "info",
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
    - service: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "warning",
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
    - service: mqtt.publish
      data:
        topic: homestatus-mini/set
        payload: >
          {
            "level": "alert",
            "title": "NETWORK",
            "main": "Internet down",
            "footer": "Check router"
          }
```

### Clear HomeStatus

```yaml
homestatus_clear:
  alias: HomeStatus Clear
  sequence:
    - service: mqtt.publish
      data:
        topic: homestatus-mini/level/set
        payload: ok
```

## Example Automations

### Garage Open Too Long

This assumes you already have a garage door sensor in Home Assistant.

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
  - service: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "alert",
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
  - service: mqtt.publish
    data:
      topic: homestatus-mini/level/set
      payload: ok
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
  - service: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "info",
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
  - service: mqtt.publish
    data:
      topic: homestatus-mini/set
      payload: >
        {
          "level": "warning",
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

This prevents one automation from accidentally clearing another automation's active alert.
