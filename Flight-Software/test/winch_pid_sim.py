#!/usr/bin/env python3
"""
Winch PID Simulator - Tune PID gains without hardware
Team LeoNUS 2026

Simulates the winch servo + encoder system to find good PID values
before uploading to the Teensy.

Usage:
    python3 test/winch_pid_sim.py

The simulator models:
- SPT5525LV-360 continuous rotation servo (55 RPM @ 6V)
- AS5600 encoder (12-bit, 0.087°/step)
- Winch drum dynamics
"""

import matplotlib.pyplot as plt
import numpy as np

# ============================================================================
# SERVO PARAMETERS (from SPT5525LV-360 specs)
# ============================================================================
SERVO_MAX_RPM = 55.0        # Max speed at 6V
SERVO_ACCEL = 500.0         # Degrees/s² (estimated)
SERVO_DEADBAND = 4          # μs deadband
PWM_CENTER = 1500           # Neutral PWM
PWM_MIN = 500               # Full CCW
PWM_MAX = 2500              # Full CW

# ============================================================================
# SIMULATION PARAMETERS
# ============================================================================
DT = 0.02                   # 50ms (20 Hz control loop)
SIM_TIME = 5.0              # 5 second simulation
TARGET_ANGLE = 90.0         # Target position (degrees)
START_ANGLE = 0.0           # Initial position

# ============================================================================
# PID PARAMETERS (tune these!)
# ============================================================================
KP = 2.0                    # Proportional gain
KI = 0.0                    # Integral gain  
KD = 0.1                    # Derivative gain
DEADBAND = 2.0              # Stop when within this range (degrees)


class WinchSimulator:
    """Simulates winch servo + encoder system"""
    
    def __init__(self, kp=KP, ki=KI, kd=KD):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        
        # State
        self.position = START_ANGLE     # Current angle (degrees)
        self.velocity = 0.0             # Current velocity (deg/s)
        self.target = TARGET_ANGLE      # Target angle
        
        # PID state
        self.integral = 0.0
        self.prev_error = 0.0
        
        # History for plotting
        self.time_history = []
        self.position_history = []
        self.error_history = []
        self.output_history = []
        
    def compute_pid(self, error, dt):
        """Compute PID output"""
        # Proportional
        p_term = self.kp * error
        
        # Integral (with anti-windup)
        self.integral += error * dt
        self.integral = np.clip(self.integral, -100, 100)
        i_term = self.ki * self.integral
        
        # Derivative
        derivative = (error - self.prev_error) / dt if dt > 0 else 0
        d_term = self.kd * derivative
        self.prev_error = error
        
        return p_term + i_term + d_term
    
    def step(self, dt):
        """Simulate one timestep"""
        # Calculate error
        error = self.target - self.position
        
        # Check deadband
        if abs(error) <= DEADBAND:
            # At target - stop
            self.velocity = 0
            return
        
        # Compute PID output
        output = self.compute_pid(error, dt)
        
        # Convert output to servo speed (deg/s)
        # Output is roughly PWM offset from center
        max_speed = SERVO_MAX_RPM * 6.0  # Convert to deg/s (55 RPM = 330 deg/s)
        target_velocity = np.clip(output, -max_speed, max_speed)
        
        # Apply acceleration limit (servo can't instantly change speed)
        accel = np.clip(target_velocity - self.velocity, -SERVO_ACCEL * dt, SERVO_ACCEL * dt)
        self.velocity += accel
        
        # Update position
        self.position += self.velocity * dt
        
        # Store history
        self.time_history.append(len(self.time_history) * dt)
        self.position_history.append(self.position)
        self.error_history.append(error)
        self.output_history.append(output)
    
    def run(self, duration=SIM_TIME):
        """Run simulation"""
        steps = int(duration / DT)
        for _ in range(steps):
            self.step(DT)
        
        return self.analyze()
    
    def analyze(self):
        """Analyze performance"""
        if len(self.position_history) == 0:
            return {}
        
        final_error = abs(self.target - self.position_history[-1])
        
        # Find rise time (10% to 90%)
        threshold_10 = START_ANGLE + 0.1 * (TARGET_ANGLE - START_ANGLE)
        threshold_90 = START_ANGLE + 0.9 * (TARGET_ANGLE - START_ANGLE)
        
        t_10 = None
        t_90 = None
        for i, pos in enumerate(self.position_history):
            if t_10 is None and pos >= threshold_10:
                t_10 = self.time_history[i]
            if t_90 is None and pos >= threshold_90:
                t_90 = self.time_history[i]
        
        rise_time = (t_90 - t_10) if (t_10 and t_90) else None
        
        # Find overshoot
        max_pos = max(self.position_history)
        overshoot = max(0, (max_pos - TARGET_ANGLE) / TARGET_ANGLE * 100)
        
        # Find settling time (within 2% of target)
        settling_time = None
        for i in range(len(self.position_history) - 1, -1, -1):
            if abs(self.position_history[i] - TARGET_ANGLE) > 0.02 * TARGET_ANGLE:
                settling_time = self.time_history[min(i + 1, len(self.time_history) - 1)]
                break
        
        return {
            'final_error': final_error,
            'rise_time': rise_time,
            'overshoot': overshoot,
            'settling_time': settling_time
        }
    
    def plot(self, title=None):
        """Plot results"""
        fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
        
        # Position plot
        axes[0].plot(self.time_history, self.position_history, 'b-', linewidth=2)
        axes[0].axhline(y=TARGET_ANGLE, color='r', linestyle='--', label='Target')
        axes[0].axhline(y=TARGET_ANGLE + DEADBAND, color='g', linestyle=':', alpha=0.5)
        axes[0].axhline(y=TARGET_ANGLE - DEADBAND, color='g', linestyle=':', alpha=0.5)
        axes[0].set_ylabel('Position (°)')
        axes[0].legend()
        axes[0].grid(True, alpha=0.3)
        
        # Error plot
        axes[1].plot(self.time_history, self.error_history, 'r-', linewidth=2)
        axes[1].set_ylabel('Error (°)')
        axes[1].grid(True, alpha=0.3)
        
        # Output plot
        axes[2].plot(self.time_history, self.output_history, 'g-', linewidth=2)
        axes[2].set_ylabel('PID Output')
        axes[2].set_xlabel('Time (s)')
        axes[2].grid(True, alpha=0.3)
        
        if title:
            fig.suptitle(title)
        else:
            fig.suptitle(f'Winch PID Response (Kp={self.kp}, Ki={self.ki}, Kd={self.kd})')
        
        plt.tight_layout()
        return fig


