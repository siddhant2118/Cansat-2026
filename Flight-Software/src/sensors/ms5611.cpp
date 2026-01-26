/**
 * @file ms5611.cpp
 * @brief MS5611 Pressure/Temperature Sensor Driver Implementation
 * @team LeoNUS
 */

#include "ms5611.h"

// MS5611 Commands
#define CMD_RESET       0x1E
#define CMD_PROM_READ   0xA0
#define CMD_CONVERT_D1  0x40  // Pressure
#define CMD_CONVERT_D2  0x50  // Temperature
#define CMD_ADC_READ    0x00

// OSR settings (add to base command)
// OSR 256  = 0x00, conversion time 0.6ms
// OSR 512  = 0x02, conversion time 1.2ms
// OSR 1024 = 0x04, conversion time 2.3ms
// OSR 2048 = 0x06, conversion time 4.5ms
// OSR 4096 = 0x08, conversion time 9.0ms
#define OSR_CMD_OFFSET  0x08  // Using OSR 4096 for best resolution

#define CONVERSION_TIME_US  10000  // 10ms for OSR 4096

MS5611Sensor::MS5611Sensor()
    : _state(IDLE)
    , _conversionStart(0)
    , _D1(0)
    , _D2(0)
{
    memset(&_data, 0, sizeof(_data));
    memset(_C, 0, sizeof(_C));
}

bool MS5611Sensor::begin() {
    // Reset device
    Wire.beginTransmission(I2C_ADDR_MS5611);
    Wire.write(CMD_RESET);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(10);  // Wait for reset
    
    // Read calibration data
    if (!readCalibration()) {
        return false;
    }
    
    _data.valid = true;
    return true;
}

bool MS5611Sensor::readCalibration() {
    for (uint8_t i = 0; i < 6; i++) {
        Wire.beginTransmission(I2C_ADDR_MS5611);
        Wire.write(CMD_PROM_READ + (i + 1) * 2);  // C1-C6 at addresses 0xA2-0xAC
        if (Wire.endTransmission() != 0) {
            return false;
        }
        
        Wire.requestFrom((uint8_t)I2C_ADDR_MS5611, (uint8_t)2);
        if (Wire.available() < 2) {
            return false;
        }
        
        _calCoeffs[i] = (Wire.read() << 8) | Wire.read();
    }
    
    // Validate calibration (basic check - values shouldn't be 0 or 0xFFFF)
    for (uint8_t i = 0; i < 6; i++) {
        if (_calCoeffs[i] == 0 || _calCoeffs[i] == 0xFFFF) {
            return false;
        }
    }
    
    return true;
}

bool MS5611Sensor::update() {
    uint32_t now = micros();
    
    switch (_state) {
        case IDLE:
            // Start pressure conversion
            startConversion(CMD_CONVERT_D1 + OSR_CMD_OFFSET);
            _state = CONVERTING_PRESSURE;
            _conversionStart = now;
            return false;
            
        case CONVERTING_PRESSURE:
            if (now - _conversionStart >= CONVERSION_TIME_US) {
                _D1 = readADC();
                // Start temperature conversion
                startConversion(CMD_CONVERT_D2 + OSR_CMD_OFFSET);
                _state = CONVERTING_TEMP;
                _conversionStart = now;
            }
            return false;
            
        case CONVERTING_TEMP:
            if (now - _conversionStart >= CONVERSION_TIME_US) {
                _D2 = readADC();
                calculate();
                _state = IDLE;
                _data.timestamp = millis();
                return true;  // New data available
            }
            return false;
    }
    
    return false;
}

void MS5611Sensor::startConversion(uint8_t cmd) {
    Wire.beginTransmission(I2C_ADDR_MS5611);
    Wire.write(cmd);
    Wire.endTransmission();
}

uint32_t MS5611Sensor::readADC() {
    Wire.beginTransmission(I2C_ADDR_MS5611);
    Wire.write(CMD_ADC_READ);
    Wire.endTransmission();
    
    Wire.requestFrom((uint8_t)I2C_ADDR_MS5611, (uint8_t)3);
    
    uint32_t result = 0;
    if (Wire.available() >= 3) {
        result = (uint32_t)Wire.read() << 16;
        result |= (uint32_t)Wire.read() << 8;
        result |= Wire.read();
    }
    
    return result;
}

void MS5611Sensor::calculate() {
    // Algorithm from MS5611 datasheet
    // dT = D2 - C5 * 2^8
    int32_t dT = (int32_t)_D2 - ((int32_t)_calCoeffs[4] << 8);
    
    // TEMP = 2000 + dT * C6 / 2^23
    int32_t TEMP = 2000 + (((int64_t)dT * _calCoeffs[5]) >> 23);
    
    // OFF = C2 * 2^16 + (C4 * dT) / 2^7
    int64_t OFF = ((int64_t)_calCoeffs[1] << 16) + (((int64_t)_calCoeffs[3] * dT) >> 7);
    
    // SENS = C1 * 2^15 + (C3 * dT) / 2^8
    int64_t SENS = ((int64_t)_calCoeffs[0] << 15) + (((int64_t)_calCoeffs[2] * dT) >> 8);
    
    // Second order temperature compensation
    int32_t T2 = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;
    
    if (TEMP < 2000) {
        // Low temperature compensation
        T2 = ((int64_t)dT * dT) >> 31;
        int32_t temp_diff = TEMP - 2000;
        OFF2 = (5 * (int64_t)temp_diff * temp_diff) >> 1;
        SENS2 = (5 * (int64_t)temp_diff * temp_diff) >> 2;
        
        if (TEMP < -1500) {
            // Very low temperature compensation
            int32_t temp_diff2 = TEMP + 1500;
            OFF2 += 7 * (int64_t)temp_diff2 * temp_diff2;
            SENS2 += (11 * (int64_t)temp_diff2 * temp_diff2) >> 1;
        }
    }
    
    TEMP -= T2;
    OFF -= OFF2;
    SENS -= SENS2;
    
    // P = (D1 * SENS / 2^21 - OFF) / 2^15
    int32_t P = (((int64_t)_D1 * SENS >> 21) - OFF) >> 15;
    
    // Convert to output units
    _data.temperature_C = TEMP / 100.0f;  // centi-degrees to degrees
    _data.pressure_kPa = P / 1000.0f;     // Pa to kPa (P is in Pa * 10)
    
    // Note: Actual pressure in Pa = P / 100
    // But the formula gives P in Pa/100, so:
    _data.pressure_kPa = P / 100000.0f;   // Corrected
    
    _data.valid = (_data.pressure_kPa > 10.0f && _data.pressure_kPa < 120.0f);
}
