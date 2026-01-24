/**
 * @file sensor_manager.h
 * @brief Unified sensor management interface
 * @team LeoNUS
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"
#include "../types.h"
#include "ms5611.h"
#include "mpu9250.h"
#include "ina219.h"
#include "gps.h"

class SensorManager {
public:
    SensorManager();
    
    /**
     * @brief Initialize all sensors
     * @return true if all critical sensors initialized
     */
    bool begin();
    
    /**
     * @brief Update all sensor readings
     * Should be called at 10 Hz
     */
    void update();
    
    /**
     * @brief Get aggregated sensor data
     */
    const SensorData& getData() const { return _data; }
    
    /**
     * @brief Calibrate ground pressure for altitude zero
     */
    void calibrateGround();
    
    /**
     * @brief Override pressure reading (for simulation mode)
     * @param pressure_Pa Pressure in Pascals
     */
    void overridePressure(float pressure_Pa);
    
    /**
     * @brief Set ground pressure from persistence
     */
    void setGroundPressure(float pressure_kPa);
    
    /**
     * @brief Get calibrated ground pressure
     */
    float getGroundPressure() const { return _groundPressure_kPa; }
    
    /**
     * @brief Set peak altitude from persistence
     */
    void setPeakAltitude(float alt) { _data.peakAltitude = alt; }
    
private:
    MS5611Sensor _ms5611;
    MPU9250Sensor _mpu9250;
    INA219Sensor _ina219;
    GPSSensor _gps;
    
    SensorData _data;
    
    // Altitude calculation
    float _groundPressure_kPa;
    bool _groundCalibrated;
    
    // Vertical velocity calculation
    static const int ALT_HISTORY_SIZE = 5;
    float _altHistory[ALT_HISTORY_SIZE];
    uint32_t _altTimeHistory[ALT_HISTORY_SIZE];
    int _altHistoryIdx;
    
    void updateAltitude();
    void updateVerticalVelocity();
};

#endif // SENSOR_MANAGER_H
