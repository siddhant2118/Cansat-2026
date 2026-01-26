/**
 * @file servo_controller.cpp
 * @brief Servo Controller Implementation with Safety Gating
 * @team LeoNUS
 */

#include "servo_controller.h"

ServoController::ServoController()
    : _armed(false)
{
    memset(&_state, 0, sizeof(_state));
    
    // Initialize positions to closed/center
    _state.positions[static_cast<uint8_t>(ServoId::SEPARATION)] = SERVO_SEP_CLOSED;
    _state.positions[static_cast<uint8_t>(ServoId::EGG_RELEASE)] = SERVO_EGG_CLOSED;
    _state.positions[static_cast<uint8_t>(ServoId::LEFT_CONTROL)] = SERVO_PWM_CENTER;
    _state.positions[static_cast<uint8_t>(ServoId::RIGHT_CONTROL)] = SERVO_PWM_CENTER;
}

void ServoController::begin() {
    // Attach servos to their pins
    _servos[static_cast<uint8_t>(ServoId::SEPARATION)].attach(PIN_SERVO_SEP);
    _servos[static_cast<uint8_t>(ServoId::EGG_RELEASE)].attach(PIN_SERVO_EGG);
    _servos[static_cast<uint8_t>(ServoId::LEFT_CONTROL)].attach(PIN_SERVO_LEFT);
    _servos[static_cast<uint8_t>(ServoId::RIGHT_CONTROL)].attach(PIN_SERVO_RIGHT);
    
    // Set initial positions (safe/closed)
    for (uint8_t i = 0; i < static_cast<uint8_t>(ServoId::NUM_SERVOS); i++) {
        _servos[i].writeMicroseconds(_state.positions[i]);
    }
    
    #if DEBUG_SERIAL
    Serial.println(F("Servos initialized (NOT armed)"));
    #endif
}

void ServoController::setArmed(bool armed) {
    _armed = armed;
    _state.armed = armed;
    
    #if DEBUG_SERIAL
    Serial.print(F("Servos "));
    Serial.println(armed ? F("ARMED") : F("DISARMED"));
    #endif
}

bool ServoController::actuate(ServoId servo, uint16_t position) {
    uint8_t idx = static_cast<uint8_t>(servo);
    
    // Safety checks
    if (!_armed) {
        #if DEBUG_SERIAL
        Serial.println(F("Actuation blocked: not armed"));
        #endif
        return false;
    }
    
    // One-shot check for deployment mechanisms
    if (isOneShot(servo) && _state.fired[idx]) {
        #if DEBUG_SERIAL
        Serial.println(F("Actuation blocked: already fired"));
        #endif
        return false;
    }
    
    // Sequencing check - delay between actuations
    uint32_t now = millis();
    if (now - _state.lastActuationTime < ACTUATION_DELAY_MS) {
        #if DEBUG_SERIAL
        Serial.println(F("Actuation delayed: sequencing"));
        #endif
        return false;
    }
    
    // Clamp position
    position = CLAMP(position, SERVO_PWM_MIN, SERVO_PWM_MAX);
    
    // Execute actuation
    _servos[idx].writeMicroseconds(position);
    _state.positions[idx] = position;
    _state.lastActuationTime = now;
    
    // Mark one-shot as fired
    if (isOneShot(servo)) {
        _state.fired[idx] = true;
    }
    
    #if DEBUG_SERIAL
    Serial.print(F("Servo "));
    Serial.print(idx);
    Serial.print(F(" actuated to "));
    Serial.println(position);
    #endif
    
    return true;
}

void ServoController::setPosition(ServoId servo, uint16_t position) {
    uint8_t idx = static_cast<uint8_t>(servo);
    
    // Clamp position
    position = CLAMP(position, SERVO_PWM_MIN, SERVO_PWM_MAX);
    
    // Only control servos allow direct position setting
    if (!isOneShot(servo)) {
        _servos[idx].writeMicroseconds(position);
        _state.positions[idx] = position;
    }
}

uint16_t ServoController::getPosition(ServoId servo) const {
    return _state.positions[static_cast<uint8_t>(servo)];
}

uint8_t ServoController::getActuatedFlags() const {
    uint8_t flags = 0;
    for (uint8_t i = 0; i < static_cast<uint8_t>(ServoId::NUM_SERVOS); i++) {
        if (_state.fired[i]) {
            flags |= (1 << i);
        }
    }
    return flags;
}

void ServoController::restoreActuatedFlags(uint8_t flags) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(ServoId::NUM_SERVOS); i++) {
        _state.fired[i] = (flags & (1 << i)) != 0;
    }
    
    #if DEBUG_SERIAL
    Serial.print(F("Restored actuated flags: 0x"));
    Serial.println(flags, HEX);
    #endif
}

void ServoController::resetActuatedFlags() {
    for (uint8_t i = 0; i < static_cast<uint8_t>(ServoId::NUM_SERVOS); i++) {
        _state.fired[i] = false;
    }
}

int ServoController::servoPin(ServoId id) const {
    switch (id) {
        case ServoId::SEPARATION:   return PIN_SERVO_SEP;
        case ServoId::EGG_RELEASE:  return PIN_SERVO_EGG;
        case ServoId::LEFT_CONTROL: return PIN_SERVO_LEFT;
        case ServoId::RIGHT_CONTROL: return PIN_SERVO_RIGHT;
        default: return -1;
    }
}

bool ServoController::isOneShot(ServoId id) const {
    // Separation and egg release are one-shot mechanisms
    // Control surfaces are continuous
    return (id == ServoId::SEPARATION || id == ServoId::EGG_RELEASE);
}
