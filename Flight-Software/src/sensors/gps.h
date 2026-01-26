/**
 * @file gps.h
 * @brief NEO-M8N GPS Sensor Driver (NMEA Parser)
 * @team LeoNUS
 */

#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class GPSSensor {
public:
    GPSSensor();
    
    /**
     * @brief Initialize GPS serial port
     * @return true always (GPS may take time to acquire fix)
     */
    bool begin();
    
    /**
     * @brief Process incoming NMEA data
     * @return true if new fix available
     */
    bool update();
    
    /**
     * @brief Get latest GPS data
     */
    const GPSData& getData() const { return _data; }
    
private:
    GPSData _data;
    
    // NMEA sentence buffer
    char _sentence[128];
    uint8_t _sentenceIdx;
    bool _sentenceReady;
    
    // Parser state
    bool _gotGGA;
    bool _gotRMC;
    
    void processByte(char c);
    void parseSentence();
    void parseGGA(char* sentence);
    void parseRMC(char* sentence);
    
    // Helper functions
    float parseLatLon(const char* str, char dir);
    float parseFloat(const char* str);
    int parseInt(const char* str);
    char* getField(char* sentence, int fieldNum);
};

#endif // GPS_H
