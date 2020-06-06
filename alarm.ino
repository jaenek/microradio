#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2SNoDAC.h>
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

// SSD1306 128x32 setup
SSD1306Wire display(0x3c, D3, D5, GEOMETRY_128_32);

// Player setup
const char *URL="http://tytus.dom:8123/";

class Player
{
private:
	AudioFileSourceHTTPStream *stream = nullptr;
	AudioOutputI2SNoDAC *out = nullptr;
	AudioGeneratorMP3 *mp3 = nullptr;
	int retryms;

	void play() {
		stream = new AudioFileSourceHTTPStream(URL);
		if (!stream->isOpen()) {
			stream->close();
			delete stream;
			stream = nullptr;
			return;
		}
		mp3 = new AudioGeneratorMP3();
		mp3->begin(stream, out);
	}

	void clear() {
		mp3->stop();
		stream->close();
		delete mp3;
		delete stream;
		mp3 = nullptr;
	}

public:
	Player()
	{
		out = new AudioOutputI2SNoDAC();
		out->SetGain(0.3);
		this->play();
		retryms = millis();
	}

	void loop()
	{
	 	if (mp3 != nullptr){
			if (!mp3->loop()) {
				this->clear();
				this->play();
			}
		} else if (millis() - retryms > 1000) {
			this->play();
			retryms = millis();
		}
	}
} *player;

void printtime()
{
	display.clear();
	display.drawString(64, 16, timeClient.getFormattedTime().substring(0,5));
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

	Serial.printf("Connecting to %s\n", STASSID);

	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(1000);
	}
	Serial.printf("\rConnected to %s\n", STASSID);

	// Connect to NTP server
	timeClient.begin();
	timeClient.update();
	printtime();

	player = new Player();
}

void loop()
{
	static int lastms = 0;

	if (millis()-lastms >= 60000) {
		printtime();
		lastms = millis();
	}

	player->loop();
}
