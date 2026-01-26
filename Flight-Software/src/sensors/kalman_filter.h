/**
 * @file kalman_filter.h
 * @brief 1D Kalman Filter for Altitude Estimation
 * @team LeoNUS 2026
 * 
 * Fuses Barometer (Altitude) + Accelerometer (Vertical Accel)
 * State x = [altitude, velocity]
 * 
 * Based on basic kinematic model:
 * alt_new = alt + vel*dt + 0.5*acc*dt^2
 * vel_new = vel + acc*dt
 */

#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include <Arduino.h>

/**
 * @brief Research-Grade 1D Adaptive Kalman Filter
 * @details Implements 3-state estimation [z, v, a_bias] based on:
 *          [1] Sabatini (2014): Closed-form Q-matrix from accel noise
 *          [2] Wu (2025): Bias state estimation for drift removal
 *          [3] Zhang/Wei: Residual-based adaptive outlier rejection
 */
class AltitudeKalman {
private:
    // State vector: [0]=Altitude(m), [1]=Velocity(m/s), [2]=AccelBias(m/s^2)
    float x[3];
    
    // Covariance matrix P (3x3)
    float P[3][3];
    
    // Process noise matrix Q (3x3) - Dynamic based on Sabatini formula
    float Q[3][3];
    
    // Measurement noise covariance R (scalar) - Adaptive
    float R;
    float base_R; // Baseline R from sensor specs
    
    // Physical constants from Sabatini [6] / MPU9250 Datasheet
    // Tuned for Rocket Dynamics (High process noise to track 5g acceleration changes)
    // Simulation Result: Lag < 1.7s with Q_accel=5.0 => Sigma ~= 2.25
    const float SIGMA_ACCEL = 2.25f;  
    const float SIGMA_BIAS  = 0.0001f; // Bias random walk (tuning param)

public:
    AltitudeKalman() {
        // Initial state
        x[0] = 0.0f; x[1] = 0.0f; x[2] = 0.0f;
        
        // Initial Uncertainty (P)
        // High uncertainty in bias initially
        for(int i=0; i<3; i++) for(int j=0; j<3; j++) P[i][j] = 0.0f;
        P[0][0] = 100.0f; 
        P[1][1] = 100.0f;
        P[2][2] = 1.0f; 

        // Baseline R (Tuned to 0.2m for faster response)
        base_R = 0.2f;
        R = base_R;
    }
    
    void configure(float meas_noise) {
        base_R = meas_noise;
        R = base_R;
    }
    
    void setState(float alt, float vel) {
        x[0] = alt;
        x[1] = vel;
        x[2] = 0.0f; // Reset bias guess
    }
    
