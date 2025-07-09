/**
 * GPS Clock with 7-Segment Display
 * and Automatic Timezone Detection based on GPS Location
 */

#include "SoftwareSerial.h"
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GPS.h"

// Configuration Constants
namespace Config {
  // Display settings
  const bool TIME_24_HOUR = false;
  const uint8_t DISPLAY_ADDRESS = 0x70;
  
  // Manual override settings
  const uint8_t DST_PIN = 9;           // Pin for DST switch
  const uint8_t TIMEZONE_PIN_A = 10;   // Pin A for timezone selection
  const uint8_t TIMEZONE_PIN_B = 11;   // Pin B for timezone selection
  const uint8_t TIMEZONE_PIN_C = 12;   // Pin C for timezone selection
  const uint8_t AUTO_MODE_PIN = 13;    // Pin for auto/manual mode selection
  
  // GPS settings
  const uint8_t GPS_RX_PIN = 8;
  const uint8_t GPS_TX_PIN = 7;
  const uint32_t GPS_BAUD = 9600;
  const uint32_t SERIAL_BAUD = 115200;
  
  // Update intervals
  const uint16_t DISPLAY_UPDATE_MS = 100;
  const uint16_t TIMEZONE_CHECK_MS = 1000;
  const uint16_t LOCATION_CHECK_MS = 30000;  // Check location every 30 seconds
  
  // Location tolerance for timezone changes (in degrees)
  const float LOCATION_TOLERANCE = 0.1;
}

// Hardware instances
Adafruit_7segment display = Adafruit_7segment();
SoftwareSerial gpsSerial(Config::GPS_RX_PIN, Config::GPS_TX_PIN);
Adafruit_GPS gps(&gpsSerial);

// Timezone definitions with geographic boundaries
struct Timezone {
  const char* name;
  const char* abbreviation;
  int8_t utcOffset;        // Standard time offset from UTC
  bool observesDST;        // Whether this timezone observes DST
  int8_t dstOffset;        // Additional offset during DST (usually +1)
  float minLat, maxLat;    // Latitude boundaries
  float minLon, maxLon;    // Longitude boundaries
};

// Expanded timezone definitions with geographic boundaries (US focus)
const Timezone TIMEZONES[] = {
  // US Continental Timezones
  {"Pacific Standard", "PST", -8, true, 1, 32.0, 49.0, -125.0, -114.0},     // 000 - Pacific
  {"Mountain Standard", "MST", -7, true, 1, 31.0, 49.0, -114.0, -104.0},    // 001 - Mountain
  {"Central Standard", "CST", -6, true, 1, 25.0, 49.0, -104.0, -87.0},      // 010 - Central
  {"Eastern Standard", "EST", -5, true, 1, 24.0, 49.0, -87.0, -67.0},       // 011 - Eastern
  {"Atlantic Standard", "AST", -4, false, 0, 44.0, 60.0, -70.0, -53.0},     // 100 - Atlantic (Canada)
  {"Hawaii Standard", "HST", -10, false, 0, 18.0, 23.0, -161.0, -154.0},    // 101 - Hawaii
  {"Alaska Standard", "AKST", -9, true, 1, 54.0, 72.0, -180.0, -130.0},     // 110 - Alaska
  {"UTC", "UTC", 0, false, 0, -90.0, 90.0, -180.0, 180.0}                   // 111 - UTC (fallback)
};

const uint8_t NUM_TIMEZONES = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

// State variables
struct ClockState {
  uint32_t lastUpdate = 0;
  uint32_t lastTimezoneCheck = 0;
  uint32_t lastLocationCheck = 0;
  bool colonState = false;
  bool gpsFixValid = false;
  uint8_t hours = 0;
  uint8_t minutes = 0;
  uint8_t seconds = 0;
  uint8_t currentTimezoneIndex = 0;
  bool isDSTActive = false;
  bool dstSwitchState = false;
  uint8_t timezoneSwitchState = 0;
  bool autoModeEnabled = true;
  float lastLat = 0.0;
  float lastLon = 0.0;
  bool locationChanged = false;
} clockState;

/**
 * Initialize all hardware components
 */
