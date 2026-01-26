/**
 * @file scheduler.cpp
 * @brief Hardware IntervalTimer Scheduler Implementation
 * @team LeoNUS 2026
 */

#include "scheduler.h"

// Teensy hardware timers
IntervalTimer timerTelemetry;
IntervalTimer timerSensor;
IntervalTimer timerGuidance;
IntervalTimer timerHealth;

// Tick flags (volatile - modified in ISR)
volatile bool tickTelemetry = false;
volatile bool tickSensor = false;
volatile bool tickGuidance = false;
volatile bool tickHealth = false;

static bool _running = false;

// ============================================================================
// ISR Callbacks (keep minimal - just set flags)
// ============================================================================

void isrTelemetry() {
    tickTelemetry = true;
}

void isrSensor() {
    tickSensor = true;
}

void isrGuidance() {
    tickGuidance = true;
}

void isrHealth() {
    tickHealth = true;
}

// ============================================================================
// Public Functions
// ============================================================================

void schedulerInit() {
    // 1 Hz telemetry - CRITICAL for mission (1,000,000 μs = 1 second)
    timerTelemetry.begin(isrTelemetry, 1000000);
    
    // 10 Hz sensor/FSM (100,000 μs = 100 ms)
    timerSensor.begin(isrSensor, 100000);
    
    // 20 Hz guidance (50,000 μs = 50 ms)
    timerGuidance.begin(isrGuidance, 50000);
    
    // 2 Hz health check (500,000 μs = 500 ms)
    timerHealth.begin(isrHealth, 500000);
    
    _running = true;
}

void schedulerStop() {
    timerTelemetry.end();
    timerSensor.end();
    timerGuidance.end();
    timerHealth.end();
    
    _running = false;
}

bool schedulerIsRunning() {
    return _running;
}
