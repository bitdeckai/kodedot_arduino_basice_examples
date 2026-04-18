/**
 * Basic MAX31329 RTC time demo: sets initial time and continuously reads/displays current time.
 * Uses fluent API (rtc.t.year = 2024) for easy time configuration on ESP32-S3.
 * Prints formatted time every second to Serial monitor.
 */
/* ───────── KODE | docs.kode.diy ───────── */

#include <kode_MAX31329.h>
#include "Wire.h"

MAX31329 rtc;

/* Helper function to read and display current time from RTC */
static void printTime()
{
	/* Read time from RTC into rtc.t structure */
	if (!rtc.readTime()) {
		Serial.println("readTime failed");
		return;
	}
	
	/* Format and print time as YYYY-MM-DD HH:MM:SS */
	Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", 
		rtc.t.year, rtc.t.month, rtc.t.day, 
		rtc.t.hour, rtc.t.minute, rtc.t.second);
}

void setup()
{
	Serial.begin(115200);
	Serial.println("MAX31329 Time example");
	
    Wire.begin(48,47);

	/* Initialize RTC */
	rtc.begin();

	/* Set initial time using fluent API - November 24, 2024 15:10:00 */
	rtc.t.year = 2024;
	rtc.t.month = 11;
	rtc.t.day = 24;
	rtc.t.hour = 15;
	rtc.t.minute = 10;
	rtc.t.second = 0;
	rtc.t.dayOfWeek = 0; /* 0=Sunday, 1=Monday, ..., 6=Saturday */
	
	/* Write configured time to RTC hardware */
	if (!rtc.writeTime()) {
		Serial.println("writeTime failed");
	}
}

void loop()
{
	delay(1000);    /* Wait 1 second between readings */
	printTime();    /* Display current time */
}
