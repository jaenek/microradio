#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// Player config
const int MAX_STATIONS = 10;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
static void metadatacallback(void *cbData, const char *type, bool isUnicode, const char *string) {
	const char *ptr = reinterpret_cast<const char *>(cbData);
	(void) isUnicode; // Punt this ball for now
	// Note that the type and string may be in PROGMEM, so copy them to RAM for printf
	Serial.printf_P(PSTR("METADATA(%s) '%s' = '%s'\n"), ptr, type, string);
	Serial.flush();
}


// Called when there's a warning or error (like a buffer underflow or decode hiccup)
static void statuscallback(void *cbData, int code, const char *string) {
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
		stream->RegisterMetadataCB(metadatacallback, (void*)"ID3");

		if (!stream->isOpen()) {
			Serial.println("Error: Couldn't open the stream");
			stream->close();
			delete stream;
			stream = nullptr;
			return;
		}

		buff = new AudioFileSourceSPIRAMBuffer(stream, 4, 128*1024);
		buff->RegisterStatusCB(statuscallback, (void*)"buffer");

		out = new AudioOutputI2S();
		volume = 100;
		this->volupdate();

		mp3 = new AudioGeneratorMP3();
		mp3->RegisterStatusCB(statuscallback, (void*)"mp3");
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

	String liststations() {
		String selectelement = "<select name=\"id\">";
		for (int id = 0; id < MAX_STATIONS; id++) {
			if (stations[id] != nullptr) {
				selectelement += "<option value=\"" + String(id) + "\">" + stations[id]->name + "</option>\n";
			}
		}
		selectelement += "</select>";
		return selectelement;
	}

	void loop() {
		if (mp3 != nullptr){
			mp3->loop();
		}
	}
} *player;

