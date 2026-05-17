## Reliability / Device Routes

```text
GET /health
GET /reboot
```

## MQTT Configuration

Stage 4A adds MQTT configuration storage only.

The setup page can save:

- MQTT enabled
- MQTT host
- MQTT port
- MQTT username
- MQTT password
- MQTT base topic

The device does not connect to MQTT yet. That is planned for the next slice.

The protected `/config` endpoint reports MQTT configuration state without exposing the MQTT password.

## MQTT Runtime

When MQTT is enabled and configured, the device uses these topics:

```text
<baseTopic>/set
<baseTopic>/status
<baseTopic>/availability
```

## MQTT Action Topic

The device also listens for simple actions on:

```text
<baseTopic>/action
```

Example:

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

These currently map to the same behavior as the physical button.

## MQTT Status Level Topic

The device listens for simple status level changes on:

```text
<baseTopic>/level/set
```

Payload values:

```text
ok
warning
alert
info
```

These map to the device's default status messages.

Example:

```text
Topic: homestatus-mini/level/set
Payload: alert
```

Home Assistant discovery creates a `Status Level` select entity that uses this topic.
