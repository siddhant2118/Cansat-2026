/**
 * @file gps.cpp
 * @brief NEO-M8N GPS Sensor Driver (NMEA Parser) Implementation
 * @team LeoNUS
 */

#include "gps.h"

// Use Serial1 for GPS
#define GPS_SERIAL Serial1

GPSSensor::GPSSensor()
    : _sentenceIdx(0)
    , _sentenceReady(false)
    , _gotGGA(false)
    , _gotRMC(false)
{
    memset(&_data, 0, sizeof(_data));
    memset(_sentence, 0, sizeof(_sentence));
}

bool GPSSensor::begin() {
    GPS_SERIAL.begin(GPS_BAUD);
    
    // GPS will take time to acquire satellites
    // Just initialize the serial port here
    _data.valid = false;
    
    return true;
}

bool GPSSensor::update() {
    bool newFix = false;
    
    // Process all available bytes
    while (GPS_SERIAL.available()) {
        char c = GPS_SERIAL.read();
        processByte(c);
        
        if (_sentenceReady) {
            parseSentence();
            _sentenceReady = false;
            _sentenceIdx = 0;
            
            // We have a complete fix when we get both GGA and RMC
            if (_gotGGA && _gotRMC) {
                _data.timestamp = millis();
                _data.valid = (_data.satellites >= GPS_MIN_SATS);
                _gotGGA = false;
                _gotRMC = false;
                newFix = true;
            }
        }
    }
    
    return newFix;
}

void GPSSensor::processByte(char c) {
    if (c == '$') {
        // Start of new sentence
        _sentenceIdx = 0;
        _sentence[_sentenceIdx++] = c;
    } else if (c == '\r' || c == '\n') {
        // End of sentence
        if (_sentenceIdx > 0) {
            _sentence[_sentenceIdx] = '\0';
            _sentenceReady = true;
        }
    } else {
        // Add to buffer
        if (_sentenceIdx < sizeof(_sentence) - 1) {
            _sentence[_sentenceIdx++] = c;
        }
    }
}

void GPSSensor::parseSentence() {
    // Validate checksum (optional for speed, but recommended)
    // Format: $GPGGA,...*XX where XX is checksum
    
    // Identify sentence type
    if (strncmp(_sentence + 3, "GGA", 3) == 0) {
        parseGGA(_sentence);
        _gotGGA = true;
    } else if (strncmp(_sentence + 3, "RMC", 3) == 0) {
        parseRMC(_sentence);
        _gotRMC = true;
    }
}

/**
 * Parse GGA sentence (Global Positioning System Fix Data)
 * Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,b,q,ss,h.h,a.a,M,g.g,M,t.t,nnnn*cc
 * Fields:
 *   1: UTC time
 *   2,3: Latitude, N/S
 *   4,5: Longitude, E/W
 *   6: Fix quality
 *   7: Number of satellites
 *   8: HDOP
 *   9,10: Altitude, M
 */
void GPSSensor::parseGGA(char* sentence) {
    char* work = sentence;
    
    // Skip $GPGGA,
    char* time = getField(work, 1);
    char* lat = getField(work, 2);
    char* latDir = getField(work, 3);
    char* lon = getField(work, 4);
    char* lonDir = getField(work, 5);
    char* quality = getField(work, 6);
    char* sats = getField(work, 7);
    char* hdop = getField(work, 8);
    char* alt = getField(work, 9);
    
    // Parse time
    if (time && strlen(time) >= 6) {
        _data.hour = (time[0] - '0') * 10 + (time[1] - '0');
        _data.minute = (time[2] - '0') * 10 + (time[3] - '0');
        _data.second = (time[4] - '0') * 10 + (time[5] - '0');
        _data.timeSynced = true;
    }
    
    // Parse position
    if (lat && latDir && strlen(lat) > 0) {
        _data.latitude = parseLatLon(lat, latDir[0]);
    }
    
    if (lon && lonDir && strlen(lon) > 0) {
        _data.longitude = parseLatLon(lon, lonDir[0]);
        // Mission guide wants degrees West as positive
        if (lonDir[0] == 'W') {
            _data.longitude = -_data.longitude;  // Store as negative for west
        }
    }
    
    // Parse fix quality
    if (quality && quality[0] != '0') {
        // Fix quality: 0=no fix, 1=GPS, 2=DGPS
    }
    
    // Parse satellites
    if (sats) {
        _data.satellites = parseInt(sats);
    }
    
    // Parse HDOP
    if (hdop) {
        _data.hdop = parseFloat(hdop);
    }
    
    // Parse altitude
    if (alt) {
        _data.altitude_m = parseFloat(alt);
    }
}

/**
 * Parse RMC sentence (Recommended Minimum Navigation Information)
 * Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,b,s.s,c.c,ddmmyy,m.m,e,mode*cc
 * Fields:
 *   1: UTC time
 *   2: Status (A=valid, V=invalid)
 *   3,4: Latitude, N/S
 *   5,6: Longitude, E/W
 *   7: Speed over ground (knots)
 *   8: Course over ground (degrees)
 */
void GPSSensor::parseRMC(char* sentence) {
    char* work = sentence;
    
    char* status = getField(work, 2);
    char* speed = getField(work, 7);
    char* course = getField(work, 8);
    
    // Check validity
    if (status && status[0] == 'A') {
        // Valid fix
    }
    
    // Parse speed (knots to m/s)
    if (speed && strlen(speed) > 0) {
        _data.speed_mps = parseFloat(speed) * 0.514444f;  // knots to m/s
    }
    
    // Parse course
    if (course && strlen(course) > 0) {
        _data.courseOverGround = parseFloat(course);
    }
}

float GPSSensor::parseLatLon(const char* str, char dir) {
    // Format: DDDMM.MMMMM or DDMM.MMMMM
    float value = parseFloat(str);
    
    // Extract degrees and minutes
    int degrees = (int)(value / 100);
    float minutes = value - (degrees * 100);
    
    // Convert to decimal degrees
    float result = degrees + (minutes / 60.0f);
    
    // Apply direction
    if (dir == 'S' || dir == 'W') {
        result = -result;
    }
    
    return result;
}

float GPSSensor::parseFloat(const char* str) {
    if (!str || strlen(str) == 0) return 0.0f;
    return atof(str);
}

int GPSSensor::parseInt(const char* str) {
    if (!str || strlen(str) == 0) return 0;
    return atoi(str);
}

char* GPSSensor::getField(char* sentence, int fieldNum) {
    static char fieldBuffer[16];
    int currentField = 0;
    int fieldStart = 0;
    int i = 0;
    
    while (sentence[i] != '\0') {
        if (sentence[i] == ',' || sentence[i] == '*') {
            if (currentField == fieldNum) {
                int len = i - fieldStart;
                if (len > 15) len = 15;
                strncpy(fieldBuffer, sentence + fieldStart, len);
                fieldBuffer[len] = '\0';
                return fieldBuffer;
            }
            currentField++;
            fieldStart = i + 1;
        }
        i++;
    }
    
    // Handle last field
    if (currentField == fieldNum) {
        int len = i - fieldStart;
        if (len > 15) len = 15;
        strncpy(fieldBuffer, sentence + fieldStart, len);
        fieldBuffer[len] = '\0';
        return fieldBuffer;
    }
    
    return nullptr;
}
