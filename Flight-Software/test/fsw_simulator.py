#!/usr/bin/env python3
"""
CanSat 2026 FSW Logic Simulator (No Hardware Required)

This simulates the Flight State Machine logic on your Mac.
Run it to see exactly how the FSW would behave during a flight.

Usage:
    python fsw_simulator.py

What to observe:
    ✓ State transitions happen at correct altitudes
    ✓ Peak altitude is correctly recorded
    ✓ 80% calculation triggers PROBE_RELEASE
    ✓ 2m AGL triggers PAYLOAD_RELEASE
    ✓ Landed detection works after 5 seconds still
"""

import time
import math
from dataclasses import dataclass
from enum import Enum
from typing import List, Tuple

# ============================================================================
# CONFIGURATION (matches config.h)
# ============================================================================

TEAM_ID = 1000

# Flight thresholds
ALT_LAUNCH_THRESHOLD = 20.0      # meters - detect launch
ALT_VELOCITY_LAUNCH = 5.0        # m/s - velocity for launch
ALT_EGG_RELEASE = 2.0            # meters AGL - egg release
PAYLOAD_RELEASE_RATIO = 0.80     # 80% of peak

# Timing
APOGEE_CONFIRM_SAMPLES = 3       # consecutive samples to confirm apogee
LANDED_CONFIRM_SECONDS = 5.0     # seconds of no movement for landed


# ============================================================================
# FSW STATE MACHINE (ported from C++)
# ============================================================================

class FlightState(Enum):
    LAUNCH_PAD = "LAUNCH_PAD"
    ASCENT = "ASCENT"
    APOGEE = "APOGEE"
    DESCENT = "DESCENT"
    PROBE_RELEASE = "PROBE_RELEASE"
    PAYLOAD_RELEASE = "PAYLOAD_RELEASE"
    LANDED = "LANDED"


@dataclass
class SensorData:
    altitude: float = 0.0
    velocity: float = 0.0
    time_s: float = 0.0


class FlightStateMachine:
    """Python port of the C++ FSM logic."""
    
    def __init__(self):
        self.state = FlightState.LAUNCH_PAD
        self.peak_altitude = 0.0
        self.ground_altitude = 0.0
        self.apogee_countdown = APOGEE_CONFIRM_SAMPLES
        self.landed_timer_start = 0.0
        
        # Actuator flags (one-shot)
        self.separation_fired = False
        self.egg_released = False
        
        # For velocity calculation
        self.last_altitude = 0.0
        self.last_time = 0.0
        
        # Telemetry
        self.packet_count = 0
        
    def update(self, data: SensorData) -> Tuple[FlightState, str]:
        """
        Update state machine with new sensor data.
        Returns (new_state, event_description)
        """
        old_state = self.state
        event = ""
        
        # Calculate velocity
        dt = data.time_s - self.last_time
        if dt > 0:
            velocity = (data.altitude - self.last_altitude) / dt
        else:
            velocity = 0.0
        data.velocity = velocity
        
        # Track peak altitude
        if data.altitude > self.peak_altitude:
            self.peak_altitude = data.altitude
        
        # STATE MACHINE LOGIC
        if self.state == FlightState.LAUNCH_PAD:
            # Transition to ASCENT when altitude > 20m AND velocity > 5 m/s
            if data.altitude > ALT_LAUNCH_THRESHOLD and velocity > ALT_VELOCITY_LAUNCH:
                self.state = FlightState.ASCENT
                event = f"Launch detected! Alt={data.altitude:.1f}m, Vel={velocity:.1f}m/s"
                
        elif self.state == FlightState.ASCENT:
            # Transition to APOGEE when velocity <= 0 for 3 samples
            if velocity <= 0:
                self.apogee_countdown -= 1
                if self.apogee_countdown <= 0:
                    self.state = FlightState.APOGEE
                    event = f"Apogee reached! Peak altitude = {self.peak_altitude:.1f}m"
            else:
                self.apogee_countdown = APOGEE_CONFIRM_SAMPLES
                
        elif self.state == FlightState.APOGEE:
            # Immediately transition to DESCENT
            self.state = FlightState.DESCENT
            event = "Descending..."
            
        elif self.state == FlightState.DESCENT:
            # Transition to PROBE_RELEASE at 80% of peak
            release_altitude = self.peak_altitude * PAYLOAD_RELEASE_RATIO
            if data.altitude <= release_altitude and not self.separation_fired:
                self.state = FlightState.PROBE_RELEASE
                self.separation_fired = True
                event = f"PROBE RELEASE at {data.altitude:.1f}m (80% of {self.peak_altitude:.1f}m = {release_altitude:.1f}m)"
                
        elif self.state == FlightState.PROBE_RELEASE:
            # Return to descent-like behavior, check for egg release
            if data.altitude <= ALT_EGG_RELEASE and not self.egg_released:
                self.state = FlightState.PAYLOAD_RELEASE
                self.egg_released = True
                event = f"PAYLOAD (EGG) RELEASE at {data.altitude:.1f}m AGL"
            # Also check for landed
            elif abs(velocity) < 0.5:
                if self.landed_timer_start == 0:
                    self.landed_timer_start = data.time_s
                elif data.time_s - self.landed_timer_start >= LANDED_CONFIRM_SECONDS:
                    self.state = FlightState.LANDED
                    event = "LANDED - velocity near zero for 5 seconds"
            else:
                self.landed_timer_start = 0
                
        elif self.state == FlightState.PAYLOAD_RELEASE:
            # Check for landed
            if abs(velocity) < 0.5:
                if self.landed_timer_start == 0:
                    self.landed_timer_start = data.time_s
                elif data.time_s - self.landed_timer_start >= LANDED_CONFIRM_SECONDS:
                    self.state = FlightState.LANDED
                    event = "LANDED - velocity near zero for 5 seconds"
            else:
                self.landed_timer_start = 0
                
        elif self.state == FlightState.LANDED:
            # Terminal state
            pass
        
        # Save for next iteration
        self.last_altitude = data.altitude
        self.last_time = data.time_s
        self.packet_count += 1
        
        return self.state, event


