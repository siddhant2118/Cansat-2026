# CanSat 2026 - Team LeoNUS

Open-source flight and ground station software for the **CanSat Competition 2026**.

## Mission Overview

Design and build a CanSat that:
- Deploys from a rocket at peak altitude
- Separates payload at **80% peak altitude** with para-glider descent control
- Steers toward a target location
- Releases a protected egg at **2 meters AGL**
- Transmits telemetry at 1 Hz throughout the mission

## Repository Structure

```
├── Flight-Software/     # Teensy 4.1 FSW (C++/PlatformIO)
├── Ground-Station/      # GCS application
└── docs/                # Shared documentation
    ├── architecture/    # System diagrams
    ├── pdr_slides/      # PDR presentation content
    └── reference/       # Mission guides, sample PDRs
```

## Flight Software

Modular FSW for Teensy 4.1 with:
- 7-state flight state machine
- 22-field telemetry (mission guide compliant)
- Sensor drivers: MS5611, MPU9250, INA219, GPS
- Para-glider guidance with GPS steering
- Simulation mode for testing
- EEPROM persistence for reset recovery

**[View Flight Software README →](Flight-Software/README.md)**

## Ground Station

Desktop application for:
- Real-time telemetry display
- Command transmission
- Data logging and visualization

**[View Ground Station README →](Ground-Station/README.md)**

## Quick Start

### Flight Software
```bash
cd Flight-Software
pio run           # Build
pio run -t upload # Flash to Teensy
```

### Ground Station
```bash
cd Ground-Station
# See Ground-Station/README.md for setup
```

## Competition

- **Competition:** CanSat 2026
- **Team:** LeoNUS
- **Mission Guide:** [CanSat_Mission_Guide_2026g-2.pdf](docs/reference/CanSat_Mission_Guide_2026g-2.pdf)

## License

MIT License - See individual component licenses.

---

*Team LeoNUS © 2026*
