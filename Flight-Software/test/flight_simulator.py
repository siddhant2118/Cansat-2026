#!/usr/bin/env python3
"""
Flight Profile Simulator for CanSat 2026 FSW Testing

This script simulates a complete flight by sending SIMP (simulated pressure)
commands to the FSW via serial, and monitors telemetry responses to verify
correct state transitions.

Usage:
    python flight_simulator.py --port /dev/ttyACM0

Requirements:
    pip install pyserial
"""

import argparse
import serial
import time
import math
from dataclasses import dataclass
from typing import List, Optional

# ============================================================================
# FLIGHT PROFILE CONFIGURATION
# ============================================================================

TEAM_ID = "1000"  # Change to your team ID

# Pressure at sea level (Pa)
SEA_LEVEL_PRESSURE = 101325.0

# Flight profile (time in seconds, altitude in meters)
FLIGHT_PROFILE = [
    # (time_s, altitude_m, expected_state)
    (0,    0,     "LAUNCH_PAD"),
    (5,    0,     "LAUNCH_PAD"),     # Waiting on pad
    (10,   50,    "ASCENT"),         # Launch detected
    (15,   200,   "ASCENT"),
    (20,   400,   "ASCENT"),
    (25,   600,   "ASCENT"),
    (30,   800,   "ASCENT"),
    (35,   950,   "ASCENT"),
    (40,   1000,  "APOGEE"),         # Peak altitude
    (45,   900,   "DESCENT"),        # Descending
    (50,   800,   "PROBE_RELEASE"),  # 80% of peak = 800m
    (55,   600,   "DESCENT"),        # After separation
    (60,   400,   "DESCENT"),
    (65,   200,   "DESCENT"),
    (70,   50,    "DESCENT"),
    (75,   10,    "DESCENT"),
    (80,   2,     "PAYLOAD_RELEASE"), # 2m AGL - egg release
    (85,   0,     "LANDED"),          # On ground
    (90,   0,     "LANDED"),
    (95,   0,     "LANDED"),          # Confirm landed (5s)
]

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def altitude_to_pressure(altitude_m: float, ground_pressure: float = SEA_LEVEL_PRESSURE) -> float:
    """Convert altitude to pressure using barometric formula."""
    # Standard atmosphere model
    # P = P0 * (1 - L*h/T0)^(g*M/(R*L))
    # Simplified: P ≈ P0 * exp(-h/8500)
    return ground_pressure * math.exp(-altitude_m / 8500.0)


def parse_telemetry(line: str) -> Optional[dict]:
    """Parse a telemetry line into a dictionary."""
    try:
        parts = line.strip().split(',')
        if len(parts) < 10:
            return None
        
        return {
            'team_id': parts[0],
            'mission_time': parts[1],
            'packet_count': parts[2],
            'mode': parts[3],
            'state': parts[4],
            'altitude': float(parts[5]) if parts[5] else 0,
            'raw': line.strip()
        }
    except (ValueError, IndexError):
        return None


# ============================================================================
# MAIN SIMULATOR
# ============================================================================

