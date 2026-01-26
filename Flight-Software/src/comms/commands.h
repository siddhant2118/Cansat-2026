/**
 * @file commands.h
 * @brief Command Handler with Ring Buffer
 * @team LeoNUS
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class CommandHandler {
public:
    CommandHandler();
    
    /**
     * @brief Initialize command receiver
     */
    void begin();
    
    /**
     * @brief Poll for incoming commands
     * @param cmd Output: parsed command if available
     * @return true if a valid command was parsed
     */
    bool poll(Command& cmd);
    
private:
    // Ring buffer for incoming data
    char _buffer[CMD_BUFFER_SIZE];
    volatile uint16_t _head;
    volatile uint16_t _tail;
    
    // Line buffer for command parsing
    char _lineBuffer[CMD_MAX_LENGTH];
    uint8_t _lineIdx;
    
    // Internal methods
    bool bufferByte(char c);
    bool readByte(char& c);
    bool parseLine(const char* line, Command& cmd);
};

#endif // COMMANDS_H
