/**
 * @file scheduler.h
 * @brief Hardware IntervalTimer Scheduler for Teensy 4.1
 * @team LeoNUS 2026
 * 
 * Uses Teensy hardware timers for guaranteed timing:
 * - Timer 1: 1 Hz telemetry (critical for mission)
 * - Timer 2: 10 Hz sensor/FSM
 * - Timer 3: 20 Hz guidance
 * 
 * Teensy 4.1 has 4 IntervalTimers available.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

// Scheduler tick flags (set by ISR, cleared by main loop)
extern volatile bool tickTelemetry;   // 1 Hz
extern volatile bool tickSensor;      // 10 Hz  
extern volatile bool tickGuidance;    // 20 Hz
extern volatile bool tickHealth;      // 2 Hz

/**
 * @brief Initialize hardware timers
 * Call this in setup() AFTER all peripherals are initialized
 */
void schedulerInit();

/**
 * @brief Stop all timers (for emergency/testing)
 */
void schedulerStop();

/**
 * @brief Check if scheduler is running
 */
bool schedulerIsRunning();

#endif // SCHEDULER_H
