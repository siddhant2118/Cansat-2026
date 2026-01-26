/**
 * @file health.cpp
 * @brief Health Monitoring System Implementation
 * @team LeoNUS
 */

#include "health.h"

HealthMonitor::HealthMonitor()
    : _lastGPSValid(0)
    , _lastRadioRx(0)
{
    memset(&_status, 0, sizeof(_status));
}

void HealthMonitor::begin() {
    _lastGPSValid = millis();
    _lastRadioRx = millis();
}

void HealthMonitor::update(const SensorData& data) {
    uint32_t now = millis();
    
    // Check pressure sensor
    _status.sensorPressureFail = !data.pressureOk;
    
    // Check IMU
    _status.sensorIMUFail = !data.imuOk;
    
    // Check GPS with timeout
    if (data.gpsOk && data.gps.valid) {
        _lastGPSValid = now;
        _status.sensorGPSFail = false;
        _status.lastGPSUpdate = now;
    } else if (now - _lastGPSValid > GPS_TIMEOUT_MS) {
        _status.sensorGPSFail = true;
    }
    
    // Check power monitor
    _status.sensorPowerFail = !data.powerOk;
    
    // Check battery voltage
    if (data.powerOk && data.power.voltage_V < LOW_BATTERY_V) {
        _status.lowBattery = true;
    } else {
        _status.lowBattery = false;
    }
    
    // Determine fallback modes
    // If pressure fails but GPS is good, use GPS altitude
    _status.gpsAltitudeFallback = _status.sensorPressureFail && !_status.sensorGPSFail;
    
    // If GPS fails, set guidance to neutral
    _status.guidanceNeutral = _status.sensorGPSFail;
    
    #if DEBUG_SERIAL
    static uint32_t lastPrint = 0;
    if (now - lastPrint > 5000) {
        lastPrint = now;
        Serial.print(F("Health: P="));
        Serial.print(!_status.sensorPressureFail);
        Serial.print(F(" I="));
        Serial.print(!_status.sensorIMUFail);
        Serial.print(F(" G="));
        Serial.print(!_status.sensorGPSFail);
        Serial.print(F(" V="));
        Serial.print(!_status.sensorPowerFail);
        Serial.print(F(" SD="));
        Serial.print(!_status.sdCardFail);
        Serial.print(F(" BAT="));
        Serial.println(!_status.lowBattery);
    }
    #endif
}

void HealthMonitor::logEvent(const char* event) {
    #if DEBUG_SERIAL
    Serial.print(F("EVENT: "));
    Serial.println(event);
    #endif
    
    // TODO: Write to event log buffer
}

bool HealthMonitor::isFlightReady() const {
    // Critical systems must be working
    return !_status.sensorPressureFail && 
           !_status.sensorIMUFail &&
           !_status.lowBattery;
    // GPS and SD are non-critical for flight (degraded mode still possible)
}
