/**
 * @file sensor_manager.cpp
 * @brief Unified sensor management implementation
 * @team LeoNUS
 */

#include "sensor_manager.h"
#include <math.h>

// Sea level standard pressure for altitude calculation
static const float SEA_LEVEL_PRESSURE_KPA = 101.325f;

SensorManager::SensorManager() 
    : _groundPressure_kPa(SEA_LEVEL_PRESSURE_KPA)
    , _groundCalibrated(false)
    , _altHistoryIdx(0)
{
    memset(&_data, 0, sizeof(_data));
    memset(_altHistory, 0, sizeof(_altHistory));
    memset(_altTimeHistory, 0, sizeof(_altTimeHistory));
}

bool SensorManager::begin() {
    bool allOk = true;
    
    // Initialize pressure sensor (critical)
    if (!_ms5611.begin()) {
        _data.pressureOk = false;
        allOk = false;
        #if DEBUG_SERIAL
        Serial.println(F("MS5611 init FAILED"));
        #endif
    } else {
        _data.pressureOk = true;
        #if DEBUG_SERIAL
        Serial.println(F("MS5611 init OK"));
        #endif
    }
    
    // Initialize IMU (critical)
    if (!_mpu9250.begin()) {
        _data.imuOk = false;
        allOk = false;
        #if DEBUG_SERIAL
        Serial.println(F("MPU9250 init FAILED"));
        #endif
    } else {
        _data.imuOk = true;
        #if DEBUG_SERIAL
        Serial.println(F("MPU9250 init OK"));
        #endif
    }
    
    // Initialize power monitor (non-critical)
    if (!_ina219.begin()) {
        _data.powerOk = false;
        #if DEBUG_SERIAL
        Serial.println(F("INA219 init FAILED (non-critical)"));
        #endif
    } else {
        _data.powerOk = true;
        #if DEBUG_SERIAL
        Serial.println(F("INA219 init OK"));
        #endif
    }
    
    // Initialize GPS (non-critical for init, needed for flight)
    if (!_gps.begin()) {
        _data.gpsOk = false;
        #if DEBUG_SERIAL
        Serial.println(F("GPS init FAILED (non-critical)"));
        #endif
    } else {
        _data.gpsOk = true;
        #if DEBUG_SERIAL
        Serial.println(F("GPS init OK"));
        #endif
    }
    
    return allOk;
}

void SensorManager::update() {
    uint32_t now = millis();
    
    // Update pressure sensor
    if (_data.pressureOk) {
        if (_ms5611.update()) {
            _data.pressure = _ms5611.getData();
            updateAltitude();
            updateVerticalVelocity();
        } else {
            _data.pressureOk = false;
        }
    }
    
    // Update IMU
    if (_data.imuOk) {
        if (_mpu9250.update()) {
            _data.imu = _mpu9250.getData();
        } else {
            _data.imuOk = false;
        }
    }
    
    // Update power monitor
    if (_data.powerOk) {
        if (_ina219.update()) {
            _data.power = _ina219.getData();
        }
    }
    
    // Update GPS (process incoming NMEA)
    if (_gps.update()) {
        _data.gps = _gps.getData();
        _data.gpsOk = _data.gps.valid && (_data.gps.satellites >= GPS_MIN_SATS);
    }
    
    // Track peak altitude
    if (_data.pressure.altitude_m > _data.peakAltitude) {
        _data.peakAltitude = _data.pressure.altitude_m;
    }
    
    #if DEBUG_SENSORS
    Serial.print(F("Alt: ")); Serial.print(_data.pressure.altitude_m);
    Serial.print(F(" VVel: ")); Serial.print(_data.verticalVelocity);
    Serial.print(F(" Peak: ")); Serial.println(_data.peakAltitude);
    #endif
}

void SensorManager::calibrateGround() {
    // Take average of several readings
    float sum = 0;
    const int samples = 10;
    
    for (int i = 0; i < samples; i++) {
        if (_ms5611.update()) {
            sum += _ms5611.getData().pressure_kPa;
        }
        delay(50);
    }
    
    _groundPressure_kPa = sum / samples;
    _groundCalibrated = true;
    
    // Reset altitude history
    memset(_altHistory, 0, sizeof(_altHistory));
    memset(_altTimeHistory, 0, sizeof(_altTimeHistory));
    _altHistoryIdx = 0;
    _data.peakAltitude = 0;
    
    #if DEBUG_SERIAL
    Serial.print(F("Ground calibrated: "));
    Serial.print(_groundPressure_kPa);
    Serial.println(F(" kPa"));
    #endif
}

void SensorManager::overridePressure(float pressure_Pa) {
    // Convert Pa to kPa
    _data.pressure.pressure_kPa = pressure_Pa / 1000.0f;
    updateAltitude();
    updateVerticalVelocity();
}

void SensorManager::setGroundPressure(float pressure_kPa) {
    _groundPressure_kPa = pressure_kPa;
    _groundCalibrated = true;
}

void SensorManager::updateAltitude() {
    // Hypsometric formula for altitude from pressure
    // h = (T0 / L) * (1 - (P/P0)^(R*L/(g*M)))
    // Simplified version using standard atmosphere
    
    float P = _data.pressure.pressure_kPa;
    float P0 = _groundPressure_kPa;
    
    if (P > 0 && P0 > 0) {
        // Standard atmosphere calculation
        // Altitude = 44330 * (1 - (P/P0)^0.1903)
        float ratio = P / P0;
        _data.pressure.altitude_m = 44330.0f * (1.0f - powf(ratio, 0.1903f));
    } else {
        _data.pressure.altitude_m = 0;
    }
}

void SensorManager::updateVerticalVelocity() {
    uint32_t now = millis();
    
    // Store current altitude in circular buffer
    _altHistory[_altHistoryIdx] = _data.pressure.altitude_m;
    _altTimeHistory[_altHistoryIdx] = now;
    
    // Calculate velocity using oldest and newest samples
    int oldestIdx = (_altHistoryIdx + 1) % ALT_HISTORY_SIZE;
    
    if (_altTimeHistory[oldestIdx] > 0) {
        float dt = (_altTimeHistory[_altHistoryIdx] - _altTimeHistory[oldestIdx]) / 1000.0f;
        if (dt > 0) {
            float dh = _altHistory[_altHistoryIdx] - _altHistory[oldestIdx];
            _data.verticalVelocity = dh / dt;
        }
    }
    
    _altHistoryIdx = (_altHistoryIdx + 1) % ALT_HISTORY_SIZE;
}
