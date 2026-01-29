import serial
import serial.tools.list_ports
import time
import yaml
import os
from PyQt6.QtCore import QThread, pyqtSignal, QMutex, QWaitCondition

class SerialHandler(QThread):
    packet_received = pyqtSignal(str) # Emits raw line
    connection_status = pyqtSignal(str) # Emitted on state change
    link_quality = pyqtSignal(int) # Signal strength (not implemented yet, but good hook)
    error_occurred = pyqtSignal(str)

    def __init__(self, config_path="../config.yaml"):
        super().__init__()
        self.running = False
        self.serial_port = None
        self.mutex = QMutex()
        
        # Load Config
        try:
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            full_config_path = os.path.join(base_dir, "config.yaml")
            with open(full_config_path, 'r') as f:
                self.config = yaml.safe_load(f)
        except Exception as e:
            print(f"Error loading config: {e}")
            self.config = {}

        self.port_name = self.config.get("SERIAL_PORT", None)
        self.baud_rate = self.config.get("SERIAL_BAUD", 9600)
        # STRICT: Default to \r if not specified, but config should satisfy this.
        term_str = self.config.get("TERMINATOR", "\r")
        # Handle escape sequences if loaded as literal string "\\r"
        if term_str == "\\r": term_str = "\r"
        elif term_str == "\\n": term_str = "\n"
        elif term_str == "\\r\\n": term_str = "\r\n"
        
        self.terminator = term_str.encode('utf-8')
        
        # Write queue
        self.write_queue = []

    def run(self):
        self.running = True
        
        while self.running:
            if self.serial_port is None or not self.serial_port.is_open:
                if not self.connect_serial():
                    self.connection_status.emit("Disconnected")
                    time.sleep(2) # Auto-reconnect delay
                    continue
            
            try:
                # 1. READ
                if self.serial_port.in_waiting:
                    # STRICT: read_until matches the terminator
                    line = self.serial_port.read_until(self.terminator)
                    if line:
                        try:
                            # Decode and strip the terminator
                            decoded_line = line.decode('utf-8', errors='ignore').strip()
                            if decoded_line:
                                self.packet_received.emit(decoded_line)
                        except Exception as e:
                            print(f"Decode error: {e}")

                # 2. WRITE
                self.mutex.lock()
                if self.write_queue:
                    cmd = self.write_queue.pop(0)
                    self.mutex.unlock()
                    try:
                        self.serial_port.write(cmd)
                        self.serial_port.flush()
                        # print(f"Wrote: {cmd}")
                    except Exception as e:
                        print(f"Write error: {e}")
                else:
                    self.mutex.unlock()

                # Yield slightly to avoid 100% CPU
                self.msleep(10) 

            except serial.SerialException as e:
                self.connection_status.emit(f"Error: {str(e)}")
                self.close_serial()
            except Exception as e:
                self.error_occurred.emit(str(e))

    def connect_serial(self):
        # Auto-detect logic
        target_port = self.port_name
        if not target_port:
             ports = list(serial.tools.list_ports.comports())
             if ports:
                 # Prefer USB
                 usb_ports = [p.device for p in ports if 'USB' in p.description or 'USB' in p.device]
                 if usb_ports:
                     target_port = usb_ports[0]
                 else:
                     target_port = ports[0].device
        
        if not target_port:
            return False

        try:
            self.serial_port = serial.Serial(
                port=target_port,
                baudrate=self.baud_rate,
                timeout=0.1, # Short timeout for non-blocking loop
                write_timeout=1.0
            )
            self.connection_status.emit(f"Connected: {target_port}")
            return True
        except serial.SerialException:
            return False

    def close_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.serial_port = None

    def send_command(self, cmd_str: str):
        """
        Signals to send a command. 
        Appends the strict terminator.
        """
        # Ensure raw bytes
        cmd_bytes = cmd_str.encode('utf-8')
        
        # Check if terminator needed
        if not cmd_bytes.endswith(self.terminator):
            cmd_bytes += self.terminator
            
        self.mutex.lock()
        self.write_queue.append(cmd_bytes)
        self.mutex.unlock()

    def stop(self):
        self.running = False
        self.wait()
        self.close_serial()
