/*
 * Neo_Chen's GNSS Clock w/RTC backup & fast fallback
 */

// Hardware: Raspberry Pi Pico (RP2040)
// Configs

#define TIMEZONE 800	// UTC+8:00
#define GPS_PPS	11
#define RTC_PPS	10
#define LED	25
#define BTN	24
#define OLED_ADDR	0x3C

#include <Arduino.h>
#include <Wire.h>
#include <DS3232RTC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPSPlus.h>
#include <TimeLib.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64);
DS3232RTC RTC;
TinyGPSPlus gps;


#define GPS_UART	Serial1

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

void rtcpps(void)
{
	flags.RTC_int = true;
	flags.RTC_pps++;
}

void gpspps(void)
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
	Wire.begin();
	Serial.begin(115200);	// Debug
	GPS_UART.setRX(13);
	GPS_UART.setTX(12);
	GPS_UART.begin(115200);

	pinMode(RTC_PPS, INPUT_PULLUP);
	pinMode(GPS_PPS, INPUT_PULLUP);
	pinMode(LED, OUTPUT);
	pinMode(BTN, INPUT_PULLUP);

	RTC.begin();
	setTime(RTC.get());
	bme.begin(BME280_ADDRESS_ALTERNATE);

	if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
	{
		Serial.println("SSD1306 initialization failed!");
		while(1);
	}
	display.clearDisplay();
	display.setTextSize(1);
	display.setCursor(0, 0);
	display.setTextColor(SSD1306_WHITE);
	display.println("BX4ACV's\nHigh Precision\nGNSS Clock\nv0.1");
	display.display();

	RTC.squareWave(DS3232RTC::SQWAVE_1_HZ);

	Serial.println("INIT @" + iso8601time());

	// init done
	delay(3000);

	attachInterrupt(RTC_PPS, rtcpps, RISING);
	attachInterrupt(GPS_PPS, gpspps, RISING);

	display.clearDisplay();
}

bool gps_valid(void)
{
	return (flags.mode == GPS_MODE);
}

String get_fix_status(void)
{
	char status[10];

	if(gps_valid())
	{
		sprintf(status, "S%02lu", gps.satellites.value());
		return String(status);
	}
	else
		return "N/A";
}

String get_altitude(void)
{
	char buf[16];
	float alt = 0;

	if(gps_valid())
	{
		alt = gps.altitude.meters();
	}
	else
	{
		alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
	}

	if(alt >= 10000.0)
		sprintf(buf, "A%05.0fM", alt);
	else
		sprintf(buf, "A=%04.0fM", alt);
	
	return String(buf);
}

String get_speed(void)
{
	char buf[10];

	if(gps_valid())
	{
		float speed=gps.speed.kmph();
		if(speed < 100.0)
			sprintf(buf, "%04.1f", speed);
		else if(speed < 1000.0)
			sprintf(buf, "%03.0f.", speed);
		else // speed > 1000kph
			sprintf(buf, "%04.0f", speed);
	}
	else
		sprintf(buf, "????");
	
	return String(buf);
}

String get_course(void)
{
	char buf[10];

	if(gps_valid())
		sprintf(buf, "%03.0f", gps.course.deg());
	else
		sprintf(buf, "???");
	
	return String(buf);
}

static const char grid_field[] = "ABCDEFGHIJKLMNOPQR";
static const char grid_square[] = "0123456789";
static const char grid_subsquare[] = "abcdefghijklmnopqrstuvwx";

String get_grid(void)
{
	static String grid = "??????";
	int32_t lng = floor((gps.location.lng() + 180.0) * 12);
	int32_t lat = floor((gps.location.lat() + 90) * 24);

	if(gps_valid() && gps.location.isValid())
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

String weekname[] =
{ "", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

void disp(void)
{
	/*
	┌──────────────┐
	│2022-06-15 WED│
	│T22:17:15+0800│
	│T+24.5 A=0170M│
	│P=0985.3 H=63%│
	│>10.0kph C^120│
	│GPS S12 PL04hf│
	└──────────────┘

	Size: 14x6
	*/

	char buf[32];
	float humidity = bme.readHumidity();

	if(humidity > 99.0)
		humidity = 99.0;

	display.clearDisplay();
	display.setCursor(0, 0);
	Serial.println("==============");

	sprintf(buf, "%04u-%02u-%02u %3s", year(), month(), day(), weekname[weekday()].c_str());
	display.println(buf);
	Serial.println(buf);

	sprintf(buf, "T%02u:%02u:%02u%+05d", hour(), minute(), second(), TIMEZONE);
	display.println(buf);
	Serial.println(buf);

	sprintf(buf, "T%+04.1f %7s", bme.readTemperature(), get_altitude().c_str());
	display.println(buf);
	Serial.println(buf);

	sprintf(buf, "P=%06.1f H=%2.0f%%", bme.readPressure() / 100.0, humidity);
	display.println(buf);
	Serial.println(buf);

	sprintf(buf, ">%4skph C^%3s", get_speed().c_str(), get_course().c_str());
	display.println(buf);
	Serial.println(buf);

	sprintf(buf, "%3s %3s %6s",
		flags.mode == GPS_MODE ? "GPS" : "RTC",
		get_fix_status().c_str(),
		get_grid().c_str());
	display.println(buf);
	Serial.println(buf);

	display.display(); 
}

void loop()
{
	if(flags.GPS_int)
	{
		flags.GPS_int = false;
		flags.RTC_pps = 0;
		flags.mode = GPS_MODE;

		setTime(gps.time.hour(), gps.time.minute(), gps.time.second(),
			gps.date.day(), gps.date.month(), gps.date.year());
		adjustTime(SECS_PER_HOUR * (TIMEZONE/100) + (TIMEZONE%100) * 60 + 1); // +1 because PPS comes before NMEA sentences
		disp();
		RTC.set(now()); // set time to RTC later, to reduce display latency
	}

	if(flags.RTC_int)
	{
		flags.RTC_int = false;

		if(flags.mode == RTC_MODE)
		{
			setTime(RTC.get());
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
		Serial.print(c);

		if(gps.encode(c))
		{
			if(! (gps.location.isValid() && gps.altitude.isValid() && gps.satellites.value() != 0))
				flags.mode = RTC_MODE;
		}
	}

}
