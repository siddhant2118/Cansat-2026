""" Source for the telemetry CSV layout and allowed enums."""

# -------- 1) REQUIRED CSV HEADER (ORDERED LIST) -----------------
# LIST = [ ... ] keeps order. The CSV columns must appear in THIS order.

REQUIRED_HEADERS = [
    "TEAM_ID", "MISSION_TIME", "PACKET_COUNT", "MODE", "STATE",
    "ALTITUDE", "TEMPERATURE", "PRESSURE", "VOLTAGE",
    "GYRO_R", "GYRO_P", "GYRO_Y",
    "ACCEL_R", "ACCEL_P", "ACCEL_Y",
    "MAG_R", "MAG_P", "MAG_Y",
    "AUTO_GYRO_ROTATION_RATE",
    "GPS_TIME", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS",
    "CMD_ECHO"
]

# Optional fields come after "CMD_ECHO"



# -------- 2) ALLOWED VALUES (UNORDERED SETS) --------------------
# SET = { ... } is for membership checks: "is this value allowed?"

ALLOWED_MODES = {"F" , "S"} # F = Flight, S = Simulation

ALLOWED_STATES = {
    "LAUNCH_PAD",
    "ASCENT",
    "APOGEE",
    "DESCENT",
    "PROBE_RELEASE",
    "PAYLOAD_RELEASE",
    "LANDED",
}



# -------- 3) NAME -> INDEX MAP (DICTIONARY) ---------------------
# DICT = {key: value, ...} maps a column name to its integer position.
# This lets you write fields[INDEX["ALTITUDE"]] instead of remembering "ALTITUDE is column 6".
INDEX = {name: i for i, name in enumerate(REQUIRED_HEADERS)}



# -------- 4) Tiny helper checks --------------------------------
def has_required_field_count(fields: list[str]) -> bool:
    """True if a parsed CSV row has at least all required fields."""
    return len(fields) >= len(REQUIRED_HEADERS)

def is_valid_mode(mode_text: str) -> bool:
    """True only if mode_text is 'F' or 'S'."""
    return mode_text in ALLOWED_MODES

def is_valid_state(state_text: str) -> bool:
    """True only if state_text is one of the allowed state strings."""
    return state_text in ALLOWED_STATES
    
    
    
# -------- 5) Self-test (runs only if you execute schema.py) -----
# if u run file directly __name__ = "__main__" runs but if you import this from another module this block is skipped

if __name__ == "__main__":
    sample_line = (
        "1000,13:14:02,123,F,ASCENT,452.3,27.5,95.3,7.4,"
        "0.12,-0.05,0.01,0.02,0.00,-0.01,0.23,0.01,0.04,15,"
        "13:14:01,455.1,1.2345,103.8234,8,CXON\n"
    )

    # Split CSV line -> list of strings
    fields = sample_line.strip().split(",")

    # Field count check
    print("Has required fields?  ", has_required_field_count(fields))

    # Use INDEX to read specific values by name
    mode  = fields[INDEX["MODE"]]
    state = fields[INDEX["STATE"]]
    alt_s = fields[INDEX["ALTITUDE"]]
    vb_s  = fields[INDEX["VOLTAGE"]]

    print("MODE valid?          ", is_valid_mode(mode), "(", mode, ")")
    print("STATE valid?         ", is_valid_state(state), "(", state, ")")
    print("ALTITUDE string      ", alt_s)
    print("VOLTAGE string       ", vb_s)

    # Convert to numbers only when needed
    alt = float(alt_s)    # "452.3" -> 452.3
    vb  = float(vb_s)     # "7.4"   -> 7.4
    print("ALTITUDE float       ", alt)
    print("VOLTAGE float        ", vb)



