#include <Arduino.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// NTP setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200);

// Wifi setup
#include "alarm.h"
#ifndef STASSID
#define STASSID "SSID"
#define STAPSK	"PASS"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

// Audio setup
const char *URL="http://tytus.dom:8000/";

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *stream;
AudioOutputI2S *out;

// SSD1306 128x32 setup
SSD1306Wire display(0x3c, D3, D5, GEOMETRY_128_32);

void printtime()
{
	display.clear();
	char time[10];
	sprintf(time, "%02d:%02d",
		timeClient.getHours(),
		timeClient.getMinutes()
	);
	display.drawString(64, 16, String(time));
	display.display();
}

void setup()
{
	Serial.begin(115200);

	// Init display
	display.init();
	display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
	display.setFont(ArialMT_Plain_24);

	// Establish WiFi connection
	WiFi.disconnect();
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);

	WiFi.begin(ssid, password);

	Serial.println("Connecting to WiFi");

	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(1000);
	}
	Serial.printf("\rConnected to %s", STASSID);

	// Connect to NTP server
	timeClient.begin();
	timeClient.update();
	printtime();

	stream = new AudioFileSourceHTTPStream(URL);
	out = new AudioOutputI2S();
	mp3 = new AudioGeneratorMP3();
	mp3->begin(stream, out);
}


void loop()
{
	static int lastms = 0;

	if (millis()-lastms >= 60000) {
		printtime();
		lastms = millis();
	}

	if (!mp3->loop()) {
		mp3->stop();
	}
}