void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  Serial.println(F("GPS Clock with Auto Timezone Starting..."));
  
  initializeDisplay();
  initializeGPS();
  initializePins();
  
  Serial.println(F("Initialization complete"));
  listAvailableTimezones();
}

/**
 * Main program loop
 */
void loop() {
  updateGPSData();
  
  if (clockState.autoModeEnabled) {
    updateLocationIfNeeded();
  } else {
    updateTimezoneIfNeeded();
  }
  
  if (millis() - clockState.lastUpdate >= Config::DISPLAY_UPDATE_MS) {
    updateDisplay();
    clockState.lastUpdate = millis();
  }
}

/**
 * Initialize the 7-segment display
 */
void initializeDisplay() {
  display.begin(Config::DISPLAY_ADDRESS);
  display.setBrightness(15);  // Max brightness
  
  // Show startup pattern
  display.print(8888);
  display.drawColon(true);
  display.writeDisplay();
  delay(1000);
  display.clear();
  display.writeDisplay();
}

/**
 * Initialize GPS module
 */
void initializeGPS() {
  gps.begin(Config::GPS_BAUD);
  
  // Configure GPS for optimal clock usage
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);  // RMC + GGA for location
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  
  enableGPSInterrupt();
  
  Serial.println(F("Waiting for GPS fix..."));
}

/**
 * Initialize input pins
 */
void initializePins() {
  pinMode(Config::DST_PIN, INPUT_PULLUP);
  pinMode(Config::TIMEZONE_PIN_A, INPUT_PULLUP);
  pinMode(Config::TIMEZONE_PIN_B, INPUT_PULLUP);
  pinMode(Config::TIMEZONE_PIN_C, INPUT_PULLUP);
  pinMode(Config::AUTO_MODE_PIN, INPUT_PULLUP);
  
  // Check initial auto mode state
  clockState.autoModeEnabled = (digitalRead(Config::AUTO_MODE_PIN) == LOW);
  
  if (!clockState.autoModeEnabled) {
    // Initialize timezone state for manual mode
    updateTimezoneSettings();
  }
  
  Serial.print(F("Mode: "));
  Serial.println(clockState.autoModeEnabled ? F("AUTO") : F("MANUAL"));
  
  if (!clockState.autoModeEnabled) {
    Serial.print(F("Initial timezone: "));
    Serial.println(getCurrentTimezone().name);
  }
}

/**
 * Update GPS data if available
 */
void updateGPSData() {
  if (gps.newNMEAreceived()) {
    if (gps.parse(gps.lastNMEA())) {
      clockState.gpsFixValid = gps.fix;
      
      if (clockState.gpsFixValid) {
        updateTimeFromGPS();
        
        // Check if location has changed significantly
        if (abs(gps.latitudeDegrees - clockState.lastLat) > Config::LOCATION_TOLERANCE ||
            abs(gps.longitudeDegrees - clockState.lastLon) > Config::LOCATION_TOLERANCE) {
          clockState.locationChanged = true;
        }
      }
    }
  }
}

/**
 * Extract and adjust time from GPS data
 */
void updateTimeFromGPS() {
  int totalOffset = calculateTimezoneOffset();
  int adjustedHours = gps.hour + totalOffset;
  
  // Handle day boundary crossings
  adjustedHours = normalizeHours(adjustedHours);
  
  clockState.hours = adjustedHours;
  clockState.minutes = gps.minute;
  clockState.seconds = gps.seconds;
}

/**
 * Normalize hours to 0-23 range, handling day boundaries
 */
int normalizeHours(int hours) {
  while (hours < 0) {
    hours += 24;
  }
  while (hours >= 24) {
    hours -= 24;
  }
  return hours;
}

/**
 * Update the display with current time
 */
void updateDisplay() {
  if (!clockState.gpsFixValid) {
    showNoGPSIndicator();
    return;
  }
  
  int displayHours = clockState.hours;
  int displayValue;
  
  // Convert to 12-hour format if needed
  if (!Config::TIME_24_HOUR) {
    displayHours = convertTo12Hour(displayHours);
  }
  
  displayValue = displayHours * 100 + clockState.minutes;
  
  // Display the time
  display.print(displayValue, DEC);
  
  // Handle leading zeros for 24-hour format
  if (Config::TIME_24_HOUR && clockState.hours < 10) {
    display.writeDigitNum(1, 0);
    if (clockState.minutes < 10) {
      display.writeDigitNum(3, 0);
    }
  }
  
  // Blink colon every second
  bool showColon = (clockState.seconds % 2 == 0);
  display.drawColon(showColon);
  
  display.writeDisplay();
}

