# Ground Station Software

Desktop application for CanSat 2026 mission control.

## Features

- Real-time telemetry reception and display
- Command transmission to payload
- Data logging to CSV
- Mission visualization

## Structure

```
├── Backend/     # Server/data processing
├── Frontend/    # UI application
└── Logs/        # Telemetry logs
```

## Setup

*Documentation in progress*

## Usage

1. Connect XBee radio to computer
2. Launch ground station application
3. Configure serial port and team ID
4. Begin receiving telemetry

## Commands

| Command | Format | Description |
|---------|--------|-------------|
| CX | `CMD,<ID>,CX,ON\|OFF` | Telemetry on/off |
| ST | `CMD,<ID>,ST,hh:mm:ss` | Set mission time |
| SIM | `CMD,<ID>,SIM,ENABLE\|ACTIVATE\|DISABLE` | Simulation mode |
| SIMP | `CMD,<ID>,SIMP,<Pa>` | Inject pressure |
| CAL | `CMD,<ID>,CAL` | Calibrate sensors |
| MEC | `CMD,<ID>,MEC,<DEV>,ON\|OFF` | Mechanism control |

---

*See main [README](../README.md) for project overview.*
