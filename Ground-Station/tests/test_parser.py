import pytest
import sys
import os

# Path adjustment for tests if running from root
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from Backend.parser import parse_telemetry_line, TelemetryPacket

def test_valid_parsing():
    line = "1000,10:00:00,1,F,ASCENT,100.5,25.0,101.3,7.4,0.5,0.1,0.2,0.3,0.1,0.1,9.8,10:00:01,105.0,37.4,78.2,5,CMD_ECHO"
    packet = parse_telemetry_line(line)
    
    assert packet is not None
    assert packet.team_id == 1000
    assert packet.mission_time == "10:00:00"
    assert packet.packet_count == 1
    assert packet.altitude == 100.5
    assert packet.cmd_echo == "CMD_ECHO"

def test_invalid_field_count():
    line = "1000,10:00:00" # Too short
    packet = parse_telemetry_line(line)
    assert packet is None

def test_invalid_type_conversion():
    # Altitude is "XYZ" -> invalid
    line = "1000,10:00:00,1,F,ASCENT,XYZ,25.0,101.3,7.4,0.5,0.1,0.2,0.3,0.1,0.1,9.8,10:00:01,105.0,37.4,78.2,5,CMD_ECHO"
    packet = parse_telemetry_line(line)
    assert packet is None
