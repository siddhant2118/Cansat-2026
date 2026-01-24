/**
 * @file guidance.h
 * @brief Paraglider Guidance Controller
 * @team LeoNUS
 */

#ifndef GUIDANCE_H
#define GUIDANCE_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class GuidanceController {
public:
    GuidanceController();
    
    /**
     * @brief Initialize guidance system
     */
    void begin();
    
    /**
     * @brief Set target coordinates
     * @param lat Target latitude (degrees N)
     * @param lon Target longitude (degrees W)
     */
    void setTarget(float lat, float lon);
    
    /**
     * @brief Update guidance calculations
     * @param gps Current GPS data
     * @param imu Current IMU data
     */
    void update(const GPSData& gps, const IMUData& imu);
    
    /**
     * @brief Get servo commands
     * @param leftCmd Output: left servo offset from center
     * @param rightCmd Output: right servo offset from center
     */
    void getServoCommands(int16_t& leftCmd, int16_t& rightCmd);
    
    /**
     * @brief Check if guidance is active
     */
    bool isActive() const { return _active; }
    
    /**
     * @brief Get current bearing to target
     */
    float getBearingToTarget() const { return _bearingToTarget; }
    
    /**
     * @brief Get current distance to target
     */
    float getDistanceToTarget() const { return _distanceToTarget; }
    
private:
    float _targetLat;
    float _targetLon;
    
    float _bearingToTarget;     // Degrees from north
    float _distanceToTarget;    // Meters
    float _crossTrackError;     // Meters
    
    float _currentHeading;      // From GPS COG or IMU
    float _steeringCommand;     // -1 to +1
    
    bool _active;
    
    // Rate limiting
    float _lastSteeringCommand;
    uint32_t _lastUpdateTime;
    
    // Calculations
    float calculateBearing(float lat1, float lon1, float lat2, float lon2);
    float calculateDistance(float lat1, float lon1, float lat2, float lon2);
    float normalizeAngle(float angle);
};

#endif // GUIDANCE_H
