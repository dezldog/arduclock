///A quick variation on the Adafruit code
///

#include "SoftwareSerial.h"
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GPS.h"

// Set to false to display time in 12 hour format, or true to use 24 hour:
#define TIME_24_HOUR      false

// Offset the hours from UTC (universal time) to your local time by changing
// this value.  The GPS time will be in UTC so lookup the offset for your
// local time from a site like:
//   https://en.wikipedia.org/wiki/List_of_UTC_time_offsets
// This value, -7, will set the time to UTC-7 or Pacific Standard Time during
// daylight savings time.
#define HOUR_OFFSET       -8

// Is it DST?
#define DST 0
const int dstPin = 9;
int dstON = 0;


#define DISPLAY_ADDRESS   0x70

Adafruit_7segment clockDisplay = Adafruit_7segment();

SoftwareSerial gpsSerial(8, 7);  

Adafruit_GPS gps(&gpsSerial);

int displayValue = 0;


void setup() {

  Serial.begin(115200);
  clockDisplay.begin(DISPLAY_ADDRESS);
  gps.begin(9600);
  gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  enableGPSInterrupt();
  pinMode(dstPin, INPUT);
  }

void loop() {

  if (gps.newNMEAreceived()) {
    gps.parse(gps.lastNMEA());
  }
  
  int hours = gps.hour + timeZone() + isDST();

  if (hours < 0) {
    hours = 24+hours;
  }
  if (hours > 23) {
    hours = 24-hours;
  }
int minutes = gps.minute;
int seconds = gps.seconds;
  
displayValue = hours*100 + minutes;

  if (!TIME_24_HOUR) {
    if (hours > 12) {
      displayValue -= 1200;
    }
    else if (hours == 0) {
      displayValue += 1200;
    }
  }

  clockDisplay.print(displayValue, DEC);

  if (TIME_24_HOUR && hours == 0) {
    clockDisplay.writeDigitNum(1, 0);
    if (minutes < 10) {
      clockDisplay.writeDigitNum(2, 0);
    }
  }

  clockDisplay.drawColon(seconds % 2 == 0);

  clockDisplay.writeDisplay();

}

SIGNAL(TIMER0_COMPA_vect) {
  gps.read();
}

void enableGPSInterrupt() {
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

int isDST()
{
  dstON = digitalRead(dstPin);
  if (dstON == HIGH)
    {
      return 1;
    } 
  else
    {
      return 0;
    }
}

int timeZone()
  {
    return HOUR_OFFSET;
  }

