/**
 * @file guidance.cpp
 * @brief Paraglider Guidance Controller Implementation
 * @team LeoNUS
 * 
 * Simple proportional steering toward target:
 * 1. Calculate bearing to target
 * 2. Compare with current heading
 * 3. Generate differential steering command
 * 4. Apply rate limiting
 */

#include "guidance.h"
#include <math.h>

// Earth radius in meters
#define EARTH_RADIUS_M 6371000.0f

// Degrees to radians
#define DEG_TO_RAD (3.14159265359f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265359f)

GuidanceController::GuidanceController()
    : _targetLat(0)
    , _targetLon(0)
    , _bearingToTarget(0)
    , _distanceToTarget(0)
    , _crossTrackError(0)
    , _currentHeading(0)
    , _steeringCommand(0)
    , _active(false)
    , _lastSteeringCommand(0)
    , _lastUpdateTime(0)
{
}

void GuidanceController::begin() {
    _active = false;
    _steeringCommand = 0;
    _lastSteeringCommand = 0;
}

void GuidanceController::setTarget(float lat, float lon) {
    _targetLat = lat;
    _targetLon = lon;
    
    #if DEBUG_SERIAL
    Serial.print(F("Guidance target: "));
    Serial.print(lat, 4);
    Serial.print(F(", "));
    Serial.println(lon, 4);
    #endif
}

void GuidanceController::update(const GPSData& gps, const IMUData& imu) {
    uint32_t now = millis();
    
    // Check if we have valid GPS
    if (!gps.valid || gps.satellites < GPS_MIN_SATS) {
        // No valid GPS - go neutral
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
    // Prefer GPS course over ground if moving, else use IMU yaw
    if (gps.speed_mps > 1.0f) {
        _currentHeading = gps.courseOverGround;
    } else {
        _currentHeading = imu.gyroY;  // Use gyro-integrated heading as backup
        // TODO: Implement proper heading from magnetometer if available
    }
    
    // Calculate heading error
    float headingError = normalizeAngle(_bearingToTarget - _currentHeading);
    
    // Proportional steering command
    float rawCommand = headingError * GUIDANCE_KP / GUIDANCE_MAX_TURN;
    
    // Clamp to [-1, 1]
    rawCommand = CLAMP(rawCommand, -1.0f, 1.0f);
    
    // Rate limiting
    float dt = (now - _lastUpdateTime) / 1000.0f;
    if (dt > 0) {
        float maxDelta = GUIDANCE_RATE_LIMIT * dt / GUIDANCE_MAX_TURN;
        float delta = rawCommand - _lastSteeringCommand;
        delta = CLAMP(delta, -maxDelta, maxDelta);
        _steeringCommand = _lastSteeringCommand + delta;
    } else {
        _steeringCommand = rawCommand;
    }
    
    _lastSteeringCommand = _steeringCommand;
    _lastUpdateTime = now;
    
    #if DEBUG_SENSORS
    Serial.print(F("Guidance: brg="));
    Serial.print(_bearingToTarget);
    Serial.print(F(" hdg="));
    Serial.print(_currentHeading);
    Serial.print(F(" err="));
    Serial.print(headingError);
    Serial.print(F(" cmd="));
    Serial.println(_steeringCommand);
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
    // Positive command = turn right = more left brake = left servo more
    // Negative command = turn left = more right brake = right servo more
    
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