def tune_comparison():
    """Compare different PID settings"""
    settings = [
        {'kp': 1.0, 'ki': 0.0, 'kd': 0.0, 'label': 'P only (Kp=1)'},
        {'kp': 2.0, 'ki': 0.0, 'kd': 0.0, 'label': 'P only (Kp=2)'},
        {'kp': 2.0, 'ki': 0.0, 'kd': 0.1, 'label': 'PD (Kp=2, Kd=0.1)'},
        {'kp': 2.0, 'ki': 0.5, 'kd': 0.1, 'label': 'PID (Kp=2, Ki=0.5, Kd=0.1)'},
    ]
    
    fig, ax = plt.subplots(figsize=(12, 6))
    
    print("=" * 60)
    print("WINCH PID TUNING COMPARISON")
    print("=" * 60)
    print(f"{'Setting':<35} {'Rise(s)':<10} {'OS%':<10} {'Settle(s)':<10}")
    print("-" * 60)
    
    for s in settings:
        sim = WinchSimulator(kp=s['kp'], ki=s['ki'], kd=s['kd'])
        results = sim.run()
        
        ax.plot(sim.time_history, sim.position_history, label=s['label'], linewidth=2)
        
        rise = f"{results['rise_time']:.2f}" if results['rise_time'] else "N/A"
        settle = f"{results['settling_time']:.2f}" if results['settling_time'] else "N/A"
        print(f"{s['label']:<35} {rise:<10} {results['overshoot']:.1f}%{'':<5} {settle:<10}")
    
    print("=" * 60)
    
    ax.axhline(y=TARGET_ANGLE, color='k', linestyle='--', alpha=0.5, label='Target')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Position (°)')
    ax.set_title('Winch PID Tuning Comparison')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    return fig


def main():
    """Main entry point"""
    print("\n" + "=" * 60)
    print("  WINCH PID SIMULATOR - Team LeoNUS 2026")
    print("=" * 60)
    print(f"\nServo: SPT5525LV-360 (55 RPM)")
    print(f"Target: {START_ANGLE}° → {TARGET_ANGLE}°")
    print(f"Deadband: ±{DEADBAND}°")
    print(f"Control rate: {1/DT:.0f} Hz")
    
    # Run single simulation with current gains
    print(f"\n--- Current PID Gains ---")
    print(f"Kp = {KP}, Ki = {KI}, Kd = {KD}")
    
    sim = WinchSimulator()
    results = sim.run()
    
    print(f"\n--- Results ---")
    print(f"Final error: {results['final_error']:.2f}°")
    if results['rise_time']:
        print(f"Rise time:   {results['rise_time']:.3f} s")
    print(f"Overshoot:   {results['overshoot']:.1f}%")
    if results['settling_time']:
        print(f"Settling:    {results['settling_time']:.3f} s")
    
    # Show comparison plot
    print("\n--- Generating comparison plot ---")
    tune_comparison()
    
    # Also plot single response
    sim.plot()
    
    plt.show()
    
    # Print recommended gains
    print("\n" + "=" * 60)
    print("  RECOMMENDED STARTING GAINS")
    print("=" * 60)
    print("For config.h:")
    print(f"  #define WINCH_KP  2.0f")
    print(f"  #define WINCH_KI  0.5f")
    print(f"  #define WINCH_KD  0.1f")
    print("\nAdjust based on actual hardware testing!")
    print("=" * 60)


if __name__ == "__main__":
    main()
