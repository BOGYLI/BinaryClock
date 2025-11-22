#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>

// ----------------------------------------------------
//        KONSTANTEN & VARIABLEN
// ----------------------------------------------------

const char* apSSID     = "BinaryClock";
const char* apPassword = "BC123456";

const char* ntpServer = "pool.ntp.org";
const long gmtOffsetSec = 3600;
const int daylightOffsetSec = 0;

ESP8266WebServer server(80);

// Dateien im Flash
const char* FILE_SSID = "/wifi_ssid.txt";
const char* FILE_PWD  = "/wifi_pwd.txt";

// Zeitvariablen
unsigned long Sekunden = 0;
int Minuten = 0;
int Stunden = 0;

// LED-Pins
int PIN0 = 16;
int PIN1 = 5;
int PIN2 = 4;
int PIN3 = 0;
int PIN4 = 2;
int PIN5 = 14;
int PIN6 = 12;
int PIN7 = 13;
int PIN8 = 15;
int PIN9 = 3;
int PIN10 = 1;

// ----------------------------------------------------
//        LITTLEFS – Hilfsfunktionen
// ----------------------------------------------------

bool saveToFile(const char* path, const String& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data);
  f.close();
  return true;
}

String readFromFile(const char* path) {
  if (!LittleFS.exists(path)) return "";
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

// ----------------------------------------------------
//        HTML FORMULAR
// ----------------------------------------------------

String configPage(String msg = "") {
  String ssid = readFromFile(FILE_SSID);
  String pwd  = readFromFile(FILE_PWD);

  String html = "<!DOCTYPE html><html><head>"
                "<meta charset='UTF-8'>"
                "<title>WLAN konfigurieren</title>"
                "<style>"
                "body{font-family:Arial;background:#eee;padding:40px;}"
                ".c{background:#fff;padding:20px;border-radius:8px;"
                "max-width:420px;margin:auto;box-shadow:0 2px 8px #0003;}"
                "input{width:100%;padding:8px;margin-top:6px;}"
                "button{margin-top:12px;padding:10px;width:100%;background:#0066cc;color:white;border:none;border-radius:4px;}"
                "</style></head><body><div class='c'>"
                "<h2>WLAN Zugangsdaten</h2>"
                "<form method='POST' action='/save'>"
                "SSID:<input name='ssid' value='" + ssid + "' required>"
                "Passwort:<input name='pwd' type='password' value='" + pwd + "' required>"
                "<button>Speichern und verbinden</button>"
                "</form>"
                "<p style='color:green;'>" + msg + "</p>"
                "</div></body></html>";

  return html;
}

// ----------------------------------------------------
//        SERVER HANDLER
// ----------------------------------------------------

void handleRoot() {
  server.send(200, "text/html", configPage());
}

void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("pwd")) {
    server.send(400, "text/plain", "Fehler: Parameter fehlen");
    return;
  }

  String ssid = server.arg("ssid");
  String pwd  = server.arg("pwd");

  if (ssid.length() == 0 || pwd.length() < 8) {
    server.send(200, "text/html", configPage("Passwort mindestens 8 Zeichen"));
    return;
  }

  saveToFile(FILE_SSID, ssid);
  saveToFile(FILE_PWD,  pwd);

  server.sendHeader("Refresh", "3; url=/status");
  server.send(200, "text/html", configPage("Daten gespeichert – verbinde..."));

  delay(300);
  yield();
}

void handleStatus() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                "<style>body{font-family:Arial;background:#eee;padding:40px;}"
                ".c{background:#fff;padding:20px;border-radius:8px;"
                "max-width:420px;margin:auto;box-shadow:0 2px 8px #0003;}"
                "</style></head><body><div class='c'><h2>Status</h2>";

  if (WiFi.status() == WL_CONNECTED) {
    html += "<p style='color:green'>✔ Verbunden mit <b>" + WiFi.SSID() + "</b></p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Zeit: " + String(Stunden) + ":" + String(Minuten) + "</p>";
  } else {
    html += "<p style='color:red'>Nicht verbunden</p>";
  }

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// ----------------------------------------------------
//        WIFI & NTP
// ----------------------------------------------------

void syncTime() {
  configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

  struct tm t;
  if (!getLocalTime(&t)) {
    Serial.println("NTP Fehler");
    return;
  }

  Sekunden = t.tm_hour * 3600UL + t.tm_min * 60UL + t.tm_sec;
}

// ----------------------------------------------------

void connectToWifi() {
  String ssid = readFromFile(FILE_SSID);
  String pwd  = readFromFile(FILE_PWD);

  if (ssid == "" || pwd == "") return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pwd.c_str());

  Serial.print("Verbinde mit ");
  Serial.println(ssid);

  unsigned long timeout = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Verbunden!");
    syncTime();
  } else {
    Serial.println("Fehler – AP wird gestartet");
    WiFi.mode(WIFI_AP);
  }
}

// ----------------------------------------------------
//        SETUP
// ----------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("\nStart");

  LittleFS.begin();

  connectToWifi();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Starte AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/status", handleStatus);
  server.begin();

  int pins[] = {PIN0,PIN1,PIN2,PIN3,PIN4,PIN5,PIN6,PIN7,PIN8,PIN9,PIN10};
  for (int p : pins) pinMode(p, OUTPUT);
}

// ----------------------------------------------------
//        HAUPTSCHLEIFE – LED-BINÄRUHR
// ----------------------------------------------------

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    Sekunden++;
    if (Sekunden >= 24 * 3600) Sekunden = 0;
  }

  Minuten = (Sekunden % 3600) / 60;
  Stunden = Sekunden / 3600;

  // LED-Logik identisch wie in deinem Original
  for (int i=0; i<50; i++) {

    digitalWrite(PIN0, Minuten & 1);
    digitalWrite(PIN1, Minuten & 2);
    digitalWrite(PIN2, Minuten & 4);
    delay(5);
    digitalWrite(PIN0, LOW); digitalWrite(PIN1, LOW); digitalWrite(PIN2, LOW);

    digitalWrite(PIN3, Minuten & 8);
    digitalWrite(PIN4, Minuten & 16);
    digitalWrite(PIN5, Minuten & 32);
    delay(5);
    digitalWrite(PIN3, LOW); digitalWrite(PIN4, LOW); digitalWrite(PIN5, LOW);

    digitalWrite(PIN6, Stunden & 1);
    digitalWrite(PIN7, Stunden & 2);
    digitalWrite(PIN8, Stunden & 4);
    delay(5);
    digitalWrite(PIN6, LOW); digitalWrite(PIN7, LOW); digitalWrite(PIN8, LOW);

    digitalWrite(PIN9, Stunden & 8);
    digitalWrite(PIN10, Stunden & 16);
    delay(5);
    digitalWrite(PIN9, LOW); digitalWrite(PIN10, LOW);
  }
}
