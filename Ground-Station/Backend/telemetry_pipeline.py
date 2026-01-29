from PyQt6.QtCore import QObject, pyqtSignal
from .parser import parse_telemetry_line, TelemetryPacket
from .store import TelemetryStore
from .csv_logger import CSVLogger

class TelemetryPipeline(QObject):
    """
    Central controller for processing telemetry data.
    Moves logic out of MainWindow (View).
    
    Flow:
    Input (String) -> Log Raw -> Parse -> Store -> Log Packet -> Emit Update
    """
    packet_processed = pyqtSignal(object) # Emits TelemetryPacket
    raw_packet_received = pyqtSignal(str) # Emits raw line
    model_updated = pyqtSignal() # General signal for UI refresh

    def __init__(self, team_id=1000):
        super().__init__()
        self.team_id = team_id
        
        # Components
        self.store = TelemetryStore()
        self.logger = CSVLogger(str(team_id))
        
        # We don't own SerialHandler, but we subscribe to it.

    def handle_packet(self, line: str):
        """
        Main entry point for new data (from Serial or Playback).
        """
        # 1. Emit Raw (for Console View)
        self.raw_packet_received.emit(line)
        
        # 2. Log Raw
        self.logger.log_raw(line)
        
        # 3. Parse
        pkt = parse_telemetry_line(line)
        
        # 4. Update Store
        self.store.process_new_packet(pkt) # Handles None internally for bad frame count
        
        # 5. Log Packet & Notify
        if pkt:
            self.logger.log_packet(pkt)
            self.packet_processed.emit(pkt)
        
        # 6. Notify General Update (link age, counters changed)
        self.model_updated.emit()

    def close(self):
        self.logger.close()