/**
 * Convert 24-hour format to 12-hour format
 */
int convertTo12Hour(int hours24) {
  if (hours24 == 0) {
    return 12;  // Midnight becomes 12 AM
  } else if (hours24 > 12) {
    return hours24 - 12;  // PM hours
  } else {
    return hours24;  // AM hours (1-12)
  }
}

/**
 * Show indicator when GPS fix is not available
 */
void showNoGPSIndicator() {
  static uint32_t lastBlink = 0;
  static bool blinkState = false;
  
  if (millis() - lastBlink >= 500) {  // Blink every 500ms
    blinkState = !blinkState;
    lastBlink = millis();
    
    if (blinkState) {
      display.print(0);  // Show "00.00"
      display.drawColon(true);
    } else {
      display.clear();
    }
    display.writeDisplay();
  }
}

/**
 * Automatically detect timezone based on GPS coordinates
 */
uint8_t detectTimezoneFromLocation(float lat, float lon) {
  Serial.print(F("Detecting timezone for: "));
  Serial.print(lat, 6);
  Serial.print(F(", "));
  Serial.println(lon, 6);
  
  // Check each timezone's boundaries
  for (uint8_t i = 0; i < NUM_TIMEZONES - 1; i++) {  // Skip UTC (last entry)
    const Timezone& tz = TIMEZONES[i];
    
    if (lat >= tz.minLat && lat <= tz.maxLat &&
        lon >= tz.minLon && lon <= tz.maxLon) {
      Serial.print(F("Found timezone: "));
      Serial.println(tz.name);
      return i;
    }
  }
  
  Serial.println(F("No specific timezone found, using UTC"));
  return NUM_TIMEZONES - 1;  // Return UTC as fallback
}

/**
 * Update location and timezone if needed
 */
void updateLocationIfNeeded() {
  if (!clockState.gpsFixValid) return;
  
  if (millis() - clockState.lastLocationCheck >= Config::LOCATION_CHECK_MS || 
      clockState.locationChanged) {
    
    if (clockState.locationChanged || clockState.lastLat == 0.0) {
      uint8_t newTimezoneIndex = detectTimezoneFromLocation(
        gps.latitudeDegrees, gps.longitudeDegrees);
      
      if (newTimezoneIndex != clockState.currentTimezoneIndex) {
        clockState.currentTimezoneIndex = newTimezoneIndex;
        
        Serial.print(F("Auto-switched to timezone: "));
        Serial.print(getCurrentTimezone().name);
        Serial.print(F(" ("));
        Serial.print(getCurrentTimezone().abbreviation);
        Serial.println(F(")"));
        
        // Auto-detect DST if timezone observes it
        if (getCurrentTimezone().observesDST) {
          clockState.isDSTActive = calculateDSTAuto();
          Serial.print(F("Auto DST: "));
          Serial.println(clockState.isDSTActive ? F("ON") : F("OFF"));
        } else {
          clockState.isDSTActive = false;
        }
        
        displayTimezoneInfo();
      }
      
      clockState.lastLat = gps.latitudeDegrees;
      clockState.lastLon = gps.longitudeDegrees;
      clockState.locationChanged = false;
    }
    
    clockState.lastLocationCheck = millis();
  }
  
  // Check if mode switch changed
  bool newAutoMode = (digitalRead(Config::AUTO_MODE_PIN) == LOW);
  if (newAutoMode != clockState.autoModeEnabled) {
    clockState.autoModeEnabled = newAutoMode;
    Serial.print(F("Mode switched to: "));
    Serial.println(clockState.autoModeEnabled ? F("AUTO") : F("MANUAL"));
    
    if (!clockState.autoModeEnabled) {
      updateTimezoneSettings();
    }
  }
}

/**
 * Get timezone offset in hours
 */
int getTimezoneOffset() {
  return getCurrentTimezone().utcOffset;
}

/**
 * Get DST offset based on switch state or auto-detection
 */