class FlightSimulator:
    def __init__(self, port: str, baudrate: int = 9600):
        self.port = port
        self.baudrate = baudrate
        self.serial: Optional[serial.Serial] = None
        self.start_time = 0
        self.test_results: List[dict] = []
        
    def connect(self) -> bool:
        """Connect to the FSW via serial (XBee or direct USB)."""
        try:
            self.serial = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # Wait for connection to stabilize
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def send_command(self, cmd: str):
        """Send a command to the FSW."""
        full_cmd = f"CMD,{TEAM_ID},{cmd}\n"
        self.serial.write(full_cmd.encode('ascii'))
        print(f"  → Sent: {full_cmd.strip()}")
        time.sleep(0.1)
    
    def read_telemetry(self, timeout: float = 2.0) -> Optional[dict]:
        """Read and parse a telemetry line."""
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.serial.in_waiting:
                line = self.serial.readline().decode('ascii', errors='ignore')
                if line.startswith(TEAM_ID):
                    return parse_telemetry(line)
        return None
    
    def enable_simulation_mode(self):
        """Enable and activate simulation mode."""
        print("\n[1/4] Enabling simulation mode...")
        self.send_command("SIM,ENABLE")
        time.sleep(0.5)
        self.send_command("SIM,ACTIVATE")
        time.sleep(0.5)
        self.send_command("CX,ON")  # Enable telemetry
        time.sleep(0.5)
        print("  ✓ Simulation mode active")
    
    def run_flight_profile(self):
        """Run through the flight profile and verify state transitions."""
        print("\n[2/4] Running flight profile...")
        print("-" * 60)
        
        self.start_time = time.time()
        last_altitude = 0
        
        for profile_time, altitude, expected_state in FLIGHT_PROFILE:
            # Wait for the right time
            target_time = self.start_time + profile_time
            wait_time = target_time - time.time()
            if wait_time > 0:
                time.sleep(wait_time)
            
            # Convert altitude to pressure
            pressure = altitude_to_pressure(altitude)
            
            # Send simulated pressure
            self.send_command(f"SIMP,{pressure:.0f}")
            
            # Wait for and read telemetry
            time.sleep(0.5)
            telem = self.read_telemetry()
            
            if telem:
                actual_state = telem.get('state', 'UNKNOWN')
                actual_alt = telem.get('altitude', 0)
                
                # Check if state matches expected
                passed = actual_state == expected_state
                status = "✓ PASS" if passed else "✗ FAIL"
                
                print(f"  t={profile_time:3d}s | Alt={altitude:4d}m | "
                      f"Expected: {expected_state:15s} | "
                      f"Actual: {actual_state:15s} | {status}")
                
                self.test_results.append({
                    'time': profile_time,
                    'altitude': altitude,
                    'expected': expected_state,
                    'actual': actual_state,
                    'passed': passed
                })
            else:
                print(f"  t={profile_time:3d}s | No telemetry received!")
                self.test_results.append({
                    'time': profile_time,
                    'altitude': altitude,
                    'expected': expected_state,
                    'actual': 'NO_TELEM',
                    'passed': False
                })
            
            last_altitude = altitude
    
    def disable_simulation_mode(self):
        """Disable simulation mode."""
        print("\n[3/4] Disabling simulation mode...")
        self.send_command("SIM,DISABLE")
        print("  ✓ Simulation mode disabled")
    
    def print_summary(self):
        """Print test summary."""
        print("\n[4/4] Test Summary")
        print("=" * 60)
        
        passed = sum(1 for r in self.test_results if r['passed'])
        total = len(self.test_results)
        
        print(f"  Passed: {passed}/{total}")
        
        if passed == total:
            print("  ✓ ALL TESTS PASSED!")
        else:
            print("  ✗ SOME TESTS FAILED:")
            for r in self.test_results:
                if not r['passed']:
                    print(f"    - t={r['time']}s: Expected {r['expected']}, got {r['actual']}")
        
        print("=" * 60)
    
    def run(self):
        """Run the complete test sequence."""
        print("=" * 60)
        print("  CanSat 2026 Flight Simulator")
        print("  Testing FSW State Transitions")
        print("=" * 60)
        
        if not self.connect():
            return False
        
        try:
            self.enable_simulation_mode()
            self.run_flight_profile()
            self.disable_simulation_mode()
            self.print_summary()
            return all(r['passed'] for r in self.test_results)
        
        finally:
            if self.serial:
                self.serial.close()


# ============================================================================
# ENTRY POINT
# ============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CanSat 2026 Flight Simulator")
    parser.add_argument("--port", "-p", default="/dev/ttyACM0",
                        help="Serial port (default: /dev/ttyACM0)")
    parser.add_argument("--baud", "-b", type=int, default=9600,
                        help="Baud rate (default: 9600)")
    args = parser.parse_args()
    
    simulator = FlightSimulator(args.port, args.baud)
    success = simulator.run()
    
    exit(0 if success else 1)
