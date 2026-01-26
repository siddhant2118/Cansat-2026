/**
 * @file as5600.cpp
 * @brief AS5600 Magnetic Rotary Encoder Driver Implementation
 */

#include "as5600.h"

// TCA9548A I2C Multiplexer address (if used)
#define TCA9548A_ADDRESS 0x70

AS5600::AS5600(int8_t multiplexerChannel) 
    : _wire(nullptr), _muxChannel(multiplexerChannel) {
}

bool AS5600::begin(TwoWire* wire) {
    _wire = wire;
    
    // Select multiplexer channel if using one
    if (_muxChannel >= 0) {
        selectMuxChannel();
    }
    
    // Check if magnet is detected
    return magnetDetected();
}

void AS5600::selectMuxChannel() {
    if (_muxChannel < 0 || _muxChannel > 7) return;
    
    _wire->beginTransmission(TCA9548A_ADDRESS);
    _wire->write(1 << _muxChannel);
    _wire->endTransmission();
}

uint8_t AS5600::readRegister(uint8_t reg) {
    if (_muxChannel >= 0) {
        selectMuxChannel();
    }
    
    _wire->beginTransmission(AS5600_ADDRESS);
    _wire->write(reg);
    _wire->endTransmission(false);
    
    _wire->requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)1);
    if (_wire->available()) {
        return _wire->read();
    }
    return 0;
}

uint16_t AS5600::readRegister16(uint8_t regH, uint8_t regL) {
    if (_muxChannel >= 0) {
        selectMuxChannel();
    }
    
    // Read high byte
    _wire->beginTransmission(AS5600_ADDRESS);
    _wire->write(regH);
    _wire->endTransmission(false);
    
    _wire->requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)2);
    
    uint16_t value = 0;
    if (_wire->available() >= 2) {
        value = _wire->read() << 8;
        value |= _wire->read();
    }
    
    return value & 0x0FFF;  // 12-bit value
}

bool AS5600::magnetDetected() {
    uint8_t status = getStatus();
    return (status & AS5600_STATUS_MD) != 0;
}

uint16_t AS5600::readRawAngle() {
    return readRegister16(AS5600_REG_RAW_ANGLE_H, AS5600_REG_RAW_ANGLE_L);
}

uint16_t AS5600::readAngle() {
    return readRegister16(AS5600_REG_ANGLE_H, AS5600_REG_ANGLE_L);
}

float AS5600::readDegrees() {
    uint16_t raw = readRawAngle();
    return (float)raw * 360.0f / 4096.0f;
}

uint8_t AS5600::getMagnetStrength() {
    return readRegister(AS5600_REG_AGC);
}

uint8_t AS5600::getStatus() {
    return readRegister(AS5600_REG_STATUS);
}
