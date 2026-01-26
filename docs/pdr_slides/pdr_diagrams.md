# FSW PDR Diagrams (Mermaid)

These diagrams correspond to the ASCII placeholders in `FSW_PDR_Slides.md`. You can view them in a Mermaid-compatible viewer or use the generated images.

## Slide 1: System Block Diagram

```mermaid
flowchart TD
    %% Styling
    classDef mcu fill:#ADD8E6,stroke:#333,stroke-width:2px;
    classDef sensor fill:#DDA0DD,stroke:#333,stroke-width:1px;
    classDef comms fill:#FFC0CB,stroke:#333,stroke-width:1px;
    classDef actuator fill:#90EE90,stroke:#333,stroke-width:1px;
    classDef storage fill:#FFFACD,stroke:#333,stroke-width:1px;
    
    %% Nodes
    Teensy[Teensy 4.1 MCU]:::mcu
    
    %% Comms
    subgraph Comms [Communications]
        XBee[XBee Radio]:::comms
        GPS[NEO-M8N GPS]:::comms
        GCS[Ground Control Station]:::comms
    end
    
    %% Sensors
    subgraph Sensors [Sensors]
        IMU[MPU9250 IMU]:::sensor
        Baro[MS5611 Barometer]:::sensor
        Power[INA219 Power Mon]:::sensor
    end
    
    %% Actuators
    subgraph Actuators [Actuators]
        ServoPara[2x Para-foil Servos]:::actuator
        ServoSep[Separation Servo]:::actuator
        ServoEgg[Egg Release Servo]:::actuator
    end
    
    %% Storage
    subgraph Storage [Storage]
        SD[SD Card Log]:::storage
        EEPROM[Flash EEPROM]:::storage
    end
    
    %% Connections
    XBee <-->|UART| Teensy
    XBee -->|Telemetry| GCS
    GCS -->|Commands| XBee
    GPS -->|UART| Teensy
    
    IMU -->|I2C| Teensy
    Baro -->|I2C| Teensy
    Power -->|I2C| Teensy
    
    Teensy -->|PWM| ServoPara
    Teensy -->|PWM| ServoSep
    Teensy -->|PWM| ServoEgg
    
    Teensy -->|SPI| SD
    Teensy <-->|Internal| EEPROM
```

## Slide 3: Main Loop Logic

```mermaid
flowchart LR
    %% Subgraph for initialization
    subgraph Init [Initialization]
        direction TB
        PowerON([Power ON]) --> EEPROM{EEPROM\nInvalid?}
        EEPROM -- Yes --> Initialise[Initialise System]
        EEPROM -- No --> Restore[Restore System]
        Initialise --> SaveSetting[Save Settings]
        Restore --> SaveSetting
        SaveSetting --> RecordStart[Record Start t0\n& Packet Count]
    end

    %% Main Logic Stream
    RecordStart --> SimCheck{Sim Mode?}
    
    subgraph Sensors [Sensor Acquisition]
        direction TB
        SimCheck -- Yes --> SIMP[Inject Pressure]
        SimCheck -- No --> SensorRead[Read Sensors\n@ 10 Hz]
        SIMP --> SensorRead
        SensorRead --> AltCalc[Calc Altitude\n& State]
    end

    %% State Logic
    subgraph Logic [Control Logic]
        direction TB
        AltCalc --> TimerCheck{1Hz Tick?}
        TimerCheck -- No --> SensorRead
        TimerCheck -- Yes --> Format[Format Telemetry]
        
        Format --> Log[Log to SD]
        Format --> Transmit[Transmit XBee]
        Log --> SensorRead
        Transmit --> SensorRead
    end

    %% Styling
    classDef box fill:#f9f9f9,stroke:#333,stroke-width:1px;
    class Init,Sensors,Logic box;
```

## Slide 4: Flight State Machine

```mermaid
stateDiagram-v2
    direction LR

    state "LAUNCH_PAD" as LP
    state "ASCENT" as ASC
    state "APOGEE" as APO
    state "DESCENT" as DESC
    state "LANDED" as LAND

    [*] --> LP
    LP --> ASC: Alt > 20m & Vel > 5m/s
    ASC --> APO: Vel <= 0 (3 samples)
    APO --> DESC: Immediate

    state DESC {
        direction LR
        state "Target Guidance" as GUID
        state "Active Braking" as BRAKE
        [*] --> GUID
        GUID --> BRAKE
    }

    DESC --> PROBE_RELEASE: Alt <= 80% Peak
    state PROBE_RELEASE {
        direction TB
        [*] --> SepServo
        SepServo --> DeployPara
    }

    PROBE_RELEASE --> PAYLOAD_RELEASE: Alt <= 2m AGL
    PAYLOAD_RELEASE --> LAND: Vel ~ 0 (5s)
    LAND --> [*]
```

## Slide 5: Recovery Logic

```mermaid
flowchart TD
    PowerON([Power ON]) --> ReadEEPROM[Read EEPROM]
    
    ReadEEPROM --> Valid{Valid Magic\nNumber?}
    
    Valid -- Yes --> Restore[Restore System:\nPacket Count\nMode F/S\nFlight State\nPeak Alt\nCalibration\nActuated Flags\nCMD_ECHO]
    
    Valid -- No --> Init[Initialise:\nState=LAUNCH_PAD\nPacket=0\nWait for CAL]
    
    Restore --> Save[Save Settings\nto EEPROM]
    Init --> Save
    
    Save --> MainLoop[Continue to Main Loop]
```
