/**
 * @file fsm.cpp
 * @brief Flight State Machine Implementation
 * @team LeoNUS
 */

#include "fsm.h"

FlightStateMachine::FlightStateMachine()
    : _state(FlightState::LAUNCH_PAD)
    , _prevState(FlightState::LAUNCH_PAD)
    , _stateChanged(false)
    , _peakAltitude(0)
    , _releaseAltitude(0)
    , _descendingCount(0)
    , _landingStartTime(0)
    , _landingDetectionActive(false)
    , _landingAltitude(0)
{
}

void FlightStateMachine::begin() {
    _state = FlightState::LAUNCH_PAD;
    _prevState = FlightState::LAUNCH_PAD;
    _stateChanged = false;
    _peakAltitude = 0;
    _releaseAltitude = 0;
    _descendingCount = 0;
}

void FlightStateMachine::update(const SensorData& data) {
    _prevState = _state;
    
    // Track peak altitude during any flight phase
    if (data.pressure.altitude_m > _peakAltitude) {
        _peakAltitude = data.pressure.altitude_m;
        _releaseAltitude = _peakAltitude * PAYLOAD_RELEASE_RATIO;
    }
    
    // State machine logic
    switch (_state) {
        case FlightState::LAUNCH_PAD:
            checkLaunchPad(data);
            break;
            
        case FlightState::ASCENT:
            checkAscent(data);
            break;
            
        case FlightState::APOGEE:
            checkApogee(data);
            break;
            
        case FlightState::DESCENT:
            checkDescent(data);
            break;
            
        case FlightState::PROBE_RELEASE:
            checkProbeRelease(data);
            break;
            
        case FlightState::PAYLOAD_RELEASE:
            checkPayloadRelease(data);
            break;
            
        case FlightState::LANDED:
            checkLanded(data);
            break;
    }
    
    // Track state changes
    _stateChanged = (_state != _prevState);
    
    #if DEBUG_FSM
    if (_stateChanged) {
        Serial.print(F("FSM: "));
        Serial.print(flightStateToString(_prevState));
        Serial.print(F(" -> "));
        Serial.println(flightStateToString(_state));
    }
    #endif
}

void FlightStateMachine::setState(FlightState state) {
    _prevState = _state;
    _state = state;
    _stateChanged = true;
}

void FlightStateMachine::restoreState(FlightState state) {
    // Restore state from persistence - don't mark as changed
    _state = state;
    _prevState = state;
    _stateChanged = false;
    
    #if DEBUG_SERIAL
    Serial.print(F("FSM restored to: "));
    Serial.println(flightStateToString(state));
    #endif
}

bool FlightStateMachine::stateChanged() {
    bool changed = _stateChanged;
    _stateChanged = false;  // Clear after reading
    return changed;
}

void FlightStateMachine::transitionTo(FlightState newState) {
    if (_state != newState) {
        _state = newState;
    }
}

// ============================================================================
// STATE TRANSITION CHECKS
// ============================================================================

void FlightStateMachine::checkLaunchPad(const SensorData& data) {
    // Transition to ASCENT when:
    // - Altitude exceeds threshold AND
    // - Vertical velocity indicates upward movement
    
    if (data.pressure.altitude_m > ALT_LAUNCH_THRESHOLD &&
        data.verticalVelocity > ALT_VELOCITY_LAUNCH) {
        transitionTo(FlightState::ASCENT);
    }
}

void FlightStateMachine::checkAscent(const SensorData& data) {
    // Transition to APOGEE when:
    // - Vertical velocity goes negative (or near zero) for several samples
    
    if (data.verticalVelocity <= VEL_APOGEE_THRESHOLD) {
        _descendingCount++;
        if (_descendingCount >= APOGEE_CONFIRM_SAMPLES) {
            transitionTo(FlightState::APOGEE);
            _descendingCount = 0;
        }
    } else {
        _descendingCount = 0;
    }
}

void FlightStateMachine::checkApogee(const SensorData& data) {
    // APOGEE is a transient state - immediately transition to DESCENT
    // This captures the moment of peak altitude before descent begins
    
    transitionTo(FlightState::DESCENT);
}

void FlightStateMachine::checkDescent(const SensorData& data) {
    // Container descending with parachute
    // Transition to PROBE_RELEASE when altitude drops to 80% of peak
    
    if (data.pressure.altitude_m <= _releaseAltitude && _releaseAltitude > 0) {
        transitionTo(FlightState::PROBE_RELEASE);
    }
}

void FlightStateMachine::checkProbeRelease(const SensorData& data) {
    // Payload has separated, para-glider deploying
    // Transition to PAYLOAD_RELEASE when altitude reaches 2m AGL
    
    if (data.pressure.altitude_m <= ALT_EGG_RELEASE) {
        transitionTo(FlightState::PAYLOAD_RELEASE);
    }
}

void FlightStateMachine::checkPayloadRelease(const SensorData& data) {
    // Egg has been released
    // Transition to LANDED when:
    // - Altitude is low AND
    // - Vertical velocity is near zero for extended period
    
    if (data.pressure.altitude_m < ALT_LANDED_THRESHOLD) {
        if (!_landingDetectionActive) {
            _landingDetectionActive = true;
            _landingStartTime = millis();
            _landingAltitude = data.pressure.altitude_m;
        }
        
        // Check if altitude stable and velocity low
        float altChange = abs(data.pressure.altitude_m - _landingAltitude);
        
        if (abs(data.verticalVelocity) < VEL_LANDED_THRESHOLD && altChange < 1.0f) {
            if (millis() - _landingStartTime >= LANDED_CONFIRM_MS) {
                transitionTo(FlightState::LANDED);
            }
        } else {
            // Reset if movement detected
            _landingStartTime = millis();
            _landingAltitude = data.pressure.altitude_m;
        }
    } else {
        _landingDetectionActive = false;
    }
}

void FlightStateMachine::checkLanded(const SensorData& data) {
    // Terminal state - no transitions
    // Audio beacon should be active (handled externally)
    (void)data;  // Unused
}
