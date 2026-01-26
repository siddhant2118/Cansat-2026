#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cstdint>

// Mocking Arduino/Teensy types for desktop simulation
typedef uint8_t byte;

// Simple Gaussian Random function
double generateGaussian(double mean, double stdDev) {
    static double z0, z1;
    static bool generate = false;
    generate = !generate;
    if (!generate) return z1 * stdDev + mean;
    double u1, u2;
    do {
       u1 = rand() * (1.0 / RAND_MAX);
       u2 = rand() * (1.0 / RAND_MAX);
    } while (u1 <= std::numeric_limits<double>::min());
    z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
    return z0 * stdDev + mean;
}

class ResearchGradeEKF {
public:
    // State: [h, v, a_bias]
    float h = 0.0f;
    float v = 0.0f;
    float b = 0.0f; // Bias
    
    // Covariance P (3x3)
    float P[3][3] = {
        {10.0f, 0, 0},
        {0, 10.0f, 0},
        {0, 0, 1.0f}
    };

    // TUNED PARAMETERS (Process Noise Higher => Faster Tracking)
    float Q_accel = 5.0f; 
    float R_baro = 0.2f; 

    void predict(float a_meas, float dt) {
        float accel_true = a_meas - b;
        
        h += v * dt + 0.5f * accel_true * dt * dt;
        v += accel_true * dt;
        
        // F matrix: [1, dt, -0.5dt^2; 0, 1, -dt; 0, 0, 1]
        float dt2 = dt*dt;
        float dt3 = dt2*dt;
        float dt4 = dt2*dt2;
        
        float P_old[3][3];
        for(int i=0; i<3; i++) for(int j=0; j<3; j++) P_old[i][j] = P[i][j];

        // FP = F * P
        float FP[3][3] = {0};
        
        for(int j=0; j<3; j++) 
            FP[0][j] = P_old[0][j] + dt * P_old[1][j] - 0.5f*dt2 * P_old[2][j];
            
        for(int j=0; j<3; j++)
            FP[1][j] = P_old[1][j] - dt * P_old[2][j];
            
        for(int j=0; j<3; j++)
            FP[2][j] = P_old[2][j];
            
        // P = FP * F^T + Q
        // Row 0
        P[0][0] = (FP[0][0] * 1.0f) + (FP[0][1] * dt) + (FP[0][2] * -0.5f*dt2) + Q_accel * dt4/4.0f; 
        P[0][1] = (FP[0][1] * 1.0f) + (FP[0][2] * -dt);
        P[0][2] = (FP[0][2] * 1.0f);
        
        // Row 1
        P[1][0] = P[0][1]; 
        P[1][1] = (FP[1][1] * 1.0f) + (FP[1][2] * -dt) + Q_accel * dt2;
        P[1][2] = (FP[1][2] * 1.0f);
        
        // Row 2
        P[2][0] = P[0][2];
        P[2][1] = P[1][2];
        P[2][2] = FP[2][2] + 0.0001f; 
    }

    void update(float z_baro) {
        // H = [1, 0, 0]
        float y = z_baro - h;
        float S = P[0][0] + R_baro;
        if (abs(y) > 5.0) S *= 10.0; // Simple adaptive outlier check (optional)

        float K[3];
        K[0] = P[0][0] / S;
        K[1] = P[1][0] / S;
        K[2] = P[2][0] / S;

        h += K[0] * y;
        v += K[1] * y;
        b += K[2] * y;

        // P = (I - KH)P
        float P_old[3][3];
        for(int i=0; i<3; i++) for(int j=0; j<3; j++) P_old[i][j] = P[i][j];

        // Row 0
        P[0][0] = (1.0f - K[0]) * P_old[0][0];
        P[0][1] = (1.0f - K[0]) * P_old[0][1];
        P[0][2] = (1.0f - K[0]) * P_old[0][2];

        // Row 1
        P[1][0] = -K[1] * P_old[0][0] + P_old[1][0];
        P[1][1] = -K[1] * P_old[0][1] + P_old[1][1];
        P[1][2] = -K[1] * P_old[0][2] + P_old[1][2];

        // Row 2
        P[2][0] = -K[2] * P_old[0][0] + P_old[2][0];
        P[2][1] = -K[2] * P_old[0][1] + P_old[2][1];
        P[2][2] = -K[2] * P_old[0][2] + P_old[2][2];
    }
};

int main() {
    std::cout << "Starting FSW Simulation Test..." << std::endl;
    std::cout << "Simulating Ascent -> Apogee -> Descent" << std::endl;
    std::cout << "Time(s), TrueAlt, MeasBaro, EstAlt, TrueVel, EstVel, EstBias" << std::endl;

    ResearchGradeEKF kf;
    
    float t = 0;
    float dt = 0.1f; 
    float true_alt = 0;
    float true_vel = 0; 
    float true_acc = 20.0f; 
    float true_bias = 0.5f; 
    
    double apogee_true_time = -1.0;
    double apogee_est_time = -1.0;

    for (int i = 0; i < 200; i++) {
        t += dt;
        
        if (t < 2.0f) { true_acc = 30.0f; } 
        else if (t < 5.0f) { true_acc = -9.81f; } 
        else if (t < 15.0f) { true_vel = -5.0f; true_acc = 0; } 
        else { true_vel = 0; true_alt = 0; } 

        if (t < 5.0f) {
           true_vel += true_acc * dt;
           true_alt += true_vel * dt; 
        } else if (t >= 5.0f && t < 15.0f) {
           true_vel = -5.0f; 
           true_alt += true_vel * dt;
        }

        if (true_alt < 0) true_alt = 0;
        
        if (true_vel < 0 && apogee_true_time < 0 && t > 2.0) {
            apogee_true_time = t;
        }

        float meas_baro = generateGaussian(true_alt, 1.0f); 
        float meas_acc = generateGaussian(true_acc + true_bias, 0.2f); 

        kf.predict(meas_acc, dt);
        kf.update(meas_baro);
        
        if (kf.v < 0 && apogee_est_time < 0 && t > 2.0) {
            apogee_est_time = t;
        }

        std::cout << std::fixed << std::setprecision(2) 
                  << t << ", " 
                  << true_alt << ", " 
                  << meas_baro << ", " 
                  << kf.h << ", "
                  << true_vel << ", "
                  << kf.v << ", "
                  << kf.b << std::endl;
    }
    
    std::cout << "\n--- SIMULATION RESULTS ---" << std::endl;
    std::cout << "True Apogee Time: " << apogee_true_time << " s" << std::endl;
    std::cout << "Est  Apogee Time: " << apogee_est_time << " s" << std::endl;
    std::cout << "LAG: " << (apogee_est_time - apogee_true_time) << " s" << std::endl;

    return 0;
}
