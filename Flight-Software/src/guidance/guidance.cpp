/**
 * @file guidance.cpp
 * @brief Paraglider Guidance Controller with PID Control
 * @team LeoNUS
 * 
 * PID steering for wind disturbance rejection:
 * 1. Calculate bearing to target
 * 2. Compare with current heading to get error
 * 3. Run error through PID controller
 * 4. Apply rate limiting and output to servos
 * 
 * Tuning tips:
 * - Start with Ki=0, Kd=0, increase Kp until oscillation
 * - Add Kd to dampen oscillation
 * - Add Ki to eliminate steady-state error (wind drift)
 */

#include "guidance.h"
#include <math.h>

// Earth radius in meters
#define EARTH_RADIUS_M 6371000.0f

// Degrees to radians
#define DEG_TO_RAD (3.14159265359f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265359f)

GuidanceController::GuidanceController()
    : _targetLat(TARGET_LATITUDE)
    , _targetLon(TARGET_LONGITUDE)
    , _bearingToTarget(0)
    , _distanceToTarget(0)
    , _headingError(0)
    , _currentHeading(0)
    , _steeringCommand(0)
    , _active(false)
    , _lastUpdateTime(0)
    , _lastOutput(0)
{
    // Initialize PID with default gains
    _headingPID.kp = GUIDANCE_KP;
    _headingPID.ki = GUIDANCE_KI;
    _headingPID.kd = GUIDANCE_KD;
    _headingPID.integralMax = GUIDANCE_INTEGRAL_MAX;
    _headingPID.reset();
    
    // Trajectory prediction defaults
    _trajectoryEnabled = true;  // Enable by default
    _lookaheadTime = 1.0f;      // 1 second lookahead
    _prevLat = 0;
    _prevLon = 0;
    _predLat = 0;
    _predLon = 0;
    _hasPrevPosition = false;
}

void GuidanceController::begin() {
    _active = false;
    _steeringCommand = 0;
    _lastOutput = 0;
    _headingPID.reset();
    
    #if DEBUG_SERIAL
    Serial.println(F("Guidance: PID controller initialized"));
    Serial.print(F("  Kp="));
    Serial.print(_headingPID.kp);
    Serial.print(F(" Ki="));
    Serial.print(_headingPID.ki);
    Serial.print(F(" Kd="));
    Serial.println(_headingPID.kd);
    #endif
}

void GuidanceController::reset() {
    _headingPID.reset();
    _steeringCommand = 0;
    _lastOutput = 0;
    
    #if DEBUG_SERIAL
    Serial.println(F("Guidance: PID reset"));
    #endif
}

void GuidanceController::setTarget(float lat, float lon) {
    _targetLat = lat;
    _targetLon = lon;
    
    // Reset integral when target changes
    _headingPID.integral = 0;
    
    #if DEBUG_SERIAL
    Serial.print(F("Guidance target: "));
    Serial.print(lat, 6);
    Serial.print(F(", "));
    Serial.println(lon, 6);
    #endif
}

void GuidanceController::setGains(float kp, float ki, float kd) {
    _headingPID.kp = kp;
    _headingPID.ki = ki;
    _headingPID.kd = kd;
    
    #if DEBUG_SERIAL
    Serial.print(F("Guidance gains: Kp="));
    Serial.print(kp);
    Serial.print(F(" Ki="));
    Serial.print(ki);
    Serial.print(F(" Kd="));
    Serial.println(kd);
    #endif
}

void GuidanceController::update(const GPSData& gps, const IMUData& imu) {
    uint32_t now = millis();
    float dt = (now - _lastUpdateTime) / 1000.0f;
    
    // Limit dt to avoid issues after pause
    if (dt > 1.0f) dt = 1.0f;
    if (dt <= 0) dt = 0.05f;  // Default to 20 Hz
    
    _lastUpdateTime = now;
    
    // Check if we have valid GPS
    if (!gps.valid || gps.satellites < GPS_MIN_SATS) {
        // No valid GPS - go neutral, but don't reset PID
        // (maintains integral for when GPS returns)
        _active = false;
        _steeringCommand = 0;
        return;
    }
    
    _active = true;
    
    // Calculate bearing to target
    _bearingToTarget = calculateBearing(gps.latitude, gps.longitude,
                                        _targetLat, _targetLon);
    
    // Calculate distance to target
    _distanceToTarget = calculateDistance(gps.latitude, gps.longitude,
                                          _targetLat, _targetLon);
    
    // Get current heading
    // Prefer GPS course over ground if moving, else use IMU
    if (gps.speed_mps > 1.0f) {
        _currentHeading = gps.courseOverGround;
    } else {
        // When nearly stationary, GPS COG is unreliable
        // Use IMU heading (would need magnetometer for absolute heading)
        // For now, maintain last known heading
        _currentHeading = gps.courseOverGround;
    }
    
    // Calculate heading error (normalized to [-180, 180])
    _headingError = normalizeAngle(_bearingToTarget - _currentHeading);
    
    // Run PID controller
    // Error is in degrees, output is normalized steering command
    float pidOutput = _headingPID.compute(_headingError, dt);
    
    // Normalize to [-1, 1] based on max turn angle
    float normalizedOutput = pidOutput / GUIDANCE_MAX_TURN;
    
    // Clamp to [-1, 1]
    normalizedOutput = constrain(normalizedOutput, -1.0f, 1.0f);
    
    // Rate limiting for smooth servo movement
    float maxDelta = GUIDANCE_RATE_LIMIT * dt / GUIDANCE_MAX_TURN;
    float delta = normalizedOutput - _lastOutput;
    delta = constrain(delta, -maxDelta, maxDelta);
    _steeringCommand = _lastOutput + delta;
    _lastOutput = _steeringCommand;
    
    #if DEBUG_SENSORS
    Serial.print(F("Guidance: brg="));
    Serial.print(_bearingToTarget, 1);
    Serial.print(F(" hdg="));
    Serial.print(_currentHeading, 1);
    Serial.print(F(" err="));
    Serial.print(_headingError, 1);
    Serial.print(F(" P="));
    Serial.print(_headingPID.kp * _headingError, 2);
    Serial.print(F(" I="));
    Serial.print(_headingPID.ki * _headingPID.integral, 2);
    Serial.print(F(" cmd="));
    Serial.println(_steeringCommand, 3);
    #endif
}

