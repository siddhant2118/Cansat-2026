/**
 * @file adaptive_pid.h
 * @brief Adaptive PID Controller (Gain Scheduling)
 * @team LeoNUS 2026
 * 
 * Implements gain scheduling based on error magnitude.
 * - Small error: Conservative gains (stability)
 * - Large error: Aggressive gains (response)
 */

#ifndef ADAPTIVE_PID_H
#define ADAPTIVE_PID_H

#include <Arduino.h>

class AdaptivePID {
public:
    // Conservative gains (for small errors < 10 degrees)
    float kp_conservative;
    float ki_conservative;
    float kd_conservative;

    // Aggressive gains (for large errors > 45 degrees)
    float kp_aggressive;
    float ki_aggressive;
    float kd_aggressive;

    // Thresholds
    float error_conservative_deg = 10.0f;
    float error_aggressive_deg = 45.0f;

    // State
    float integral;
    float prevError;
    float integralMax;
    float lastOutput;

    AdaptivePID() {
        // Defaults
        kp_conservative = 1.0f;
        ki_conservative = 0.0f;
        kd_conservative = 0.1f;

        kp_aggressive = 2.5f;
        ki_aggressive = 0.0f;
        kd_aggressive = 0.5f;

        integralMax = 30.0f;
        reset();
    }

    void reset() {
        integral = 0;
        prevError = 0;
        lastOutput = 0;
    }

    float compute(float error, float dt) {
        if (dt <= 0) return lastOutput;

        // 1. Determine gains based on error magnitude
        float absError = abs(error);
        float kp, ki, kd;

        if (absError <= error_conservative_deg) {
            // Pure conservative
            kp = kp_conservative;
            ki = ki_conservative;
            kd = kd_conservative;
        } 
        else if (absError >= error_aggressive_deg) {
            // Pure aggressive
            kp = kp_aggressive;
            ki = ki_aggressive;
            kd = kd_aggressive;
        } 
        else {
            // Linear interpolation
            float ratio = (absError - error_conservative_deg) / 
                          (error_aggressive_deg - error_conservative_deg);
            
            kp = kp_conservative + ratio * (kp_aggressive - kp_conservative);
            ki = ki_conservative + ratio * (ki_aggressive - ki_conservative);
            kd = kd_conservative + ratio * (kd_aggressive - kd_conservative);
        }

        // 2. Standard PID calc using scheduled gains
        // Proportional
        float p = kp * error;

        // Integral
        integral += error * dt;
        integral = constrain(integral, -integralMax, integralMax);
        float i = ki * integral;

        // Derivative
        float derivative = (error - prevError) / dt;
        float d = kd * derivative;

        prevError = error;
        lastOutput = p + i + d;
        return lastOutput;
    }
};

#endif // ADAPTIVE_PID_H
