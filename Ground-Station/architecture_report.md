# CanSat 2026 Ground Control Station - Architecture Report (v2)

This document details the software architecture, class interactions, and data flow of the Ground Control Station (GCS).

## 1. High-Level Architecture
The GCS follows a **Controller-View** pattern:
*   **Controller (Backend)**: `TelemetryPipeline` is the central hub. It owns the Model (`TelemetryStore`) and Business Logic (`Parser`, `Logger`).
*   **View (frontend)**: `MainWindow` purely visualizes data pushed by the Pipeline signals.

## 2. Core Modules & Methods

### A. Backend Modules

#### 1. `TelemetryPipeline` (The Controller)
*   **Role**: Orchestrates the entire data lifecycle. Decouples logic from UI.
*   **Key Methods**:
    *   `handle_packet(line)`: 
        1. Emits `raw_packet_received` (for Console).
        2. Logs raw to file.
        3. Parses via `TelemetryParser`.
        4. Updates `TelemetryStore`.
        5. Logs parsed packet.
        6. Emits `packet_processed` (for Plots) and `model_updated` (for Labels).
*   **Signals**: `raw_packet_received`, `packet_processed`, `model_updated`.

#### 2. `SerialHandler` (Hardware I/O)
*   **Role**: Manages XBee connection on a background thread.
*   **Key Methods**: `run()` (Loop), `send_command()`.
*   **Output**: Emits `packet_received` -> Connected to `Pipeline.handle_packet`.

#### 3. `TelemetryReplayer` (Virtual I/O)
*   **Role**: Replays flight CSVs at 1Hz.
*   **Output**: Emits `packet_emitted` -> Connected to `Pipeline.handle_packet`.

#### 4. `TelemetryStore` (The Model)
*   **Role**: Pure data container for the *current state* of the mission.
*   **State**: `latest_packet`, `received_count`, `lost_count`, `link_age`.

---

### B. GUI Modules

#### 1. `MainWindow` (The View)
*   **Role**: Displays state. logic is minimized.
*   **Key Interactions**:
    *   **Subscribes** to `Pipeline.packet_processed` -> Appends to Plot Buffers.
    *   **Subscribes** to `Pipeline.model_updated` -> Reads `Pipeline.store` to update Text Labels.
    *   **User Action (Send Cmd)** -> Calls `SerialHandler.send_command` directly (Bypass Pipeline ok for TX).

---

## 3. Data Flow Diagrams

### A. Live Telemetry Flow (New)
1.  **Hardware** -> `SerialHandler` -> Signal `packet_received`.
2.  **`TelemetryPipeline.handle_packet()`**:
    *   -> `CSVLogger` (Disk I/O).
    *   -> `Parser` -> `Store` (State Update).
    *   -> Signal `packet_processed`.
3.  **`MainWindow`**:
    *   `update_data_buffers()`: Appends point to `pyqtgraph`.
    *   `update_ui()`: Polls `Pipeline.store` for text stats.

This architecture allows "Headless" operation testing where `TelemetryPipeline` runs without any GUI window instanced.