void GuidanceController::getServoCommands(int16_t& leftCmd, int16_t& rightCmd) {
    if (!_active) {
        // Neutral steering
        leftCmd = 0;
        rightCmd = 0;
        return;
    }
    
    // Differential steering:
    // Positive command = turn right = more left brake = left servo pulls more
    // Negative command = turn left = more right brake = right servo pulls more
    
    int16_t maxOffset = (SERVO_PWM_MAX - SERVO_PWM_CENTER) / 2;
    
    leftCmd = (int16_t)(_steeringCommand * maxOffset);
    rightCmd = (int16_t)(-_steeringCommand * maxOffset);
}

float GuidanceController::calculateBearing(float lat1, float lon1, 
                                           float lat2, float lon2) {
    // Convert to radians
    float lat1r = lat1 * DEG_TO_RAD;
    float lat2r = lat2 * DEG_TO_RAD;
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    
    // Calculate bearing using spherical formula
    float x = cosf(lat2r) * sinf(dlon);
    float y = cosf(lat1r) * sinf(lat2r) - sinf(lat1r) * cosf(lat2r) * cosf(dlon);
    
    float bearing = atan2f(x, y) * RAD_TO_DEG;
    
    // Normalize to 0-360
    if (bearing < 0) bearing += 360.0f;
    
    return bearing;
}

float GuidanceController::calculateDistance(float lat1, float lon1,
                                            float lat2, float lon2) {
    // Haversine formula
    float lat1r = lat1 * DEG_TO_RAD;
    float lat2r = lat2 * DEG_TO_RAD;
    float dlat = (lat2 - lat1) * DEG_TO_RAD;
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    
    float a = sinf(dlat/2) * sinf(dlat/2) +
              cosf(lat1r) * cosf(lat2r) * sinf(dlon/2) * sinf(dlon/2);
    float c = 2 * atan2f(sqrtf(a), sqrtf(1-a));
    
    return EARTH_RADIUS_M * c;
}

float GuidanceController::normalizeAngle(float angle) {
    // Normalize to [-180, 180]
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

// ============================================================================
// TRAJECTORY PREDICTION
// ============================================================================

void GuidanceController::setTrajectoryPrediction(bool enabled, float lookaheadSeconds) {
    _trajectoryEnabled = enabled;
    _lookaheadTime = constrain(lookaheadSeconds, 0.1f, 5.0f);
    
    #if DEBUG_SERIAL
    Serial.print(F("Trajectory prediction: "));
    Serial.print(enabled ? F("ON") : F("OFF"));
    Serial.print(F(", lookahead: "));
    Serial.print(_lookaheadTime);
    Serial.println(F("s"));
    #endif
}

void GuidanceController::getPredictedPosition(float& lat, float& lon) {
    if (_trajectoryEnabled && _hasPrevPosition) {
        lat = _predLat;
        lon = _predLon;
    } else {
        lat = 0;
        lon = 0;
    }
}

void GuidanceController::updateTrajectoryPrediction(const GPSData& gps, float dt) {
    if (!_hasPrevPosition) {
        // First position - just store it
        _prevLat = gps.latitude;
        _prevLon = gps.longitude;
        _hasPrevPosition = true;
        _predLat = gps.latitude;
        _predLon = gps.longitude;
        return;
    }
    
    // Calculate velocity in degrees/second
    float velLat = (gps.latitude - _prevLat) / dt;
    float velLon = (gps.longitude - _prevLon) / dt;
    
    // Sanity check - reject unreasonable velocities (GPS glitch)
    // Max ~100 km/h in degrees/s: ~0.001 deg/s
    float maxVel = 0.002f;
    if (abs(velLat) > maxVel || abs(velLon) > maxVel) {
        // Probably GPS glitch - don't update prediction
        _predLat = gps.latitude;
        _predLon = gps.longitude;
    } else {
        // Predict future position
        _predLat = gps.latitude + velLat * _lookaheadTime;
        _predLon = gps.longitude + velLon * _lookaheadTime;
    }
    
    // Store for next iteration
    _prevLat = gps.latitude;
    _prevLon = gps.longitude;
    
    #if DEBUG_SENSORS
    Serial.print(F("Pred pos: "));
    Serial.print(_predLat, 6);
    Serial.print(F(", "));
    Serial.println(_predLon, 6);
    #endif
}
