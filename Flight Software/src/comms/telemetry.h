/**
 * @file telemetry.h
 * @brief Telemetry Formatting and Transmission
 * @team LeoNUS
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class TelemetryManager {
public:
    TelemetryManager();
    
    /**
     * @brief Initialize XBee serial port
     */
    void begin();
    
    /**
     * @brief Transmit telemetry frame
     * @param frame Telemetry data to transmit
     */
    void transmit(const TelemetryFrame& frame);
    
    /**
     * @brief Format frame to buffer without transmitting
     * @param frame Telemetry data
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes written
     */
    size_t format(const TelemetryFrame& frame, char* buffer, size_t bufferSize);
    
private:
    // Formatted packet buffer
    char _buffer[256];
};

#endif // TELEMETRY_H
