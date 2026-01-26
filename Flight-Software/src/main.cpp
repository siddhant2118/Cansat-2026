/**
 * @file main.cpp
 * @brief CanSat 2026 Flight Software - Main Entry Point
 * @team LeoNUS
 * 
 * Implements cooperative scheduler for:
 * - 10 Hz sensor sampling + FSM update
 * - 20 Hz guidance loop
 * - 1 Hz telemetry transmission and SD logging
 */

#include <Arduino.h>
#include "config.h"
#include "types.h"

// Module includes
#include "sensors/sensor_manager.h"
#include "fsm/fsm.h"
#include "comms/telemetry.h"
#include "comms/commands.h"
#include "storage/sd_logger.h"
#include "storage/persistence.h"
#include "actuators/servo_controller.h"
#include "sim/sim_mode.h"
#include "guidance/guidance.h"
#include "health/health.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Module instances
SensorManager sensors;
FlightStateMachine fsm;
TelemetryManager telemetry;
CommandHandler commands;
SDLogger sdLogger;
PersistenceManager persistence;
ServoController servos;
SimulationMode simMode;
GuidanceController guidance;
HealthMonitor health;

// Scheduler timing
static uint32_t lastSensorTick = 0;
static uint32_t lastGuidanceTick = 0;
static uint32_t lastTelemetryTick = 0;
static uint32_t lastHealthTick = 0;

// Global state
static bool telemetryEnabled = false;
static uint32_t packetCount = 0;
static char cmdEcho[CMD_ECHO_SIZE] = "";

// Mission time (can be set via ST command or GPS)
static uint32_t missionTimeBase = 0;   // millis() at time set
static uint32_t missionTimeOffset = 0; // Seconds offset from base

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void processCommand(const Command& cmd);
TelemetryFrame buildTelemetryFrame();
void handleActuations();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // Initialize debug serial
    #if DEBUG_SERIAL
    Serial.begin(SERIAL_DEBUG_BAUD);
    while (!Serial && millis() < 3000); // Wait up to 3s for USB serial
    Serial.println(F("CanSat 2026 FSW - Team LeoNUS"));
    Serial.print(F("TEAM_ID: "));
    Serial.println(TEAM_ID);
    #endif

    // Initialize I2C
    Wire.begin();
    Wire.setClock(400000); // 400 kHz fast mode

    // Initialize persistence and check for reset recovery
    persistence.begin();
    if (persistence.isValid()) {
        #if DEBUG_SERIAL
        Serial.println(F("Recovering from reset..."));
        #endif
        PersistentData data = persistence.load();
        packetCount = data.packetCount;
        fsm.restoreState(static_cast<FlightState>(data.state));
        simMode.setMode(data.mode == 'S' ? SimState::ACTIVATED : SimState::DISABLED);
        sensors.setGroundPressure(data.groundPressure);
        sensors.setPeakAltitude(data.peakAltitude);
        servos.restoreActuatedFlags(data.actuatedFlags);
        strncpy(cmdEcho, data.cmdEcho, CMD_ECHO_SIZE);
        health.logEvent("RESET_RECOVERY");
    } else {
        #if DEBUG_SERIAL
        Serial.println(F("Fresh start - initializing..."));
        #endif
        fsm.setState(FlightState::LAUNCH_PAD);
    }

    // Initialize sensors
    sensors.begin();
    
    // Initialize communications
    telemetry.begin();
    commands.begin();
    
    // Initialize SD logging
    if (!sdLogger.begin()) {
        health.setSdFail(true);
        #if DEBUG_SERIAL
        Serial.println(F("WARNING: SD card init failed"));
        #endif
    }
    
    // Initialize servos (but don't arm yet)
    servos.begin();
    
    // Initialize guidance
    guidance.begin();
    guidance.setTarget(TARGET_LATITUDE, TARGET_LONGITUDE);
    
    // Initialize health monitor
    health.begin();
    
    // Record startup
    uint32_t now = millis();
    lastSensorTick = now;
    lastGuidanceTick = now;
    lastTelemetryTick = now;
    lastHealthTick = now;
    
    #if DEBUG_SERIAL
    Serial.println(F("Initialization complete"));
    Serial.println(F("Waiting for CXON command..."));
    #endif
}

