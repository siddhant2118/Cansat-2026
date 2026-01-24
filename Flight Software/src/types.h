/**
 * @file types.h
 * @brief Core data structures for CanSat 2026 Flight Software
 * @team LeoNUS
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "config.h"

// ============================================================================
// FLIGHT STATE ENUMERATION
// Per Mission Guide: LAUNCH_PAD, ASCENT, APOGEE, DESCENT, 
//                    PROBE_RELEASE, PAYLOAD_RELEASE, LANDED
// ============================================================================
enum class FlightState : uint8_t {
    LAUNCH_PAD = 0,
    ASCENT,
    APOGEE,
    DESCENT,
    PROBE_RELEASE,      // 80% peak altitude - payload separates from container
    PAYLOAD_RELEASE,    // 2m AGL - egg/instrument released
    LANDED
};

// State string lookup (for telemetry)
inline const char* flightStateToString(FlightState state) {
    switch (state) {
        case FlightState::LAUNCH_PAD:       return "LAUNCH_PAD";
        case FlightState::ASCENT:           return "ASCENT";
        case FlightState::APOGEE:           return "APOGEE";
        case FlightState::DESCENT:          return "DESCENT";
        case FlightState::PROBE_RELEASE:    return "PROBE_RELEASE";
        case FlightState::PAYLOAD_RELEASE:  return "PAYLOAD_RELEASE";
        case FlightState::LANDED:           return "LANDED";
        default:                            return "UNKNOWN";
    }
}

// ============================================================================
// SIMULATION MODE STATE
// ============================================================================
enum class SimState : uint8_t {
    DISABLED = 0,
    ENABLED,
    ACTIVATED
};

// ============================================================================
// COMMAND TYPES
// ============================================================================
enum class CommandType : uint8_t {
    NONE = 0,
    CX,         // Telemetry on/off
    ST,         // Set time
    SIM,        // Simulation mode control
    SIMP,       // Simulated pressure
    CAL,        // Calibrate altitude
    MEC,        // Mechanism control
    UNKNOWN
};

// ============================================================================
// SENSOR DATA STRUCTURES
// ============================================================================

// Pressure sensor data (MS5611)
struct PressureData {
    float pressure_kPa;     // Pressure in kPa
    float temperature_C;    // Temperature in °C
    float altitude_m;       // Calculated altitude in meters AGL
    bool valid;             // Data validity flag
    uint32_t timestamp;     // Sample timestamp (millis)
};

// IMU data (MPU9250)
struct IMUData {
    float gyroR, gyroP, gyroY;      // Gyro rates (deg/s)
    float accelR, accelP, accelY;   // Acceleration (deg/s² per spec, unusual)
    float accelX, accelY_raw, accelZ; // Raw accel (m/s²) for internal use
    bool valid;
    uint32_t timestamp;
};

// GPS data (NEO-M8N)
struct GPSData {
    float latitude;         // Degrees North
    float longitude;        // Degrees West (note: stored as positive for west)
    float altitude_m;       // Altitude MSL in meters
    uint8_t hour, minute, second;   // UTC time
    uint8_t satellites;     // Number of satellites tracked
    float hdop;             // Horizontal dilution of precision
    float courseOverGround; // Heading in degrees
    float speed_mps;        // Speed in m/s
    bool valid;             // Fix validity
    bool timeSynced;        // Time has been synced from GPS
    uint32_t timestamp;
};

// Power data (INA219)
struct PowerData {
    float voltage_V;        // Bus voltage in volts
    float current_A;        // Current in amperes
    float power_W;          // Power in watts
    bool valid;
    uint32_t timestamp;
};

// ============================================================================
// AGGREGATED SENSOR DATA
// ============================================================================
struct SensorData {
    PressureData pressure;
    IMUData imu;
    GPSData gps;
    PowerData power;
    
    // Derived values
    float verticalVelocity; // m/s, computed from altitude history
    float peakAltitude;     // Maximum altitude seen
    
    // Health flags
    bool pressureOk;
    bool imuOk;
    bool gpsOk;
    bool powerOk;
};

// ============================================================================
// TELEMETRY FRAME
// Per Mission Guide: 22 required fields
// ============================================================================
struct TelemetryFrame {
    // Field 1: TEAM_ID
    uint16_t teamId;
    
    // Field 2: MISSION_TIME (hh:mm:ss)
    uint8_t missionHour;
    uint8_t missionMinute;
    uint8_t missionSecond;
    
    // Field 3: PACKET_COUNT (persisted across reset)
    uint32_t packetCount;
    
    // Field 4: MODE ('F' or 'S')
    char mode;
    
    // Field 5: STATE
    FlightState state;
    
    // Field 6: ALTITUDE (m AGL, 0.1m resolution)
    float altitude;
    
    // Field 7: TEMPERATURE (°C, 0.1° resolution)
    float temperature;
    
    // Field 8: PRESSURE (kPa, 0.1 kPa resolution)
    float pressure;
    
    // Field 9: VOLTAGE (V, 0.1V resolution)
    float voltage;
    
    // Field 10: CURRENT (A, 0.01A resolution)
    float current;
    
    // Fields 11-13: GYRO_R, GYRO_P, GYRO_Y (deg/s)
    float gyroR, gyroP, gyroY;
    
    // Fields 14-16: ACCEL_R, ACCEL_P, ACCEL_Y (deg/s² per spec)
    float accelR, accelP, accelY;
    
    // Field 17: GPS_TIME (hh:mm:ss UTC)
    uint8_t gpsHour;
    uint8_t gpsMinute;
    uint8_t gpsSecond;
    
    // Field 18: GPS_ALTITUDE (m MSL, 0.1m resolution)
    float gpsAltitude;
    
    // Field 19: GPS_LATITUDE (deg N, 0.0001° resolution)
    float gpsLatitude;
    
    // Field 20: GPS_LONGITUDE (deg W, 0.0001° resolution)
    float gpsLongitude;
    
    // Field 21: GPS_SATS
    uint8_t gpsSats;
    
    // Field 22: CMD_ECHO (no commas!)
    char cmdEcho[CMD_ECHO_SIZE];
};

// ============================================================================
// COMMAND STRUCTURE
// ============================================================================
struct Command {
    CommandType type;
    uint16_t teamId;
    char arg1[16];      // First argument (e.g., ON/OFF, ENABLE/ACTIVATE)
    char arg2[16];      // Second argument (e.g., device name for MEC)
    int32_t numericArg; // Numeric argument (e.g., pressure for SIMP)
    bool valid;
};

// ============================================================================
// SERVO IDENTIFIERS
// ============================================================================
enum class ServoId : uint8_t {
    SEPARATION = 0,     // Container separation
    EGG_RELEASE,        // Egg/instrument release
    LEFT_CONTROL,       // Paraglider left
    RIGHT_CONTROL,      // Paraglider right
    NUM_SERVOS
};

// ============================================================================
// ACTUATOR STATE
// ============================================================================
struct ActuatorState {
    bool armed;                                     // Global arm flag
    bool fired[static_cast<uint8_t>(ServoId::NUM_SERVOS)];  // One-shot flags
    uint16_t positions[static_cast<uint8_t>(ServoId::NUM_SERVOS)]; // Current positions
    uint32_t lastActuationTime;                     // For sequencing
};

// ============================================================================
// PERSISTENCE DATA (EEPROM layout)
// ============================================================================
struct PersistentData {
    uint32_t magic;             // Validity marker
    uint32_t packetCount;       // Packet count
    uint8_t mode;               // 'F' or 'S'
    uint8_t state;              // FlightState enum value
    float peakAltitude;         // Peak altitude for 80% calc
    float groundPressure;       // Calibrated ground pressure
    uint8_t actuatedFlags;      // Bit field of fired actuators
    char cmdEcho[CMD_ECHO_SIZE];// Last command echo
    uint32_t checksum;          // Simple checksum for validation
};

// ============================================================================
// HEALTH STATUS FLAGS
// ============================================================================
struct HealthStatus {
    bool sensorPressureFail;
    bool sensorIMUFail;
    bool sensorGPSFail;
    bool sensorPowerFail;
    bool sdCardFail;
    bool radioTimeout;
    bool lowBattery;
    bool actuatorFault;
    
    uint32_t lastGPSUpdate;
    uint32_t lastRadioRx;
    
    // Degradation flags
    bool gpsAltitudeFallback;   // Using GPS altitude instead of pressure
    bool guidanceNeutral;       // Steering disabled due to bad GPS
};

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Clamp value between min and max
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// Array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif // TYPES_H
