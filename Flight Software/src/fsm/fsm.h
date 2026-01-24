/**
 * @file fsm.h
 * @brief Flight State Machine
 * @team LeoNUS
 * 
 * States: LAUNCH_PAD -> ASCENT -> APOGEE -> DESCENT -> 
 *         PROBE_RELEASE -> PAYLOAD_RELEASE -> LANDED
 */

#ifndef FSM_H
#define FSM_H

#include <Arduino.h>
#include "../config.h"
#include "../types.h"

class FlightStateMachine {
public:
    FlightStateMachine();
    
    /**
     * @brief Initialize FSM to LAUNCH_PAD
     */
    void begin();
    
    /**
     * @brief Update FSM based on sensor data
     * @param data Current sensor readings
     */
    void update(const SensorData& data);
    
    /**
     * @brief Get current flight state
     */
    FlightState getState() const { return _state; }
    
    /**
     * @brief Force set state (for recovery)
     */
    void setState(FlightState state);
    
    /**
     * @brief Restore state from persistence
     */
    void restoreState(FlightState state);
    
    /**
     * @brief Check if state changed since last check
     */
    bool stateChanged();
    
    /**
     * @brief Get peak altitude
     */
    float getPeakAltitude() const { return _peakAltitude; }
    
private:
    FlightState _state;
    FlightState _prevState;
    bool _stateChanged;
    
    // Tracking variables
    float _peakAltitude;
    float _releaseAltitude;  // 80% of peak
    
    // Apogee detection
    int _descendingCount;
    
    // Landing detection
    uint32_t _landingStartTime;
    bool _landingDetectionActive;
    float _landingAltitude;
    
    // State transition functions
    void checkLaunchPad(const SensorData& data);
    void checkAscent(const SensorData& data);
    void checkApogee(const SensorData& data);
    void checkDescent(const SensorData& data);
    void checkProbeRelease(const SensorData& data);
    void checkPayloadRelease(const SensorData& data);
    void checkLanded(const SensorData& data);
    
    void transitionTo(FlightState newState);
};

#endif // FSM_H
