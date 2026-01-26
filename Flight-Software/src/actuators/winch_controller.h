/**
 * @file winch_controller.h
 * @brief Closed-Loop Winch Controller for Para-glider Steering
 * @team LeoNUS 2026
 * 
 * Uses AS5600 magnetic encoders for position feedback and
 * continuous rotation servos for line control.
 */

#ifndef WINCH_CONTROLLER_H
#define WINCH_CONTROLLER_H

#include <Arduino.h>
#include <Servo.h>
#include "../config.h"
#include "../sensors/as5600.h"

/**
 * @brief Winch position control with encoder feedback
 */
class WinchController {
public:
    WinchController();
    
    /**
     * @brief Initialize winch system
     * @return true if both encoders detected
     */
    bool begin();
    
    /**
     * @brief Update control loop (call at 20-50 Hz)
     * Should be called from main loop
     */
    void update();
    
    /**
     * @brief Set target position for a winch
     * @param isLeft true = left winch, false = right winch
     * @param degrees Target angle (0=released, 180=max pull)
     */
    void setTargetPosition(bool isLeft, float degrees);
    
    /**
     * @brief Set both winches for differential steering
     * @param leftDeg Left winch target (0-180)
     * @param rightDeg Right winch target (0-180)
     */
    void setSteeringPositions(float leftDeg, float rightDeg);
    
    /**
     * @brief Apply steering correction (-1.0 to +1.0)
     * @param steerValue -1.0 = full left, 0 = straight, +1.0 = full right
     */
    void applySteering(float steerValue);
    
    /**
     * @brief Get current position of a winch
     * @param isLeft true = left winch, false = right winch
     * @return Current angle in degrees (0-360)
     */
    float getCurrentPosition(bool isLeft);
    
    /**
     * @brief Get target position of a winch
     */
    float getTargetPosition(bool isLeft);
    
    /**
     * @brief Check if winches are at target (within deadband)
     */
    bool isAtTarget();
    
    /**
     * @brief Stop both winches immediately
     */
    void stop();
    
    /**
     * @brief Enable/disable closed-loop control
     */
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }
    
    /**
     * @brief Check if encoders are healthy
     */
    bool encodersHealthy() const { return _leftEncoderOk && _rightEncoderOk; }
    
private:
    // Encoders
    AS5600 _leftEncoder;
    AS5600 _rightEncoder;
    bool _leftEncoderOk;
    bool _rightEncoderOk;
    
    // Servos (continuous rotation)
    Servo _leftServo;
    Servo _rightServo;
    
    // Target positions (degrees)
    float _leftTarget;
    float _rightTarget;
    
    // Current positions (from encoders)
    float _leftCurrent;
    float _rightCurrent;
    
    // PID state
    float _leftIntegral;
    float _rightIntegral;
    float _leftPrevError;
    float _rightPrevError;
    unsigned long _lastUpdateTime;
    
    // Control state
    bool _enabled;
    
    // Internal methods
    int computePID(float error, float* integral, float* prevError, float dt);
    void setServoSpeed(Servo& servo, int speed);
};

#endif // WINCH_CONTROLLER_H
