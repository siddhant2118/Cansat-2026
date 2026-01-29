from PyQt6.QtCore import QObject, QTimer, pyqtSignal
import csv

class SimulationManager(QObject):
    send_command = pyqtSignal(str) # To SerialHandler
    status_update = pyqtSignal(str) 

    def __init__(self, team_id=1000):
        super().__init__()
        self.team_id = team_id
        
        # State Machine
        self.state = "IDLE" # IDLE -> ENABLED -> ACTIVE
        self.pressure_data = [] 
        self.current_index = 0
        
        self.timer = QTimer()
        self.timer.setInterval(1000) # 1Hz
        self.timer.timeout.connect(self._tick)

    def load_pressure_csv(self, filepath):
        self.pressure_data = []
        self.current_index = 0
        try:
            with open(filepath, 'r') as f:
                reader = csv.reader(f)
                header = next(reader, None)
                # Assume first column is pressure if no header match
                # Strictly convert to Pascals (Integer usually)
                for row in reader:
                    if not row: continue
                    val = row[0].strip() # Assume col 0
                    try:
                         # Convert to float then int to handle "101325.0"
                         pa_val = int(float(val))
                         self.pressure_data.append(pa_val)
                    except ValueError:
                        continue
            self.status_update.emit(f"Loaded {len(self.pressure_data)} samples.")
            return True
        except Exception as e:
            self.status_update.emit(f"CSV Load Error: {e}")
            return False

    def enable(self):
        self.state = "ENABLED"
        self.send_command.emit(f"CMD,{self.team_id},SIM,ENABLE")
        self.status_update.emit("SIM ENABLED")

    def activate(self):
        if self.state != "ENABLED":
            self.status_update.emit("Error: Must ENABLE first.")
            return
        
        self.state = "ACTIVE"
        self.send_command.emit(f"CMD,{self.team_id},SIM,ACTIVATE")
        self.timer.start()
        self.status_update.emit("SIM ACTIVATED")

    def disable(self):
        self.state = "IDLE"
        self.timer.stop()
        self.current_index = 0
        self.send_command.emit(f"CMD,{self.team_id},SIM,DISABLE")
        self.status_update.emit("SIM DISABLED")

    def _tick(self):
        if self.state != "ACTIVE":
            self.timer.stop()
            return
            
        if self.current_index < len(self.pressure_data):
            val = self.pressure_data[self.current_index]
            # STRICT FORMAT: CMD,<TEAM_ID>,SIMP,<PRESSURE>
            # Pressure in Pa
            self.send_command.emit(f"CMD,{self.team_id},SIMP,{val}")
            self.current_index += 1
        else:
            self.status_update.emit("Simulation Completed.")
            self.disable()
