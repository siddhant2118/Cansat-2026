# CanSat 2026 Flight Software

**Team LeoNUS** | CanSat Competition 2026

## Overview

Flight software for Teensy 4.1 implementing:
- 7-state FSM (LAUNCH_PAD → ASCENT → APOGEE → DESCENT → PROBE_RELEASE → PAYLOAD_RELEASE → LANDED)
- 22-field telemetry at 1 Hz (mission guide compliant)
- Para-glider guidance with GPS steering
- Simulation mode support
- EEPROM persistence for reset recovery

## Project Structure

```
src/
├── main.cpp           # Entry point, scheduler
├── config.h           # Pin mappings, timing, thresholds
├── types.h            # Data structures, enums
├── sensors/           # MS5611, MPU9250, INA219, GPS drivers
├── fsm/               # Flight state machine
├── comms/             # XBee telemetry TX, command RX
├── storage/           # SD logging, EEPROM persistence
├── actuators/         # Servo control with safety gating
├── sim/               # Simulation mode handler
├── guidance/          # Para-glider steering
└── health/            # Fault detection
```

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload to Teensy
pio run -t upload

# Monitor serial
pio device monitor
```

## Commands

| Command | Format | Action |
|---------|--------|--------|
| CX | `CMD,<ID>,CX,ON\|OFF` | Telemetry on/off |
| ST | `CMD,<ID>,ST,hh:mm:ss\|GPS` | Set mission time |
| SIM | `CMD,<ID>,SIM,ENABLE\|ACTIVATE\|DISABLE` | Simulation mode |
| SIMP | `CMD,<ID>,SIMP,<Pa>` | Inject pressure |
| CAL | `CMD,<ID>,CAL` | Zero altitude |
| MEC | `CMD,<ID>,MEC,<DEV>,ON\|OFF` | Mechanism control |

## Configuration

Edit `src/config.h` to set:
- `TEAM_ID` - Your assigned team number
- Pin assignments
- Flight thresholds
- Target coordinates

## License

Team LeoNUS © 2026
