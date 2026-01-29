import sys
import os
import time
from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                             QPushButton, QLabel, QLineEdit, QGridLayout, 
                             QGroupBox, QPlainTextEdit, QFileDialog, QFrame, 
                             QCheckBox, QApplication, QSplitter)
from PyQt6.QtCore import QTimer, Qt
from PyQt6.QtGui import QFont, QColor, QPalette, QTextCursor
import pyqtgraph as pg
import yaml
from dataclasses import asdict

# Adjust path to import backend
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from Backend.serial_handler import SerialHandler
from Backend.serial_handler import SerialHandler
from Backend.simulation import SimulationManager
from Backend.playback import TelemetryReplayer
from Backend.telemetry_pipeline import TelemetryPipeline

class FHighContrastTheme:
    WINDOW_BG = "#F2F3F5"
    CARD_BG = "#FFFFFF"
    BORDER = "#C9CDD3"
    TEXT_MAIN = "#2B2F36"
    TEXT_SUB = "#5F6368"
    
    # Status Colors
    OK = "#1E8E3E"      # Strong Green
    WARN = "#D93025"    # Strong Red
    ATTN = "#F9AB00"    # Amber
    
    # Plot Colors (Thick, Distinct)
    P_BLUE = "#1967D2"
    P_RED = "#D93025"
    P_GREEN = "#188038"
    P_ORANGE = "#E37400"
    P_PURPLE = "#8E24AA"
    P_CYAN = "#007B83"

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("CanSat 2026 GCS - Flight Operations")
        self.resize(1024, 600) # Compact Flight Ops
        
        self.load_config()
        self.apply_theme()

        # Components
        self.serial_handler = SerialHandler()
        self.pipeline = TelemetryPipeline(self.team_id)
        self.simulation_manager = SimulationManager(self.team_id)
        self.replayer = TelemetryReplayer()

        # Connects
        # Pipeline Inputs
        self.serial_handler.packet_received.connect(self.pipeline.handle_packet)
        self.replayer.packet_emitted.connect(self.pipeline.handle_packet)
        
        # Pipeline Outputs (To UI)
        self.pipeline.raw_packet_received.connect(self.handle_raw_packet) # Console log
        self.pipeline.packet_processed.connect(self.update_data_buffers)
        
        self.serial_handler.connection_status.connect(self.update_conn_status)
        self.simulation_manager.send_command.connect(self.serial_handler.send_command)
        self.simulation_manager.status_update.connect(lambda s: self.console_log(f"[SYS] {s}"))
        self.replayer.status_update.connect(lambda s: self.console_log(f"[PLAY] {s}"))

        # Main Layout
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(5, 5, 5, 5) # COMPACT margin
        self.main_layout.setSpacing(5) # COMPACT spacing
        
        # 1. Header (Single Line Compact)
        self.setup_header()
        
        # 2. Splitter Layout (3 Columns)
        self.splitter = QSplitter(Qt.Orientation.Horizontal)
        self.main_layout.addWidget(self.splitter)
        
        # COL 1: Operations & Data (Left) [Fixed/Min Width ~300]
        self.col1_widget = QWidget()
        col1_layout = QVBoxLayout(self.col1_widget)
        col1_layout.setContentsMargins(0,0,0,0)
        col1_layout.setSpacing(5)
        self.setup_command_card(col1_layout)
        self.setup_telemetry_card(col1_layout)
        # col1_layout.addStretch() # Push Up
        self.splitter.addWidget(self.col1_widget)
        
        # COL 2: Console (Center) [Fixed/Min Width ~280]
        self.col2_widget = QWidget()
        col2_layout = QVBoxLayout(self.col2_widget)
        col2_layout.setContentsMargins(0,0,0,0)
        self.setup_console_card(col2_layout)
        self.splitter.addWidget(self.col2_widget)
        
        # COL 3: Plots (Right) [Takes Remaining Space ~700]
        self.col3_widget = QWidget()
        col3_layout = QVBoxLayout(self.col3_widget)
        col3_layout.setContentsMargins(0,0,0,0)
        self.setup_plots_card(col3_layout)
        self.splitter.addWidget(self.col3_widget)

        # Splitter Sizing (STRICT)
        # Left: 300, Center: 280, Right: 700 (Sum=1280)
        self.splitter.setSizes([300, 280, 700])
        # Enforce Minimums to prevent collapse
        self.col1_widget.setMinimumWidth(260)
        self.col2_widget.setMinimumWidth(240)
        self.col3_widget.setMinimumWidth(600)
        
        # Stretch Logic: Plot column gets most resize
        self.splitter.setStretchFactor(0, 0) # Left fixed-ish
        self.splitter.setStretchFactor(1, 0) # Center fixed-ish
        self.splitter.setStretchFactor(2, 1) # Right stretches

        # Timer
        self.ui_timer = QTimer()
        self.ui_timer.setInterval(100) # 10Hz
        self.ui_timer.timeout.connect(self.update_ui)
        self.ui_timer.start()

        self.start_time = time.time()
        self.serial_handler.start()

    def load_config(self):
        self.team_id = 1000
        self.font_size = 14
        try:
            p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config.yaml")
            with open(p, 'r') as f:
                c = yaml.safe_load(f)
                self.team_id = c.get("TEAM_ID", 1000)
                # Ensure we don't scale font down too much
                # User asked "Do not reduce base UI font below 14pt"
                # but allow header/plots slightly smaller
                self.font_size = max(c.get("UI_FONT_SIZE", 14), 14)
        except: pass

    def apply_theme(self):
        p = self.palette()
        p.setColor(QPalette.ColorRole.Window, QColor(FHighContrastTheme.WINDOW_BG))
        p.setColor(QPalette.ColorRole.WindowText, QColor(FHighContrastTheme.TEXT_MAIN))
        self.setPalette(p)
        
        font = QFont("Arial", self.font_size)
        self.setFont(font)
        
        # Compact Stylesheet
        self.setStyleSheet(f"""
            QFrame.Card {{
                background-color: {FHighContrastTheme.CARD_BG};
                border: 1px solid {FHighContrastTheme.BORDER};
                border-radius: 4px;
            }}
            QLabel.Header {{
                color: {FHighContrastTheme.TEXT_MAIN};
                font-weight: bold;
                font-size: {self.font_size}pt; /* Compact Header */
                padding-bottom: 2px;
                border-bottom: 1px solid {FHighContrastTheme.BORDER};
            }}
            QLabel.SubHeader {{
                color: {FHighContrastTheme.TEXT_SUB};
                font-weight: bold;
                font-size: 11pt; /* Slightly smaller subheader */
                margin-top: 4px;
            }}
            QPushButton {{
                background-color: #F8F9FA;
                border: 1px solid {FHighContrastTheme.BORDER};
                border-radius: 3px;
                padding: 2px; /* Compact Padding */
                font-weight: bold;
                color: {FHighContrastTheme.TEXT_MAIN};
                height: 28px; /* Fixed Compact Height */
            }}
            QPushButton:hover {{ background-color: #E8EAED; }}
            QPushButton:pressed {{ background-color: #DADCE0; }}
            QLineEdit {{
                border: 1px solid {FHighContrastTheme.BORDER};
                border-radius: 3px;
                padding: 1px;
                background-color: #FFF;
                height: 22px; /* Super compact */
            }}
        """)

    def create_card_frame(self, layout):
        f = QFrame()
        f.setProperty("class", "Card")
        l = QVBoxLayout(f)
        l.setContentsMargins(5, 5, 5, 5) # Compact Margins
        l.setSpacing(4) # Compact Spacing
        layout.addWidget(f)
        return l, f # Return frame to allow stretch set

    def setup_header(self):
        f = QFrame()
        f.setProperty("class", "Card")
        l = QHBoxLayout(f)
        l.setContentsMargins(8, 4, 8, 4) # Compact
        l.setSpacing(15) # Spacing between items
        
        # Team ID
        lbl_id = QLabel(f"TEAM {self.team_id}")
        lbl_id.setStyleSheet("font-size: 16pt; font-weight: 900; color: #202124;")
        l.addWidget(lbl_id)
        
        l.addStretch()
        
        # Single Line Items
        self.lbl_mode = self.create_status_label("MODE", "WAITING")
        l.addWidget(self.lbl_mode)
        
        self.lbl_utc = self.create_status_label("UTC", "--:--:--")
        l.addWidget(self.lbl_utc)
        
        self.lbl_mt = self.create_status_label("MT", "--:--:--")
        l.addWidget(self.lbl_mt)
        
        self.lbl_age = self.create_status_label("AGE", "-- s")
        l.addWidget(self.lbl_age)
        
        self.lbl_csv = self.create_status_label("CSV", "WAITING")
        l.addWidget(self.lbl_csv)

        # Connection
        self.lbl_conn = QLabel("DISCON")
        self.lbl_conn.setStyleSheet(f"color: {FHighContrastTheme.WARN}; font-size: 14pt; font-weight: 900;")
        l.addWidget(self.lbl_conn)

        self.main_layout.addWidget(f)

    def create_status_label(self, title, val):
        # Compact HStack: TITLE: VAL
        container = QFrame()
        lay = QHBoxLayout(container)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(5)
        
        t = QLabel(title + ":")
        t.setStyleSheet(f"color: {FHighContrastTheme.TEXT_SUB}; font-size: 10pt; font-weight: bold;")
        v = QLabel(val)
        v.setStyleSheet(f"color: {FHighContrastTheme.TEXT_MAIN}; font-size: 14pt; font-weight: bold;")
        
        lay.addWidget(t)
        lay.addWidget(v)
        
        container.val_label = v 
        return container

    def update_status_label(self, container, val, color=None):
        container.val_label.setText(val)
        if color:
             container.val_label.setStyleSheet(f"color: {color}; font-size: 14pt; font-weight: bold;")
        else:
             container.val_label.setStyleSheet(f"color: {FHighContrastTheme.TEXT_MAIN}; font-size: 14pt; font-weight: bold;")

    def setup_command_card(self, layout):
        l, frame = self.create_card_frame(layout)
        
        head = QLabel("COMMANDS / REPLAY") 
        head.setProperty("class", "Header")
        l.addWidget(head)

        def subheader(txt):
            lbl = QLabel(txt)
            lbl.setProperty("class", "SubHeader")
            l.addWidget(lbl)

        # REPLAY
        subheader("Telemetry Replay")
        h_rep = QHBoxLayout()
        b_load_tlm = QPushButton("Load TLM")
        b_load_tlm.setStyleSheet("background-color: #E3F2FD;")
        b_load_tlm.clicked.connect(self.load_telemetry_csv)
        h_rep.addWidget(b_load_tlm)
        
        b_play = QPushButton("▶")
        b_play.setFixedWidth(30)
        b_play.clicked.connect(self.replayer.play)
        h_rep.addWidget(b_play)
        
        b_pause = QPushButton("||")
        b_pause.setFixedWidth(30)
        b_pause.clicked.connect(self.replayer.pause)
        h_rep.addWidget(b_pause)
        
        b_stop = QPushButton("■")
        b_stop.setFixedWidth(30)
        b_stop.clicked.connect(self.replayer.stop)
        h_rep.addWidget(b_stop)
        l.addLayout(h_rep)


        # A. Telemetry
        subheader("Telemetry Control")
        h1 = QHBoxLayout()
        for txt, cmd in [("CX ON", "CX,ON"), ("CX OFF", "CX,OFF")]:
            b = QPushButton(txt)
            b.clicked.connect(lambda _, c=cmd: self.send_cmd(c))
            h1.addWidget(b)
        l.addLayout(h1)

        # B. Time
        subheader("Time Sync")
        h2 = QHBoxLayout()
        b1 = QPushButton("GPS")
        b1.clicked.connect(lambda: self.send_cmd("ST,GPS"))
        h2.addWidget(b1)
        b2 = QPushButton("UTC (Sys)")
        b2.clicked.connect(self.send_st_utc)
        h2.addWidget(b2)
        l.addLayout(h2)

        # C. Simulation
        subheader("Simulation")
        # Horizontal Layout for Enable/Act/Dis to save space
        h_sim_btns = QHBoxLayout()
        h_sim_btns.addWidget(self.create_sim_btn("EN", self.simulation_manager.enable))
        h_sim_btns.addWidget(self.create_sim_btn("ACT", self.simulation_manager.activate, "green"))
        h_sim_btns.addWidget(self.create_sim_btn("DIS", self.simulation_manager.disable, "red"))
        l.addLayout(h_sim_btns)
        
        # CSV load row
        h3 = QHBoxLayout()
        b_load = QPushButton("Load CSV")
        b_load.clicked.connect(self.load_pressure_csv)
        self.lbl_sim_status = QLabel("None")
        self.lbl_sim_status.setStyleSheet("color: #666; font-size: 9pt;")
        h3.addWidget(b_load)
        h3.addWidget(self.lbl_sim_status)
        
        # Manual Pa
        # Move into same layout or tight next row
        self.txt_sim_pa = QLineEdit("101325")
        self.txt_sim_pa.setPlaceholderText("Pa")
        self.txt_sim_pa.setFixedWidth(70)
        b_send = QPushButton("Send")
        b_send.setFixedWidth(50)
        b_send.clicked.connect(lambda: self.send_cmd(f"SIMP,{self.txt_sim_pa.text()}"))
        h3.addWidget(self.txt_sim_pa)
        h3.addWidget(b_send)
        l.addLayout(h3)

        # D. Cal & Mec
        subheader("Calibration")
        b_cal = QPushButton("CAL (0m)")
        b_cal.setStyleSheet(f"background-color: #FFF8E1; color: {FHighContrastTheme.P_ORANGE}; border: 1px solid {FHighContrastTheme.P_ORANGE};")
        b_cal.clicked.connect(lambda: self.send_cmd("CAL"))
        l.addWidget(b_cal)

        subheader("Mechanisms")
        h5 = QHBoxLayout()
        self.txt_mec = QLineEdit("RELEASE")
        self.txt_mec.setPlaceholderText("Device")
        h5.addWidget(self.txt_mec)
        for t in ["ON", "OFF"]:
            b = QPushButton(t)
            b.setFixedWidth(40) # Compact
            b.clicked.connect(lambda _, s=t: self.send_mec(s))
            h5.addWidget(b)
        l.addLayout(h5)
        
        l.addStretch()

    def create_sim_btn(self, text, func, tone=None):
        b = QPushButton(text)
        b.clicked.connect(func)
        if tone == "green":
             b.setStyleSheet(f"background-color: #E6F4EA; color: {FHighContrastTheme.OK}; border: 1px solid {FHighContrastTheme.OK};")
        elif tone == "red":
             b.setStyleSheet(f"background-color: #FCE8E6; color: {FHighContrastTheme.WARN}; border: 1px solid {FHighContrastTheme.WARN};")
        return b

    def setup_telemetry_card(self, layout):
        l, frame = self.create_card_frame(layout)
        
        head = QLabel("TELEMETRY")
        head.setProperty("class", "Header")
        l.addWidget(head)

        self.telem_grid = QGridLayout()
        self.telem_grid.setSpacing(4)
        l.addLayout(self.telem_grid)
        
        # Fields: Flat List for 3-Column Grid
        # KEY | VAL UNIT
        fields = [
            ("Alt", "m"), ("Temp", "°C"), ("Press", "kPa"),
            ("Volt", "V"),("Curr", "A"), ("Sats", ""),
            ("Lat", ""), ("Lon", ""), ("Echo", ""),
            ("RX", ""), ("Lost", ""), ("Bad", "")
        ]
        
        self.telem_vals = {}
        
        # 3 Columns: col 0, 1, 2
        # But we need Label+Val+Unit per cell? Or just Label: Val
        # Let's do: Label: Val (Unit)
        
        for i, (lbl, unit) in enumerate(fields):
            row = i // 3
            col = i % 3
            
            # Container for cell
            cell = QFrame()
            cl = QVBoxLayout(cell)
            cl.setContentsMargins(2,2,2,2)
            cl.setSpacing(0)
            
            title = QLabel(lbl)
            title.setStyleSheet(f"color: {FHighContrastTheme.TEXT_SUB}; font-weight: bold; font-size: 9pt;")
            
            val_text = "--"
            # if unit: val_text += f" {unit}"
            val = QLabel(val_text)
            val.setStyleSheet(f"color: {FHighContrastTheme.P_BLUE}; font-size: 13pt; font-weight: bold;")
            val.setAlignment(Qt.AlignmentFlag.AlignLeft)
            
            cl.addWidget(title)
            cl.addWidget(val)
            
            self.telem_grid.addWidget(cell, row, col)
            
            key_map = {
                "Alt": "altitude", "Temp": "temperature",
                "Press": "pressure", "Volt": "voltage",
                "Curr": "current", "Sats": "gps_sats",
                "Lat": "gps_latitude", "Lon": "gps_longitude",
                "RX": "received_count", "Lost": "lost_count",
                "Bad": "bad_frame_count", "Echo": "cmd_echo"
            }
            self.telem_vals[key_map[lbl]] = val

        l.addStretch()

    def setup_console_card(self, layout):
        l, frame = self.create_card_frame(layout)
        l.setContentsMargins(0,0,0,0)
        
        # Header + Controls (Compact Single Line)
        ctrls = QWidget()
        ctrls.setStyleSheet(f"background-color: {FHighContrastTheme.WINDOW_BG}; border-bottom: 1px solid {FHighContrastTheme.BORDER};")
        cl = QHBoxLayout(ctrls)
        cl.setContentsMargins(5, 2, 5, 2)
        
        head = QLabel("CONSOLE")
        head.setStyleSheet("font-weight: bold; color: #333; font-size: 11pt;")
        cl.addWidget(head)

        self.chk_rx = QCheckBox("RX"); self.chk_rx.setChecked(True)
        self.chk_tx = QCheckBox("TX"); self.chk_tx.setChecked(True)
        self.chk_sys = QCheckBox("SYS"); self.chk_sys.setChecked(True)
        cl.addWidget(self.chk_rx)
        cl.addWidget(self.chk_tx)
        cl.addWidget(self.chk_sys)
        
        cl.addStretch()
        
        b_copy = QPushButton("Cpy")
        b_copy.setFixedWidth(35)
        b_copy.clicked.connect(lambda: QApplication.clipboard().setText(self.console.toPlainText()))
        b_clear = QPushButton("Clr")
        b_clear.setFixedWidth(35)
        b_clear.clicked.connect(lambda: self.console.clear())
        cl.addWidget(b_copy)
        cl.addWidget(b_clear)
        
        l.addWidget(ctrls)
        
        # Text Area (Tall)
        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setFont(QFont("Courier New", 10)) # Monospaced
        self.console.setStyleSheet("background-color: #202124; color: #E8EAED; border: none;")
        self.console.setMaximumBlockCount(1000)
        l.addWidget(self.console)

    def console_log(self, msg):
        if "[RX]" in msg and not self.chk_rx.isChecked(): return
        if "[TX]" in msg and not self.chk_tx.isChecked(): return
        if "[SYS]" in msg and not self.chk_sys.isChecked(): return
        if "[PLAY]" in msg and not self.chk_sys.isChecked(): return
        self.console.appendPlainText(msg.strip())
        self.console.moveCursor(QTextCursor.MoveOperation.End)

    def setup_plots_card(self, layout):
        l, frame = self.create_card_frame(layout)
        l.setContentsMargins(0,0,0,0) # Flush for max space
        
        self.plot_layout = pg.GraphicsLayoutWidget()
        self.plot_layout.setBackground('w')
        # TIGHT spacing
        self.plot_layout.ci.layout.setSpacing(2) 
        self.plot_layout.ci.layout.setContentsMargins(2, 2, 2, 2)
        l.addWidget(self.plot_layout)
        
        self.plots = {}
        self.curves = {}
        keys = ['alt', 'temp', 'volt', 'press', 'curr', 
                'ax','ay','az', 'gr','gp','gy', 
                'galt','glat','glon']
        self.plot_data = {k: {'t': [], 'y': []} for k in keys}
        
        def add_p(row, col, title, unit, pen=FHighContrastTheme.P_BLUE):
            p = self.plot_layout.addPlot(row=row, col=col)
            # Minimalist Style
            p.showGrid(x=True, y=True, alpha=0.3)
            p.setLabel('left', unit, color='#333', **{'font-size': '9pt'})
            p.setTitle(title, color='#000', size='10pt')
            p.getAxis('left').setPen('#888')
            p.getAxis('bottom').setPen('#888')
            p.getAxis('left').setWidth(40) # Fixed width aligned axes
            return p, p.plot(pen=pg.mkPen(pen, width=2))

        # 2x4 Grid
        # Row 0
        _, self.curves['alt'] = add_p(0, 0, "Altitude", "m", FHighContrastTheme.P_BLUE)
        _, self.curves['temp'] = add_p(0, 1, "Temperature", "°C", FHighContrastTheme.P_RED)
        # Row 1
        _, self.curves['press'] = add_p(1, 0, "Pressure", "kPa", FHighContrastTheme.P_ORANGE)
        p_volt, _ = add_p(1, 1, "Battery", "V / A", FHighContrastTheme.P_GREEN)
        self.curves['volt'] = p_volt.listDataItems()[0]
        # self.curves['volt'].setName("V")
        self.curves['curr'] = p_volt.plot(pen=pg.mkPen(FHighContrastTheme.P_PURPLE, width=2))
        # Row 2 (Acc / Gyr)
        p_acc, _ = add_p(2, 0, "Accel", "m/s²", FHighContrastTheme.P_RED)
        self.curves['ax'] = p_acc.listDataItems()[0]
        self.curves['ay'] = p_acc.plot(pen=pg.mkPen(FHighContrastTheme.P_GREEN, width=2))
        self.curves['az'] = p_acc.plot(pen=pg.mkPen(FHighContrastTheme.P_BLUE, width=2))
        
        p_gyr, _ = add_p(2, 1, "Gyro", "deg/s", FHighContrastTheme.P_CYAN)
        self.curves['gr'] = p_gyr.listDataItems()[0]
        self.curves['gp'] = p_gyr.plot(pen=pg.mkPen(FHighContrastTheme.P_PURPLE, width=2))
        self.curves['gy'] = p_gyr.plot(pen=pg.mkPen(FHighContrastTheme.P_ORANGE, width=2))
        
        # Row 3 (GPS)
        _, self.curves['galt'] = add_p(3, 0, "GPS Alt", "m", FHighContrastTheme.P_BLUE)
        p_pos, _ = add_p(3, 1, "GPS Pos", "Deg", "#333")
        self.curves['glat'] = p_pos.listDataItems()[0]
        self.curves['glon'] = p_pos.plot(pen=pg.mkPen("#666", width=2))

    def handle_packet(self, line):
        self.csv_logger.log_raw(line)
        self.console_log(f"[RX] {line}")
        pkt = parse_telemetry_line(line)
        self.telemetry_store.process_new_packet(pkt)
        if pkt:
            self.csv_logger.log_packet(pkt)
            self.update_data_buffers(pkt)
        else:
            self.telemetry_store.increment_bad_frame()

    def update_data_buffers(self, pkt):
        t = time.time() - self.start_time
        d = {
            'alt': pkt.altitude, 'temp': pkt.temperature, 'volt': pkt.voltage, 'press': pkt.pressure, 'curr': pkt.current,
            'ax': pkt.accel_r, 'ay': pkt.accel_p, 'az': pkt.accel_y,
            'gr': pkt.gyro_r, 'gp': pkt.gyro_p, 'gy': pkt.gyro_y,
            'galt': pkt.gps_altitude, 'glat': pkt.gps_latitude, 'glon': pkt.gps_longitude
        }
        for k, v in d.items():
            self.plot_data[k]['t'].append(t)
            self.plot_data[k]['y'].append(v)
            if len(self.plot_data[k]['t']) > 300:
                self.plot_data[k]['t'].pop(0)
                self.plot_data[k]['y'].pop(0)

    def update_ui(self):
        st = self.pipeline.store
        
        age = st.get_link_age()
        if age < 900:
            self.update_status_label(self.lbl_age, f"{age:.1f} s", 
                FHighContrastTheme.OK if age < 3 else FHighContrastTheme.WARN)
        
        if st.csv_ready: self.update_status_label(self.lbl_csv, "LOGGING", FHighContrastTheme.OK)
            
        if st.latest_packet:
            p = st.latest_packet
            self.update_status_label(self.lbl_mode, p.mode, 
                FHighContrastTheme.OK if p.mode == 'F' else FHighContrastTheme.P_PURPLE)
            self.update_status_label(self.lbl_utc, p.gps_time)
            self.update_status_label(self.lbl_mt, p.mission_time)
            
            vals = asdict(p)
            vals['received_count'] = str(st.received_count)
            vals['lost_count'] = str(st.lost_count)
            vals['bad_frame_count'] = str(st.bad_frame_count + st.parse_error_count)
            
            for k, lbl in self.telem_vals.items():
                if k in vals:
                    v = vals[k]
                    if isinstance(v, float): v = f"{v:.2f}"
                    lbl.setText(str(v))
        try:
            for k, c in self.curves.items():
                c.setData(self.plot_data[k]['t'], self.plot_data[k]['y'])
        except: pass

    def update_conn_status(self, s):
        self.lbl_conn.setText(s.upper()[:6]) # Shorten to fit: DISCON, CONNEC
        color = FHighContrastTheme.OK if "CONNECTED" in s.upper() else FHighContrastTheme.WARN
        self.lbl_conn.setStyleSheet(f"color: {color}; font-weight: bold; font-size: 14pt;")

    def send_cmd(self, body):
        full = f"CMD,{self.team_id},{body}"
        self.serial_handler.send_command(full)
        self.console_log(f"[TX] {full}")

    def send_st_utc(self):
        t = time.strftime("%H:%M:%S", time.gmtime())
        self.send_cmd(f"ST,{t}")

    def send_mec(self, s):
        d = self.txt_mec.text()
        if d: self.send_cmd(f"MEC,{d},{s}")

    def load_pressure_csv(self):
        f, _ = QFileDialog.getOpenFileName(self, "Load Pressure CSV", ".", "CSV (*.csv)")
        if f:
            if self.simulation_manager.load_pressure_csv(f):
                self.lbl_sim_status.setText("Ready")
                self.lbl_sim_status.setStyleSheet(f"color: {FHighContrastTheme.OK};")
            else:
                self.lbl_sim_status.setText("Error")

    def load_telemetry_csv(self):
        f, _ = QFileDialog.getOpenFileName(self, "Load Telemetry CSV", ".", "CSV (*.csv)")
        if f:
             if self.replayer.load_flight_csv(f):
                 self.console_log(f"[REPLAY] Loaded {f}")
             else:
                 self.console_log("[REPLAY] Load Failed")

    def closeEvent(self, e):
        self.serial_handler.stop()
        self.pipeline.close()
        self.replayer.stop()
        e.accept()
