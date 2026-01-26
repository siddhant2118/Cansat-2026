/**
 * @file mpu9250.h
 * @brief MPU9250 IMU Sensor Driver
 * @team LeoNUS
 */

#ifndef MPU9250_H
#define MPU9250_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"
#include "../types.h"

class MPU9250Sensor {
public:
    MPU9250Sensor();
    
    /**
     * @brief Initialize the sensor
     * @return true if successful
     */
    bool begin();
    
    /**
     * @brief Read sensor data
     * @return true if successful
     */
    bool update();
    
    /**
     * @brief Get latest IMU data
     */
    const IMUData& getData() const { return _data; }
    
private:
    IMUData _data;
    
    // Scale factors
    float _gyroScale;   // deg/s per LSB
    float _accelScale;  // g per LSB
    
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count);
};

#endif // MPU9250_H
