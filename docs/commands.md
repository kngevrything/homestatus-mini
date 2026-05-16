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
