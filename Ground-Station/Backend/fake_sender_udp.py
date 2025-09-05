# fake_sender_udp.py — simple telemetry-only simulator (no command handling)

import socket, time, random
from datetime import datetime

from config import TEAM_ID, UDP_LISTEN_HOST, UDP_LISTEN_PORT
from schema import REQUIRED_HEADERS  # reminder of field order (not strictly required)

def build_line(packet_count: int, mode: str, state: str,
               alt_m: float, temp_c: float, press_kpa: float, vbatt_v: float,
               gyro=(0.12, -0.05, 0.01), accel=(0.02, 0.00, -0.01),
               mag=(0.23, 0.01, 0.04), auto_rate_dps=15,
               gps_time="00:00:00", gps_alt_m=5.0, lat=1.3000, lon=103.8000,
               gps_sats=6, cmd_echo="CXON", mission_time="00:00:00") -> str:
    """
    Build ONE telemetry CSV line in the exact Mission Guide order.
    All values are converted to strings; DO NOT put commas inside cmd_echo.
    """
    GYRO_R, GYRO_P, GYRO_Y = gyro
    ACCEL_R, ACCEL_P, ACCEL_Y = accel
    MAG_R, MAG_P, MAG_Y = mag

    fields = [
        TEAM_ID,                     # TEAM_ID (string)
        mission_time,                # MISSION_TIME (hh:mm:ss)
        str(packet_count),           # PACKET_COUNT
        mode,                        # MODE ('F' or 'S')
        state,                       # STATE (ASCII text)
        f"{alt_m:.1f}",              # ALTITUDE (m) 0.1
        f"{temp_c:.1f}",             # TEMPERATURE (°C) 0.1
        f"{press_kpa:.1f}",          # PRESSURE (kPa) 0.1
        f"{vbatt_v:.1f}",            # VOLTAGE (V) 0.1
        f"{GYRO_R:.2f}", f"{GYRO_P:.2f}", f"{GYRO_Y:.2f}",  # GYRO_* (deg/s)
        f"{ACCEL_R:.2f}", f"{ACCEL_P:.2f}", f"{ACCEL_Y:.2f}",  # ACCEL_* (deg/s^2)
        f"{MAG_R:.2f}", f"{MAG_P:.2f}", f"{MAG_Y:.2f}",    # MAG_* (gauss)
        str(auto_rate_dps),          # AUTO_GYRO_ROTATION_RATE (int deg/s)
        gps_time,                    # GPS_TIME (UTC hh:mm:ss)
        f"{gps_alt_m:.1f}",          # GPS_ALTITUDE (m)
        f"{lat:.4f}",                # GPS_LATITUDE (decimal degrees)
        f"{lon:.4f}",                # GPS_LONGITUDE (decimal degrees)
        str(gps_sats),               # GPS_SATS (int)
        cmd_echo                     # CMD_ECHO (no commas)
        # OPTIONAL fields would go here after ",," if you add them later
    ]
    return ",".join(fields) + "\n"   # newline marks end-of-packet


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    count = 0
    mode = "S"          # start in Simulation mode while testing
    state = "LAUNCH_PAD"
    alt = 0.0
    vbatt = 7.5
    temp = 27.5
    press_kpa = 101.3
    gps_sats = 6
    lat, lon = 1.3000, 103.8000
    gps_alt = 5.0
    cmd_echo = "CXON"   # pretend last command was "CXON"

    t0 = time.time()
    print(f"[fake] sending to {UDP_LISTEN_HOST}:{UDP_LISTEN_PORT} at 1 Hz. Ctrl+C to stop.")
    try:
        while True:
            # time since start, as hh:mm:ss (mission time)
            mission_time = time.strftime("%H:%M:%S", time.gmtime(time.time() - t0))

            # simple “flight-ish” profile
            if count < 10:
                state = "LAUNCH_PAD"
            elif count < 30:
                state = "ASCENT"; alt += 20.0
            elif count < 35:
                state = "APOGEE"
            elif count < 80:
                state = "DESCENT"; alt = max(0.0, alt - 15.0)
            else:
                state = "LANDED"; alt = 0.0

            # small realistic variations
            temp = 27.0 + 0.2 * random.uniform(-1, 1)
            press_kpa = 101.3 + 0.1 * random.uniform(-1, 1)
            vbatt = max(6.5, vbatt - 0.002)  # slow droop
            lat += 0.00001
            lon += 0.00001

            # GPS time can lag by a second; here we just match mission time
            gps_time = mission_time

            # build the CSV line in exact required order
            line = build_line(
                packet_count=count,
                mode=mode,
                state=state,
                alt_m=alt,
                temp_c=temp,
                press_kpa=press_kpa,
                vbatt_v=vbatt,
                gps_time=gps_time,
                gps_alt_m=gps_alt,
                lat=lat,
                lon=lon,
                gps_sats=gps_sats,
                cmd_echo=cmd_echo,
                mission_time=mission_time
            )

            # send one datagram per packet
            sock.sendto(line.encode("ascii"), (UDP_LISTEN_HOST, UDP_LISTEN_PORT))

            count += 1
            time.sleep(1.0)  # 1 Hz
    except KeyboardInterrupt:
        print("\n[fake] stopped.")
    finally:
        sock.close()

if __name__ == "__main__":
    main()