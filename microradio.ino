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

class Player
{
private:
	AudioFileSourceHTTPStream *stream = nullptr;
	AudioFileSourceSPIRAMBuffer *buff = nullptr;
	AudioGeneratorMP3 *mp3 = nullptr;
	AudioOutputI2S *out = nullptr;
	int retryms;

public:
	Player() {
		out = new AudioOutputI2S();
		out->SetGain(0.3);
		retryms = millis();
	}

	void play() {
		audioLogger = &Serial;

		stream = new AudioFileSourceHTTPStream(URL);
		stream->RegisterMetadataCB(MDCallback, (void*)"ID3");

		stream = new AudioFileSourceHTTPStream(URL);
		if (!stream->isOpen()) {
			stream->close();
			delete stream;
			stream = nullptr;
			return;
		}

		buff = new AudioFileSourceSPIRAMBuffer(stream, 4, 128*1024);
		buff->RegisterStatusCB(StatusCallback, (void*)"buffer");

		out = new AudioOutputI2S();

		mp3 = new AudioGeneratorMP3();
		mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
		mp3->begin(buff, out);
	}

	void stop() {
		mp3->stop();
		stream->close();
		delete mp3;
		delete buff;
		delete stream;
		mp3 = nullptr;
	}

	void loop() {
		if (mp3 != nullptr){
			mp3->loop();
		}
	}
} *player;

//This function redirects user to desired url
void redirect(String url) {
	Serial.println("Redirecting to " + url);
	server.sendHeader("Location", url, true);
	server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server.client().stop(); // Stop is needed because we sent no content length
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

	LittleFS.begin();

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(ap_ip, ap_ip, subnet);
	WiFi.softAP(ap_ssid, ap_pass);

	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	server.on("/setup", HTTP_GET, wifisetup);
	server.on("/setup", HTTP_POST, savewifisetup);
	server.onNotFound([]{ redirect("http://192.168.1.1/setup"); });

	Serial.println("Connecting to WiFi");
	loadwifisetup();

	server.begin();

	while (WiFi.status() != WL_CONNECTED) {
		server.handleClient();
	}
	Serial.println("Connected");

	server.stop();
	WiFi.softAPdisconnect();
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}
	Serial.println("Start playing");

	player = new Player();

	server.on("/control", []{ server.send(200, "text/html", "control"); });
	server.on("/play", []{ player->play(); server.send(200, "text/html", "control"); });
	server.on("/stop", []{ player->stop(); server.send(200, "text/html", "control"); });
	server.onNotFound([]{ redirect("http://" + WiFi.localIP().toString() + "/control"); });
	server.begin();
	Serial.println("Serving control panel at " + WiFi.localIP().toString() + "/control");
}

void loop() {
	server.handleClient();

	player->loop();
}
