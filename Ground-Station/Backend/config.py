"""
Central settings your backend needs.
Keeping these in one place avoids 'magic numbers' scattered in code.
"""

# Your 4-digit team ID as a string (matches column TEAM_ID in telemetry)
TEAM_ID = "1000"   # <-- change to your real team ID when you know it

# Uplink (commands) listener for the fake CanSat (closed-loop sim)
UDP_CMD_LISTEN_HOST = "127.0.0.1"
UDP_CMD_LISTEN_PORT = 9001

# Where the receiver listens for telemetry during local testing.
# 127.0.0.1 means 'this computer' (localhost). Port 9000 is arbitrary but consistent.
UDP_LISTEN_HOST = "127.0.0.1"
UDP_LISTEN_PORT = 9000

# Where to put log files (receiver will create files here)
CSV_OUT_DIR = "../Logs"