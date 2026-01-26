/**
 * @file mpu9250.cpp
 * @brief MPU9250 IMU Sensor Driver Implementation
 * @team LeoNUS
 */

#include "mpu9250.h"
#include <math.h>

// MPU9250 Registers
#define REG_WHO_AM_I        0x75
#define REG_PWR_MGMT_1      0x6B
#define REG_PWR_MGMT_2      0x6C
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_CONFIG2   0x1D
#define REG_INT_PIN_CFG     0x37
#define REG_ACCEL_XOUT_H    0x3B

// WHO_AM_I response
#define MPU9250_WHO_AM_I    0x71
#define MPU6500_WHO_AM_I    0x70  // Some modules have MPU6500

// Gravity constant
#define GRAVITY_MPS2        9.80665f

MPU9250Sensor::MPU9250Sensor()
    : _gyroScale(0)
    , _accelScale(0)
{
    memset(&_data, 0, sizeof(_data));
}

bool MPU9250Sensor::begin() {
    // Check WHO_AM_I
    uint8_t whoami;
    if (!readRegisters(REG_WHO_AM_I, &whoami, 1)) {
        return false;
    }
    
    if (whoami != MPU9250_WHO_AM_I && whoami != MPU6500_WHO_AM_I) {
        #if DEBUG_SERIAL
        Serial.print(F("MPU9250 WHO_AM_I: 0x"));
        Serial.println(whoami, HEX);
        #endif
        return false;
    }
    
    // Reset device
    if (!writeRegister(REG_PWR_MGMT_1, 0x80)) return false;  // Device reset
    delay(100);
    
    // Wake up and use best clock source (auto select)
    if (!writeRegister(REG_PWR_MGMT_1, 0x01)) return false;
    delay(10);
    
    // Enable all sensors
    if (!writeRegister(REG_PWR_MGMT_2, 0x00)) return false;
    
    // Configure gyroscope: ±500 deg/s
    // Bits 4:3 = FS_SEL: 0=±250, 1=±500, 2=±1000, 3=±2000 deg/s
    if (!writeRegister(REG_GYRO_CONFIG, 0x08)) return false;  // ±500 deg/s
    _gyroScale = 500.0f / 32768.0f;  // deg/s per LSB
    
    // Configure accelerometer: ±4g
    // Bits 4:3 = AFS_SEL: 0=±2g, 1=±4g, 2=±8g, 3=±16g
    if (!writeRegister(REG_ACCEL_CONFIG, 0x08)) return false;  // ±4g
    _accelScale = 4.0f / 32768.0f;  // g per LSB
    
    // Digital low-pass filter configuration
    if (!writeRegister(REG_CONFIG, 0x03)) return false;  // DLPF: 41Hz
    if (!writeRegister(REG_ACCEL_CONFIG2, 0x03)) return false;  // Accel DLPF: 44.8Hz
    
    _data.valid = true;
    return true;
}

bool MPU9250Sensor::update() {
    // Read 14 bytes: accel (6) + temp (2) + gyro (6)
    uint8_t buffer[14];
    if (!readRegisters(REG_ACCEL_XOUT_H, buffer, 14)) {
        _data.valid = false;
        return false;
    }
    
    // Parse accelerometer (high byte first)
    int16_t accelX = (int16_t)((buffer[0] << 8) | buffer[1]);
    int16_t accelY = (int16_t)((buffer[2] << 8) | buffer[3]);
    int16_t accelZ = (int16_t)((buffer[4] << 8) | buffer[5]);
    
    // Skip temperature bytes [6,7]
    
    // Parse gyroscope
    int16_t gyroX = (int16_t)((buffer[8] << 8) | buffer[9]);
    int16_t gyroY = (int16_t)((buffer[10] << 8) | buffer[11]);
    int16_t gyroZ = (int16_t)((buffer[12] << 8) | buffer[13]);
    
    // Convert to engineering units
    // Gyro: deg/s - direct mapping for Roll, Pitch, Yaw
    _data.gyroR = gyroX * _gyroScale;  // Roll rate
    _data.gyroP = gyroY * _gyroScale;  // Pitch rate
    _data.gyroY = gyroZ * _gyroScale;  // Yaw rate
    
    // Accelerometer: convert to m/s², store raw
    _data.accelX = accelX * _accelScale * GRAVITY_MPS2;
    _data.accelY_raw = accelY * _accelScale * GRAVITY_MPS2;
    _data.accelZ = accelZ * _accelScale * GRAVITY_MPS2;
    
    // Per mission guide, ACCEL_R/P/Y should be in deg/s² (unusual specification)
    // Converting from m/s² to "angular acceleration" representation
    // This is interpreted as tilt rates - using atan2 to get angles
    // For simplicity, we report the acceleration vector projected onto reference frame
    // The spec is ambiguous - reporting raw m/s² * conversion factor
    
    // TODO: Verify with competition judges what exactly deg/s² means for accel
    // For now, using a simple approximation based on tilt angle change rate
    // Assuming small angles: accel_angle ≈ atan(a_perpendicular / g) * (180/π)
    // Rate would require differentiation which we approximate here
    
    float totalAccel = sqrtf(_data.accelX * _data.accelX + 
                             _data.accelY_raw * _data.accelY_raw + 
                             _data.accelZ * _data.accelZ);
    
    if (totalAccel > 0.1f) {
        // Convert to degrees (tilt from vertical) approximation
        _data.accelR = atan2f(_data.accelY_raw, _data.accelZ) * (180.0f / 3.14159f);
        _data.accelP = atan2f(-_data.accelX, sqrtf(_data.accelY_raw * _data.accelY_raw + 
                                                    _data.accelZ * _data.accelZ)) * (180.0f / 3.14159f);
        _data.accelY = 0;  // Yaw cannot be determined from accelerometer alone
    }
    
    _data.valid = true;
    _data.timestamp = millis();
    return true;
}

bool MPU9250Sensor::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDR_MPU9250);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

bool MPU9250Sensor::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count) {
    Wire.beginTransmission(I2C_ADDR_MPU9250);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    
    Wire.requestFrom((uint8_t)I2C_ADDR_MPU9250, count);
    if (Wire.available() < count) {
        return false;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        buffer[i] = Wire.read();
    }
    
    return true;
}
