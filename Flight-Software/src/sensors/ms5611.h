/**
 * @file ms5611.h
 * @brief MS5611 Pressure/Temperature Sensor Driver
 * @team LeoNUS
 */

#ifndef MS5611_H
#define MS5611_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"
#include "../types.h"

class MS5611Sensor {
public:
    MS5611Sensor();
    
    /**
     * @brief Initialize the sensor and read calibration data
     * @return true if successful
     */
    bool begin();
    
    /**
     * @brief Trigger new conversion and read data
     * @return true if new data available
     */
    bool update();
    
    /**
     * @brief Get latest pressure data
     */
    const PressureData& getData() const { return _data; }
    
private:
    PressureData _data;
    
    // Calibration coefficients (from PROM)
    uint16_t _C[6];
    
    // Raw readings
    uint32_t _D1;  // Pressure
    uint32_t _D2;  // Temperature
    
    // State machine for async conversion
    enum State { IDLE, CONVERTING_PRESSURE, CONVERTING_TEMP };
    State _state;
    uint32_t _conversionStart;
    
    bool readCalibration();
    void startConversion(uint8_t cmd);
    uint32_t readADC();
    void calculate();
};

#endif // MS5611_H
