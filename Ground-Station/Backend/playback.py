from PyQt6.QtCore import QObject, QTimer, pyqtSignal
import csv

class TelemetryReplayer(QObject):
    packet_emitted = pyqtSignal(str) # Emits raw CSV line (reconstructed)
    status_update = pyqtSignal(str) 

    def __init__(self):
        super().__init__()
        
        self.flight_data = [] 
        self.current_index = 0
        self.is_playing = False
        
        self.timer = QTimer()
        self.timer.setInterval(1000) # 1Hz
        self.timer.timeout.connect(self._tick)

    def load_flight_csv(self, filepath):
        self.stop()
        self.flight_data = []
        self.current_index = 0
        
        try:
            with open(filepath, 'r') as f:
                reader = csv.reader(f)
                header = next(reader, None)
                
                # Validation: Check essential headers
                # Minimal check: TEAM_ID, MISSION_TIME
                if not header or "TEAM_ID" not in header[0]:
                     self.status_update.emit("Invalid Telemetry CSV")
                     return False
                
                # Store rows
                for row in reader:
                    if row:
                        # Reconstruct CSV line
                        line = ",".join(row)
                        self.flight_data.append(line)
            
            self.status_update.emit(f"Loaded Flight: {len(self.flight_data)} pkts")
            return True
        except Exception as e:
            self.status_update.emit(f"Load Error: {e}")
            return False

    def play(self):
        if not self.flight_data: return
        self.is_playing = True
        self.timer.start()
        self.status_update.emit("Replay Started")

    def pause(self):
        self.is_playing = False
        self.timer.stop()
        self.status_update.emit("Replay Paused")

    def stop(self):
        self.is_playing = False
        self.timer.stop()
        self.current_index = 0
        self.status_update.emit("Replay Stopped")

    def _tick(self):
        if self.current_index < len(self.flight_data):
            line = self.flight_data[self.current_index]
            self.packet_emitted.emit(line)
            self.current_index += 1
        else:
            self.stop()
            self.status_update.emit("Replay Finished")
