#!/usr/bin/env python3
"""
ground_station_competition.py — CanSat 2026 Ground Station (mission-compliant)
Framework: PyQt6 + pyqtgraph

Features (Mission Guide compliant):
- One window, high-contrast light theme, large fonts (sunlight-readable)
- Telemetry at 1 Hz (or faster): display ALL fields from schema.REQUIRED_HEADERS
- Real-time plots: Altitude, Battery Voltage, Temperature, Pressure, Accel (R/P/Y), Gyro (R/P/Y)
- UDP source indicators: last sender IP:port, packet rate (Hz), packet count, dropped (from PACKET_COUNT gaps)
- Command panel: CX ON/OFF, ST GPS, ST SYS TIME, CAL, SIM ENABLE/ACTIVATE/DISABLE, MEC (device ON/OFF)
- CSV Logging: Start/Stop log to Flight_<TEAMID>.csv with exact REQUIRED_HEADERS
- Replay mode: Load CSV and play back at adjustable speed (0.1x–10x)
- Simulation profile streamer: Load pressure CSV (column 'pressure_pa') and send SIMP <pa> at chosen rate
- Console log: RX/TX/status/errors

Run:
  pip install PyQt6 pyqtgraph
  python ground_station_competition.py
"""

import sys, socket, threading, time, csv, os, math
from typing import Dict, Any, List, Optional, Tuple

from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QFileDialog, QPlainTextEdit, QFrame, QComboBox,
    QDoubleSpinBox
)
import pyqtgraph as pg

# ---- Project backend ----
from config import UDP_LISTEN_HOST, UDP_LISTEN_PORT, TEAM_ID
from schema import REQUIRED_HEADERS
try:
    from cmd_sender_udp import maybe_send_udp
    HAVE_CMD = True
except Exception:
    HAVE_CMD = False


# ---- Helpers ----
def now_hms() -> str:
    return time.strftime("%H:%M:%S")

def safe_float(s, default=float('nan')) -> float:
    try:
        return float(s)
    except Exception:
        return default


# ---- Telemetry Model ----
class TelemetryModel(QObject):
    updated = pyqtSignal(dict)
    error = pyqtSignal(str)

    def __init__(self, maxlen=20000):
        super().__init__()
        self.rows: List[Dict[str, Any]] = []
        self.maxlen = maxlen
        self.start_wall = time.time()
        self.last_pkt: Optional[int] = None
        self.dropped = 0

    def append_csv_line(self, line: str):
        parts = line.strip().split(",")
        if len(parts) != len(REQUIRED_HEADERS):
            self.error.emit(f"Bad columns: {len(parts)} != {len(REQUIRED_HEADERS)}")
            return
        row = dict(zip(REQUIRED_HEADERS, parts))
        # packet loss tracking
        pkt = row.get("PACKET_COUNT")
        if pkt and pkt.isdigit():
            pkt = int(pkt)
            if self.last_pkt is not None and pkt > self.last_pkt + 1:
                self.dropped += (pkt - self.last_pkt - 1)
            self.last_pkt = pkt
        row["_t_wall"] = time.time()
        self.rows.append(row)
        if len(self.rows) > self.maxlen:
            self.rows = self.rows[-self.maxlen:]
        self.updated.emit(row)

    def series(self, key: str) -> Tuple[List[float], List[float]]:
        xs = [r["_t_wall"] - self.start_wall for r in self.rows]
        ys = [safe_float(r.get(key, "")) for r in self.rows]
        return xs, ys


