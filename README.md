# Instructions for `arduclock`

## Project Overview
This repository contains Arduino-based GPS clock projects with 7-segment displays. There are three main variants:
- `gps_clock_auto_timezone/`: GPS clock with automatic timezone detection based on GPS location.
- `gps_clock_tz_dst/`: GPS clock with manual DST and timezone selection via hardware pins.
- `original/`: Early version with manual UTC offset and DST toggle.

## Architecture & Key Files
- Each variant is a standalone `.ino` file, using similar hardware and libraries:
  - `Adafruit_LEDBackpack`, `Adafruit_GFX`, `Adafruit_GPS`, `SoftwareSerial`, `Wire`
- Hardware pins for timezone/DST selection are defined in each file (see `Config` namespace or `#define` macros).
- Display updates and GPS polling are managed via periodic intervals (see `DISPLAY_UPDATE_MS`, `TIMEZONE_CHECK_MS`, etc.).
- Timezone logic:
  - `gps_clock_auto_timezone`: Uses GPS coordinates to set timezone automatically.
  - `gps_clock_tz_dst`: Uses hardware pins for manual selection.
  - `original`: Uses fixed UTC offset and DST toggle.

## Developer Workflows
- **Build/Upload:**
  - Open the desired `.ino` file in the Arduino IDE or VS Code with Arduino extension.
  - Select the correct board and port, then upload as usual.
- **Debugging:**
  - Use `Serial.begin(115200)` for serial output (default baud rate).
  - Monitor serial output for GPS data and time updates.
- **Dependencies:**
  - Install required libraries via Arduino Library Manager:
    - Adafruit_LEDBackpack
    - Adafruit_GFX
    - Adafruit_GPS
    - SoftwareSerial
    - Wire (built-in)

## Project-Specific Patterns & Conventions
- **Pin Assignments:**
  - Timezone/DST selection pins are consistently assigned (see `Config` or macros).
  - GPS RX/TX pins: 8/7 (all variants).
- **Display Address:**
  - I2C address for display is `0x70`.
- **Time Format:**
  - 12-hour format by default (`TIME_24_HOUR = false`).
- **Serial Baud Rates:**
  - GPS: 9600
  - Serial Monitor: 115200
- **Update Intervals:**
  - Display: 100ms
  - Timezone check: 1s
  - Location check (auto mode): 30s

## Integration Points
- **External Services:**
  - No network/cloud integration; all logic is local to the Arduino.
- **PCB Files:**
  - Hardware design files in `original/` (`main_board.brd`, `.sch`).
  - Order PCBs via OSH Park (see README).

## Example: Adding a New Variant
- Copy an existing `.ino` file and update pin assignments, display logic, or timezone handling as needed.
- Follow the established pattern for hardware initialization and periodic updates.

## References
- Key files: `gps_clock_auto_timezone/gps_clock_auto_tz.ino`, `gps_clock_tz_dst/gps_clock_tz_dst.ino`, `original/clock_dst_tz.ino`
- Hardware: See `original/main_board.brd` and `.sch`
- README: Contains PCB ordering info and project links

---
**If any section is unclear or missing details, please provide feedback so this guide can be improved.**


View this project on [CADLAB.io](https://cadlab.io/node/898). 

# arduclock
PWBs available @ <a href="https://oshpark.com/shared_projects/jVSOJ3Xw"><img src="https://oshpark.com/assets/badge-5b7ec47045b78aef6eb9d83b3bac6b1920de805e9a0c227658eac6e19a045b9c.png" alt="Order from OSH Park"></img></a>
