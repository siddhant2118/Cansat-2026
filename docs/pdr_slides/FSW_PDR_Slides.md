# CanSat 2026 PDR - Flight Software Slides

**Team LeoNUS • CanSat 2026**

---

## Key 2026 Mission Changes (vs 2025)

| Feature | 2026 Mission |
|---------|--------------|
| Payload separation | **80% peak altitude** |
| Descent control | **Para-glider** (not auto-gyro) |
| Descent rate | **5 m/s** average |
| Instrument release | **2m AGL** (egg, 54-64g) |
| Cameras required | 2 (separation view + ground view) |
| Steering | **Must steer toward target position** |

---

## Slide 1: FSW Overview (1/2) - Block Diagram

**Title:** FSW Overview (1/2)

### Diagram to Create in draw.io:

**CENTER BOX (Large, Light Blue):**
- **Teensy 4.1** - Central MCU

**LEFT SIDE (Comms - Pink/Salmon):**
```
┌─────────┐
│  XBee   │──── UART ────▶ [Teensy 4.1]
└─────────┘
     │
     │ Telemetry (1 Hz)
     │ GCS Commands
     ▼
┌─────────┐
│   GCS   │
└─────────┘

┌─────────┐
│   GPS   │──── UART ────▶ [Teensy 4.1]
└─────────┘
```

**RIGHT SIDE (Sensors - Purple):**
```
┌─────────────────────┐
│        IMU          │
│     (MPU9250)       │──── I2C ────▶ [Teensy 4.1]
└─────────────────────┘

┌─────────────────────┐
│ Air Pressure/Temp   │
│     (MS5611)        │──── I2C ────▶ [Teensy 4.1]
└─────────────────────┘

┌─────────────────────┐
│ Voltage/Current     │
│     (INA219)        │──── I2C ────▶ [Teensy 4.1]
└─────────────────────┘
```

**TOP (Actuators - Green):**
```
                      PWM
[Teensy 4.1] ────────────────▶ ┌──────────────────┐
                               │ 2x Servo         │
                               │ (Para-glider     │
                               │  control)        │
                               └──────────────────┘

                      PWM
[Teensy 4.1] ────────────────▶ ┌──────────────────┐
                               │ Servo            │
                               │ (CanSat          │
                               │  separation)     │
                               └──────────────────┘

                      PWM
[Teensy 4.1] ────────────────▶ ┌──────────────────┐
                               │ 2x Servo         │
                               │ (Egg Payload     │
                               │  deployment)     │
                               └──────────────────┘
```

**BOTTOM (Storage - Yellow):**
```
                               ┌──────────────────┐
[Teensy 4.1] ────────────────▶ │ SD Card          │
                   SPI         │ (Telemetry log)  │
                               └──────────────────┘

                               ┌──────────────────┐
[Teensy 4.1] ◀───────────────▶ │ EEPROM           │
                  Internal     │ (Recovery data)  │
                               └──────────────────┘
```

**SEPARATE (Light Purple - Audio Beacon):**
```
┌──────────────────────────────┐
│        Audio Beacon          │
│  (Independent coin cell)     │
│         NOT in FSW           │
└──────────────────────────────┘
```

### Arrow Labels:
- XBee ↔ Teensy: "UART - Telemetry TX, Commands RX"
- GPS → Teensy: "UART - NMEA Position"
- IMU → Teensy: "I2C - Gyro, Accel"
- MS5611 → Teensy: "I2C - Pressure, Temp, Altitude"
- INA219 → Teensy: "I2C - Voltage, Current"
- Teensy → Servos: "PWM - Actuation signals"
- Teensy → SD: "SPI - Log telemetry + events"

---

## Slide 2: FSW Overview (2/2) - Text Content

**Title:** FSW Overview (2/2)

### Content:

**★ Programming Language:**
- C++, efficient for memory handling and real-time control

**★ Development Environment:**
- PlatformIO with Arduino framework, targeting Teensy 4.1

**★ FSW Tasks:**
- Receive commands from GCS via XBee
  - Handle simulation/flight modes (F/S)
  - Process CX, ST, SIM, SIMP, CAL, MEC commands
- Process sensor measurements at **10 Hz**
  - MS5611: Pressure, temperature → altitude calculation
  - MPU9250: Gyro rates, accelerometer → tilt/rotation
  - INA219: Battery voltage/current monitoring
  - GPS: Position, time, satellite count
