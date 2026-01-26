/**
 * @file as5600.h
 * @brief AS5600 Magnetic Rotary Encoder Driver
 * 
 * The AS5600 is a 12-bit magnetic rotary position sensor.
 * Used for winch servo position feedback in para-glider control.
 * 
 * Key specs:
 * - 12-bit resolution (4096 positions, ~0.087° per step)
 * - I2C interface (address 0x36)
 * - 3.3V operation
 * - Requires diametrically magnetized magnet
 */

#ifndef AS5600_H
#define AS5600_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"

// AS5600 I2C Address (fixed, cannot be changed on AS5600)
#define AS5600_ADDRESS  0x36

// Register addresses
#define AS5600_REG_RAW_ANGLE_H  0x0C   // Raw angle high byte
#define AS5600_REG_RAW_ANGLE_L  0x0D   // Raw angle low byte
#define AS5600_REG_ANGLE_H      0x0E   // Filtered angle high byte
#define AS5600_REG_ANGLE_L      0x0F   // Filtered angle low byte
#define AS5600_REG_STATUS       0x0B   // Status register
#define AS5600_REG_AGC          0x1A   // Automatic gain control
#define AS5600_REG_MAGNITUDE_H  0x1B   // Magnitude high byte
#define AS5600_REG_MAGNITUDE_L  0x1C   // Magnitude low byte

// Status bits
#define AS5600_STATUS_MH        0x08   // Magnet too strong
#define AS5600_STATUS_ML        0x10   // Magnet too weak
#define AS5600_STATUS_MD        0x20   // Magnet detected

/**
 * @brief AS5600 magnetic encoder driver
 * 
 * Supports multiple encoders via TCA9548A I2C multiplexer.
 * If no multiplexer is used, only one encoder can be read.
 */
class AS5600 {
public:
    /**
     * @brief Construct AS5600 encoder
     * @param multiplexerChannel Channel on TCA9548A (0-7), or -1 if no multiplexer
     */
    AS5600(int8_t multiplexerChannel = -1);
    
    /**
     * @brief Initialize the encoder
     * @param wire Wire interface to use (default Wire)
     * @return true if magnet detected, false otherwise
     */
    bool begin(TwoWire* wire = &Wire);
    
    /**
     * @brief Check if magnet is detected
     * @return true if magnet is present and in range
     */
    bool magnetDetected();
    
    /**
     * @brief Read raw angle (0-4095)
     * @return 12-bit raw angle value
     */
    uint16_t readRawAngle();
    
    /**
     * @brief Read angle in degrees (0-360)
     * @return Angle in degrees (float)
     */
    float readDegrees();
    
    /**
     * @brief Read filtered angle (0-4095)
     * @return 12-bit filtered angle value with hysteresis
     */
    uint16_t readAngle();
    
    /**
     * @brief Get magnet strength (for debugging)
     * @return AGC value (0-255), lower = stronger magnet
     */
    uint8_t getMagnetStrength();
    
    /**
     * @brief Get status byte
     * @return Status register value
     */
    uint8_t getStatus();

private:
    TwoWire* _wire;
    int8_t _muxChannel;
    
    void selectMuxChannel();
    uint8_t readRegister(uint8_t reg);
    uint16_t readRegister16(uint8_t regH, uint8_t regL);
};

#endif // AS5600_H
