#include <FS.h>
#include <LittleFS.h>
#include "util.h"
#include "web.h"
#include "player.h"
#ifdef BUTTONS
#include "buttons.h"
#endif

// Main setup sets up serial connection and LittleFS filesystem.
// Operation begins in access point mode, the server waits for the user to
// enter credentials. After the user entered the credentials the WiFi config
// changes to access point and station mode, the music player is started
// and the server reconfigured. WiFi access point ssid is changed to
// Microradio - <obtained ip address>
void setup() {
	Serial.begin(115200);

	LittleFS.begin();

	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(ap_ssid, ap_pass);

	Serial.print("AP IP address: ");
	Serial.println(APIP);

	dnsServer.start(DNS_PORT, "*", APIP);

	server.on("/setup", HTTP_GET, []{ servefile("/wifisetup.html"); });
	server.on("/setup", HTTP_POST, []{ savewifisetup(server.arg("ssid"), server.arg("pass")); servefile("/wifiresponse.html"); });
	server.onNotFound([]{ redirect("/setup"); });

	server.begin();

	Serial.println("Connecting to WiFi");
	WiFi.mode(WIFI_AP_STA);
	loadwifisetup();

	while (WiFi.status() != WL_CONNECTED) {
		dnsServer.processNextRequest();
		server.handleClient();
	}
	Serial.println("Connected");

	server.stop();

	player = new Player();

#ifdef BUTTONS
	buttonmanager = new ButtonManager();
	buttonmanager->on(5, []{ player->nextstation(); }, []{ player->volup(); });
	buttonmanager->on(0, []{ player->prevstation(); }, []{ player->voldown(); });
#endif

	server.on("/", []{ redirect("control.html"); });
	server.on("/list", []{ server.send(200, "text/html", player->liststations()); });
	server.on("/add", []{ player->addstation(server.arg("name"), server.arg("url")); redirect("/list"); });
	server.on("/delete", []{ player->deletestation(server.arg("id").toInt()); redirect("/list"); });
	server.on("/volume", []{ server.send(200, "text/html", String(player->getvolume())); });
	server.on("/stop", []{ player->stop(); server.send(200, "text/html", "stopped"); });
	server.on("/volup", []{ player->volup(); redirect("/volume"); });
	server.on("/voldown", []{ player->voldown(); redirect("/volume"); });
	server.on("/station", []{ server.send(200, "text/html", player->getstation()); });
	server.on("/select", []{  player->setstation(server.arg("id").toInt()); redirect("/station"); });
	server.onNotFound([]{ if (LittleFS.exists(server.uri())) servefile(server.uri()); else redirect("control.html"); });
	server.begin();
	Serial.println("Serving control panel at http://" + WiFi.localIP().toString());

	WiFi.softAP(ap_ssid + " - " + WiFi.localIP().toString(), ap_pass);
}

void loop() {
#ifdef BUTTONS
	buttonmanager->loop();
#endif

	dnsServer.processNextRequest();
	server.handleClient();

	player->loop();
}
