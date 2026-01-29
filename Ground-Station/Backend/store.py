import time
from typing import Optional
from dataclasses import asdict
from .parser import TelemetryPacket

class TelemetryStore:
    def __init__(self):
        self.latest_packet: Optional[TelemetryPacket] = None
        self.last_packet_time: float = 0.0
        
        # Counters
        self.received_count: int = 0
        self.last_packet_tx_count: int = -1
        self.lost_count: int = 0
        self.bad_frame_count: int = 0
        self.parse_error_count: int = 0
        
        # State
        self.csv_ready: bool = False # Becomes True on first valid packet

    def process_new_packet(self, packet: Optional[TelemetryPacket]):
        if packet is None:
            self.parse_error_count += 1
            return

        self.latest_packet = packet
        self.last_packet_time = time.time()
        self.received_count += 1
        self.csv_ready = True # We have at least one valid packet

        # Loss Logic
        current_seq = packet.packet_count
        if self.last_packet_tx_count == -1:
            self.last_packet_tx_count = current_seq - 1 # Initialize
        
        # If current sequence > last + 1, we missed some
        # If current sequence < last, it might be a reset, ignore
        if current_seq > self.last_packet_tx_count + 1:
            missed = (current_seq - self.last_packet_tx_count - 1)
            self.lost_count += missed
        
        # Update trackers if sequence moved forward or reset (allow reset)
        if current_seq >= self.last_packet_tx_count or (self.last_packet_tx_count - current_seq > 100):
             # The > 100 check is a heuristic for reboot vs out-of-order
             self.last_packet_tx_count = current_seq

    def get_link_age(self) -> float:
        """Returns seconds since last packet."""
        if self.last_packet_time == 0:
            return 999.0 # Infinity effectively
        return time.time() - self.last_packet_time

    def is_stale(self) -> bool:
        return self.get_link_age() > 3.0

    def increment_bad_frame(self):
        self.bad_frame_count += 1
