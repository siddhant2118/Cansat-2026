/**
 * @file test_as5600.cpp
 * @brief AS5600 Encoder Test Sketch
 * @team LeoNUS 2026
 * 
 * Upload this to Teensy 4.1 to test the AS5600 magnetic encoders.
 * Prints encoder readings to Serial monitor.
 * 
 * Wiring (with TCA9548A multiplexer):
 *   Teensy SDA (18) -> TCA9548A SDA
 *   Teensy SCL (19) -> TCA9548A SCL
 *   TCA9548A SD0/SC0 -> Left AS5600
 *   TCA9548A SD1/SC1 -> Right AS5600
 *   All VCC -> 3.3V
 *   All GND -> GND
 * 
 * Without multiplexer (single encoder):
 *   Teensy SDA (18) -> AS5600 SDA
 *   Teensy SCL (19) -> AS5600 SCL
 */

#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// Configuration - Adjust these for your setup
// ============================================================================
#define USE_MULTIPLEXER     true    // Set to false if testing single encoder
#define LEFT_MUX_CHANNEL    0       // TCA9548A channel for left encoder
#define RIGHT_MUX_CHANNEL   1       // TCA9548A channel for right encoder

#define TCA9548A_ADDRESS    0x70
#define AS5600_ADDRESS      0x36
#define AS5600_ANGLE_H      0x0C
#define AS5600_STATUS       0x0B

// ============================================================================
// Helper Functions
// ============================================================================

void selectMuxChannel(uint8_t channel) {
    if (!USE_MULTIPLEXER) return;
    
    Wire.beginTransmission(TCA9548A_ADDRESS);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

bool checkMagnet() {
    Wire.beginTransmission(AS5600_ADDRESS);
    Wire.write(AS5600_STATUS);
    Wire.endTransmission(false);
    
    Wire.requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)1);
    if (Wire.available()) {
        uint8_t status = Wire.read();
        return (status & 0x20) != 0;  // MD bit = magnet detected
    }
    return false;
}

uint16_t readAngleRaw() {
    Wire.beginTransmission(AS5600_ADDRESS);
    Wire.write(AS5600_ANGLE_H);
    Wire.endTransmission(false);
    
    Wire.requestFrom((uint8_t)AS5600_ADDRESS, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint16_t high = Wire.read();
        uint16_t low = Wire.read();
        return ((high << 8) | low) & 0x0FFF;
    }
    return 0;
}

float readAngleDegrees() {
    return readAngleRaw() * 360.0f / 4096.0f;
}

// ============================================================================
// Main
// ============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);  // Wait for Serial (max 3 sec)
    
    Wire.begin();
    Wire.setClock(400000);  // 400 kHz I2C
    
    Serial.println("======================================");
    Serial.println("  AS5600 Encoder Test");
    Serial.println("  Team LeoNUS 2026");
    Serial.println("======================================");
    Serial.println();
    
    if (USE_MULTIPLEXER) {
        Serial.println("Mode: Using TCA9548A I2C Multiplexer");
        Serial.print("  Left encoder on channel: ");
        Serial.println(LEFT_MUX_CHANNEL);
        Serial.print("  Right encoder on channel: ");
        Serial.println(RIGHT_MUX_CHANNEL);
    } else {
        Serial.println("Mode: Single encoder (no multiplexer)");
    }
    Serial.println();
    
    // Check encoders
    Serial.println("Checking encoders...");
    
    if (USE_MULTIPLEXER) {
        selectMuxChannel(LEFT_MUX_CHANNEL);
        Serial.print("  Left encoder:  ");
        Serial.println(checkMagnet() ? "OK (magnet detected)" : "FAIL (no magnet!)");
        
        selectMuxChannel(RIGHT_MUX_CHANNEL);
        Serial.print("  Right encoder: ");
        Serial.println(checkMagnet() ? "OK (magnet detected)" : "FAIL (no magnet!)");
    } else {
        Serial.print("  Encoder: ");
        Serial.println(checkMagnet() ? "OK (magnet detected)" : "FAIL (no magnet!)");
    }
    
    Serial.println();
    Serial.println("Starting continuous reading...");
    Serial.println("Rotate the magnets to see angle changes.");
    Serial.println();
    
    if (USE_MULTIPLEXER) {
        Serial.println("Left (deg)  |  Right (deg)");
        Serial.println("------------|-------------");
    } else {
        Serial.println("Angle (deg)");
        Serial.println("-----------");
    }
}

void loop() {
    if (USE_MULTIPLEXER) {
        // Read left encoder
        selectMuxChannel(LEFT_MUX_CHANNEL);
        float leftAngle = readAngleDegrees();
        
        // Read right encoder
        selectMuxChannel(RIGHT_MUX_CHANNEL);
        float rightAngle = readAngleDegrees();
        
        // Print both
        Serial.print(leftAngle, 1);
        Serial.print("         |  ");
        Serial.println(rightAngle, 1);
    } else {
        float angle = readAngleDegrees();
        Serial.println(angle, 1);
    }
    
    delay(100);  // 10 Hz update
}