int getDSTOffset() {
  const Timezone& tz = getCurrentTimezone();
  
  if (!tz.observesDST) {
    return 0;  // This timezone doesn't observe DST
  }
  
  return clockState.isDSTActive ? tz.dstOffset : 0;
}

/**
 * Calculate total timezone offset including DST
 */
int calculateTimezoneOffset() {
  return getTimezoneOffset() + getDSTOffset();
}

/**
 * Get current timezone object
 */
const Timezone& getCurrentTimezone() {
  return TIMEZONES[clockState.currentTimezoneIndex];
}

/**
 * Update timezone settings based on switches (manual mode only)
 */
void updateTimezoneSettings() {
  if (clockState.autoModeEnabled) return;
  
  // Read timezone selector switches (3-bit binary)
  uint8_t newTimezoneState = 0;
  if (digitalRead(Config::TIMEZONE_PIN_A) == LOW) newTimezoneState |= 0x01;
  if (digitalRead(Config::TIMEZONE_PIN_B) == LOW) newTimezoneState |= 0x02;
  if (digitalRead(Config::TIMEZONE_PIN_C) == LOW) newTimezoneState |= 0x04;
  
  // Read DST switch
  bool newDSTState = (digitalRead(Config::DST_PIN) == LOW);
  
  // Check if timezone changed
  if (newTimezoneState != clockState.timezoneSwitchState) {
    clockState.timezoneSwitchState = newTimezoneState;
    clockState.currentTimezoneIndex = newTimezoneState % NUM_TIMEZONES;
    
    Serial.print(F("Manual timezone changed to: "));
    Serial.print(getCurrentTimezone().name);
    Serial.print(F(" ("));
    Serial.print(getCurrentTimezone().abbreviation);
    Serial.println(F(")"));
  }
  
  // Check if DST setting changed
  if (newDSTState != clockState.dstSwitchState) {
    clockState.dstSwitchState = newDSTState;
    clockState.isDSTActive = newDSTState;
    
    Serial.print(F("Manual DST "));
    Serial.println(clockState.isDSTActive ? F("ON") : F("OFF"));
  }
}

/**
 * Check and update timezone settings periodically (manual mode)
 */
void updateTimezoneIfNeeded() {
  if (millis() - clockState.lastTimezoneCheck >= Config::TIMEZONE_CHECK_MS) {
    updateTimezoneSettings();
    clockState.lastTimezoneCheck = millis();
  }
}

/**
 * Get timezone name for display
 */
String getTimezoneDisplayName() {
  const Timezone& tz = getCurrentTimezone();
  String name = tz.abbreviation;
  
  if (tz.observesDST && clockState.isDSTActive) {
    // Convert standard time abbreviation to DST
    if (name == "PST") name = "PDT";
    else if (name == "MST") name = "MDT";
    else if (name == "CST") name = "CDT";
    else if (name == "EST") name = "EDT";
    else if (name == "AKST") name = "AKDT";
    else name += "D";  // Generic DST indicator
  }
  
  return name;
}

/**
 * Enhanced DST calculation based on US rules
 */
bool calculateDSTAuto() {
  if (!clockState.gpsFixValid) return false;
  
  const Timezone& tz = getCurrentTimezone();
  if (!tz.observesDST) return false;
  
  uint8_t month = gps.month;
  uint8_t day = gps.day;
  uint8_t year = gps.year;
  
  // Calculate DST for US rules (2007 onwards)
  // DST begins: Second Sunday in March
  // DST ends: First Sunday in November
  
  if (month < 3 || month > 11) {
    return false;  // January, February, December
  }
  
  if (month > 3 && month < 11) {
    return true;   // April through October
  }
  
  // For March and November, need to calculate the exact Sunday
  uint8_t dstStartDay = calculateSecondSunday(3, year);
  uint8_t dstEndDay = calculateFirstSunday(11, year);
  
  if (month == 3) {
    return day >= dstStartDay;
  } else if (month == 11) {
    return day < dstEndDay;
  }
  
  return false;
}

/**
 * Calculate the second Sunday of a given month
 */
uint8_t calculateSecondSunday(uint8_t month, uint8_t year) {
  // Find the first Sunday, then add 7 days
  uint8_t firstSunday = calculateFirstSunday(month, year);
  return firstSunday + 7;
}