- Transmit telemetry at **1 Hz** (22 fields per mission guide)
- Store telemetry + events to SD card
- **Determine flight states based on altitude & velocity:**
  - LAUNCH_PAD → ASCENT → APOGEE → DESCENT
  - **At 80% peak altitude:** PROBE_RELEASE (para-glider deploys)
  - **At 2m AGL:** PAYLOAD_RELEASE (egg released)
  - LANDED
- **Para-glider guidance at 20 Hz:**
  - Calculate bearing to target
  - Differential servo control for steering
- Recover operations upon power reset via EEPROM:
  - Packet count, flight state, mode, calibration data, CMD_ECHO

---

## Slide 3: Payload FSW State Diagram (1/3) - Main Loop

**Title:** Payload FSW State Diagram (1/3)

### Diagram (Flowchart Style):

```
        ┌─────────────┐
        │  Power ON   │
        └──────┬──────┘
               ▼
        ┌─────────────┐
        │   EEPROM    │
        └──────┬──────┘
               ▼
        ◆─────────────◆           ┌─────────┐
        │   EEPROM    │           │   RTC   │ (separate box)
        │   empty?    │           └─────────┘
        ◆──────┬──────◆
          Yes  │  No
        ┌──────┴──────┐
        ▼             ▼
┌────────────┐  ┌──────────────┐
│Initialise  │  │Restore       │
│system      │  │system        │
└─────┬──────┘  └──────┬───────┘
      │                │
      │ Save           │
      │ settings       │
      └────────┬───────┘
               ▼
┌──────────────────────────┐
│Record start time t₀      │
│& packet count            │
└───────────┬──────────────┘
            │
            ▼
     ◆─────────────◆ ◀─────── [GCS commands via XBee]
     │  Simulation │          (interrupts)
     │    mode?    │
     ◆──────┬──────◆
       Yes  │  No
     ┌──────┴──────┐
     ▼             │
┌─────────┐        │
│  SIMP   │        │ (Inject simulated pressure)
│ pressure│        │
└────┬────┘        │
     │             │
     └──────┬──────┘
            │
            ▼
┌──────────────────────────┐
│ Sensor measurements      │
│ @ 10 Hz (polling)        │
└───────────┬──────────────┘
            │
     ┌──────┴────────────────────┐
     │                           │
     ▼                           ▼
┌─────────────┐          ┌───────────────────┐
│ Altitude    │          │ Flight state      │ ──▶ "Next slide"
│ calculation │          │ & controls        │
└─────────────┘          └───────────────────┘
     │                           │
     │                           │
     └───────────┬───────────────┘
                 ▼
         ◆─────────────◆
         │ t - t₀ = 1s?│
         ◆──────┬──────◆
            No  │  Yes
         ┌──────┴──────┐
         │             ▼
         │    ┌────────────────┐
         │    │Format telemetry│
         │    │data (22 fields)│
         │    └───────┬────────┘
         │            │
         │      ┌─────┴─────┐
         │      ▼           ▼
         │ ┌─────────────┐ ┌────────────────┐
         │ │Save to SD   │ │Forward to XBee │
         │ │(buffered)   │ │(transmit)      │
         │ └─────────────┘ └────────────────┘
         │            │
         └────────────┘ (loop back to sensor measurements)
```

---

## Slide 4: Payload FSW State Diagram (2/3) - Flight States

**Title:** Payload FSW State Diagram (2/3)

### Diagram (Flowchart Style):

**Note box (top right, yellow):** "ONE-TIME ACTUATION"

```
                    ┌───────────────────┐
                    │Altitude calculation│
                    │(from MS5611)      │
                    └─────────┬─────────┘
         "Update            │
          flight state"     │
              ───▶          ▼
                   ┌───────────────────┐
                   │   Flight state     │
                   │(default LAUNCH_PAD)│
                   └─────────┬─────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
    "Alt > 20m        "Velocity         "Velocity < 0
     AND              ≤ 0 for           (descending)"
     Velocity > 5m/s" 3 samples"              │
          │                  │                  │
          ▼                  ▼                  ▼
    ┌────────────┐     ┌────────────┐    ◆─────────────◆
    │ LAUNCH_PAD │     │   APOGEE   │    │ Alt ≤ 80%  │
    └─────┬──────┘     │(peak recorded)│   │   Peak?    │
          │            └─────┬──────┘    ◆──────┬──────◆
          ▼                  │              No  │  Yes
    ┌────────────┐           │                  │
    │   ASCENT   │───────────┘                  ▼
    └────────────┘                    ┌──────────────────────┐
                              No      │ PROBE_RELEASE        │
         ┌────────────────────────────│ • Separation servo    │
         │                            │ • Para-glider deploys │
         ▼                            └───────────┬───────────┘
   ┌────────────┐                                 │
   │  DESCENT   │◀────────────────────────────────┘
   └─────┬──────┘
         │
         ▼
   ◆─────────────◆
   │ Alt ≤ 2m   │
   │   AGL?     │
   ◆──────┬─────◆
      No  │  Yes
         │    │
         │    ▼
         │  ┌──────────────────────┐
         │  │ PAYLOAD_RELEASE      │
         │  │ • Egg release servo  │
         │  └───────────┬──────────┘
         │              │
         │              ▼
         │        ◆─────────────◆
         │        │ Velocity ≈ 0│
         │        │ for 5 sec?  │
         │        ◆──────┬──────◆
         │           No  │  Yes
         │               │
         └───────────────┘
                         ▼
                   ┌────────────┐
                   │   LANDED   │
                   └────────────┘
```

