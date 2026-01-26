/**
 * @file guidance.h
 * @brief Paraglider Guidance Controller with PID Control
 * @team LeoNUS
 * 
 * Implements PID steering for wind disturbance rejection:
 * - P: Corrects current heading error
 * - I: Eliminates steady-state drift (constant wind)
 * - D: Dampens oscillations and overshoot
 */

#ifndef GUIDANCE_H
#define GUIDANCE_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

#ifdef USE_ADAPTIVE_PID
#include "adaptive_pid.h"
#endif

// Forward declaration or struct definition
#ifndef USE_ADAPTIVE_PID
/**
 * @brief PID Controller structure (Fixed Gains)
 */
struct PIDController {
    float kp;               // Proportional gain
    float ki;               // Integral gain
    float kd;               // Derivative gain
    
    float integral;         // Accumulated integral
    float prevError;        // Previous error for derivative
    float integralMax;      // Anti-windup limit
    
    float output;           // Current output
    
    void reset() {
        integral = 0;
        prevError = 0;
        output = 0;
    }
    
    float compute(float error, float dt) {
        if (dt <= 0) return output;
        
        // Proportional term
        float pTerm = kp * error;
        
        // Integral term with anti-windup
        integral += error * dt;
        integral = constrain(integral, -integralMax, integralMax);
        float iTerm = ki * integral;
        
        // Derivative term (on error)
        float derivative = (error - prevError) / dt;
        float dTerm = kd * derivative;
        prevError = error;
        
        // Combined output
        output = pTerm + iTerm + dTerm;
        return output;
    }
};
#endif

/**
 * @brief Paraglider Guidance Controller
 */
class GuidanceController {
public:
    GuidanceController();
    
    /**
     * @brief Initialize guidance system
     */
    void begin();
    
    /**
     * @brief Reset PID controllers (call after mode change)
     */
    void reset();
    
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
    
    /**
     * @brief Get current heading error
     */
    float getHeadingError() const { return _headingError; }
    
    /**
     * @brief Set PID gains (for tuning)
     */
    void setGains(float kp, float ki, float kd);
    
    /**
     * @brief Enable/disable trajectory prediction
     * When enabled, uses predicted future position instead of current position
     */
    void setTrajectoryPrediction(bool enabled, float lookaheadSeconds = 1.0f);
    
    /**
     * @brief Get predicted intercept point
     */
    void getPredictedPosition(float& lat, float& lon);
    
private:
    float _targetLat;
    float _targetLon;
    
    float _bearingToTarget;     // Degrees from north
    float _distanceToTarget;    // Meters
    float _headingError;        // Current heading error
    
    float _currentHeading;      // From GPS COG or IMU
    float _steeringCommand;     // -1 to +1
    
    bool _active;
    
    // PID controller for heading (Swappable)
    #ifdef USE_ADAPTIVE_PID
    AdaptivePID _headingPID;
    #else
    PIDController _headingPID;
    #endif
    
    // Timing
    uint32_t _lastUpdateTime;
    
    // Rate limiting for servo output
    float _lastOutput;
    
    // Calculations
    float calculateBearing(float lat1, float lon1, float lat2, float lon2);
    float calculateDistance(float lat1, float lon1, float lat2, float lon2);
    float normalizeAngle(float angle);
    
    // Trajectory prediction
    bool _trajectoryEnabled;
    float _lookaheadTime;       // Seconds to look ahead
    float _prevLat, _prevLon;   // Previous GPS position
    float _predLat, _predLon;   // Predicted future position
    bool _hasPrevPosition;
    
    void updateTrajectoryPrediction(const GPSData& gps, float dt);
};

#endif // GUIDANCE_H
