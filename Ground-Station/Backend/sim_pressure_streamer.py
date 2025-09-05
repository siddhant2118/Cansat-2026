# sim_pressure_streamer.py
# Streams pressure values from a CSV at a precise rate as SIMP commands.
import csv
import time
from pathlib import Path
from typing import Optional

from config import TEAM_ID
from cmd_sender_udp import maybe_send_udp


def stream_pressure(csv_path: str,
                    rate_hz: float = 1.0,
                    enforce_range: bool = False,
                    min_pa: int = 50000,
                    max_pa: int = 110000) -> None:
    """
    Read pressure values from a CSV file (first column per row) and send each
    as CMD,<TEAM_ID>,SIMP,<pressure_pa> at a fixed rate.

    Parameters
    ----------
    csv_path : str
        Path to a CSV file. First column should be pressure in Pascals.
    rate_hz : float
        Commands per second (default 1.0). Use 1.0 to match Mission Guide.
    enforce_range : bool
        If True, drop values outside [min_pa, max_pa] and warn.
    min_pa, max_pa : int
        Allowed range (only used if enforce_range=True).
    """
    p = Path(csv_path)
    if not p.exists():
        raise FileNotFoundError(f"Pressure profile file not found: {csv_path}")

    if rate_hz <= 0:
        raise ValueError("rate_hz must be > 0")

    delay = 1.0 / rate_hz
    next_t = time.perf_counter()

    with p.open(newline="") as f:
        reader = csv.reader(f)
        try:
            for row in reader:
                # Skip empty rows
                if not row:
                    continue

                # Take first column; strip whitespace
                raw = row[0].strip()
                if not raw:
                    continue

                # Parse number (accepts integers or floats); convert to integer Pascals
                try:
                    pressure_val = int(round(float(raw)))
                except ValueError:
                    print(f"[sim] skipping invalid value (not a number): {raw!r}")
                    continue

                # Optional range check
                if enforce_range and not (min_pa <= pressure_val <= max_pa):
                    print(f"[sim] skipping out-of-range value: {pressure_val} Pa "
                          f"(allowed {min_pa}..{max_pa})")
                    continue

                line = f"CMD,{TEAM_ID},SIMP,{pressure_val}"
                print(f"[sim] {line}")
                maybe_send_udp(line)

                # Precise pacing to reduce drift
                next_t += delay
                sleep_for = next_t - time.perf_counter()
                if sleep_for > 0:
                    time.sleep(sleep_for)
        except KeyboardInterrupt:
            print("\n[sim] stopped by user")


def main():
    import sys
    if len(sys.argv) not in (2, 3):
        print("Usage: python sim_pressure_streamer.py <pressure_profile.csv> [rate_hz]")
        print("  Example: python sim_pressure_streamer.py profile.csv 1.0")
        return

    csv_path = sys.argv[1]
    rate_hz: Optional[float] = 1.0
    if len(sys.argv) == 3:
        try:
            rate_hz = float(sys.argv[2])
        except ValueError:
            print("[sim] invalid rate_hz (must be a number). Using 1.0.")
            rate_hz = 1.0

    stream_pressure(csv_path, rate_hz=rate_hz)


if __name__ == "__main__":
    main()

