import pytest
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from Backend.serial_handler import SerialHandler

class MockSerial:
    def __init__(self):
        self.is_open = True
        self.write_buffer = b""
        self.in_waiting = 0
    
    def write(self, data):
        self.write_buffer += data
    
    def flush(self):
        pass

def test_command_format():
    handler = SerialHandler()
    handler.serial_port = MockSerial()
    handler.running = True
    
    # Test 1: Standard Command with \r
    handler.write_queue = []
    handler.send_command("CMD,1000,CAL")
    # Expected: \r termination (0x0D), NO \n (0x0A)
    assert len(handler.write_queue) == 1
    assert handler.write_queue[0] == b"CMD,1000,CAL\r" 
    assert b"\n" not in handler.write_queue[0]

    # Test 2: SIMP Command in Pa
    handler.write_queue = []
    handler.send_command("CMD,1000,SIMP,101325")
    assert handler.write_queue[0] == b"CMD,1000,SIMP,101325\r"

def test_terminator_config():
    handler = SerialHandler()
    assert handler.terminator == b"\r"
