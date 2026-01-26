/**
 * @file sim_mode.h
 * @brief Simulation Mode Handler
 * @team LeoNUS
 */

#ifndef SIM_MODE_H
#define SIM_MODE_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class SimulationMode {
public:
    SimulationMode();
    
    /**
     * @brief Enable simulation mode (step 1)
     */
    void enable();
    
    /**
     * @brief Activate simulation mode (step 2)
     */
    void activate();
    
    /**
     * @brief Disable and deactivate simulation mode
     */
    void disable();
    
    /**
     * @brief Set simulated pressure
     * @param pressure_Pa Pressure in Pascals
     */
    void setPressure(int32_t pressure_Pa);
    
    /**
     * @brief Get simulated pressure
     * @return Pressure in Pascals
     */
    float getSimulatedPressure() const { return _simulatedPressure_Pa; }
    
    /**
     * @brief Check if simulation is activated
     */
    bool isActivated() const { return _state == SimState::ACTIVATED; }
    
    /**
     * @brief Check if simulation is enabled (but not necessarily activated)
     */
    bool isEnabled() const { return _state != SimState::DISABLED; }
    
    /**
     * @brief Get current simulation state
     */
    SimState getState() const { return _state; }
    
    /**
     * @brief Set mode from persistence
     */
    void setMode(SimState state) { _state = state; }
    
private:
    SimState _state;
    float _simulatedPressure_Pa;
};

#endif // SIM_MODE_H
