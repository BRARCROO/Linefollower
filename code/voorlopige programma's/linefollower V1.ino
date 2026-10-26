#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <BluetoothSerial.h>
#include <math.h>

// ESP32 Preferences bewaart instellingen blijvend in de interne flash (NVS).
// Wi-Fi: verbind met LineFollower en open http://192.168.4.1
// Bluetooth Classic: stuur bijvoorbeeld "speed=120 kp=1.2 ki=0 kd=0".
// Let op: BluetoothSerial vereist een ESP32 met Bluetooth Classic (niet ESP32-C3/S2).
const char *WIFI_SSID = "LineFollower";
const char *WIFI_PASSWORD = "line1234"; // Minimaal 8 tekens

const uint8_t sensor1Pin = 39;
const uint8_t sensor2Pin = 36;
const uint8_t sensor3Pin = 34;
const uint8_t sensor4Pin = 32;
const uint8_t sensor5Pin = 33;
const uint8_t sensor6Pin = 35;
const uint8_t sensor7Pin = 26;	// optioneel, voor achtsensorenconfiguratie
const uint8_t sensor8Pin = 27;	// optioneel, voor achtsensorenconfiguratie

const uint8_t motorLinksVooruitPin = 22;	//AIN1
const uint8_t motorLinksAchteruitPin = 21;	//AIN2
const uint8_t motorRechtsVooruitPin = 18;	//BIN1
const uint8_t motorRechtsAchteruitPin = 17;	//BIN2
const uint8_t motorPwmAPin = 23;	// PWM/enable voor motorkanaal A
const uint8_t motorPwmBPin = 19;	// PWM/enable voor motorkanaal B

struct FollowerSettings {
	int speed;       // 0-255; gebruik deze waarde in de motorregeling
	float kp;
	float ki;
	float kd;
};

FollowerSettings settings = {120, 1.0f, 0.0f, 0.0f};
Preferences preferences;
WebServer server(80);
BluetoothSerial SerialBT;
String bluetoothLine;

void loadSettings() {
	settings.speed = preferences.getInt("speed", 120);
	settings.kp = preferences.getFloat("kp", 1.0f);
	settings.ki = preferences.getFloat("ki", 0.0f);
	settings.kd = preferences.getFloat("kd", 0.0f);
}

void saveSettings(const FollowerSettings &value) {
	preferences.putInt("speed", value.speed);
	preferences.putFloat("kp", value.kp);
	preferences.putFloat("ki", value.ki);
	preferences.putFloat("kd", value.kd);
}

String settingsText() {
	return "speed=" + String(settings.speed) + " kp=" + String(settings.kp, 3) +
				 " ki=" + String(settings.ki, 3) +
				 " kd=" + String(settings.kd, 3);
}

bool applySetting(FollowerSettings &value, const String &key, const String &text) {
	if (!text.length()) return false;
	char *end = nullptr;
	float number = strtof(text.c_str(), &end);
	if (end == text.c_str() || *end != '\0' || !isfinite(number)) return false;

	if (key == "speed" && number >= 0 && number <= 255) value.speed = (int)number;
	else if (key == "kp" && number >= 0 && number <= 100) value.kp = number;
	else if (key == "ki" && number >= 0 && number <= 100) value.ki = number;
	else if (key == "kd" && number >= 0 && number <= 100) value.kd = number;
	else return false;
	return true;
}

void handleRoot() {
	String page = R"HTML(<!doctype html><html><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Line follower</title><h2>Line follower instellingen</h2>
<form action="/set" method="get">
Snelheid (0-255): <input type="number" name="speed" min="0" max="255" value=")HTML";
	page += String(settings.speed) + R"HTML("><br>
Kp: <input type="number" name="kp" min="0" max="100" step="0.01" value=")HTML";
	page += String(settings.kp, 3) + R"HTML("><br>
Ki: <input type="number" name="ki" min="0" max="100" step="0.01" value=")HTML";
	page += String(settings.ki, 3) + R"HTML("><br>
Kd: <input type="number" name="kd" min="0" max="100" step="0.01" value=")HTML";
	page += String(settings.kd, 3) + R"HTML("><br><button type="submit">Opslaan</button></form></html>)HTML";
	server.send(200, "text/html; charset=utf-8", page);
}

void handleSet() {
	FollowerSettings updated = settings;
	bool changed = false;
	bool valid = true;
	const char *keys[] = {"speed", "kp", "ki", "kd"};
	for (const char *key : keys) {
		if (server.hasArg(key)) {
			changed = true;
			if (!applySetting(updated, key, server.arg(key))) valid = false;
		}
	}
	if (valid && changed) {
		settings = updated;
		saveSettings(settings);
	}
	if (!valid) {
		server.send(400, "text/plain; charset=utf-8",
								"Ongeldige waarde. speed: 0-255, kp/ki/kd: 0-100.");
	} else {
		server.send(200, "text/plain; charset=utf-8", "Opgeslagen: " + settingsText());
	}
}

void handleBluetoothCommand(String command) {
	command.trim();
	if (command == "show" || command == "?") {
		SerialBT.println(settingsText());
		return;
	}

	FollowerSettings updated = settings;
	bool valid = true;
	bool changed = false;
	int start = 0;
	while (start < command.length()) {
		while (start < command.length() && command[start] == ' ') start++;
		if (start >= command.length()) break;
		int end = command.indexOf(' ', start);
		if (end < 0) end = command.length();
		String item = command.substring(start, end);
		int equals = item.indexOf('=');
		if (equals <= 0 || !applySetting(updated, item.substring(0, equals), item.substring(equals + 1))) {
			valid = false;
			break;
		}
		changed = true;
		start = end + 1;
	}

	if (valid && changed) {
		settings = updated;
		saveSettings(settings);
		SerialBT.println("Opgeslagen: " + settingsText());
	} else {
		SerialBT.println("Ongeldig. Gebruik: speed=120 kp=1.2 ki=0 kd=0 of show");
	}
}

void setup() {
	Serial.begin(115200);
	analogReadResolution(12);
	pinMode(sensor1Pin, INPUT);
	pinMode(sensor2Pin, INPUT);
	pinMode(sensor3Pin, INPUT);
	pinMode(sensor4Pin, INPUT);
	pinMode(sensor5Pin, INPUT);
	pinMode(sensor6Pin, INPUT);
	pinMode(motorLinksVooruitPin, OUTPUT);
	pinMode(motorLinksAchteruitPin, OUTPUT);
	pinMode(motorRechtsVooruitPin, OUTPUT);
	pinMode(motorRechtsAchteruitPin, OUTPUT);
	pinMode(motorPwmAPin, OUTPUT);
	pinMode(motorPwmBPin, OUTPUT);

	preferences.begin("linefollow", false);
	loadSettings();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
	server.on("/", HTTP_GET, handleRoot);
	server.on("/set", HTTP_GET, handleSet);
	server.begin();

	if (SerialBT.begin("LineFollower")) {
		SerialBT.println("LineFollower instellingen. Stuur speed=120 kp=1.2 ki=0 kd=0 of show");
	}
	Serial.println("Wi-Fi: LineFollower, open http://192.168.4.1");
	Serial.println(settingsText());
}

void loop() {
	server.handleClient();

	while (SerialBT.available()) {
		char c = (char)SerialBT.read();
		if (c == '\n' || c == '\r') {
			if (bluetoothLine.length()) {
				handleBluetoothCommand(bluetoothLine);
				bluetoothLine = "";
			}
		} else if (bluetoothLine.length() < 120) {
			bluetoothLine += c;
		}
	}
}
