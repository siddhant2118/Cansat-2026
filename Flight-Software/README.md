# Flight Software

Teensy 4.1 flight software for CanSat 2026.

## Build

```bash
pio run              # Build
pio run -t upload    # Flash to Teensy
pio device monitor   # Serial monitor
```

## Structure

```
src/
├── main.cpp           # Entry point, scheduler
├── config.h           # Pins, timing, thresholds
├── types.h            # Data structures
├── sensors/           # MS5611, MPU9250, INA219, GPS
├── fsm/               # Flight state machine
├── comms/             # Telemetry TX, command RX
├── storage/           # SD logging, EEPROM
├── actuators/         # Servo control
├── sim/               # Simulation mode
├── guidance/          # Para-glider steering
└── health/            # Fault detection
```

## Configuration

Edit `src/config.h`:
- `TEAM_ID` - Your team number
- Pin assignments
- Flight thresholds
- Target coordinates

## States

| State | Trigger |
|-------|---------|
| LAUNCH_PAD | Default |
| ASCENT | Alt > 20m, vel > 5 m/s |
| APOGEE | Velocity ≤ 0 |
| DESCENT | After apogee |
| PROBE_RELEASE | Alt ≤ 80% peak |
| PAYLOAD_RELEASE | Alt ≤ 2m AGL |
| LANDED | Velocity ≈ 0 for 5s |

---

*See main [README](../README.md) for project overview.*
