#include <vector>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSPIRAMBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

#ifndef NSTATUS
// Called when there's a warning or error (like a buffer underflow or decode hiccup)
static void statuscallback(void *cbData, int code, const char *string) {
	const char *ptr = reinterpret_cast<const char *>(cbData);
	static uint32_t lastTime = 0;
	static int lastCode = -99999;
	uint32_t now = millis();
	if ((lastCode != code) || (now - lastTime > 1000)) {
		DBG.printf_P(PSTR("STATUS(%s) '%d' = '%s'\n"), ptr, code, string);
		DBG.flush();
		lastTime = now;
		lastCode = code;
	}
}
#endif

// Stores basic information about a station
struct Station {
	String name;
	String url;

	Station(String name, String url) {
		this->name = name;
		this->url = url;
	}
};

// Main music player class handles http and icy audio/mpeg streams,
// adding/deleting/loading/saving stations to file, listing stations
class Player {
private:
	AudioFileSourceHTTPStream *stream = nullptr;
	AudioFileSourceSPIRAMBuffer *buff = nullptr;
	AudioGeneratorMP3 *mp3 = nullptr;
	AudioOutputI2S *out = nullptr;
	int retryms;
	int volume;
	std::vector<Station*> stations;
	int currentstationid;

	// Plays the station url
	void play(Station *station) {
		DBG.println("Opening (" + station->name + ", " + station->url + ")");

		if (station == nullptr) {
			Serial.println("Error: Cannot open, station doesn't exist");
			return;
		}

		if (mp3 != nullptr)
			this->stop();

		stream = new AudioFileSourceHTTPStream(station->url.c_str());

		if (!stream->isOpen()) {
			Serial.println("Error: Couldn't open the stream" + station->url);
			stream->close();
			delete stream;
			stream = nullptr;
			return;
		}

		buff = new AudioFileSourceSPIRAMBuffer(stream, 4, 128*1024);
		out = new AudioOutputI2S();
		mp3 = new AudioGeneratorMP3();

#ifndef NSTATUS
		buff->RegisterStatusCB(statuscallback, (void*)"buffer");
		mp3->RegisterStatusCB(statuscallback, (void*)"mp3");
#endif

		this->volupdate();
		mp3->begin(buff, out);
	}

	// Loads stations from /stations file
	void loadstations() {
		DBG.println("Loading stations");

		stations.clear();

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
					stations.push_back(new Station(name, buf));
					DBG.println(String(id) + ". " + name + "; " + buf);
					id++;
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

	// Saves stations to /station file, removes tabs/newlines from input,
	// the stations are saved line by line with this format:
	// <station name>\t<station url>\n.
	void savestations() {
		DBG.println("Saving stations");

		File file = LittleFS.open("/stations", "w");
		for (int id = 0; id < stations.size(); id++) {
			if (stations[id] != nullptr) {
				auto removeall = [](String s, char c){
					int index;
					while ((index = s.indexOf(c)) != -1) {
						s.remove(index);
					}
				};

				removeall(stations[id]->name, '\t');
				removeall(stations[id]->name, '\n');
				removeall(stations[id]->url, '\t');
				removeall(stations[id]->url, '\n');

				file.print(stations[id]->name + "\t" + stations[id]->url + "\n");
				delete stations[id];
				stations[id] = nullptr;
			}
		}
		file.close();
	}

	// Loads previous status(station id and volume) from a /status file
	void loadstatus() {
		DBG.println("Loading status");

		String buf, filepath = "/status";
		char c;
		if (LittleFS.exists(filepath)) {
			File file = LittleFS.open(filepath, "r");
			while (file.available()) {
				char c = file.read();
				if (c == '\n') {
					volume = buf.toInt();
					buf = "";
					continue;
				}
				buf += c;
			}
			currentstationid = buf.toInt();
			file.close();
		} else {
			Serial.println("Warning: no /status file");
			volume = 100;
			currentstationid = 0;
		}
	}

	// Saves status to a /status file
	void savestatus() {
		DBG.println("Saving status");

		File file = LittleFS.open("/status", "w");
		file.print(String(volume) + "\n" + String(currentstationid));
		file.close();
	}


public:
	Player() {
		retryms = millis();
		loadstations();
		loadstatus();
		this->play(stations[currentstationid]);
	}

	// Resumes playback
	void resume() {
		this->play(stations[currentstationid]);
	}

	// Stops playback and cleans up
	// return false when no mp3 playing
	bool stop() {
		if (mp3 == nullptr)
			return false;

		mp3->stop();
		stream->close();
		delete mp3;
		delete buff;
		delete stream;
		delete out;
		mp3 = nullptr;

		return true;
	}

	// Updates volume with a set value
	void volupdate() {
		DBG.println("Volume update");

		if (mp3 != nullptr)
			out->SetGain((float)volume/100);
	}

	// Increases volume
	void volup() {
		DBG.println("Volume increase");

		volume += 10;
		if (volume > 100)
			volume = 100;
		this->volupdate();
		this->savestatus();
	}

	// Decreases volume
	void voldown() {
		DBG.println("Volume decrease");

		volume -= 10;
		if (volume < 0)
			volume = 0;
		this->volupdate();
		this->savestatus();
	}

	// Returns current volume
	int getvolume() {
		return volume;
	}

	// Adds a station to list
	void addstation(String name, String url) {
		if (url.startsWith("https")) {
			url = "http" + url.substring(5);
		}

		DBG.println("Adding station: " + name + "; " + url);

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

	// Deletes a station from list
	void deletestation(int id) {
		DBG.println("Delete station");
		if (id > stations.size()-1) return;
		stations.erase(stations.begin()+id);
		savestations();
		loadstations();
	}

	// Sets a station with an id for playback
	void setstation(int id) {
		if (id > stations.size()-1) return;
		DBG.println("Setting station: " + String(id));
		currentstationid = id;
		this->play(stations[currentstationid]);
		this->savestatus();
	}

	void prevstation() {
		DBG.println("Previous station");
		int id = currentstationid - 1;
		if (id < 0) id = stations.size() - 1;
		this->setstation(id);
	}

	void nextstation() {
		DBG.println("Next station");
		int id = currentstationid + 1;
		if (id > stations.size() - 1) id = 0;
		this->setstation(id);
	}

	// Lists station in html <select> format
	String liststations() {
		String selectelement = "<select name=\"id\">";
		for (int id = 0; id < stations.size(); id++) {
			if (stations[id] != nullptr) {
				selectelement += "<option value=\"" + String(id) + "\""
					+ (currentstationid == id ? " selected>" : ">")
					+ stations[id]->name + "</option>\n";
			}
		}
		selectelement += "</select>";
		return selectelement;
	}

	// Returns current station name if exists
	String getstation() {
		if (stations[currentstationid] != nullptr)
			return stations[currentstationid]->name;
		else
			return "";
	}

	void loop() {
		if (mp3 != nullptr){
			mp3->loop();
		}
	}
} *player;