# ============================================================================
# FLIGHT PROFILE GENERATOR
# ============================================================================

def generate_flight_profile(
    peak_altitude: float = 1000.0,
    ascent_rate: float = 30.0,
    descent_rate: float = 5.0,
    pad_time: float = 10.0
) -> List[Tuple[float, float]]:
    """
    Generate a realistic flight altitude profile.
    Returns list of (time_s, altitude_m) tuples.
    """
    profile = []
    t = 0.0
    dt = 1.0  # 1 second intervals (1 Hz telemetry)
    
    # Phase 1: On launch pad
    while t < pad_time:
        profile.append((t, 0.0))
        t += dt
    
    # Phase 2: Ascent (rocket carries CanSat up)
    altitude = 0.0
    while altitude < peak_altitude:
        altitude += ascent_rate * dt
        if altitude > peak_altitude:
            altitude = peak_altitude
        profile.append((t, altitude))
        t += dt
    
    # Phase 3: At apogee (brief moment)
    profile.append((t, peak_altitude))
    t += dt
    profile.append((t, peak_altitude - 10))  # Start descending
    t += dt
    
    # Phase 4: Descent (para-glider)
    altitude = peak_altitude - 20
    while altitude > 0:
        altitude -= descent_rate * dt
        if altitude < 0:
            altitude = 0
        profile.append((t, altitude))
        t += dt
    
    # Phase 5: Landed (stay at 0 for a while)
    for _ in range(10):
        profile.append((t, 0.0))
        t += dt
    
    return profile


# ============================================================================
# MAIN SIMULATION
# ============================================================================

def run_simulation():
    print("=" * 70)
    print("  CanSat 2026 FSW Logic Simulator")
    print("  No Hardware Required - Pure Python")
    print("=" * 70)
    print()
    
    # Create FSM
    fsm = FlightStateMachine()
    
    # Generate flight profile
    print("[1] Generating flight profile...")
    print("    - Peak altitude: 503m (from OpenRocket)")
    print("    - Ascent rate: 30 m/s (rocket)")
    print("    - Descent rate: 5 m/s (para-glider target)")
    print()
    
    profile = generate_flight_profile(
        peak_altitude=503.0,
        ascent_rate=30.0,
        descent_rate=5.0,
        pad_time=10.0
    )
    
    # Run simulation
    print("[2] Running simulation...")
    print("-" * 70)
    print(f"{'Time':>6s} | {'Altitude':>10s} | {'Velocity':>10s} | {'State':>17s} | Event")
    print("-" * 70)
    
    last_state = None
    events = []
    
    for t, altitude in profile:
        data = SensorData(altitude=altitude, time_s=t)
        state, event = fsm.update(data)
        
        # Print every line, highlight state changes
        if state != last_state or event:
            marker = ">>>" if event else "   "
            print(f"{t:6.1f}s | {altitude:10.1f}m | {data.velocity:+10.1f}m/s | {state.value:>17s} | {marker} {event}")
            if event:
                events.append((t, event))
        last_state = state
    
    # Summary
    print("-" * 70)
    print()
    print("[3] Simulation Complete!")
    print()
    print("=" * 70)
    print("  KEY EVENTS TO VERIFY")
    print("=" * 70)
    
    for t, event in events:
        print(f"  t={t:6.1f}s: {event}")
    
    print()
    print("=" * 70)
    print("  WHAT TO CHECK")
    print("=" * 70)
    print("""
  ✓ LAUNCH detected when altitude > 20m AND velocity > 5 m/s
  ✓ APOGEE detected when velocity goes negative (3 samples)
  ✓ PROBE_RELEASE triggers at exactly 80% of peak (800m for 1000m peak)
  ✓ PAYLOAD_RELEASE triggers at 2m AGL (egg release)
  ✓ LANDED triggers after 5 seconds of near-zero velocity
  
  If these all happen correctly, the FSW logic is working!
""")
    print("=" * 70)


if __name__ == "__main__":
    run_simulation()
