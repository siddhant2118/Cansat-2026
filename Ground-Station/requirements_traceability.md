# Requirements Traceability Matrix (Strict Compliance)

| Req ID | Description | Implementation Details | Status |
|--------|-------------|------------------------|--------|
| G1 | Telemetry Format | `backend/parser.py`: Strict 22-field check. UTC time. | **COMPLIANT** |
| G2 | CSV Logging | `backend/csv_logger.py`: Logs parsed + raw. Header matches spec. | **COMPLIANT** |
| G3 | 1 Hz Transmission | GCS receives at any rate. Sim sends at 1 Hz (`backend/simulation.py`). | **COMPLIANT** |
| G4 | Telemetry Fields | `backend/parser.py`: All fields mapped. State/Mode strict strings. | **COMPLIANT** |
| G5 | Real-time Plotting | `gui/main_window.py`: 8+ Plots (Alt, Temp, Volt, Press, Acc, Gyro, GPS). | **COMPLIANT** |
| G6 | SI Units & Display | `gui/main_window.py`: Explicit units (Pa for SIMP, m/s², deg/s, etc). | **COMPLIANT** |
| G7 | Plot Visibility | `gui/main_window.py`: All plots visible in scrolling layout. | **COMPLIANT** |
| G8 | Packet Accounting | `backend/store.py`: RX, Lost, Bad Frames, Link Age. | **COMPLIANT** |
| G11 | Simulation Commands | `backend/simulation.py`: ENABLE -> ACTIVATE -> DISABLE enforced. | **COMPLIANT** |
| G12 | Simulation Pressure | `backend/simulation.py`: Reads CSV, sends `SIMP` in **Pascals**. | **COMPLIANT** |
| G14 | Sunlight Visibility | `gui/main_window.py`: High Contrast Light Theme, Large Fonts. | **COMPLIANT** |
| G15 | Single Window | `gui/main_window.py`: "Cockpit" layout, no tabs. | **COMPLIANT** |
| G16 | Packet Loss Calc | `backend/store.py`: Sequence-based loss calculation. | **COMPLIANT** |

## Manual Demo Checklist (Final)

### 1. Setup
- [ ] Connect XBee. Check `config.yaml` for `TERMINATOR: "\r"`.
- [ ] Launch `python3 main.py`.
- [ ] Verify "High Contrast" White/Black UI.

### 2. Commands & Simulation
- [ ] **SIM ENABLE**: Verify `CMD,1000,SIM,ENABLE\r`.
- [ ] **Load CSV**: Verify "Ready".
- [ ] **SIM ACTIVATE**: Verify `CMD,1000,SIM,ACTIVATE\r` then `SIMP` stream.
- [ ] **Verify Units**: SIMP commands should have values like `101325` (Pa), NOT `101.3`.
- [ ] **Manual SIMP**: Enter `100000` -> Verify `CMD,1000,SIMP,100000\r`.

### 3. Telemetry & Plots
- [ ] Verify **Link Age** < 1s when data flows.
- [ ] Verify **Mission Time** matches UTC (e.g. `14:30:05`).
- [ ] Verify **9 Plots** are scrolling.
- [ ] Verify **Raw Console** shows incoming data with `\r` handled cleanly.

### 4. Logging
- [ ] Verify `Flight_<ID>_*.csv` created.
- [ ] Verify last column is Raw Data.