    // Predict step (Kinematic model with Bias)
    // accel is vertical acceleration (m/s^2)
    void predict(float accel, float dt) {
        if (dt <= 0) return;

        // 1. State Prediction (x = F*x + B*u)
        // Correct accel with estimated bias: (accel - bias)
        float a_corr = accel - x[2];
        
        float dt2 = dt * dt;
        float dt3 = dt2 * dt;
        float dt4 = dt2 * dt2;
        
        x[0] += x[1] * dt + 0.5f * a_corr * dt2;
        x[1] += a_corr * dt;
        // x[2] (bias) stays constant (Random Walk model)
        
        // 2. Process Noise Q (Sabatini Formula [6])
        // Continuous Q = [0 0 0; 0 sigma_a^2 0; 0 0 sigma_b^2]
        // Discrete Q calculation:
        float sa2 = SIGMA_ACCEL * SIGMA_ACCEL;
        float sb2 = SIGMA_BIAS * SIGMA_BIAS;

        // Q elements derived from double integration of white noise
        // This is the "Research-Grade" replacement for magic numbers
        Q[0][0] = sa2 * dt4 / 4.0f;
        Q[0][1] = sa2 * dt3 / 2.0f;
        Q[0][2] = -sa2 * dt3 / 6.0f; // Correlation term!
        
        Q[1][0] = Q[0][1];
        Q[1][1] = sa2 * dt2;
        Q[1][2] = -sa2 * dt2 / 2.0f;
        
        Q[2][0] = Q[0][2];
        Q[2][1] = Q[1][2];
        Q[2][2] = sb2 * dt; // Bias random walk

        // 3. Covariance Prediction (P = F*P*F' + Q)
        // Jacobian F:
        // [1  dt  -0.5*dt^2]
        // [0  1   -dt      ]
        // [0  0   1        ]
        
        // Temp matrix to store F*P
        float FP[3][3];
        FP[0][0] = P[0][0] + dt*P[1][0] - 0.5f*dt2*P[2][0];
        FP[0][1] = P[0][1] + dt*P[1][1] - 0.5f*dt2*P[2][1];
        FP[0][2] = P[0][2] + dt*P[1][2] - 0.5f*dt2*P[2][2];
        
        FP[1][0] = P[1][0] - dt*P[2][0];
        FP[1][1] = P[1][1] - dt*P[2][1];
        FP[1][2] = P[1][2] - dt*P[2][2];
        
        FP[2][0] = P[2][0];
        FP[2][1] = P[2][1];
        FP[2][2] = P[2][2];
        
        // P = FP * F' + Q
        // Note: F' is transpose of F
        float F00=1, F01=dt, F02=-0.5f*dt2;
        float F10=0, F11=1,  F12=-dt;
        float F20=0, F21=0,  F22=1;
        
        P[0][0] = FP[0][0]*F00 + FP[0][1]*F01 + FP[0][2]*F02 + Q[0][0];
        P[0][1] = FP[0][0]*F10 + FP[0][1]*F11 + FP[0][2]*F12 + Q[0][1];
        P[0][2] = FP[0][0]*F20 + FP[0][1]*F21 + FP[0][2]*F22 + Q[0][2];
        
        P[1][0] = P[0][1]; // Symmetric
        P[1][1] = FP[1][0]*F10 + FP[1][1]*F11 + FP[1][2]*F12 + Q[1][1];
        P[1][2] = FP[1][0]*F20 + FP[1][1]*F21 + FP[1][2]*F22 + Q[1][2];
        
        P[2][0] = P[0][2];
        P[2][1] = P[1][2];
        P[2][2] = FP[2][0]*F20 + FP[2][1]*F21 + FP[2][2]*F22 + Q[2][2];
    }
    
    // Update step (Measurement)
    // z_meas is measured altitude from barometer
    void update(float z_meas) {
        // 1. Innovation (Residual)
        // y = z - H*x, H = [1 0 0]
        float y = z_meas - x[0];
        
        // 2. Innovation Covariance
        // S = H*P*H' + R
        float S = P[0][0] + R;
        
        // 3. Adaptive R Mechanism (Zhang/Wei [1,2])
        // If residual is > 3 sigma, inflate R to reject outlier
        float sigma = sqrt(S);
        if (abs(y) > 3.0f * sigma) {
            // Strong outlier (plume/shock)
            // Inflate R temporarily to reduce Kalman Gain
            // This protects the state from bad barometer data
            S = P[0][0] + (R * 100.0f); 
        } else {
            // Nominal
            S = P[0][0] + R;
        }

        // 4. Kalman Gain K = P*H' / S
        float K[3];
        K[0] = P[0][0] / S;
        K[1] = P[1][0] / S;
        K[2] = P[2][0] / S;
        
        // 5. Update State (x = x + K*y)
        x[0] += K[0] * y;
        x[1] += K[1] * y;
        x[2] += K[2] * y;
        
        // 6. Update Covariance (P = (I - K*H)*P)
        // Optimized for H=[1 0 0]
        float p00=P[0][0], p01=P[0][1], p02=P[0][2];
        
        for(int i=0; i<3; i++) {
            P[i][0] -= K[i] * p00;
            P[i][1] -= K[i] * p01;
            P[i][2] -= K[i] * p02;
        }
    }
    
    float getAltitude() { return x[0]; }
    float getVelocity() { return x[1]; }
    float getAccelBias() { return x[2]; }
};

#endif // KALMAN_FILTER_H
