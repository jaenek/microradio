#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// Randomly picked URL
const char *URL="http://stream.rcs.revma.com/ypqt40u0x1zuv";

AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceSPIRAMBuffer *buff;
AudioOutputI2S *out;

// WiFi config
String configfile = "/wifisetup";
String ssid = "";
String pass = "";

// Web interface
const char *ap_ssid = "Microradio";
const char *ap_pass = "microradio123";
IPAddress ap_ip(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
	const char *ptr = reinterpret_cast<const char *>(cbData);
	(void) isUnicode; // Punt this ball for now
	// Note that the type and string may be in PROGMEM, so copy them to RAM for printf
	Serial.printf_P(PSTR("METADATA(%s) '%s' = '%s'\n"), ptr, type, string);
	Serial.flush();
}


// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string) {
	const char *ptr = reinterpret_cast<const char *>(cbData);
	static uint32_t lastTime = 0;
	static int lastCode = -99999;
	uint32_t now = millis();
	if ((lastCode != code) || (now - lastTime > 1000)) {
		Serial.printf_P(PSTR("STATUS(%s) '%d' = '%s'\n"), ptr, code, string);
		Serial.flush();
		lastTime = now;
		lastCode = code;
	}
}

//This function redirects user to desired url
void redirect(ESP8266WebServer *serv, String url) {
	Serial.println("Redirecting to " + url);
	serv->sendHeader("Location", url, true);
	serv->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	serv->client().stop(); // Stop is needed because we sent no content length
}

void wifisetup() {
	// read wifisetup.html form
	String filepath = "/wifisetup.html";
	if (LittleFS.exists(filepath)) {
		File file = LittleFS.open(filepath, "r");
		server.streamFile(file, "text/html");
		file.close();
	} else {
		String err = "Error: " + filepath + " not found, please upload html files to esp.";
		Serial.println(err);
		server.send(404, "text/html", err);
	}
}

void savewifisetup() {
	String ssid = server.arg("ssid");
	String pass = server.arg("pass");

	File file = LittleFS.open(configfile, "w");
	file.print(ssid + "\n" + pass + "\n");
	file.close();

	WiFi.begin(ssid, pass);
}

void loadwifisetup() {
	if (LittleFS.exists(configfile)) {
		File file = LittleFS.open(configfile, "r");

		int line = 0;
		while (file.available()) {
			char c = char(file.read());
			if (c == '\n') {
				line++;
				continue;
			}

			if (line == 0) {
				ssid += c;
			} else if (line == 1) {
				pass += c;
			} else {
				break;
			}
		}

		file.close();
	} else {
		Serial.println("Warning: Waiting for wifi credentials under 192.168.1.1/setup");
	}

	WiFi.begin(ssid, pass);
}

void setup() {
	Serial.begin(115200);
	delay(1000);

	LittleFS.begin();

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(ap_ip, ap_ip, subnet);
	WiFi.softAP(ap_ssid, ap_pass);

	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	server.on("/setup", HTTP_GET, wifisetup);
	server.on("/setup", HTTP_POST, savewifisetup);
	server.onNotFound([]{ redirect(&server, "http://192.168.1.1/setup"); });

	Serial.println("Connecting to WiFi");
	loadwifisetup();

	server.begin();

	// Try forever
	while (WiFi.status() != WL_CONNECTED) {
		server.handleClient();
	}
	Serial.println("Connected");

	server.stop();
	WiFi.softAPdisconnect();
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	audioLogger = &Serial;
	file = new AudioFileSourceHTTPStream(URL);
	file->RegisterMetadataCB(MDCallback, (void*)"ID3");
	// Initialize 23LC1024 SPI RAM buffer with chip select ion GPIO4 and ram size of 128KByte
	buff = new AudioFileSourceSPIRAMBuffer(file, 4, 128*1024);
	buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
	out = new AudioOutputI2S();
	mp3 = new AudioGeneratorMP3();
	mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
	mp3->begin(buff, out);

	server.on("/control", []{ server.send(200, "text/html", "control"); });
	server.onNotFound([]{ redirect(&server, "http://" + WiFi.localIP().toString() + "/control"); });
	server.begin();
}

void loop() {
	server.handleClient();

	static int lastms = 0;
	if (mp3->isRunning()) {
		if (millis()-lastms > 1000) {
			lastms = millis();
			Serial.printf("Running for %d ms...\n", lastms);
			Serial.flush();
		 }
		if (!mp3->loop()) mp3->stop();
	} else {
		if (millis()-lastms > 1000) {
			lastms = millis();
			Serial.printf("MP3 done\n");
			Serial.flush();
		}
	}
}
