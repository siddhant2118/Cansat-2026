/**
 * @file ina219.cpp
 * @brief INA219 Voltage/Current Monitor Driver Implementation
 * @team LeoNUS
 */

#include "ina219.h"

// INA219 Registers
#define REG_CONFIG          0x00
#define REG_SHUNT_VOLTAGE   0x01
#define REG_BUS_VOLTAGE     0x02
#define REG_POWER           0x03
#define REG_CURRENT         0x04
#define REG_CALIBRATION     0x05

// Configuration bits
// Bus voltage range: 0 = 16V, 1 = 32V
// PGA gain: 00 = ±40mV, 01 = ±80mV, 10 = ±160mV, 11 = ±320mV
// ADC resolution/averaging
// Mode: 111 = Shunt and bus, continuous

// Default config: 32V bus, ±320mV shunt, 12-bit, continuous
#define CONFIG_DEFAULT      0x399F

INA219Sensor::INA219Sensor()
    : _currentLSB(0)
{
    memset(&_data, 0, sizeof(_data));
}

bool INA219Sensor::begin() {
    // Reset the device
    if (!writeRegister(REG_CONFIG, 0x8000)) {
        return false;
    }
    delay(1);
    
    // Configure for our application
    // 32V bus voltage range, ±320mV shunt range (PGA /8)
    // 12-bit resolution, continuous shunt and bus
    if (!writeRegister(REG_CONFIG, CONFIG_DEFAULT)) {
        return false;
    }
    
    // Calculate calibration value
    // Cal = trunc(0.04096 / (Current_LSB * Rshunt))
    // We want Current_LSB = 0.1mA = 0.0001A for good resolution
    // With 0.1 ohm shunt: Cal = 0.04096 / (0.0001 * 0.1) = 4096
    
    _currentLSB = 0.0001f;  // 0.1mA per LSB
    uint16_t calValue = (uint16_t)(0.04096f / (_currentLSB * INA219_SHUNT_OHMS));
    
    if (!writeRegister(REG_CALIBRATION, calValue)) {
        return false;
    }
    
    _data.valid = true;
    return true;
}

bool INA219Sensor::update() {
    // Read bus voltage
    uint16_t busVoltageRaw = readRegister(REG_BUS_VOLTAGE);
    
    // Check for overflow or conversion not ready
    if (busVoltageRaw == 0xFFFF) {
        _data.valid = false;
        return false;
    }
    
    // Bits 15-3 are voltage, bit 1 is CNVR (conversion ready), bit 0 is OVF
    bool overflow = (busVoltageRaw & 0x0001);
    // bool ready = (busVoltageRaw & 0x0002);
    
    // Bus voltage: shift right by 3, LSB = 4mV
    float busVoltage = ((busVoltageRaw >> 3) & 0x1FFF) * 4.0f / 1000.0f;
    
    // Read current
    int16_t currentRaw = (int16_t)readRegister(REG_CURRENT);
    float current = currentRaw * _currentLSB;
    
    // Read shunt voltage for verification (optional)
    // int16_t shuntVoltageRaw = (int16_t)readRegister(REG_SHUNT_VOLTAGE);
    // float shuntVoltage = shuntVoltageRaw * 10.0f / 1000000.0f;  // 10uV per LSB
    
    // Update data
    _data.voltage_V = busVoltage;
    _data.current_A = current;
    _data.power_W = busVoltage * current;
    _data.valid = !overflow;
    _data.timestamp = millis();
    
    return true;
}

bool INA219Sensor::writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(I2C_ADDR_INA219);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);  // High byte first
    Wire.write(value & 0xFF);
    return (Wire.endTransmission() == 0);
}

uint16_t INA219Sensor::readRegister(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR_INA219);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0xFFFF;
    }
    
    Wire.requestFrom((uint8_t)I2C_ADDR_INA219, (uint8_t)2);
    if (Wire.available() < 2) {
        return 0xFFFF;
    }
    
    uint16_t value = (Wire.read() << 8) | Wire.read();
    return value;
}
