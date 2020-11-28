#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const String configfile = "/wifisetup";
const String ap_ssid = "Microradio";
const String ap_pass = "microradio123";

// Saves ssid and password to /wifisetup file which stores ssid and password
// in seperate lines, yes plaintext but it doesn't matter beacause
// to extract this information somebody is already in the same WiFi network.
void savewifisetup(String ssid, String pass) {
	File file = LittleFS.open(configfile, "w");
	file.print(ssid + "\n" + pass + "\n");
	file.close();

	WiFi.begin(ssid, pass);
}

// Loads wifi credentials from /wifisetup
void loadwifisetup() {
	String ssid, pass;
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
		DBG.println("Warning: Waiting for wifi credentials under 192.168.4.1/setup");
	}

	WiFi.begin(ssid, pass);
}

// Global server class
ESP8266WebServer server(80);

// Serves file with apriopriate mime type, doesn't serve files without extentions
// because it would expose internal files like /wifisetup and /stations
void servefile(String filepath) {
	String mime = "";
	if (filepath.endsWith(".html")) mime = "text/html";
	else if (filepath.endsWith(".css")) mime = "text/css";
	else if (filepath.endsWith(".js")) mime = "text/javascript";
	else if (filepath.endsWith(".ttf")) mime = "font/ttf";

	if (LittleFS.exists(filepath) && mime != "") {
		File file = LittleFS.open(filepath, "r");
		server.streamFile(file, mime);
		file.close();
	} else {
		String err = "Error: " + filepath + " not found";
		Serial.println(err);
		server.send(404, "text/html", err);
	}
}

//This function redirects user to desired url
void redirect(String url) {
	DBG.println("Redirecting to " + url);
	server.sendHeader("Location", url, true);
	server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server.client().stop(); // Stop is needed because we sent no content length
}

