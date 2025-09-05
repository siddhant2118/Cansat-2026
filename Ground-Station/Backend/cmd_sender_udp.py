import sys
import socket

from config import TEAM_ID
# Optional: enable actual UDP sending by defining these in config.py
# UDP_SEND_HOST = "127.0.0.1"
# UDP_SEND_PORT = 9001
try:
    from config import UDP_SEND_HOST, UDP_SEND_PORT
    ENABLE_UDP_SEND = True
except ImportError:
    ENABLE_UDP_SEND = False


def format_cmd(parts: list[str]) -> str:
    if not parts:
        raise ValueError("Empty input")
    name = parts[0].upper()

    if name == "CAL":
        return f"CMD,{TEAM_ID},CAL"

    if name == "CX":
        if len(parts) != 2 or parts[1].upper() not in {"ON", "OFF"}:
            raise ValueError("Usage: CX ON|OFF")
        return f"CMD,{TEAM_ID},CX,{parts[1].upper()}"

    if name == "ST":
        if len(parts) != 2:
            raise ValueError("Usage: ST hh:mm:ss | ST GPS")
        arg = parts[1].upper()
        if arg == "GPS":
            return f"CMD,{TEAM_ID},ST,GPS"
        if len(arg) == 8 and arg[2] == ":" and arg[5] == ":":
            return f"CMD,{TEAM_ID},ST,{arg}"
        raise ValueError("ST must be hh:mm:ss or GPS")

    if name == "SIM":
        if len(parts) != 2 or parts[1].upper() not in {"ENABLE", "ACTIVATE", "DISABLE"}:
            raise ValueError("Usage: SIM ENABLE|ACTIVATE|DISABLE")
        return f"CMD,{TEAM_ID},SIM,{parts[1].upper()}"

    if name == "SIMP":
        if len(parts) != 2 or not parts[1].isdigit():
            raise ValueError("Usage: SIMP <pressure_pa>")
        return f"CMD,{TEAM_ID},SIMP,{parts[1]}"

    if name == "MEC":
        if len(parts) != 3 or parts[2].upper() not in {"ON", "OFF"}:
            raise ValueError("Usage: MEC <DEVICE> <ON|OFF>")
        device = parts[1].upper()
        if "," in device:
            raise ValueError("DEVICE must not contain commas")
        return f"CMD,{TEAM_ID},MEC,{device},{parts[2].upper()}"

    raise ValueError(f"Unknown command {name}")


def maybe_send_udp(line: str):
    if ENABLE_UDP_SEND:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.sendto(line.encode("ascii"), (UDP_SEND_HOST, UDP_SEND_PORT))
            sock.close()
            print(f"[cmd] sent over UDP to {UDP_SEND_HOST}:{UDP_SEND_PORT}")
        except Exception as e:
            print(f"[cmd] UDP send failed: {e}")


def main():
    print("[cmd] Enter commands (CAL, CX ON, ST GPS, SIM ENABLE, etc.) Ctrl+C to quit.")
    try:
        for raw in sys.stdin:
            parts = raw.strip().split()
            if not parts:
                continue
            try:
                cmd_line = format_cmd(parts)
            except ValueError as e:
                print(f"[cmd] error: {e}")
                continue
            print(f"[cmd] {cmd_line}")
            maybe_send_udp(cmd_line)
    except KeyboardInterrupt:
        print("\n[cmd] stopped.")


if __name__ == "__main__":
    main()