/**
 * Calculate the first Sunday of a given month
 */
uint8_t calculateFirstSunday(uint8_t month, uint8_t year) {
  // Use Zeller's congruence to find day of week for the 1st
  uint8_t fullYear = 2000 + year;
  uint8_t adjustedMonth = month;
  uint8_t adjustedYear = fullYear;
  
  if (month < 3) {
    adjustedMonth += 12;
    adjustedYear--;
  }
  
  uint8_t dayOfWeek = (1 + (13 * (adjustedMonth + 1)) / 5 + adjustedYear + 
                      adjustedYear / 4 - adjustedYear / 100 + adjustedYear / 400) % 7;
  
  // Convert to Sunday = 0 format
  dayOfWeek = (dayOfWeek + 5) % 7;
  
  // Calculate first Sunday
  if (dayOfWeek == 0) {
    return 1;  // 1st is Sunday
  } else {
    return 8 - dayOfWeek;  // Days until next Sunday
  }
}

/**
 * Enable GPS interrupt for data reading
 */
void enableGPSInterrupt() {
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

/**
 * GPS interrupt service routine
 */
SIGNAL(TIMER0_COMPA_vect) {
  gps.read();
}

/**
 * Debug function to print current status
 */
void printStatus() {
  Serial.print(F("GPS Fix: "));
  Serial.print(clockState.gpsFixValid ? F("YES") : F("NO"));
  
  if (clockState.gpsFixValid) {
    Serial.print(F(" | Time: "));
    if (clockState.hours < 10) Serial.print('0');
    Serial.print(clockState.hours);
    Serial.print(':');
    if (clockState.minutes < 10) Serial.print('0');
    Serial.print(clockState.minutes);
    Serial.print(':');
    if (clockState.seconds < 10) Serial.print('0');
    Serial.print(clockState.seconds);
    
    Serial.print(F(" "));
    Serial.print(getTimezoneDisplayName());
    
    Serial.print(F(" | Satellites: "));
    Serial.print((int)gps.satellites);
    
    Serial.print(F(" | Location: "));
    Serial.print(gps.latitudeDegrees, 6);
    Serial.print(F(", "));
    Serial.print(gps.longitudeDegrees, 6);
    
    Serial.print(F(" | Date: "));
    Serial.print(gps.month);
    Serial.print('/');
    Serial.print(gps.day);
    Serial.print('/');
    Serial.print(2000 + gps.year);
  }
  
  Serial.print(F(" | Mode: "));
  Serial.print(clockState.autoModeEnabled ? F("AUTO") : F("MANUAL"));
  
  Serial.print(F(" | TZ: "));
  Serial.print(getCurrentTimezone().name);
  Serial.print(F(" (UTC"));
  int totalOffset = calculateTimezoneOffset();
  if (totalOffset >= 0) Serial.print('+');
  Serial.print(totalOffset);
  Serial.print(F(")"));
  
  Serial.println();
}

/**
 * Display timezone information on startup or timezone change
 */
void displayTimezoneInfo() {
  const Timezone& tz = getCurrentTimezone();
  
  Serial.println(F("=== Timezone Information ==="));
  Serial.print(F("Mode: "));
  Serial.println(clockState.autoModeEnabled ? F("AUTO") : F("MANUAL"));
  Serial.print(F("Name: "));
  Serial.println(tz.name);
  Serial.print(F("Abbreviation: "));
  Serial.println(getTimezoneDisplayName());
  Serial.print(F("UTC Offset: "));
  if (tz.utcOffset >= 0) Serial.print('+');
  Serial.println(tz.utcOffset);
  Serial.print(F("Observes DST: "));
  Serial.println(tz.observesDST ? F("Yes") : F("No"));
  if (tz.observesDST) {
    Serial.print(F("DST Currently: "));
    Serial.println(clockState.isDSTActive ? F("Active") : F("Inactive"));
  }
  Serial.print(F("Total Offset: UTC"));
  int totalOffset = calculateTimezoneOffset();
  if (totalOffset >= 0) Serial.print('+');
  Serial.println(totalOffset);
  
  if (clockState.gpsFixValid) {
    Serial.print(F("Current Location: "));
    Serial.print(gps.latitudeDegrees, 6);
    Serial.print(F(", "));
    Serial.println(gps.longitudeDegrees, 6);
  }
  Serial.println(F("============================"));
}

/**
 * List all available timezones with their geographic boundaries
 */
void listAvailableTimezones() {
  Serial.println(F("Available Timezones with Geographic Boundaries:"));
  Serial.println(F("ID | Name | Abbrev | UTC | Lat Range | Lon Range"));
  Serial.println(F("---|------|--------|-----|-----------|----------"));
  
  for (uint8_t i = 0; i < NUM_TIMEZONES; i++) {
    Serial.print(i);
    Serial.print(F("  | "));
    Serial.print(TIMEZONES[i].name);
    Serial.print(F(" | "));
    Serial.print(TIMEZONES[i].abbreviation);
    Serial.print(F(" | "));
    if (TIMEZONES[i].utcOffset >= 0) Serial.print('+');
    Serial.print(TIMEZONES[i].utcOffset);
    Serial.print(F(" | "));
    Serial.print(TIMEZONES[i].minLat, 1);
    Serial.print(F(" to "));
    Serial.print(TIMEZONES[i].maxLat, 1);
    Serial.print(F(" | "));
    Serial.print(TIMEZONES[i].minLon, 1);
    Serial.print(F(" to "));
    Serial.print(TIMEZONES[i].maxLon, 1);
    if (TIMEZONES[i].observesDST) {
      Serial.print(F(" (DST)"));
    }
    Serial.println();
  }
  Serial.println();
  Serial.println(F("Pin 13: HIGH=Manual Mode, LOW=Auto Mode"));
  Serial.println(F("Auto mode uses GPS location to set timezone"));
  Serial.println(F("Manual mode uses pins 9-12 for timezone/DST selection"));
  Serial.println();
}

/**
 * Convert UTC time to any timezone
 */
void convertUTCToTimezone(uint8_t utcHour, uint8_t utcMinute, uint8_t timezoneIndex, 
                         uint8_t& localHour, uint8_t& localMinute, bool useDST = false) {
  if (timezoneIndex >= NUM_TIMEZONES) return;
  
  const Timezone& tz = TIMEZONES[timezoneIndex];
  int offset = tz.utcOffset;
  
  if (useDST && tz.observesDST) {
    offset += tz.dstOffset;
  }
  
  int totalMinutes = utcHour * 60 + utcMinute + (offset * 60);
  
  // Handle day boundary crossings
  while (totalMinutes < 0) totalMinutes += 24 * 60;
  while (totalMinutes >= 24 * 60) totalMinutes -= 24 * 60;
  
  localHour = totalMinutes / 60;
  localMinute = totalMinutes % 60;
}

/**
 * Show multiple timezone clocks (useful for world clock feature)
 */
void showWorldClock() {
  if (!clockState.gpsFixValid) {
    Serial.println(F("No GPS fix for world clock"));
    return;
  }
  
  Serial.println(F("=== World Clock ==="));
  Serial.print(F("UTC: "));
  if (gps.hour < 10) Serial.print('0');
  Serial.print(gps.hour);
  Serial.print(':');
  if (gps.minute < 10) Serial.print('0');
  Serial.println(gps.minute);
  
  Serial.print(F("Location: "));
  Serial.print(gps.latitudeDegrees, 6);
  Serial.print(F(", "));
  Serial.println(gps.longitudeDegrees, 6);
  Serial.println();
  
  for (uint8_t i = 0; i < NUM_TIMEZONES; i++) {
    uint8_t localHour, localMinute;
    bool useDST = TIMEZONES[i].observesDST && calculateDSTAuto();
    convertUTCToTimezone(gps.hour, gps.minute, i, localHour, localMinute, useDST);
    
    Serial.print(TIMEZONES[i].abbreviation);
    if (useDST) {
      Serial.print(F("(DST)"));
    }
    Serial.print(F(": "));
    if (localHour < 10) Serial.print('0');
    Serial.print(localHour);
    Serial.print(':');
    if (localMinute < 10) Serial.print('0');
    Serial.print(localMinute);
    Serial.print(F(" ("));
    Serial.print(TIMEZONES[i].name);
    Serial.println(F(")"));
  }
  Serial.println(F("=================="));
}