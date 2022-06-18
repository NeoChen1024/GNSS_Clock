/*
 * Neo_Chen's GNSS Clock w/RTC backup & fast fallback
 */

// Hardware: ESP32, NodeMCU-32S

#include <Arduino.h>
#include <Wire.h>
#include <DS3232RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <TimeLib.h>
#include "soc/rtc_wdt.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DS3232RTC RTC;
TinyGPSPlus gps;

// Definitions

#define TIMEZONE 800	// UTC+8:00
#define GPS_PPS	39
#define RTC_PPS	36
#define LED	2

#define GPS_UART	Serial2

enum modes
{
	GPS_MODE,
	RTC_MODE
};

// Global flags

struct flags
{
	volatile bool RTC_int;
	volatile bool GPS_int;
	volatile int RTC_pps;
	volatile enum modes mode;
} flags = { false, false, 0, RTC_MODE };



// ISRs

void IRAM_ATTR rtcpps(void)
{
	flags.RTC_int = true;
	flags.RTC_pps++;
}

void IRAM_ATTR gpspps(void)
{
	flags.GPS_int = true;
}

// ISO-8601 time/date format
String iso8601time(void)
{
	char buf[21];
	time_t t = now();
	sprintf(buf, "%04u%02u%02uT%02u%02u%02u%+05d", 
		year(t), month(t), day(t), hour(t), minute(t), second(t), TIMEZONE);
	return String(buf);
}

// Init

void setup() {
	Serial.begin(115200);	// Debug
	Serial2.begin(115200); // GPS

	rtc_wdt_set_time(RTC_WDT_STAGE0, 500); // set watchdog timeout to 500ms

	pinMode(RTC_PPS, INPUT);
	pinMode(GPS_PPS, INPUT);
	pinMode(LED, OUTPUT);
	RTC.begin();
	setSyncProvider(RTC.get);
	setTime(RTC.get());
	Serial.println("INIT @" + iso8601time());

	if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
		Serial.println(F("SSD1306 allocation failed"));
		for(;;);
	}

	// init done

	display.clearDisplay();
	delay(1000);
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);
	display.println("Neo_Chen\n(BX4ACV)\nGNSS Clock v0.01");
	display.display();
	display.setTextSize(1);
	RTC.squareWave(DS3232RTC::SQWAVE_1_HZ);

	delay(3000);
	attachInterrupt(RTC_PPS, rtcpps, RISING);
	attachInterrupt(GPS_PPS, gpspps, RISING);
}

float gettemp(void)
{
	return RTC.temperature() / 4.0;
}

String get_fix_status(void)
{
	String status;

	if(gps.location.isValid())
	{
		if(gps.altitude.isValid())
			status = "3DFIX";
		else
			status = "2DFIX";
	}
	else
		status = "NOFIX";

	return status;
}

String get_accuracy(void)
{
	char buf[10];
	String accuracy;
	float hdop = gps.hdop.hdop();

	if(flags.mode == RTC_MODE)
		return "E=N/A";
	
	if(hdop >= 10.0)
	{
		sprintf(buf, "E%02.1f", hdop);
	}
	else
	{
		sprintf(buf, "E=%01.1f", hdop);
	}
	return String(buf);
}

String get_altitude(void)
{
	char buf[10];

	if(gps.altitude.isValid())
		sprintf(buf, "%04.0f", gps.altitude.meters());
	else
		sprintf(buf, "????");
	
	return String(buf);
}

PROGMEM char grid_field[] = "ABCDEFGHIJKLMNOPQR";
PROGMEM char grid_square[] = "0123456789";
PROGMEM char grid_subsquare[] = "abcdefghijklmnopqrstuvwx";

String get_grid(void)
{
	static String grid = "??????";
	int32_t lng = floor((gps.location.lng() + 180.0) * 12);
	int32_t lat = floor((gps.location.lat() + 90) * 24);

	if(gps.location.isValid())
	{
		grid[5] = grid_subsquare[lat % 24];
		grid[4] = grid_subsquare[lng % 24];
		lat /= 24;
		lng /= 24;

		grid[3] = grid_square[lat % 10];
		grid[2] = grid_square[lng % 10];
		lat /= 10;
		lng /= 10;

		grid[1] = grid_field[lat];
		grid[0] = grid_field[lng];
	}
	else
	{
		grid[5] = '?';
		grid[4] = '?';
	}

	return grid;
}

void disp(void)
{
	/*
	20201209T221715+0800
	T+24.0C H=72% P=1014
	PL04hf A=0170M [___]
	GNSS 3DFIX E=0.6 S24

	new size: 21x8
	*/
	char buf[32];
	//Serial.println("ping");
	display.clearDisplay();
	display.setCursor(0, 0);
	display.println(iso8601time());
	sprintf(buf, "T%+04.1fC H=??%% P=????", gettemp());
	display.println(buf);
	sprintf(buf, "%7sA=%4sM [___]",
		get_grid().c_str(),
		get_altitude().c_str());
	display.println(buf);
	sprintf(buf, "%4s %5s %5s S%02u",
		flags.mode == GPS_MODE ? "GNSS" : "TCXO",
		get_fix_status().c_str(),
		get_accuracy().c_str(),
		gps.satellites.value());
	display.println(buf);
	display.display(); 
}

void loop() {
	if(flags.RTC_int)
	{
		flags.RTC_int = false;
		if(flags.mode == RTC_MODE)
		{
			disp();
		}
		//Serial.print("TCXO: ");
		//Serial.println(String(flags.RTC_pps) + ", millis=" + String(millis()));
	}

	if(flags.GPS_int)
	{
		flags.GPS_int = false;
		flags.RTC_pps = 0;
		if(gps.location.isValid())
		{
			flags.mode = GPS_MODE;

			setTime(gps.time.hour(), gps.time.minute(), gps.time.second(),
				gps.date.day(), gps.date.month(), gps.date.year());
			adjustTime(SECS_PER_HOUR * (TIMEZONE/100) + (TIMEZONE%100) * 60 + 1); // +1 because pps comes before NMEA sentences
			RTC.set(now());
			disp();
		}
	}

	if(flags.RTC_pps >= 2)
	{
		// GPS PPS timeout
		flags.mode = RTC_MODE;
	}

	if(GPS_UART.available() > 0)
	{
		char c = GPS_UART.read();
		if(gps.encode(c))
		{
			//sprintf(buf, "SATS: %d, %s", gps.satellites.value(), gps.location.isValid() ? "VALID" : "INVALID");
			if(! (gps.location.isValid() && gps.altitude.isValid() && gps.satellites.value() != 0))
				flags.mode = RTC_MODE;
		}
	}
}