### State Strings (for telemetry field 5):
| State | Trigger Condition |
|-------|-------------------|
| LAUNCH_PAD | Default at power-on |
| ASCENT | Altitude > 20m AND velocity > 5 m/s |
| APOGEE | Vertical velocity ≤ 0 (3 consecutive samples) |
| DESCENT | After APOGEE |
| PROBE_RELEASE | Altitude ≤ **80% peak altitude** |
| PAYLOAD_RELEASE | Altitude ≤ **2m AGL** |
| LANDED | Near-zero velocity for 5 seconds |

---

## Slide 5: Payload FSW State Diagram (3/3) - Recovery Process

**Title:** Payload FSW State Diagram (3/3)

### Left Side - Text Content:

**★ Recovery Process:**
- Power reset reasons can include:
  - High temperatures affecting Teensy
  - Power disconnects due to shock/vibrations
  - Voltage drops or current surges from servo actuations
  - Watchdog timer timeouts

- Upon receiving power, FSW checks Teensy EEPROM:
  - Teensy 4.1 uses emulated EEPROM in flash memory
  - **If no valid data:** Default initialisation, LAUNCH_PAD state
  - **If valid data found:** Restore and continue mission

- **Data persisted to EEPROM:**
  - Packet count (continues incrementing)
  - Mode (F = Flight, S = Simulation)
  - Flight state (current FSM state)
  - Peak altitude seen (for 80% calculation)
  - Ground pressure (calibration baseline)
  - Actuated flags (prevents re-firing separation/egg)
  - CMD_ECHO (last command response)

- **Safety on recovery:**
  - One-shot actuators (separation, egg) will NOT re-fire if already fired
  - Para-glider control servos resume normal operation

### Right Side - Diagram:

```
        ┌─────────────┐
        │  Power ON   │
        └──────┬──────┘
               ▼
        ┌─────────────┐
        │ Read EEPROM │
        └──────┬──────┘
               ▼
        ◆─────────────◆
        │ Valid magic │
        │   number?   │
        ◆──────┬──────◆
          Yes  │  No
        ┌──────┴──────┐
        ▼             ▼
┌────────────────┐  ┌────────────────┐
│ Restore:       │  │ Initialise:    │
│ • Packet count │  │ • State=       │
│ • Mode (F/S)   │  │   LAUNCH_PAD   │
│ • Flight state │  │ • Packet=0     │
│ • Peak altitude│  │ • Wait for CAL │
│ • Calibration  │  │   command      │
│ • Actuated flags│ └───────┬────────┘
│ • CMD_ECHO     │          │
└───────┬────────┘          │
        │                   │
        └─────────┬─────────┘
                  │
                  ▼
           ┌─────────────┐
           │ Save settings│◀──── (ongoing)
           │ to EEPROM    │
           └──────┬──────┘
                  │
                  ▼
           [Continue to
            main loop]
```

---

## Box Color Reference (for draw.io)

| Component | Color (Hex) | Example |
|-----------|-------------|---------|
| Teensy 4.1 (central) | Light Blue #ADD8E6 | MCU |
| Sensors | Purple #DDA0DD | IMU, MS5611, INA219 |
| Communications | Pink/Salmon #FFC0CB | XBee, GPS |
| Actuators | Light Green #90EE90 | Servos |
| Storage | Yellow #FFFACD | SD Card, EEPROM |
| Decision diamonds | Dark Blue #4169E1 | EEPROM empty? |
| Audio Beacon | Light Purple #E6E6FA | Separate system |
| GCS | Yellow #FFFACD | Ground station |
