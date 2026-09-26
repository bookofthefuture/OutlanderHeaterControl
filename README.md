# OutlanderHeaterControl
Arduino sketch for an Arduino with an MCP2515 to control a Mitsubishi Outlander PHEV Gen3 (2016+) water heater based on Jamie Jones's original. Tweaking to add on/off switch integrated into pot and potentially remote control via Home Assistant node down the line.

Heater control itself is now handled by [Zombieverter](https://openinverter.org/wiki/Zombieverter), rather than this
controller talking to the heater directly. On CAN ID 509 (0x1FD), this controller sends a single message with:
- Byte 0: HeatReq, `0x01` if the pot's switch is on, `0x00` otherwise
- Bytes 1-2: the pot value mapped to 0-4095, little-endian

Zombieverter's CAN input map needs to be configured to match this layout.

This is now the only CAN message this controller sends - it no longer generates a BMS heartbeat (0x285) or
rebroadcasts heater/HV status telemetry (0x300).

See https://openinverter.org/wiki/Mitsubishi_Outlander_Water_Heater
