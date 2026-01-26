/**
 * @file telemetry.cpp
 * @brief Telemetry Formatting and Transmission Implementation
 * @team LeoNUS
 * 
 * Format per Mission Guide:
 * TEAM_ID,MISSION_TIME,PACKET_COUNT,MODE,STATE,ALTITUDE,
 * TEMPERATURE,PRESSURE,VOLTAGE,CURRENT,GYRO_R,GYRO_P,GYRO_Y,
 * ACCEL_R,ACCEL_P,ACCEL_Y,GPS_TIME,GPS_ALTITUDE,GPS_LATITUDE,
 * GPS_LONGITUDE,GPS_SATS,CMD_ECHO\r
 */

#include "telemetry.h"

// XBee Serial port
#define XBEE_SERIAL Serial2

TelemetryManager::TelemetryManager() {
    memset(_buffer, 0, sizeof(_buffer));
}

void TelemetryManager::begin() {
    XBEE_SERIAL.begin(XBEE_BAUD);
}

void TelemetryManager::transmit(const TelemetryFrame& frame) {
    size_t len = format(frame, _buffer, sizeof(_buffer));
    
    if (len > 0) {
        XBEE_SERIAL.write(_buffer, len);
        
        #if DEBUG_TELEMETRY
        Serial.print(F("TX: "));
        Serial.print(_buffer);
        #endif
    }
}

size_t TelemetryManager::format(const TelemetryFrame& frame, char* buffer, size_t bufferSize) {
    // Format each field per mission guide specifications
    
    int written = snprintf(buffer, bufferSize,
        // Field 1: TEAM_ID
        "%u,"
        // Field 2: MISSION_TIME (hh:mm:ss)
        "%02u:%02u:%02u,"
        // Field 3: PACKET_COUNT
        "%lu,"
        // Field 4: MODE
        "%c,"
        // Field 5: STATE
        "%s,"
        // Field 6: ALTITUDE (0.1m resolution)
        "%.1f,"
        // Field 7: TEMPERATURE (0.1° resolution)
        "%.1f,"
        // Field 8: PRESSURE (0.1 kPa resolution)
        "%.1f,"
        // Field 9: VOLTAGE (0.1V resolution)
        "%.1f,"
        // Field 10: CURRENT (0.01A resolution)
        "%.2f,"
        // Field 11-13: GYRO_R, GYRO_P, GYRO_Y
        "%.1f,%.1f,%.1f,"
        // Field 14-16: ACCEL_R, ACCEL_P, ACCEL_Y
        "%.1f,%.1f,%.1f,"
        // Field 17: GPS_TIME (hh:mm:ss)
        "%02u:%02u:%02u,"
        // Field 18: GPS_ALTITUDE (0.1m resolution)
        "%.1f,"
        // Field 19: GPS_LATITUDE (0.0001° resolution)
        "%.4f,"
        // Field 20: GPS_LONGITUDE (0.0001° resolution)
        "%.4f,"
        // Field 21: GPS_SATS
        "%u,"
        // Field 22: CMD_ECHO (no commas!)
        "%s"
        // Carriage return terminator
        "\r",
        
        // Field values
        frame.teamId,
        frame.missionHour, frame.missionMinute, frame.missionSecond,
        frame.packetCount,
        frame.mode,
        flightStateToString(frame.state),
        frame.altitude,
        frame.temperature,
        frame.pressure,
        frame.voltage,
        frame.current,
        frame.gyroR, frame.gyroP, frame.gyroY,
        frame.accelR, frame.accelP, frame.accelY,
        frame.gpsHour, frame.gpsMinute, frame.gpsSecond,
        frame.gpsAltitude,
        frame.gpsLatitude,
        frame.gpsLongitude,
        frame.gpsSats,
        frame.cmdEcho
    );
    
    if (written < 0 || (size_t)written >= bufferSize) {
        // Error or truncation
        return 0;
    }
    
    return (size_t)written;
}
