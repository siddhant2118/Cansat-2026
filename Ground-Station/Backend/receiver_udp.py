import csv
import socket
import time
import pathlib
from datetime import datetime

#Import schema and config
from schema import REQUIRED_HEADERS, ALLOWED_STATES, ALLOWED_MODES, INDEX, has_required_field_count, is_valid_mode, is_valid_state
from config import TEAM_ID, UDP_LISTEN_HOST, UDP_LISTEN_PORT, CSV_OUT_DIR

def open_csv():
    """
    Make sure logs folder exists, open a new CSV, write the header row.
    Returns: (file_handle, csv_writer, path_to_file)
    """
    outdir = pathlib.Path(__file__).parent.joinpath(CSV_OUT_DIR)
    outdir.mkdir(parents=True, exist_ok=True)

    # Timestamp so each run writes a new file
    ts = datetime.utcnow().strftime("%Y%m%d_%H%M%S")
    path = outdir / f"Flight_{TEAM_ID}_{ts}.csv"

    f = path.open("w", newline="")          # newline="" is the correct way for CSV on all OSes
    writer = csv.writer(f)
    writer.writerow(REQUIRED_HEADERS)        # judges expect a header row
    return f, writer, path


def parse_and_validate(line: str):
    """
    Take a raw UDP line (ASCII), split it by commas, and validate key fields.
    Returns (row_as_list_of_strings, packet_count_int) if OK, or raises ValueError.
    """
    fields = line.strip().split(",")         # 'strip' removes \r\n at end; split(',') -> list[str]

    # 1) Enough fields?
    if not has_required_field_count(fields):
        raise ValueError(f"too few fields: {len(fields)} < {len(REQUIRED_HEADERS)}")

    # 2) TEAM_ID must match our config (prevents mixing radios/teams)
    team_id = fields[INDEX["TEAM_ID"]].strip()
    if team_id != TEAM_ID:
        raise ValueError(f"wrong TEAM_ID {team_id} != {TEAM_ID}")

    # 3) MODE in {'F','S'}
    mode = fields[INDEX["MODE"]].strip()
    if not is_valid_mode(mode):
        raise ValueError(f"bad MODE {mode}")

    # 4) STATE in allowed set
    state = fields[INDEX["STATE"]].strip()
    if not is_valid_state(state):
        raise ValueError(f"bad STATE {state}")

    # 5) PACKET_COUNT must be an integer (used for lost-packet calculation)
    try:
        tx_count = int(fields[INDEX["PACKET_COUNT"]])
    except ValueError:
        raise ValueError(f"PACKET_COUNT not an int: {fields[INDEX['PACKET_COUNT']]}")

    # Return only the required columns (exact order), plus the packet count (as int)
    return fields[:len(REQUIRED_HEADERS)], tx_count


def main():
    # ---- Open network socket (UDP) ----
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # DGRAM = UDP
    sock.bind((UDP_LISTEN_HOST, UDP_LISTEN_PORT))            # start listening
    sock.settimeout(1.0)                                     # don't block forever

    # ---- Open CSV log ----
    f, writer, path = open_csv()

    # ---- Runtime counters ----
    received = 0
    lost = 0
    last_tx_count = None
    last_rx_time = time.time()

    print(f"[receiver] listening on {UDP_LISTEN_HOST}:{UDP_LISTEN_PORT}")
    print(f"[receiver] writing CSV to {path}")

    try:
        while True:
            try:
                data, addr = sock.recvfrom(8192)     # read up to 8192 bytes from any sender
            except socket.timeout:
                # report link health if nothing for a while
                if time.time() - last_rx_time > 2.5:
                    print("[receiver] no packets in >2.5s")
                continue

            # Convert bytes -> string; ignore any weird non-ASCII bytes
            line = data.decode("ascii", errors="ignore")
            if not line.strip():
                continue

            try:
                row, tx_count = parse_and_validate(line)
            except Exception as e:
                # give context + the first ~80 chars of the bad line
                print(f"[receiver] parse error: {e} :: {line[:80]!r}")
                continue

            # ---- lost packets from PACKET_COUNT gap ----
            if last_tx_count is not None and tx_count > last_tx_count + 1:
                # e.g., last=41, new=45 -> lost 42,43 -> +2
                lost += (tx_count - last_tx_count - 1)
            last_tx_count = tx_count

            # ---- write to CSV (exact strings in exact order) ----
            writer.writerow(row)
            f.flush()                # keep file current even if program exits unexpectedly
            received += 1
            last_rx_time = time.time()

            # ---- heartbeat every 5 packets ----
            if received % 5 == 0:
                state = row[INDEX["STATE"]]
                alt   = row[INDEX["ALTITUDE"]]
                batt  = row[INDEX["VOLTAGE"]]
                print(f"[receiver] rx={received} lost={lost} state={state} alt={alt}m batt={batt}V")
    except KeyboardInterrupt:
        print("\n[receiver] stopping...")
    finally:
        f.close()
        sock.close()


if __name__ == "__main__":
    main()