// ============================================================================
// MAIN LOOP - COOPERATIVE SCHEDULER
// ============================================================================
void loop() {
    uint32_t now = millis();
    
    // -------------------------------------------------------------------------
    // COMMAND POLLING (continuous)
    // -------------------------------------------------------------------------
    Command cmd;
    if (commands.poll(cmd)) {
        processCommand(cmd);
    }
    
    // -------------------------------------------------------------------------
    // 10 Hz: SENSOR SAMPLING + FSM UPDATE (100ms)
    // -------------------------------------------------------------------------
    if (now - lastSensorTick >= TICK_SENSOR_MS) {
        lastSensorTick = now;
        
        // Update sensors
        sensors.update();
        
        // In simulation mode, override pressure with simulated value
        if (simMode.isActivated()) {
            sensors.overridePressure(simMode.getSimulatedPressure());
        }
        
        // Update FSM based on sensor data
        const SensorData& data = sensors.getData();
        fsm.update(data);
        
        // Handle any actuations triggered by FSM
        handleActuations();
    }
    
    // -------------------------------------------------------------------------
    // 20 Hz: GUIDANCE LOOP (50ms)
    // -------------------------------------------------------------------------
    if (now - lastGuidanceTick >= TICK_GUIDANCE_MS) {
        lastGuidanceTick = now;
        
        FlightState state = fsm.getState();
        const SensorData& data = sensors.getData();
        
        // Only run guidance during paraglider descent phases
        if (state == FlightState::PROBE_RELEASE || 
            state == FlightState::PAYLOAD_RELEASE) {
            guidance.update(data.gps, data.imu);
            
            // Apply steering commands to servos
            int16_t leftCmd, rightCmd;
            guidance.getServoCommands(leftCmd, rightCmd);
            servos.setPosition(ServoId::LEFT_CONTROL, SERVO_PWM_CENTER + leftCmd);
            servos.setPosition(ServoId::RIGHT_CONTROL, SERVO_PWM_CENTER + rightCmd);
        } else {
            // Neutral steering when not in guidance mode
            servos.setPosition(ServoId::LEFT_CONTROL, SERVO_PWM_CENTER);
            servos.setPosition(ServoId::RIGHT_CONTROL, SERVO_PWM_CENTER);
        }
    }
    
    // -------------------------------------------------------------------------
    // 1 Hz: TELEMETRY + LOGGING (1000ms)
    // -------------------------------------------------------------------------
    if (now - lastTelemetryTick >= TICK_TELEMETRY_MS) {
        lastTelemetryTick = now;
        
        if (telemetryEnabled) {
            // Build and transmit telemetry frame
            TelemetryFrame frame = buildTelemetryFrame();
            telemetry.transmit(frame);
            
            // Log to SD
            sdLogger.logTelemetry(frame);
            
            // Increment and persist packet count
            packetCount++;
            persistence.savePacketCount(packetCount);
            
            #if DEBUG_TELEMETRY
            Serial.print(F("TX Packet #"));
            Serial.println(packetCount);
            #endif
        }
    }
    
    // -------------------------------------------------------------------------
    // 2 Hz: HEALTH CHECK (500ms)
    // -------------------------------------------------------------------------
    if (now - lastHealthTick >= TICK_HEALTH_MS) {
        lastHealthTick = now;
        
        health.update(sensors.getData());
        
        // Save state periodically in case of crash
        if (fsm.stateChanged()) {
            persistence.saveState(static_cast<uint8_t>(fsm.getState()));
            sdLogger.logEvent("STATE_CHANGE", flightStateToString(fsm.getState()));
        }
    }
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================
void processCommand(const Command& cmd) {
    if (!cmd.valid) return;
    
    // Validate TEAM_ID
    if (cmd.teamId != TEAM_ID) {
        #if DEBUG_COMMANDS
        Serial.print(F("CMD rejected: wrong TEAM_ID "));
        Serial.println(cmd.teamId);
        #endif
        return;
    }
    
    #if DEBUG_COMMANDS
    Serial.print(F("Processing command type: "));
    Serial.println(static_cast<int>(cmd.type));
    #endif
    
    switch (cmd.type) {
        case CommandType::CX:
            // Telemetry on/off
            if (strcmp(cmd.arg1, "ON") == 0) {
                telemetryEnabled = true;
                strncpy(cmdEcho, "CXON", CMD_ECHO_SIZE);
            } else if (strcmp(cmd.arg1, "OFF") == 0) {
                telemetryEnabled = false;
                strncpy(cmdEcho, "CXOFF", CMD_ECHO_SIZE);
            }
            break;
            
        case CommandType::ST:
            // Set time
            if (strcmp(cmd.arg1, "GPS") == 0) {
                // Sync time from GPS
                const GPSData& gps = sensors.getData().gps;
                if (gps.valid) {
                    missionTimeOffset = gps.hour * 3600 + gps.minute * 60 + gps.second;
                    missionTimeBase = millis();
                }
                strncpy(cmdEcho, "STGPS", CMD_ECHO_SIZE);
            } else {
                // Parse time string hh:mm:ss
                int h, m, s;
                if (sscanf(cmd.arg1, "%d:%d:%d", &h, &m, &s) == 3) {
                    missionTimeOffset = h * 3600 + m * 60 + s;
                    missionTimeBase = millis();
                }
                // Format echo without colons
                snprintf(cmdEcho, CMD_ECHO_SIZE, "ST%s", cmd.arg1);
                // Remove colons from echo
                char* p = cmdEcho;
                char* q = cmdEcho;
                while (*p) {
                    if (*p != ':') *q++ = *p;
                    p++;
                }
                *q = '\0';
            }
            break;
            
        case CommandType::SIM:
            // Simulation mode control
            if (strcmp(cmd.arg1, "ENABLE") == 0) {
                simMode.enable();
                strncpy(cmdEcho, "SIMENABLE", CMD_ECHO_SIZE);
            } else if (strcmp(cmd.arg1, "ACTIVATE") == 0) {
                simMode.activate();
                strncpy(cmdEcho, "SIMACTIVATE", CMD_ECHO_SIZE);
                persistence.saveMode('S');
            } else if (strcmp(cmd.arg1, "DISABLE") == 0) {
                simMode.disable();
                strncpy(cmdEcho, "SIMDISABLE", CMD_ECHO_SIZE);
                persistence.saveMode('F');
            }
            break;
            
        case CommandType::SIMP:
            // Simulated pressure (only when activated)
            if (simMode.isActivated()) {
                simMode.setPressure(cmd.numericArg);
                snprintf(cmdEcho, CMD_ECHO_SIZE, "SP%ld", cmd.numericArg);
            }
            break;
            
        case CommandType::CAL:
            // Calibrate altitude to zero
            sensors.calibrateGround();
            // Also reset packet count and prepare for flight
            packetCount = 0;
            fsm.setState(FlightState::LAUNCH_PAD);
            servos.resetActuatedFlags();
            persistence.saveAll(packetCount, simMode.isActivated() ? 'S' : 'F',
                               static_cast<uint8_t>(FlightState::LAUNCH_PAD),
                               0.0f, sensors.getGroundPressure(), 0, cmdEcho);
            strncpy(cmdEcho, "CAL", CMD_ECHO_SIZE);
            break;
            
        case CommandType::MEC:
            // Mechanism control
            {
                bool on = (strcmp(cmd.arg2, "ON") == 0);
                if (strcmp(cmd.arg1, "SEP") == 0) {
                    if (on) servos.actuate(ServoId::SEPARATION, SERVO_SEP_OPEN);
                    else servos.setPosition(ServoId::SEPARATION, SERVO_SEP_CLOSED);
                } else if (strcmp(cmd.arg1, "EGG") == 0) {
                    if (on) servos.actuate(ServoId::EGG_RELEASE, SERVO_EGG_OPEN);
                    else servos.setPosition(ServoId::EGG_RELEASE, SERVO_EGG_CLOSED);
                } else if (strcmp(cmd.arg1, "ARM") == 0) {
                    servos.setArmed(on);
                }
                snprintf(cmdEcho, CMD_ECHO_SIZE, "MEC%s%s", cmd.arg1, cmd.arg2);
            }
            break;
            
        default:
            break;
    }
    
    // Save command echo
    persistence.saveCmdEcho(cmdEcho);
}

// ============================================================================
// BUILD TELEMETRY FRAME
// ============================================================================
TelemetryFrame buildTelemetryFrame() {
    TelemetryFrame frame;
    const SensorData& data = sensors.getData();
    
    // Field 1: TEAM_ID
    frame.teamId = TEAM_ID;
    
    // Field 2: MISSION_TIME
    uint32_t elapsed = (millis() - missionTimeBase) / 1000;
    uint32_t totalSeconds = missionTimeOffset + elapsed;
    frame.missionHour = (totalSeconds / 3600) % 24;
    frame.missionMinute = (totalSeconds / 60) % 60;
    frame.missionSecond = totalSeconds % 60;
    
    // Field 3: PACKET_COUNT
    frame.packetCount = packetCount;
    
    // Field 4: MODE
    frame.mode = simMode.isActivated() ? 'S' : 'F';
    
    // Field 5: STATE
    frame.state = fsm.getState();
    
    // Field 6: ALTITUDE
    frame.altitude = data.pressure.altitude_m;
    
    // Field 7: TEMPERATURE
    frame.temperature = data.pressure.temperature_C;
    
    // Field 8: PRESSURE
    frame.pressure = data.pressure.pressure_kPa;
    
    // Field 9: VOLTAGE
    frame.voltage = data.power.voltage_V;
    
    // Field 10: CURRENT
    frame.current = data.power.current_A;
    
    // Fields 11-13: GYRO
    frame.gyroR = data.imu.gyroR;
    frame.gyroP = data.imu.gyroP;
    frame.gyroY = data.imu.gyroY;
    
    // Fields 14-16: ACCEL (deg/s² per spec - unusual but per mission guide)
    frame.accelR = data.imu.accelR;
    frame.accelP = data.imu.accelP;
    frame.accelY = data.imu.accelY;
    
    // Field 17: GPS_TIME
    frame.gpsHour = data.gps.hour;
    frame.gpsMinute = data.gps.minute;
    frame.gpsSecond = data.gps.second;
    
    // Field 18: GPS_ALTITUDE
    frame.gpsAltitude = data.gps.altitude_m;
    
    // Field 19: GPS_LATITUDE
    frame.gpsLatitude = data.gps.latitude;
    
    // Field 20: GPS_LONGITUDE
    frame.gpsLongitude = data.gps.longitude;
    
    // Field 21: GPS_SATS
    frame.gpsSats = data.gps.satellites;
    
    // Field 22: CMD_ECHO
    strncpy(frame.cmdEcho, cmdEcho, CMD_ECHO_SIZE);
    
    return frame;
}

// ============================================================================
// HANDLE ACTUATIONS TRIGGERED BY FSM
// ============================================================================
void handleActuations() {
    FlightState state = fsm.getState();
    const SensorData& data = sensors.getData();
    
    // PROBE_RELEASE state: Payload separates from container
    if (state == FlightState::PROBE_RELEASE && 
        data.pressure.altitude_m <= data.peakAltitude * PAYLOAD_RELEASE_RATIO) {
        if (servos.actuate(ServoId::SEPARATION, SERVO_SEP_OPEN)) {
            sdLogger.logEvent("ACTUATION", "SEPARATION");
            persistence.saveActuatedFlags(servos.getActuatedFlags());
            #if DEBUG_FSM
            Serial.println(F(">>> SEPARATION ACTUATED"));
            #endif
        }
    }
    
    // PAYLOAD_RELEASE state: Egg released at 2m AGL
    if (state == FlightState::PAYLOAD_RELEASE && 
        data.pressure.altitude_m <= ALT_EGG_RELEASE) {
        if (servos.actuate(ServoId::EGG_RELEASE, SERVO_EGG_OPEN)) {
            sdLogger.logEvent("ACTUATION", "EGG_RELEASE");
            persistence.saveActuatedFlags(servos.getActuatedFlags());
            #if DEBUG_FSM
            Serial.println(F(">>> EGG RELEASED"));
            #endif
        }
    }
}
