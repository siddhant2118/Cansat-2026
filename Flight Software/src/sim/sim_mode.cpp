/**
 * @file sim_mode.cpp
 * @brief Simulation Mode Handler Implementation
 * @team LeoNUS
 * 
 * Simulation mode state machine:
 * DISABLED -> (SIM,ENABLE) -> ENABLED -> (SIM,ACTIVATE) -> ACTIVATED
 *    ^                                                          |
 *    +------------------------ (SIM,DISABLE) -------------------+
 */

#include "sim_mode.h"

// Default pressure (approximately sea level)
#define DEFAULT_PRESSURE_PA 101325

SimulationMode::SimulationMode()
    : _state(SimState::DISABLED)
    , _simulatedPressure_Pa(DEFAULT_PRESSURE_PA)
{
}

void SimulationMode::enable() {
    if (_state == SimState::DISABLED) {
        _state = SimState::ENABLED;
        
        #if DEBUG_SERIAL
        Serial.println(F("Simulation mode ENABLED"));
        #endif
    }
}

void SimulationMode::activate() {
    if (_state == SimState::ENABLED) {
        _state = SimState::ACTIVATED;
        
        #if DEBUG_SERIAL
        Serial.println(F("Simulation mode ACTIVATED"));
        #endif
    }
}

void SimulationMode::disable() {
    _state = SimState::DISABLED;
    _simulatedPressure_Pa = DEFAULT_PRESSURE_PA;
    
    #if DEBUG_SERIAL
    Serial.println(F("Simulation mode DISABLED"));
    #endif
}

void SimulationMode::setPressure(int32_t pressure_Pa) {
    if (_state == SimState::ACTIVATED) {
        _simulatedPressure_Pa = (float)pressure_Pa;
        
        #if DEBUG_SENSORS
        Serial.print(F("Sim pressure: "));
        Serial.println(pressure_Pa);
        #endif
    }
}
