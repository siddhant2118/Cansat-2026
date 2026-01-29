import pytest
import sys
import os
import time
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from Backend.store import TelemetryStore
from Backend.parser import TelemetryPacket

def create_packet(seq):
    return TelemetryPacket(
        team_id=1000, mission_time="00:00", packet_count=seq, mode="F", state="S",
        altitude=0, temperature=0, pressure=0, voltage=0, current=0,
        gyro_r=0, gyro_p=0, gyro_y=0, accel_r=0, accel_p=0, accel_y=0,
        gps_time="", gps_altitude=0, gps_latitude=0, gps_longitude=0, gps_sats=0, cmd_echo="NONE"
    )

def test_link_age():
    store = TelemetryStore()
    assert store.get_link_age() > 900 # Initially infinite
    
    store.process_new_packet(create_packet(1))
    assert store.get_link_age() < 0.1 # Just received
    
    time.sleep(0.1)
    assert store.get_link_age() >= 0.1

def test_csv_ready():
    store = TelemetryStore()
    assert not store.csv_ready
    store.process_new_packet(create_packet(1))
    assert store.csv_ready
