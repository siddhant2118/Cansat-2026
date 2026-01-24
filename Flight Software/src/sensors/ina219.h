/**
 * @file ina219.h
 * @brief INA219 Voltage/Current Monitor Driver
 * @team LeoNUS
 */

#ifndef INA219_H
#define INA219_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"
#include "../types.h"

class INA219Sensor {
public:
    INA219Sensor();
    
    /**
     * @brief Initialize the sensor
     * @return true if successful
     */
    bool begin();
    
    /**
     * @brief Read voltage and current
     * @return true if successful
     */
    bool update();
    
    /**
     * @brief Get latest power data
     */
    const PowerData& getData() const { return _data; }
    
private:
    PowerData _data;
    
    float _currentLSB;  // A per LSB
    
    bool writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t reg);
};

#endif // INA219_H
