/**
 * @file commands.cpp
 * @brief Command Handler Implementation
 * @team LeoNUS
 * 
 * Command formats per Mission Guide:
 * - CMD,<TEAM_ID>,CX,<ON|OFF>
 * - CMD,<TEAM_ID>,ST,<hh:mm:ss|GPS>
 * - CMD,<TEAM_ID>,SIM,<ENABLE|ACTIVATE|DISABLE>
 * - CMD,<TEAM_ID>,SIMP,<PRESSURE>
 * - CMD,<TEAM_ID>,CAL
 * - CMD,<TEAM_ID>,MEC,<DEVICE>,<ON|OFF>
 */

#include "commands.h"

// XBee Serial port
#define XBEE_SERIAL Serial2

CommandHandler::CommandHandler()
    : _head(0)
    , _tail(0)
    , _lineIdx(0)
{
    memset(_buffer, 0, sizeof(_buffer));
    memset(_lineBuffer, 0, sizeof(_lineBuffer));
}

void CommandHandler::begin() {
    // XBee serial already initialized by telemetry
    _head = 0;
    _tail = 0;
    _lineIdx = 0;
}

bool CommandHandler::poll(Command& cmd) {
    // Read all available bytes into ring buffer
    while (XBEE_SERIAL.available()) {
        char c = XBEE_SERIAL.read();
        bufferByte(c);
    }
    
    // Process ring buffer to find complete lines
    char c;
    while (readByte(c)) {
        if (c == '\r' || c == '\n') {
            if (_lineIdx > 0) {
                _lineBuffer[_lineIdx] = '\0';
                
                #if DEBUG_COMMANDS
                Serial.print(F("RX: "));
                Serial.println(_lineBuffer);
                #endif
                
                if (parseLine(_lineBuffer, cmd)) {
                    _lineIdx = 0;
                    return true;
                }
                _lineIdx = 0;
            }
        } else if (_lineIdx < CMD_MAX_LENGTH - 1) {
            _lineBuffer[_lineIdx++] = c;
        }
    }
    
    cmd.valid = false;
    return false;
}

bool CommandHandler::bufferByte(char c) {
    uint16_t nextHead = (_head + 1) % CMD_BUFFER_SIZE;
    if (nextHead == _tail) {
        // Buffer full, drop byte
        return false;
    }
    _buffer[_head] = c;
    _head = nextHead;
    return true;
}

bool CommandHandler::readByte(char& c) {
    if (_tail == _head) {
        return false;  // Buffer empty
    }
    c = _buffer[_tail];
    _tail = (_tail + 1) % CMD_BUFFER_SIZE;
    return true;
}

bool CommandHandler::parseLine(const char* line, Command& cmd) {
    cmd.valid = false;
    cmd.type = CommandType::UNKNOWN;
    memset(cmd.arg1, 0, sizeof(cmd.arg1));
    memset(cmd.arg2, 0, sizeof(cmd.arg2));
    cmd.numericArg = 0;
    
    // Commands must start with "CMD,"
    if (strncmp(line, "CMD,", 4) != 0) {
        return false;
    }
    
    // Parse TEAM_ID
    const char* p = line + 4;
    char* endPtr;
    long teamId = strtol(p, &endPtr, 10);
    if (endPtr == p || *endPtr != ',') {
        return false;
    }
    cmd.teamId = (uint16_t)teamId;
    p = endPtr + 1;
    
    // Parse command type
    if (strncmp(p, "CX,", 3) == 0) {
        cmd.type = CommandType::CX;
        p += 3;
        // Parse ON/OFF
        strncpy(cmd.arg1, p, sizeof(cmd.arg1) - 1);
        // Remove trailing whitespace/newline
        for (int i = strlen(cmd.arg1) - 1; i >= 0; i--) {
            if (cmd.arg1[i] == ' ' || cmd.arg1[i] == '\r' || cmd.arg1[i] == '\n') {
                cmd.arg1[i] = '\0';
            } else {
                break;
            }
        }
        cmd.valid = true;
        
    } else if (strncmp(p, "ST,", 3) == 0) {
        cmd.type = CommandType::ST;
        p += 3;
        strncpy(cmd.arg1, p, sizeof(cmd.arg1) - 1);
        // Clean up
        for (int i = strlen(cmd.arg1) - 1; i >= 0; i--) {
            if (cmd.arg1[i] == ' ' || cmd.arg1[i] == '\r' || cmd.arg1[i] == '\n') {
                cmd.arg1[i] = '\0';
            } else {
                break;
            }
        }
        cmd.valid = true;
        
    } else if (strncmp(p, "SIM,", 4) == 0) {
        cmd.type = CommandType::SIM;
        p += 4;
        strncpy(cmd.arg1, p, sizeof(cmd.arg1) - 1);
        for (int i = strlen(cmd.arg1) - 1; i >= 0; i--) {
            if (cmd.arg1[i] == ' ' || cmd.arg1[i] == '\r' || cmd.arg1[i] == '\n') {
                cmd.arg1[i] = '\0';
            } else {
                break;
            }
        }
        cmd.valid = true;
        
    } else if (strncmp(p, "SIMP,", 5) == 0) {
        cmd.type = CommandType::SIMP;
        p += 5;
        cmd.numericArg = strtol(p, nullptr, 10);
        cmd.valid = true;
        
    } else if (strncmp(p, "CAL", 3) == 0) {
        cmd.type = CommandType::CAL;
        // No arguments
        cmd.valid = true;
        
    } else if (strncmp(p, "MEC,", 4) == 0) {
        cmd.type = CommandType::MEC;
        p += 4;
        // Parse DEVICE
        const char* comma = strchr(p, ',');
        if (comma) {
            size_t deviceLen = comma - p;
            if (deviceLen < sizeof(cmd.arg1)) {
                strncpy(cmd.arg1, p, deviceLen);
                cmd.arg1[deviceLen] = '\0';
            }
            // Parse ON/OFF
            p = comma + 1;
            strncpy(cmd.arg2, p, sizeof(cmd.arg2) - 1);
            for (int i = strlen(cmd.arg2) - 1; i >= 0; i--) {
                if (cmd.arg2[i] == ' ' || cmd.arg2[i] == '\r' || cmd.arg2[i] == '\n') {
                    cmd.arg2[i] = '\0';
                } else {
                    break;
                }
            }
            cmd.valid = true;
        }
    }
    
    return cmd.valid;
}
