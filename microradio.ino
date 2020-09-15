#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// Player config
const int MAX_STATIONS = 10;

// WiFi config
String configfile = "/wifisetup";
String ssid = "";
String pass = "";

// Web interface
String ap_ssid = "Microradio";
String ap_pass = "microradio123";

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

struct Station {
	String name;
	String url;

	Station(String name, String url) {
		this->name = name;
		this->url = url;
	}
};

class Player
{
private:
	AudioFileSourceHTTPStream *stream = nullptr;
	AudioFileSourceSPIRAMBuffer *buff = nullptr;
	AudioGeneratorMP3 *mp3 = nullptr;
	AudioOutputI2S *out = nullptr;
	int retryms;
	int volume;
	Station *stations[MAX_STATIONS] = {};
	int currentstationid;

	void play(Station *station) {
		if (station == nullptr) {
			Serial.println("Error: Station doesn't exist");
			return;
		}

		if (mp3 != nullptr)
			this->stop();

		audioLogger = &Serial;

		Serial.println("Opening " + station->url);
		stream = new AudioFileSourceHTTPStream(station->url.c_str());
		stream->RegisterMetadataCB(MDCallback, (void*)"ID3");

		if (!stream->isOpen()) {
			Serial.println("Error: Couldn't open the stream");
			stream->close();
			delete stream;
			stream = nullptr;
			return;
		}

		buff = new AudioFileSourceSPIRAMBuffer(stream, 4, 128*1024);
		buff->RegisterStatusCB(StatusCallback, (void*)"buffer");

		out = new AudioOutputI2S();
		volume = 100;
		this->volupdate();

		mp3 = new AudioGeneratorMP3();
		mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
		mp3->begin(buff, out);
	}

	void loadstations() {
		Serial.println("Loading stations");
		String filepath = "/stations";
		if (LittleFS.exists(filepath)) {
			File file = LittleFS.open(filepath, "r");

			int id = 0;
			String buf;
			String name;
			while (file.available()) {
				char c = file.read();
				if (c == '\t') {
					name = buf;
					buf = "";
					continue;
				}

				if (c == '\n') {
					stations[id] = new Station(name, buf);
					Serial.println(String(id) + ". " + name + "; " + buf);
					id++;
					if (id == MAX_STATIONS) {
						Serial.println("Maximum limit of stations reached!");
						break;
					}
					buf = "";
					continue;
				}

				buf += c;
			}

			file.close();
		} else {
			Serial.println("Error: no /stations file.");
		}
	}

	void savestations() {
		Serial.println("Saving stations");
		File file = LittleFS.open("/stations", "w");
		for (int id = 0; id < MAX_STATIONS; id++) {
			if (stations[id] != nullptr) {
				file.print(stations[id]->name + "\t" + stations[id]->url + "\n");
				delete stations[id];
				stations[id] = nullptr;
			}
		}
		file.close();
	}

public:
	Player() {
		retryms = millis();
		loadstations();
		currentstationid = 0;
		this->play(stations[currentstationid]);
	}

	void stop() {
		mp3->stop();
		stream->close();
		delete mp3;
		delete buff;
		delete stream;
		delete out;
		mp3 = nullptr;
	}

	void volupdate() {
		out->SetGain((float)volume/100);
	}

	void volup() {
		volume += 10;
		if (volume > 100)
			volume = 100;
		this->volupdate();
	}

	void voldown() {
		volume -= 10;
		if (volume < 0)
			volume = 0;
		this->volupdate();
	}

	void addstation(String name, String url) {
		Serial.println("Adding station: " + name + "; " + url);
		String filepath = "/stations";
		File file;
		if (LittleFS.exists(filepath)) {
			file = LittleFS.open(filepath, "a");
		} else {
			file = LittleFS.open(filepath, "w");
		}
		file.print(name + "\t" + url + "\n");
		file.close();
		loadstations();
	}

	void deletestation(int id) {
		if (stations[id] != nullptr) {
			delete stations[id];
			stations[id] = nullptr;
			savestations();
			loadstations();
		}
	}

	void setstation(int id) {
		Serial.println("Setting station: " + String(id));
		currentstationid = id;
		this->play(stations[currentstationid]);
	}

	void liststations() {
		String selectelement = "<select name=\"id\">";
		for (int id = 0; id < MAX_STATIONS; id++) {
			if (stations[id] != nullptr) {
				selectelement += "<option value=\"" + String(id) + "\">" + stations[id]->name + "</option>\n";
			}
		}
		selectelement += "</select>";
		server.send(200, "text/html", selectelement);
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

void servefile(String filepath) {
	if (LittleFS.exists(filepath)) {
		File file = LittleFS.open(filepath, "r");
		String mime = "";
		if (filepath.endsWith(".css")) mime = "text/css";
		else if (filepath.endsWith(".js")) mime = "text/javascript";
		else mime = "text/html";
		server.streamFile(file, mime);
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
		Serial.println("Warning: Waiting for wifi credentials under 192.168.4.1/setup");
	}

	WiFi.begin(ssid, pass);
}

void setup() {
	Serial.begin(115200);

	LittleFS.begin();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(ap_ssid, ap_pass);

	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	server.on("/setup", HTTP_GET, []{ servefile("/wifisetup.html"); });
	server.on("/setup", HTTP_POST, []{ savewifisetup(); server.send(200, "text/html", "<meta charset=\"utf-8\"><h1>Łączenie z siecią... ⏳</h1>\n<p>Jeśli nie pojawi się sieć Microradio - &#60;adres ip&#62; to spróbuj ponownie.</p>"); });
	server.onNotFound([]{ redirect("/setup"); });

	server.begin();

	Serial.println("Connecting to WiFi");
	WiFi.mode(WIFI_AP_STA);
	loadwifisetup();

	while (WiFi.status() != WL_CONNECTED) {
		server.handleClient();
	}
	Serial.println("Connected");

	server.stop();

	player = new Player();

	server.on("/", []{ redirect("control.html"); });
	server.on("/list", []{ player->liststations(); });
	server.on("/select", []{  redirect("control.html"); player->setstation(server.arg("id").toInt()); });
	server.on("/delete", []{ player->deletestation(server.arg("id").toInt()); redirect("control.html"); });
	server.on("/add", []{ player->addstation(server.arg("name"), server.arg("url")); redirect("control.html"); });
	server.on("/stop", []{ redirect("control.html"); player->stop(); });
	server.on("/volup", []{ redirect("control.html"); player->volup(); });
	server.on("/voldown", []{ redirect("control.html"); player->voldown(); });
	server.onNotFound([]{ if (LittleFS.exists(server.uri())) servefile(server.uri()); else redirect("control.html"); });
	server.begin();
	Serial.println("Serving control panel at http://" + WiFi.localIP().toString());

	WiFi.softAP(ap_ssid + " - " + WiFi.localIP().toString(), ap_pass);
}

void loop() {
	server.handleClient();

	player->loop();
}
