/**
 * @file persistence.h
 * @brief EEPROM Persistence Manager
 * @team LeoNUS
 */

#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "../config.h"
#include "../types.h"

class PersistenceManager {
public:
    PersistenceManager();
    
    /**
     * @brief Initialize persistence system
     */
    void begin();
    
    /**
     * @brief Check if stored data is valid
     */
    bool isValid() const { return _valid; }
    
    /**
     * @brief Load all persistent data
     */
    PersistentData load();
    
    /**
     * @brief Save all persistent data
     */
    void saveAll(uint32_t packetCount, uint8_t mode, uint8_t state,
                 float peakAltitude, float groundPressure, 
                 uint8_t actuatedFlags, const char* cmdEcho);
    
    // Individual field saves (for efficiency)
    void savePacketCount(uint32_t count);
    void saveMode(uint8_t mode);
    void saveState(uint8_t state);
    void savePeakAltitude(float altitude);
    void saveGroundPressure(float pressure);
    void saveActuatedFlags(uint8_t flags);
    void saveCmdEcho(const char* echo);
    
    /**
     * @brief Clear all stored data
     */
    void clear();
    
private:
    bool _valid;
    
    // EEPROM offsets
    static const int OFFSET_MAGIC = 0;
    static const int OFFSET_PACKET_COUNT = 4;
    static const int OFFSET_MODE = 8;
    static const int OFFSET_STATE = 9;
    static const int OFFSET_PEAK_ALT = 10;
    static const int OFFSET_GROUND_PRESS = 14;
    static const int OFFSET_ACTUATED = 18;
    static const int OFFSET_CMD_ECHO = 19;
    static const int OFFSET_CHECKSUM = 51;
    
    void writeMagic();
    bool validateMagic();
    uint32_t calculateChecksum();
};

#endif // PERSISTENCE_H
