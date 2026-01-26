/**
 * @file health.h
 * @brief Health Monitoring System
 * @team LeoNUS
 */

#ifndef HEALTH_H
#define HEALTH_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class HealthMonitor {
public:
    HealthMonitor();
    
    /**
     * @brief Initialize health monitor
     */
    void begin();
    
    /**
     * @brief Update health status
     * @param data Current sensor data
     */
    void update(const SensorData& data);
    
    /**
     * @brief Get current health status
     */
    const HealthStatus& getStatus() const { return _status; }
    
    /**
     * @brief Log an event
     * @param event Event description
     */
    void logEvent(const char* event);
    
    /**
     * @brief Set SD fail flag
     */
    void setSdFail(bool fail) { _status.sdCardFail = fail; }
    
    /**
     * @brief Check if system is healthy for flight
     */
    bool isFlightReady() const;
    
private:
    HealthStatus _status;
    
    // Timing
    uint32_t _lastGPSValid;
    uint32_t _lastRadioRx;
    
    // Thresholds
    static const uint32_t GPS_TIMEOUT_MS = 5000;
    static const uint32_t RADIO_TIMEOUT_MS = 10000;
    static const float LOW_BATTERY_V = 6.0f;
};

#endif // HEALTH_H
