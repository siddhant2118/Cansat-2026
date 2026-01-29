import csv
import os
import datetime
from typing import Optional
from .parser import TelemetryPacket
from dataclasses import asdict

class CSVLogger:
    def __init__(self, team_id_str: str):
        self.team_id_str = team_id_str
        self.log_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Logs")
        os.makedirs(self.log_dir, exist_ok=True)
        
        # Flight Log (Parsed Data)
        timestamp = datetime.datetime.utcnow().strftime("%Y%m%d_%H%M%S")
        self.filename = f"Flight_{team_id_str}_{timestamp}.csv"
        self.filepath = os.path.join(self.log_dir, self.filename)
        
        self.file_handle = open(self.filepath, 'w', newline='', buffering=1) # buffering=1 means line buffered
        self.csv_writer = csv.writer(self.file_handle)
        
        # Write Header
        # Get fields from dataclass keys
        # We need an instance or just the class annotations
        # Using a dummy instance or hardcoded list to ensure order
        # Hardcoding to match the strict order is safe
        self.header = [
            "TEAM_ID", "MISSION_TIME", "PACKET_COUNT", "MODE", "STATE", 
            "ALTITUDE", "TEMPERATURE", "PRESSURE", "VOLTAGE", "CURRENT", 
            "GYRO_R", "GYRO_P", "GYRO_Y", "ACCEL_R", "ACCEL_P", "ACCEL_Y", 
            "GPS_TIME", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS", "CMD_ECHO"
        ]
        self.csv_writer.writerow(self.header)
        
        # Raw Log (Debug) option
        self.raw_filename = f"Raw_{team_id_str}_{timestamp}.log"
        self.raw_filepath = os.path.join(self.log_dir, self.raw_filename)
        self.raw_handle = open(self.raw_filepath, 'a', buffering=1)

    def log_packet(self, packet: TelemetryPacket):
        if not packet:
            return
        
        # row = asdict(packet).values() # Relying on dict insertion order (Py3.7+)
        # Safer:
        row = [
            packet.team_id, packet.mission_time, packet.packet_count, packet.mode, packet.state,
            packet.altitude, packet.temperature, packet.pressure, packet.voltage, packet.current,
            packet.gyro_r, packet.gyro_p, packet.gyro_y, packet.accel_r, packet.accel_p, packet.accel_y,
            packet.gps_time, packet.gps_altitude, packet.gps_latitude, packet.gps_longitude, packet.gps_sats,
            packet.cmd_echo
        ]
        
        try:
            self.csv_writer.writerow(row)
            self.file_handle.flush() # Ensure it hits disk
        except Exception as e:
            print(f"CSV Write Error: {e}")

    def log_raw(self, line: str):
        try:
            self.raw_handle.write(line + "\n")
            self.raw_handle.flush()
        except Exception as e:
            print(f"Raw Log Error: {e}")

    def close(self):
        if self.file_handle:
            self.file_handle.close()
        if self.raw_handle:
            self.raw_handle.close()
