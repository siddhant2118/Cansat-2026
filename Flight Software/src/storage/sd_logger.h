/**
 * @file sd_logger.h
 * @brief SD Card Logger
 * @team LeoNUS
 */

#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include "../config.h"
#include "../types.h"

class SDLogger {
public:
    SDLogger();
    
    /**
     * @brief Initialize SD card and create log file
     * @return true if successful
     */
    bool begin();
    
    /**
     * @brief Log telemetry frame
     * @param frame Telemetry data
     */
    void logTelemetry(const TelemetryFrame& frame);
    
    /**
     * @brief Log event with message
     * @param event Event type/name
     * @param message Additional message
     */
    void logEvent(const char* event, const char* message = nullptr);
    
    /**
     * @brief Flush buffer to SD
     */
    void flush();
    
    /**
     * @brief Check if SD is available
     */
    bool isAvailable() const { return _available; }
    
private:
    bool _available;
    File _file;
    char _filename[32];
    
    // Write buffer
    char _buffer[LOG_BUFFER_SIZE];
    size_t _bufferIdx;
    uint32_t _lastFlush;
    
    void createNewFile();
    void writeBuffer();
};

#endif // SD_LOGGER_H
