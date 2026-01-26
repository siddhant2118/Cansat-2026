# Flight Software - Future Improvements

**Status Legend:** ⬜ Planned | 🔄 In Progress | ✅ Implemented

---

## Sensor Improvements

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| Medium | **Kalman Filter** | ✅ | Implemented Research-Grade (Sabatini/Wu) 3-State EKF |
| Low | **TinyGPS++** | ⬜ | Replace custom GPS parser with proven library |
| Low | **Magnetometer heading** | ⬜ | Use MPU9250 magnetometer for heading when stationary |

---

## Control System

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| High | **PID Tuning** | 🔄 | Python sim created: `test/winch_pid_sim.py` |
| Medium | **Trajectory prediction** | ✅ | 1s lookahead implemented in guidance.cpp |
| Low | **LQR Controller** | ⬜ | Alternative to PID if more precision needed |
| Low | **Wind compensation** | ⬜ | Estimate wind from GPS drift vs heading |

---

## Timing & Performance

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| Medium | **IntervalTimer** | ✅ | Hardware timers in scheduler.h/cpp |
| Low | **DMA for sensors** | ⬜ | Non-blocking sensor reads |

---

## Telemetry & Logging

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| Medium | **SdFat library** | ⬜ | Faster SD writes than SD.h |
| Low | **Binary telemetry** | ⬜ | More efficient than ASCII CSV (optional) |
| Low | **Telemetry compression** | ⬜ | Reduce radio bandwidth |

---

## Safety & Reliability

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| High | **Watchdog timer** | ⬜ | Auto-reset if FSW hangs |
| Medium | **CRC on commands** | ⬜ | Verify command integrity |
| Low | **Redundant altitude** | ⬜ | Use GPS altitude as backup |

---

## Testing

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| High | **SIL Simulator** | ⬜ | Test FSW on PC without hardware |
| High | **Unit tests** | ⬜ | Test FSM transitions, PID, etc. |
| Medium | **HIL Testing** | ⬜ | Hardware-in-the-loop with simulated flight profile |

---

## Cameras

| Priority | Feature | Status | Notes |
|----------|---------|--------|-------|
| ? | **Camera trigger** | ⬜ | Need to confirm if FSW controls cameras or if always-on |
| ? | **Camera selection** | ⬜ | TBD - waiting for hardware specs |

---

## Completed

| Feature | Date | Notes |
|---------|------|-------|
| PID Guidance Controller | 2026-01-26 | Replaced proportional-only with full PID |
| Basic FSM (7 states) | 2026-01-24 | All state transitions implemented |
| EEPROM Persistence | 2026-01-24 | Packet count, state, peak altitude |

---

*Last updated: 2026-01-26*