# ---- UDP Receiver ----
class UdpReceiver(QObject):
    rx = pyqtSignal(str, str)  # (line, addr_str)
    status = pyqtSignal(str)

    def __init__(self, host: str, port: int):
        super().__init__()
        self.host, self.port = host, port
        self._thr: Optional[threading.Thread] = None
        self._stop = threading.Event()

    def start(self):
        if self._thr and self._thr.is_alive(): return
        self._stop.clear()
        self._thr = threading.Thread(target=self._loop, daemon=True)
        self._thr.start()
        self.status.emit(f"UDP listening on {self.host}:{self.port}")

    def stop(self):
        self._stop.set()
        self.status.emit("UDP stopped")

    def _loop(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((self.host, self.port))
        sock.settimeout(0.5)
        try:
            while not self._stop.is_set():
                try:
                    data, addr = sock.recvfrom(65535)
                except socket.timeout:
                    continue
                line = data.decode("ascii", errors="ignore").strip()
                if not line:
                    continue
                self.rx.emit(line, f"{addr[0]}:{addr[1]}")
        finally:
            sock.close()


# ---- Profile SIMP Streamer ----
class ProfileStreamer(QObject):
    status = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self._thr: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self.path: Optional[str] = None
        self.period_s: float = 1.0

    def configure(self, path: str, rate_hz: float):
        self.path = path
        self.period_s = 1.0 / max(0.1, rate_hz)

    def start(self):
        if not HAVE_CMD:
            self.status.emit("cmd_sender_udp not available; cannot SIMP")
            return
        if not self.path:
            self.status.emit("Load a pressure profile CSV first")
            return
        if self._thr and self._thr.is_alive(): return
        self._stop.clear()
        self._thr = threading.Thread(target=self._loop, daemon=True)
        self._thr.start()
        self.status.emit(f"SIMP streaming {os.path.basename(self.path)} @ {1.0/self.period_s:.2f} Hz")

    def stop(self):
        self._stop.set()
        self.status.emit("SIMP streaming stopped")

    def _loop(self):
        assert self.path
        try:
            with open(self.path, 'r') as f:
                rdr = csv.DictReader(f)
                if "pressure_pa" not in (rdr.fieldnames or []):
                    self.status.emit("Profile must have 'pressure_pa' column")
                    return
                for row in rdr:
                    if self._stop.is_set(): break
                    p = row.get("pressure_pa", "")
                    try:
                        maybe_send_udp(f"SIMP {p}")
                    except Exception as e:
                        self.status.emit(f"SIMP error: {e}")
                        break
                    time.sleep(self.period_s)
        except Exception as e:
            self.status.emit(f"Streamer error: {e}")


# ---- CSV Replay ----
class CsvReplayer(QObject):
    line = pyqtSignal(str)
    status = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self._thr: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self.path: Optional[str] = None
        self.speed: float = 1.0

    def configure(self, path: str, speed: float = 1.0):
        self.path = path
        self.speed = max(0.1, speed)

    def start(self):
        if not self.path:
            self.status.emit("Choose a CSV to replay")
            return
        if self._thr and self._thr.is_alive(): return
        self._stop.clear()
        self._thr = threading.Thread(target=self._loop, daemon=True)
        self._thr.start()
        self.status.emit(f"Replaying {os.path.basename(self.path)} x{self.speed:.1f}")

    def stop(self):
        self._stop.set()
        self.status.emit("Replay stopped")

    def _loop(self):
        try:
            with open(self.path, 'r') as f:
                rdr = csv.reader(f)
                headers = next(rdr, None)
                if headers != REQUIRED_HEADERS:
                    self.status.emit("CSV headers mismatch; expected REQUIRED_HEADERS")
                    return
                period = 1.0 / self.speed
                for row in rdr:
                    if self._stop.is_set(): break
                    self.line.emit(",".join(row))
                    time.sleep(period)
        except Exception as e:
            self.status.emit(f"Replay error: {e}")


# ---- GUI ----
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("CanSat 2026 Ground Station — Competition Console")
        self.resize(1500, 900)

        # Data & IO
        self.model = TelemetryModel()
        self.model.updated.connect(self.on_latest)
        self.model.error.connect(self.on_error)

        self.rx = UdpReceiver(UDP_LISTEN_HOST, UDP_LISTEN_PORT)
        self.rx.rx.connect(self.on_udp_line)
        self.rx.status.connect(self.log)

        self.streamer = ProfileStreamer()
        self.streamer.status.connect(self.log)

        self.replayer = CsvReplayer()
        self.replayer.line.connect(self.model.append_csv_line)
        self.replayer.status.connect(self.log)

        # Logging
        self.logging = False
        self.log_file: Optional[object] = None
        self.log_writer: Optional[csv.writer] = None

        # Stats
        self.recv_times: List[float] = []
        self.packet_count = 0
        self.last_sender = "—"

        # ---- Build UI ----
        central = QWidget(self)
        root = QVBoxLayout(central)
        root.setContentsMargins(12,12,12,12)
        root.setSpacing(10)

        # Top header: source & control
        header = QHBoxLayout()
        self.lbl_sender = QLabel("Sender: —")
        self.lbl_rate = QLabel("Rate: 0.00 Hz")
        self.lbl_pkts = QLabel("Packets: 0")
        self.lbl_drop = QLabel("Dropped: 0")
        for w in (self.lbl_sender, self.lbl_rate, self.lbl_pkts, self.lbl_drop):
            w.setStyleSheet("font: 14pt; font-weight: 600; color: #0b0f14;")
            header.addWidget(w)
        header.addStretch(1)

        self.btn_udp = QPushButton("Start UDP")
        self.btn_udp.clicked.connect(self.toggle_udp)
        self.btn_log = QPushButton("Start Log")
        self.btn_log.setCheckable(True)
        self.btn_log.clicked.connect(self.toggle_log)
        self.btn_save = QPushButton("Save Snapshot CSV")
        self.btn_save.clicked.connect(self.save_snapshot)

        for b in (self.btn_udp, self.btn_log, self.btn_save):
            b.setMinimumHeight(36)
            header.addWidget(b)
        root.addLayout(header)

        # Mid: Telemetry grid (left) + Plots (right)
        mid = QHBoxLayout()

        # Telemetry card
        tele_card = QFrame(); tele_card.setStyleSheet("QFrame{background:#ffffff;border:1px solid #e5e7eb;border-radius:12px;}")
        tlay = QGridLayout(tele_card); tlay.setHorizontalSpacing(12); tlay.setVerticalSpacing(6); tlay.setContentsMargins(12,12,12,12)
        keys = [
            "TEAM_ID","MISSION_TIME","MODE","STATE","PACKET_COUNT","CMD_ECHO",
            "ALTITUDE","TEMPERATURE","PRESSURE","VOLTAGE",
            "GYRO_R","GYRO_P","GYRO_Y",
            "ACCEL_R","ACCEL_P","ACCEL_Y",
            "MAG_R","MAG_P","MAG_Y",
            "GPS_TIME","GPS_LATITUDE","GPS_LONGITUDE","GPS_ALTITUDE","GPS_SATS",
        ]
        self.tele_labels: Dict[str, QLabel] = {}
        r = 0
        for k in keys:
            name = QLabel(k+":"); name.setStyleSheet("font: 700 13pt 'Segoe UI'; color:#0b0f14;")
            val = QLabel("—"); val.setStyleSheet("font: 13pt 'Segoe UI'; color:#111827;")
            self.tele_labels[k] = val
            tlay.addWidget(name, r, 0, alignment=Qt.AlignmentFlag.AlignRight)
            tlay.addWidget(val, r, 1)
            r += 1
        mid.addWidget(tele_card, 1)

        # Plots card
        plots_card = QFrame(); plots_card.setStyleSheet("QFrame{background:#ffffff;border:1px solid #e5e7eb;border-radius:12px;}")
        pv = QVBoxLayout(plots_card); pv.setSpacing(8); pv.setContentsMargins(12,12,12,12)

        # Row 1: Altitude & Voltage
        row1 = QHBoxLayout()
        self.p_alt = pg.PlotWidget(background='w'); self.cur_alt = self.p_alt.plot(pen=pg.mkPen('#1f77b4', width=3))
        self.p_alt.setLabel('left','Altitude (m)'); self.p_alt.setLabel('bottom','Time (s)')
        self.p_v = pg.PlotWidget(background='w'); self.cur_v = self.p_v.plot(pen=pg.mkPen('#2ca02c', width=3))
        self.p_v.setLabel('left','Voltage (V)'); self.p_v.setLabel('bottom','Time (s)')
        row1.addWidget(self.p_alt,1); row1.addWidget(self.p_v,1)
        pv.addLayout(row1,1)

        # Row 2: Temperature & Pressure
        row2 = QHBoxLayout()
        self.p_t = pg.PlotWidget(background='w'); self.cur_t = self.p_t.plot(pen=pg.mkPen('#ff7f0e', width=3))
        self.p_t.setLabel('left','Temp (°C)'); self.p_t.setLabel('bottom','Time (s)')
        self.p_p = pg.PlotWidget(background='w'); self.cur_p = self.p_p.plot(pen=pg.mkPen('#111827', width=2))
        self.p_p.setLabel('left','Pressure'); self.p_p.setLabel('bottom','Time (s)')
        row2.addWidget(self.p_t,1); row2.addWidget(self.p_p,1)
        pv.addLayout(row2,1)

        # Row 3: Accel & Gyro
        row3 = QHBoxLayout()
        self.p_acc = pg.PlotWidget(background='w')
        self.cur_acc = [self.p_acc.plot(pen=pg.mkPen(c, width=2)) for c in ('#dc2626','#10b981','#2563eb')]
        self.p_acc.setLabel('left','Accel (g)'); self.p_acc.setLabel('bottom','Time (s)')
        self.p_gyro = pg.PlotWidget(background='w')
        self.cur_gyro = [self.p_gyro.plot(pen=pg.mkPen(c, width=2)) for c in ('#dc2626','#10b981','#2563eb')]
        self.p_gyro.setLabel('left','Gyro (°/s)'); self.p_gyro.setLabel('bottom','Time (s)')
        row3.addWidget(self.p_acc,1); row3.addWidget(self.p_gyro,1)
        pv.addLayout(row3,1)

        mid.addWidget(plots_card, 2)
        root.addLayout(mid, 1)

        # Bottom command bar
        cmd = QHBoxLayout()
        def mkbtn(text, fn):
            b = QPushButton(text); b.setMinimumHeight(40); b.setStyleSheet("font: 600 13pt 'Segoe UI';"); b.clicked.connect(fn); return b

        self.btn_cx_on = mkbtn("CX ON", lambda: self.send_cmd("CX ON"))
        self.btn_cx_off = mkbtn("CX OFF", lambda: self.send_cmd("CX OFF"))
        self.btn_st_gps = mkbtn("ST GPS", lambda: self.send_cmd("ST GPS"))
        self.btn_st_sys = mkbtn("ST SYS TIME", self.st_sys_time)
        self.btn_cal = mkbtn("CAL", lambda: self.send_cmd("CAL"))

        self.btn_sim_en = mkbtn("SIM ENABLE", lambda: self.send_cmd("SIM ENABLE"))
        self.btn_sim_act = mkbtn("SIM ACTIVATE", lambda: self.send_cmd("SIM ACTIVATE"))
        self.btn_sim_dis = mkbtn("SIM DISABLE", lambda: self.send_cmd("SIM DISABLE"))

        self.mec_sel = QComboBox(); self.mec_sel.addItems(["SERVO1","SERVO2","CAM","BEACON"])
        self.mec_sel.setStyleSheet("font: 12pt;")
        self.btn_mec_on = mkbtn("MEC ON", lambda: self.send_cmd(f"MEC {self.mec_sel.currentText()} ON"))
        self.btn_mec_off = mkbtn("MEC OFF", lambda: self.send_cmd(f"MEC {self.mec_sel.currentText()} OFF"))

        for w in [self.btn_cx_on, self.btn_cx_off, self.btn_st_gps, self.btn_st_sys, self.btn_cal,
                  self.btn_sim_en, self.btn_sim_act, self.btn_sim_dis,
                  self.mec_sel, self.btn_mec_on, self.btn_mec_off]:
            cmd.addWidget(w)

        # Replay & SIMP streamer
        self.btn_load_replay = QPushButton("Replay CSV…"); self.btn_load_replay.clicked.connect(self.load_replay); self.btn_load_replay.setMinimumHeight(40)
        self.spin_speed = QDoubleSpinBox(); self.spin_speed.setRange(0.1, 10.0); self.spin_speed.setValue(1.0); self.spin_speed.setPrefix("x "); self.spin_speed.setMinimumHeight(40)
        self.btn_replay = QPushButton("Start Replay"); self.btn_replay.setCheckable(True); self.btn_replay.clicked.connect(self.toggle_replay); self.btn_replay.setMinimumHeight(40)

        self.btn_load_prof = QPushButton("Load Profile…"); self.btn_load_prof.clicked.connect(self.load_profile); self.btn_load_prof.setMinimumHeight(40)
        self.spin_prof = QDoubleSpinBox(); self.spin_prof.setRange(0.1, 10.0); self.spin_prof.setValue(1.0); self.spin_prof.setSuffix(" Hz"); self.spin_prof.setMinimumHeight(40)
        self.btn_stream = QPushButton("Start SIMP Stream"); self.btn_stream.setCheckable(True); self.btn_stream.clicked.connect(self.toggle_stream); self.btn_stream.setMinimumHeight(40)

        for w in [self.btn_load_replay, self.spin_speed, self.btn_replay, self.btn_load_prof, self.spin_prof, self.btn_stream]:
            w.setStyleSheet("font: 12pt;")
            cmd.addWidget(w)

        cmd.addStretch(1)
        root.addLayout(cmd)

        # Console
        self.console = QPlainTextEdit(); self.console.setReadOnly(True); self.console.setMaximumBlockCount(600)
        self.console.setStyleSheet("background:#111827;color:#e5e7eb;font: 12pt 'Consolas', 'Menlo', monospace; border-radius:8px; padding:8px;")
        root.addWidget(self.console, 1)

        self.setCentralWidget(central)

        # Light, high-contrast background (sunlight readable)
        self.setStyleSheet("QWidget{background:#f9fafb;color:#111827;} QLabel{color:#111827;} QPushButton{background:#e5e7eb;border:1px solid #d1d5db;border-radius:10px;padding:6px 12px;} QPushButton:hover{background:#e2e8f0;}")

        # Timers
        self.ui_timer = QTimer(self); self.ui_timer.setInterval(250); self.ui_timer.timeout.connect(self.refresh_ui); self.ui_timer.start(250)

    # ---- Actions ----
    def toggle_udp(self):
        if self.btn_udp.text().startswith("Start"):
            self.rx.start()
            self.btn_udp.setText("Stop UDP")
        else:
            self.rx.stop()
            self.btn_udp.setText("Start UDP")

    def toggle_log(self):
        if self.btn_log.isChecked():
            # Default name per mission guide
            ts = time.strftime("%Y%m%d_%H%M%S")
            default = f"Flight_{TEAM_ID}_{ts}.csv"
            path, _ = QFileDialog.getSaveFileName(self, "Log to CSV", default, "CSV (*.csv)")
            if not path:
                self.btn_log.setChecked(False); return
            try:
                f = open(path, 'w', newline='')
                w = csv.writer(f)
                w.writerow(REQUIRED_HEADERS)
                self.log_file, self.log_writer = f, w
                self.logging = True
                self.log(f"Logging → {path}")
                self.btn_log.setText("Stop Log")
            except Exception as e:
                self.log(f"Log open error: {e}")
                self.btn_log.setChecked(False)
        else:
            self.logging = False
            if self.log_file:
                try: self.log_file.close()
                except Exception: pass
            self.log_file = None; self.log_writer = None
            self.btn_log.setText("Start Log")
            self.log("Logging stopped")

    def save_snapshot(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save Snapshot CSV", f"Snapshot_{TEAM_ID}.csv", "CSV (*.csv)")
        if not path: return
        try:
            with open(path, 'w', newline='') as f:
                w = csv.writer(f); w.writerow(REQUIRED_HEADERS)
                for r in self.model.rows:
                    w.writerow([r.get(k,"") for k in REQUIRED_HEADERS])
            self.log(f"Snapshot saved ({len(self.model.rows)} rows) → {path}")
        except Exception as e:
            self.log(f"Snapshot error: {e}")

    def load_replay(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open CSV to replay", "", "CSV (*.csv)")
        if not path: return
        self.replayer.configure(path, self.spin_speed.value())
        self.log(f"Loaded replay: {path}")

    def toggle_replay(self):
        if self.btn_replay.isChecked():
            self.replayer.configure(self.replayer.path or "", self.spin_speed.value())
            self.replayer.start()
            self.btn_replay.setText("Stop Replay")
        else:
            self.replayer.stop()
            self.btn_replay.setText("Start Replay")

    def load_profile(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open pressure profile CSV", "", "CSV (*.csv)")
        if not path: return
        self.streamer.configure(path, self.spin_prof.value())
        self.log(f"Loaded profile: {path}")

    def toggle_stream(self):
        if self.btn_stream.isChecked():
            self.streamer.configure(self.streamer.path or "", self.spin_prof.value())
            self.streamer.start()
            self.btn_stream.setText("Stop SIMP Stream")
        else:
            self.streamer.stop()
            self.btn_stream.setText("Start SIMP Stream")

    def st_sys_time(self):
        # Send ST with host system time hh:mm:ss
        t = time.strftime("%H:%M:%S")
        self.send_cmd(f"ST {t}")

    def send_cmd(self, cmd: str):
        try:
            if HAVE_CMD:
                maybe_send_udp(cmd)
                self.log("TX "+cmd)
            else:
                self.log("TX (backend not present) "+cmd)
        except Exception as e:
            self.log(f"cmd error: {e}")

    # ---- RX & UI ----
    def on_udp_line(self, line: str, addr: str):
        self.last_sender = addr
        self.packet_count += 1
        self.recv_times.append(time.time())
        if len(self.recv_times) > 2000:
            self.recv_times = self.recv_times[-2000:]

        # Append to model (validates columns)
        self.model.append_csv_line(line)

        # If logging, write raw row in REQUIRED_HEADERS order
        if self.logging and self.log_writer:
            parts = line.strip().split(",")
            if len(parts) == len(REQUIRED_HEADERS):
                try:
                    self.log_writer.writerow(parts)
                except Exception:
                    pass

    def on_latest(self, row: Dict[str, Any]):
        # update labels
        for k, lab in self.tele_labels.items():
            if k in row:
                lab.setText(str(row[k]))

    def on_error(self, msg: str):
        self.log("ERR "+msg)

    def refresh_ui(self):
        # compute rate over last 5s
        now = time.time()
        self.recv_times = [t for t in self.recv_times if now - t <= 5.0]
        rate = len(self.recv_times) / 5.0
        self.lbl_rate.setText(f"Rate: {rate:.2f} Hz")
        self.lbl_pkts.setText(f"Packets: {self.packet_count}")
        self.lbl_drop.setText(f"Dropped: {self.model.dropped}")
        self.lbl_sender.setText(f"Sender: {self.last_sender}")

        # plots
        if not self.model.rows: return
        xs, alt = self.model.series("ALTITUDE")
        _, volt = self.model.series("VOLTAGE")
        _, temp = self.model.series("TEMPERATURE")
        _, pres = self.model.series("PRESSURE")
        self.cur_alt.setData(xs, alt)
        self.cur_v.setData(xs, volt)
        self.cur_t.setData(xs, temp)
        self.cur_p.setData(xs, pres)

        for (curve, key) in zip(self.cur_acc, ["ACCEL_R","ACCEL_P","ACCEL_Y"]):
            _, ys = self.model.series(key); curve.setData(xs, ys)
        for (curve, key) in zip(self.cur_gyro, ["GYRO_R","GYRO_P","GYRO_Y"]):
            _, ys = self.model.series(key); curve.setData(xs, ys)

    def log(self, s: str):
        self.console.appendPlainText(f"[{now_hms()}] {s}")


def main():
    app = QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    win = MainWindow(); win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()