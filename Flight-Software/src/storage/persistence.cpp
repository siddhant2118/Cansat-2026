/**
 * @file persistence.cpp
 * @brief EEPROM Persistence Manager Implementation
 * @team LeoNUS
 */

#include "persistence.h"

PersistenceManager::PersistenceManager()
    : _valid(false)
{
}

void PersistenceManager::begin() {
    // Teensy 4.1 uses emulated EEPROM (part of flash)
    // Size is configurable, but we only need ~55 bytes
    
    _valid = validateMagic();
    
    #if DEBUG_SERIAL
    if (_valid) {
        Serial.println(F("EEPROM: Valid data found"));
    } else {
        Serial.println(F("EEPROM: No valid data"));
    }
    #endif
}

bool PersistenceManager::validateMagic() {
    uint32_t magic;
    EEPROM.get(OFFSET_MAGIC, magic);
    return (magic == EEPROM_MAGIC);
}

void PersistenceManager::writeMagic() {
    uint32_t magic = EEPROM_MAGIC;
    EEPROM.put(OFFSET_MAGIC, magic);
}

PersistentData PersistenceManager::load() {
    PersistentData data;
    memset(&data, 0, sizeof(data));
    
    if (!_valid) {
        return data;
    }
    
    EEPROM.get(OFFSET_MAGIC, data.magic);
    EEPROM.get(OFFSET_PACKET_COUNT, data.packetCount);
    EEPROM.get(OFFSET_MODE, data.mode);
    EEPROM.get(OFFSET_STATE, data.state);
    EEPROM.get(OFFSET_PEAK_ALT, data.peakAltitude);
    EEPROM.get(OFFSET_GROUND_PRESS, data.groundPressure);
    EEPROM.get(OFFSET_ACTUATED, data.actuatedFlags);
    
    for (int i = 0; i < CMD_ECHO_SIZE; i++) {
        data.cmdEcho[i] = EEPROM.read(OFFSET_CMD_ECHO + i);
    }
    data.cmdEcho[CMD_ECHO_SIZE - 1] = '\0';
    
    return data;
}

void PersistenceManager::saveAll(uint32_t packetCount, uint8_t mode, uint8_t state,
                                  float peakAltitude, float groundPressure,
                                  uint8_t actuatedFlags, const char* cmdEcho) {
    writeMagic();
    EEPROM.put(OFFSET_PACKET_COUNT, packetCount);
    EEPROM.put(OFFSET_MODE, mode);
    EEPROM.put(OFFSET_STATE, state);
    EEPROM.put(OFFSET_PEAK_ALT, peakAltitude);
    EEPROM.put(OFFSET_GROUND_PRESS, groundPressure);
    EEPROM.put(OFFSET_ACTUATED, actuatedFlags);
    
    for (int i = 0; i < CMD_ECHO_SIZE; i++) {
        if (cmdEcho[i] != '\0') {
            EEPROM.write(OFFSET_CMD_ECHO + i, cmdEcho[i]);
        } else {
            EEPROM.write(OFFSET_CMD_ECHO + i, '\0');
            break;
        }
    }
    
    _valid = true;
}

void PersistenceManager::savePacketCount(uint32_t count) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_PACKET_COUNT, count);
}

void PersistenceManager::saveMode(uint8_t mode) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_MODE, mode);
}

void PersistenceManager::saveState(uint8_t state) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_STATE, state);
}

void PersistenceManager::savePeakAltitude(float altitude) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_PEAK_ALT, altitude);
}

void PersistenceManager::saveGroundPressure(float pressure) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_GROUND_PRESS, pressure);
}

void PersistenceManager::saveActuatedFlags(uint8_t flags) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    EEPROM.put(OFFSET_ACTUATED, flags);
}

void PersistenceManager::saveCmdEcho(const char* echo) {
    if (!_valid) {
        writeMagic();
        _valid = true;
    }
    
    for (int i = 0; i < CMD_ECHO_SIZE; i++) {
        if (echo[i] != '\0') {
            EEPROM.write(OFFSET_CMD_ECHO + i, echo[i]);
        } else {
            EEPROM.write(OFFSET_CMD_ECHO + i, '\0');
            break;
        }
    }
}

void PersistenceManager::clear() {
    // Write invalid magic
    uint32_t invalidMagic = 0;
    EEPROM.put(OFFSET_MAGIC, invalidMagic);
    _valid = false;
}

uint32_t PersistenceManager::calculateChecksum() {
    // Simple checksum over stored data
    uint32_t sum = 0;
    for (int i = OFFSET_MAGIC; i < OFFSET_CHECKSUM; i++) {
        sum += EEPROM.read(i);
    }
    return sum;
}
