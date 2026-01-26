/**
 * @file config.h
 * @brief Configuration constants for CanSat 2026 Flight Software
 * @team LeoNUS
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ============================================================================
// TEAM CONFIGURATION
// ============================================================================
#ifndef TEAM_ID
#define TEAM_ID 1000  // Replace with actual assigned team ID
#endif

// ============================================================================
// PIN ASSIGNMENTS - Teensy 4.1
// ============================================================================

// I2C Bus (Wire)
#define PIN_I2C_SDA         18
#define PIN_I2C_SCL         19

// SPI Bus (optional for sensors)
#define PIN_SPI_MOSI_ALT    11
#define PIN_SPI_MISO_ALT    12
#define PIN_SPI_SCK_ALT     13

// GPS UART (Serial1)
#define PIN_GPS_TX          1
#define PIN_GPS_RX          0

// XBee UART (Serial2)
#define PIN_XBEE_TX         8
#define PIN_XBEE_RX         7

// SD Card (built-in on Teensy 4.1)
#define PIN_SD_CS           BUILTIN_SDCARD

// Servo Outputs
#define PIN_SERVO_SEP       2   // Container separation servo
#define PIN_SERVO_EGG       3   // Egg/payload release servo
#define PIN_SERVO_LEFT      4   // Paraglider left control
#define PIN_SERVO_RIGHT     5   // Paraglider right control

// Status LEDs
#define PIN_LED_STATUS      13  // Built-in LED
#define PIN_LED_GPS         6   // GPS lock indicator (optional)

// Power Switch/Indicator
#define PIN_POWER_SENSE     A0  // Optional power sense

// ============================================================================
// I2C ADDRESSES
// ============================================================================
#define I2C_ADDR_MS5611     0x77  // Pressure sensor (can be 0x76)
#define I2C_ADDR_MPU9250    0x68  // IMU (can be 0x69)
#define I2C_ADDR_INA219     0x40  // Power monitor

// ============================================================================
// TIMING CONFIGURATION (milliseconds)
// ============================================================================
#define TICK_SENSOR_MS      100   // 10 Hz sensor sampling
#define TICK_GUIDANCE_MS    50    // 20 Hz guidance loop
#define TICK_TELEMETRY_MS   1000  // 1 Hz telemetry transmission
#define TICK_LOG_MS         1000  // 1 Hz SD logging
#define TICK_HEALTH_MS      500   // 2 Hz health check

// ============================================================================
// FLIGHT PARAMETERS
// ============================================================================
// OpenRocket Simulation Values (cansat_2025.ork):
//   Apogee: 503m, Max velocity: 107m/s, Max accel: 66.9m/s²
//   Rocket mass: 4305g with motor
//
// Competition: Monterey, VA (June 4-7, 2026), Elevation: ~760m

// Expected peak altitude from OpenRocket
#define EXPECTED_APOGEE_M       503.0f  // From OpenRocket simulation

// Altitude thresholds (meters)
#define ALT_LAUNCH_THRESHOLD    20.0f   // Detect launch
#define ALT_VELOCITY_LAUNCH     5.0f    // m/s vertical velocity for launch
#define ALT_LANDED_THRESHOLD    2.0f    // Near ground for landing detection
#define ALT_EGG_RELEASE         2.0f    // Egg release altitude AGL

// Payload release at 80% of peak altitude
#define PAYLOAD_RELEASE_RATIO   0.80f

// Vertical velocity thresholds (m/s)
#define VEL_APOGEE_THRESHOLD    0.0f    // Zero crossing for apogee
#define VEL_LANDED_THRESHOLD    0.5f    // Movement threshold for landed

// Timing thresholds
#define APOGEE_CONFIRM_SAMPLES  3       // Consecutive samples to confirm apogee
#define LANDED_CONFIRM_MS       5000    // Time with low movement to confirm landing

// Descent parameters (for calculations)
#define PAYLOAD_MASS_KG         0.5735f // 573.5g (Payload + Wing)
#define CONTAINER_MASS_KG       0.4265f // 426.5g
#define TOTAL_MASS_KG           1.0000f // 1000g Max (Requirement)
#define TARGET_DESCENT_RATE     5.0f    // m/s (Requirement: 5 ± 3 m/s)

// ============================================================================
// SERVO CONFIGURATION
// ============================================================================
// Servo Setup (Team LeoNUS 2026):
//   - Winch Servo 1: Para-glider left brake (continuous rotation + AS5600 encoder)
//   - Winch Servo 2: Para-glider right brake (continuous rotation + AS5600 encoder)
//   - CanSat Release Servo: MG996R - separates payload from container at 80% peak
//   - Egg Release Servo: MG996R - releases egg at 2m AGL
//
// PWM Range for MG996R: 1000-2000μs, center at 1500μs
// ============================================================================
#define SERVO_PWM_MIN       1000  // Minimum pulse width (μs)
#define SERVO_PWM_MAX       2000  // Maximum pulse width (μs)
#define SERVO_PWM_CENTER    1500  // Center position (μs)

// Servo positions for actuations
#define SERVO_SEP_CLOSED    1000
#define SERVO_SEP_OPEN      2000
#define SERVO_EGG_CLOSED    1000
#define SERVO_EGG_OPEN      2000

// Actuation timing
#define ACTUATION_DELAY_MS  100   // Delay between consecutive actuations

// ============================================================================
// WINCH SERVO CONFIGURATION (Para-glider control)
// ============================================================================
// Servo Model: SPT5525LV-360 (Continuous Rotation)
// - Neutral point: 1500 μs
// - Pulse width: 500 - 2500 μs
// - Operating speed: 55 RPM @ 6.0V
// - Deadband: 4 μs
// - Weight: 63g
//
// Winch servos controlled by position feedback from AS5600 magnetic encoders
// via I2C multiplexer (TCA9548A)

// Winch servo pins (PWM output)
#define PIN_WINCH_LEFT      4     // Left brake winch servo
#define PIN_WINCH_RIGHT     5     // Right brake winch servo

// AS5600 encoder I2C configuration
#define AS5600_I2C_ADDRESS  0x36  // Fixed address for AS5600
#define TCA9548A_ADDRESS    0x70  // I2C multiplexer address
#define WINCH_LEFT_MUX_CH   0     // Mux channel for left encoder
#define WINCH_RIGHT_MUX_CH  1     // Mux channel for right encoder

// Winch position limits (in degrees)
#define WINCH_MIN_ANGLE     0.0f    // Fully released
#define WINCH_MAX_ANGLE     180.0f  // Fully pulled (max brake)
#define WINCH_CENTER_ANGLE  90.0f   // Neutral position

// Winch drum specs (for line speed calculation)
// Line speed = π × diameter × RPM / 60
// At 55 RPM, 10mm drum: ~28.8 mm/s line speed
#define WINCH_DRUM_DIA_MM   10.0f   // Effective diameter with line buildup
#define WINCH_LINE_SPEED    0.029f  // Approx m/s at full speed

// Winch PID control parameters
#define WINCH_KP            2.0f    // Proportional gain
#define WINCH_KI            0.0f    // Integral gain (start at 0)
#define WINCH_KD            0.1f    // Derivative gain
#define WINCH_DEADBAND      2.0f    // Degrees - stop when within this range

// SPT5525LV-360 Continuous rotation servo PWM (μs)
#define WINCH_STOP          1500    // Stop (neutral)
#define WINCH_CW_FULL       2500    // Full clockwise
#define WINCH_CCW_FULL      500     // Full counter-clockwise
#define WINCH_PWM_MIN       500     // Min pulse width
#define WINCH_PWM_MAX       2500    // Max pulse width

// ============================================================================
// COMMUNICATION CONFIGURATION
// ============================================================================
#define SERIAL_DEBUG_BAUD   115200
#define GPS_BAUD            9600
#define XBEE_BAUD           9600

// Command buffer sizes
#define CMD_BUFFER_SIZE     256   // Ring buffer size for command RX
#define CMD_MAX_LENGTH      64    // Maximum command string length
#define CMD_ECHO_SIZE       32    // CMD_ECHO field size

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================

// MS5611 oversampling (affects speed vs precision)
#define MS5611_OSR          4096  // Highest precision

// GPS minimum satellites for valid fix
#define GPS_MIN_SATS        4

// INA219 shunt resistor (Ohms)
#define INA219_SHUNT_OHMS   0.1f

// ============================================================================
// PERSISTENCE (EEPROM) CONFIGURATION
// ============================================================================
#define EEPROM_MAGIC        0xCAFE2026
#define EEPROM_BASE_ADDR    0

// ============================================================================
// SD LOGGING CONFIGURATION
// ============================================================================
#define LOG_BUFFER_SIZE     512   // Write buffer size
#define LOG_FLUSH_INTERVAL  5000  // Flush to SD every 5 seconds

// ============================================================================
// GUIDANCE CONFIGURATION
// ============================================================================

// Target coordinates (Competition: Monterey, VA)
// Drop Zone: 38°22'33"N 79°36'28"W (Decimal: 38.37583, -79.60778)
// Elevation: ~817.5 m
#define TARGET_LATITUDE     38.37583f   // degrees N
#define TARGET_LONGITUDE    -79.60778f  // degrees W

// Steering gains (PID)
#define GUIDANCE_KP         0.8f    // Proportional gain
#define GUIDANCE_KI         0.05f   // Integral gain (wind drift correction)
#define GUIDANCE_KD         0.3f    // Derivative gain (damping)
#define GUIDANCE_INTEGRAL_MAX 30.0f // Anti-windup limit (degrees*seconds)
#define GUIDANCE_MAX_TURN   30.0f   // Maximum steering angle (degrees)
#define GUIDANCE_RATE_LIMIT 15.0f   // Rate limit (deg/s)

// ============================================================================
// ADVANCED CONTROL (Uncomment to enable)
// ============================================================================
// #define USE_ADAPTIVE_PID    // Enable Gain Scheduling based on error magnitude
// #define USE_KALMAN_FILTER   // Enable Altitude fusion (Baro + Accel)

// ============================================================================
// DEBUG FLAGS
// ============================================================================
#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL        1       // Enable USB serial debug output
#endif

#define DEBUG_SENSORS       0       // Verbose sensor debug
#define DEBUG_FSM           1       // FSM state transitions
#define DEBUG_COMMANDS      1       // Command parsing
#define DEBUG_TELEMETRY     0       // Telemetry formatting

#endif // CONFIG_H
