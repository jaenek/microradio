#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.

// Enter your WiFi setup here:
#include "alarm.h"
#ifndef STASSID
#define STASSID "SSID"
#define STAPSK	"PASS"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

// Randomly picked URL
const char *URL="http://tytus.dom:8000/";

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *stream;
AudioOutputI2S *out;

void setup()
{
	Serial.begin(115200);
	delay(1000);
	Serial.println("Connecting to WiFi");

	WiFi.disconnect();
	WiFi.softAPdisconnect(true);
	WiFi.mode(WIFI_STA);

	WiFi.begin(ssid, password);

	// Try forever
	while (WiFi.status() != WL_CONNECTED) {
		Serial.println("...Connecting to WiFi");
		delay(1000);
	}
	Serial.println("Connected");

	audioLogger = &Serial;
	stream = new AudioFileSourceHTTPStream(URL);
	stream->RegisterMetadataCB(MDCallback, (void*)"HTTP");
	out = new AudioOutputI2S();
	mp3 = new AudioGeneratorMP3();
	mp3->begin(stream, out);
}


void loop()
{
	static int lastms = 0;

	if (mp3->isRunning()) {
		if (millis()-lastms > 1000) {
			lastms = millis();
			Serial.printf("Running for %d ms...\n", lastms);
			Serial.flush();
		}
		if (!mp3->loop()) {
			mp3->stop();
			Serial.printf("MP3 done\n");
		}
	}
}
