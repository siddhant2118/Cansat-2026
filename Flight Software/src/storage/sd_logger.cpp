/**
 * @file sd_logger.cpp
 * @brief SD Card Logger Implementation
 * @team LeoNUS
 */

#include "sd_logger.h"
#include "../comms/telemetry.h"

SDLogger::SDLogger()
    : _available(false)
    , _bufferIdx(0)
    , _lastFlush(0)
{
    memset(_filename, 0, sizeof(_filename));
    memset(_buffer, 0, sizeof(_buffer));
}

bool SDLogger::begin() {
    if (!SD.begin(PIN_SD_CS)) {
        #if DEBUG_SERIAL
        Serial.println(F("SD card init failed"));
        #endif
        _available = false;
        return false;
    }
    
    _available = true;
    createNewFile();
    
    #if DEBUG_SERIAL
    Serial.print(F("SD logging to: "));
    Serial.println(_filename);
    #endif
    
    return true;
}

void SDLogger::createNewFile() {
    // Find next available file number
    // Format: LOG_<TEAMID>_<SEQ>.CSV
    
    int seq = 0;
    do {
        snprintf(_filename, sizeof(_filename), "LOG_%04u_%03d.CSV", TEAM_ID, seq);
        seq++;
    } while (SD.exists(_filename) && seq < 999);
    
    // Create file and write header
    _file = SD.open(_filename, FILE_WRITE);
    if (_file) {
        // Write CSV header
        _file.println(F("TEAM_ID,MISSION_TIME,PACKET_COUNT,MODE,STATE,ALTITUDE,"
                       "TEMPERATURE,PRESSURE,VOLTAGE,CURRENT,"
                       "GYRO_R,GYRO_P,GYRO_Y,ACCEL_R,ACCEL_P,ACCEL_Y,"
                       "GPS_TIME,GPS_ALTITUDE,GPS_LATITUDE,GPS_LONGITUDE,"
                       "GPS_SATS,CMD_ECHO"));
        _file.close();
    }
}

void SDLogger::logTelemetry(const TelemetryFrame& frame) {
    if (!_available) return;
    
    // Use telemetry manager to format (reuse formatting logic)
    TelemetryManager tm;
    char line[256];
    size_t len = tm.format(frame, line, sizeof(line));
    
    if (len > 0) {
        // Convert \r to \r\n for CSV compatibility
        if (line[len-1] == '\r') {
            line[len-1] = '\0';
            len--;
        }
        
        // Add to buffer
        if (_bufferIdx + len + 2 < LOG_BUFFER_SIZE) {
            memcpy(_buffer + _bufferIdx, line, len);
            _bufferIdx += len;
            _buffer[_bufferIdx++] = '\r';
            _buffer[_bufferIdx++] = '\n';
        } else {
            // Buffer full, flush first
            writeBuffer();
            memcpy(_buffer + _bufferIdx, line, len);
            _bufferIdx += len;
            _buffer[_bufferIdx++] = '\r';
            _buffer[_bufferIdx++] = '\n';
        }
    }
    
    // Periodic flush
    if (millis() - _lastFlush >= LOG_FLUSH_INTERVAL) {
        flush();
    }
}

void SDLogger::logEvent(const char* event, const char* message) {
    if (!_available) return;
    
    char line[128];
    int len;
    
    if (message) {
        len = snprintf(line, sizeof(line), "EVENT,%lu,%s,%s\r\n", 
                       millis(), event, message);
    } else {
        len = snprintf(line, sizeof(line), "EVENT,%lu,%s\r\n", 
                       millis(), event);
    }
    
    if (len > 0 && (size_t)len < sizeof(line)) {
        // Add to buffer
        if (_bufferIdx + len < LOG_BUFFER_SIZE) {
            memcpy(_buffer + _bufferIdx, line, len);
            _bufferIdx += len;
        } else {
            writeBuffer();
            memcpy(_buffer + _bufferIdx, line, len);
            _bufferIdx += len;
        }
    }
}

void SDLogger::flush() {
    if (_bufferIdx > 0) {
        writeBuffer();
    }
    _lastFlush = millis();
}

void SDLogger::writeBuffer() {
    if (!_available || _bufferIdx == 0) return;
    
    _file = SD.open(_filename, FILE_WRITE);
    if (_file) {
        _file.write(_buffer, _bufferIdx);
        _file.close();
    }
    
    _bufferIdx = 0;
}
