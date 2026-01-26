/**
 * @file servo_controller.h
 * @brief Servo Controller with Safety Gating
 * @team LeoNUS
 */

#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include <Servo.h>
#include "../config.h"
#include "../types.h"

class ServoController {
public:
    ServoController();
    
    /**
     * @brief Initialize servo outputs
     */
    void begin();
    
    /**
     * @brief Arm servos for actuation
     * @param armed Enable/disable actuation
     */
    void setArmed(bool armed);
    
    /**
     * @brief Check if servos are armed
     */
    bool isArmed() const { return _armed; }
    
    /**
     * @brief Actuate a one-shot mechanism
     * @param servo Servo to actuate
     * @param position Target position
     * @return true if actuation occurred
     */
    bool actuate(ServoId servo, uint16_t position);
    
    /**
     * @brief Set servo position (for continuous control like steering)
     * @param servo Servo to control
     * @param position Target position
     */
    void setPosition(ServoId servo, uint16_t position);
    
    /**
     * @brief Get current servo positions
     */
    uint16_t getPosition(ServoId servo) const;
    
    /**
     * @brief Get one-shot fired flags as bitmask
     */
    uint8_t getActuatedFlags() const;
    
    /**
     * @brief Restore actuated flags from persistence
     */
    void restoreActuatedFlags(uint8_t flags);
    
    /**
     * @brief Reset all actuated flags (for CAL command)
     */
    void resetActuatedFlags();
    
private:
    Servo _servos[static_cast<uint8_t>(ServoId::NUM_SERVOS)];
    ActuatorState _state;
    bool _armed;
    
    int servoPin(ServoId id) const;
    bool isOneShot(ServoId id) const;
};

#endif // SERVO_CONTROLLER_H
