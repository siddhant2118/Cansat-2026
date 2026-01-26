/**
 * @file winch_controller.cpp
 * @brief Closed-Loop Winch Controller Implementation
 * @team LeoNUS 2026
 */

#include "winch_controller.h"

WinchController::WinchController()
    : _leftEncoder(WINCH_LEFT_MUX_CH),
      _rightEncoder(WINCH_RIGHT_MUX_CH),
      _leftEncoderOk(false),
      _rightEncoderOk(false),
      _leftTarget(WINCH_CENTER_ANGLE),
      _rightTarget(WINCH_CENTER_ANGLE),
      _leftCurrent(0),
      _rightCurrent(0),
      _leftIntegral(0),
      _rightIntegral(0),
      _leftPrevError(0),
      _rightPrevError(0),
      _lastUpdateTime(0),
      _enabled(false)
{
}

bool WinchController::begin() {
    // Initialize I2C (should already be done in main)
    Wire.begin();
    
    // Initialize encoders
    _leftEncoderOk = _leftEncoder.begin(&Wire);
    _rightEncoderOk = _rightEncoder.begin(&Wire);
    
    // Attach continuous rotation servos
    _leftServo.attach(PIN_WINCH_LEFT);
    _rightServo.attach(PIN_WINCH_RIGHT);
    
    // Start at neutral (stopped)
    stop();
    
    _lastUpdateTime = millis();
    
    return _leftEncoderOk && _rightEncoderOk;
}

void WinchController::update() {
    if (!_enabled) {
        stop();
        return;
    }
    
    unsigned long now = millis();
    float dt = (now - _lastUpdateTime) / 1000.0f;
    if (dt < 0.01f) return;  // Minimum 10ms between updates
    
    _lastUpdateTime = now;
    
    // Read current positions from encoders
    if (_leftEncoderOk) {
        _leftCurrent = _leftEncoder.readDegrees();
    }
    if (_rightEncoderOk) {
        _rightCurrent = _rightEncoder.readDegrees();
    }
    
    // Compute errors
    float leftError = _leftTarget - _leftCurrent;
    float rightError = _rightTarget - _rightCurrent;
    
    // Handle angle wrap-around (e.g., 350° to 10° should be +20°, not -340°)
    if (leftError > 180) leftError -= 360;
    if (leftError < -180) leftError += 360;
    if (rightError > 180) rightError -= 360;
    if (rightError < -180) rightError += 360;
    
    // Check if within deadband
    int leftSpeed = 0;
    int rightSpeed = 0;
    
    if (abs(leftError) > WINCH_DEADBAND) {
        leftSpeed = computePID(leftError, &_leftIntegral, &_leftPrevError, dt);
    } else {
        _leftIntegral = 0;  // Reset integral when at target
    }
    
    if (abs(rightError) > WINCH_DEADBAND) {
        rightSpeed = computePID(rightError, &_rightIntegral, &_rightPrevError, dt);
    } else {
        _rightIntegral = 0;
    }
    
    // Apply to servos
    setServoSpeed(_leftServo, leftSpeed);
    setServoSpeed(_rightServo, rightSpeed);
}

int WinchController::computePID(float error, float* integral, float* prevError, float dt) {
    // PID calculation
    *integral += error * dt;
    
    // Anti-windup: limit integral
    float maxIntegral = 100.0f;
    if (*integral > maxIntegral) *integral = maxIntegral;
    if (*integral < -maxIntegral) *integral = -maxIntegral;
    
    float derivative = (error - *prevError) / dt;
    *prevError = error;
    
    float output = WINCH_KP * error + WINCH_KI * (*integral) + WINCH_KD * derivative;
    
    // Clamp to servo range (-500 to +500 from center)
    int speed = constrain((int)output, -500, 500);
    
    return speed;
}

void WinchController::setServoSpeed(Servo& servo, int speed) {
    // Continuous rotation servo: 1500 = stop, 1000 = full CCW, 2000 = full CW
    int pwm = WINCH_STOP + speed;
    pwm = constrain(pwm, WINCH_CCW_FULL, WINCH_CW_FULL);
    servo.writeMicroseconds(pwm);
}

void WinchController::setTargetPosition(bool isLeft, float degrees) {
    degrees = constrain(degrees, WINCH_MIN_ANGLE, WINCH_MAX_ANGLE);
    if (isLeft) {
        _leftTarget = degrees;
    } else {
        _rightTarget = degrees;
    }
}

void WinchController::setSteeringPositions(float leftDeg, float rightDeg) {
    setTargetPosition(true, leftDeg);
    setTargetPosition(false, rightDeg);
}

void WinchController::applySteering(float steerValue) {
    // steerValue: -1.0 = full left, 0 = straight, +1.0 = full right
    steerValue = constrain(steerValue, -1.0f, 1.0f);
    
    // Differential steering:
    // Left turn = pull left brake (increase left winch angle)
    // Right turn = pull right brake (increase right winch angle)
    
    float baseAngle = WINCH_CENTER_ANGLE;
    float maxPull = WINCH_MAX_ANGLE - WINCH_CENTER_ANGLE;
    
    float leftPull = 0;
    float rightPull = 0;
    
    if (steerValue < 0) {
        // Turn left: pull left brake
        leftPull = -steerValue * maxPull;
    } else {
        // Turn right: pull right brake
        rightPull = steerValue * maxPull;
    }
    
    setTargetPosition(true, baseAngle + leftPull);
    setTargetPosition(false, baseAngle + rightPull);
}

float WinchController::getCurrentPosition(bool isLeft) {
    return isLeft ? _leftCurrent : _rightCurrent;
}

float WinchController::getTargetPosition(bool isLeft) {
    return isLeft ? _leftTarget : _rightTarget;
}

bool WinchController::isAtTarget() {
    float leftError = abs(_leftTarget - _leftCurrent);
    float rightError = abs(_rightTarget - _rightCurrent);
    
    // Handle wrap-around
    if (leftError > 180) leftError = 360 - leftError;
    if (rightError > 180) rightError = 360 - rightError;
    
    return (leftError <= WINCH_DEADBAND) && (rightError <= WINCH_DEADBAND);
}

void WinchController::stop() {
    _leftServo.writeMicroseconds(WINCH_STOP);
    _rightServo.writeMicroseconds(WINCH_STOP);
    _leftIntegral = 0;
    _rightIntegral = 0;
}
