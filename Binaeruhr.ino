/*
  Multiplexing + Ticker (ESP8266)
  - 1s-Timer für Zeit (Sekunden++)
  - 5ms-Timer zum Durchschalten der 3er-Gruppen (Multiplexing)
  - LittleFS + Webserver zum Speichern von SSID/PWD
  - configTzTime() für automatische Sommer-/Winterzeit
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <Ticker.h>

// -----------------------------
// Konfiguration
// -----------------------------
const char* apSSID     = "BinaryClock";
const char* apPassword = "BC123456";

const char* ntpServer = "pool.ntp.org";
const char* TZ_DE = "CET-1CEST,M3.5.0,M10.5.0/3";

ESP8266WebServer server(80);

const char* FILE_SSID = "/wifi_ssid.txt";
const char* FILE_PWD  = "/wifi_pwd.txt";

// -----------------------------
// Pins anpassen, damit die tinkercad arduino Pins zum ESP Board passen
// -----------------------------
const uint8_t PIN0 = 16;
const uint8_t PIN1 = 5;
const uint8_t PIN2 = 4;
const uint8_t PIN3 = 0;
const uint8_t PIN4 = 2;
const uint8_t PIN5 = 14;
const uint8_t PIN6 = 12;
const uint8_t PIN7 = 13;
const uint8_t PIN8 = 15;
const uint8_t PIN9 = 3;
const uint8_t PIN10 = 1;

// Gruppenzuordnung (3er-Gruppen). -1 bedeutet kein Pin in dieser Position.
const int8_t groups[4][3] = {
  { PIN0,  PIN1,  PIN2 },   // Gruppe 0 -> Minuten: 1,2,4
  { PIN3,  PIN4,  PIN5 },   // Gruppe 1 -> Minuten: 8,16,32
  { PIN6,  PIN7,  PIN8 },   // Gruppe 2 -> Stunden: 1,2,4
  { PIN9,  PIN10, -1   }    // Gruppe 3 -> Stunden: 8,16 (nur 2 LEDs)
};

// -----------------------------
// Zeit & Multiplexing (volatile für ISR-Zugriff)
// -----------------------------
volatile unsigned long Sekunden = 0;    // globaler Zeitzähler (Sekunden seit Mitternacht)
int Minuten = 0;
int Stunden = 0;

volatile uint8_t aktuelleGruppe = 0;   // 0..3, wird von 5ms-Ticker erhöht
volatile bool connectRequest = false;  // wenn gesetzt: connectToWifi() im loop() ausführen
volatile bool syncRequest = false;     // NTP-Sync anfordern (set by tick)

// Ticker-Objekte
Ticker sekundenTicker;
Ticker multiplexTicker;

// -----------------------------
// LittleFS Hilfsfunktionen
// -----------------------------
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

// -----------------------------
// Web-UI
// -----------------------------
String configPage(String msg = "") {
  String ssid = readFromFile(FILE_SSID);
  String pwd  = readFromFile(FILE_PWD);

  String html =
    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WLAN konfigurieren</title>"
    "<style>body{font-family:Arial;background:#eee;padding:40px}.c{background:#fff;padding:20px;border-radius:8px;max-width:420px;margin:auto;box-shadow:0 2px 8px #0003}input{width:100%;padding:8px;margin-top:6px}button{margin-top:12px;padding:10px;width:100%;background:#0066cc;color:white;border:none;border-radius:4px}</style>"
    "</head><body><div class='c'><h2>WLAN Zugangsdaten</h2>"
    "<form method='POST' action='/save'>SSID:<input name='ssid' value='" + ssid + "' required>"
    "Passwort:<input name='pwd' type='password' value='" + pwd + "' required>"
    "<button>Speichern und verbinden</button></form>"
    "<p style='color:green;'>" + msg + "</p></div></body></html>";
  return html;
}

void handleRoot() { server.send(200, "text/html", configPage()); }

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

  if (!saveToFile(FILE_SSID, ssid) || !saveToFile(FILE_PWD, pwd)) {
    server.send(200, "text/html", configPage("Fehler beim Schreiben in den Flash-Speicher."));
    return;
  }

  // Signalisiere dem Hauptloop, dass eine Verbindung versucht werden soll
  connectRequest = true;

  server.sendHeader("Refresh", "3; url=/status");
  server.send(200, "text/html", configPage("Daten gespeichert – verbinde..."));
}

void handleStatus() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Status</title>"
                "<style>body{font-family:Arial;background:#eee;padding:40px}.c{background:#fff;padding:20px;border-radius:8px;max-width:420px;margin:auto;box-shadow:0 2px 8px #0003}</style></head><body><div class='c'><h2>Status</h2>";
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

// -----------------------------
// Ticker ISR-Funktionen (müssen sehr kurz sein)
// -----------------------------
void sekundenISR() {
  Sekunden++;
  // einmal täglich um 03:00 Uhr einen Sync anstoßen (nur Flag)
  if ((Sekunden % 86400UL) == (3UL * 3600UL)) {
    syncRequest = true;
  }
}

void multiplexISR() {
  // nur Gruppenindex inkrementieren — keine PIN-Aktionen in ISR!
  aktuelleGruppe++;
  if (aktuelleGruppe >= 4) aktuelleGruppe = 0;
}

// -----------------------------
// Hilfsfunktionen für LED-Ausgabe
// -----------------------------

// Setze alle verwendeten Pins auf LOW
void clearAllGroupPins() {
  for (uint8_t g = 0; g < 4; g++) {
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[g][i];
      if (p >= 0) digitalWrite((uint8_t)p, LOW);
    }
  }
}

// Schalte die LEDs der aktuell aktiven Gruppe entsprechend der Zeitbits
// group: 0..3, minute und hour Werte werden übergeben
void setGroupOutputs(uint8_t group, int minuteVal, int hourVal) {
  // group 0 -> minute bits 0..2 (1,2,4)
  // group 1 -> minute bits 3..5 (8,16,32)
  // group 2 -> hour bits 0..2 (1,2,4)
  // group 3 -> hour bits 3..4 (8,16) mapped to bit 0 and 1 of that group
  if (group == 0) {
    // bits 0,1,2 of minute
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[group][i];
      if (p < 0) continue;
      bool bitOn = (minuteVal & (1 << i));
      digitalWrite((uint8_t)p, bitOn ? HIGH : LOW);
    }
  } else if (group == 1) {
    // bits 3,4,5 of minute -> positions 0,1,2 in this group
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[group][i];
      if (p < 0) continue;
      bool bitOn = (minuteVal & (1 << (i + 3)));
      digitalWrite((uint8_t)p, bitOn ? HIGH : LOW);
    }
  } else if (group == 2) {
    // bits 0,1,2 of hour
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[group][i];
      if (p < 0) continue;
      bool bitOn = (hourVal & (1 << i));
      digitalWrite((uint8_t)p, bitOn ? HIGH : LOW);
    }
  } else if (group == 3) {
    // bits 3 and 4 of hour -> map to positions 0 and 1
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[group][i];
      if (p < 0) continue;
      bool bitOn = false;
      if (i <= 1) {
        bitOn = (hourVal & (1 << (i + 3)));
      } else {
        bitOn = false; // no third LED in this group
      }
      digitalWrite((uint8_t)p, bitOn ? HIGH : LOW);
    }
  }
}

// -----------------------------
// WIFI / NTP Funktionen
// -----------------------------
void syncTime() {
  configTzTime(TZ_DE, ntpServer);
  struct tm t;
  if (!getLocalTime(&t)) {
    Serial.println("[NTP] Fehler beim Laden der Zeit");
    return;
  }
  // setze Sekunden seit Mitternacht (atomic write)
  noInterrupts();
  Sekunden = t.tm_hour * 3600UL + t.tm_min * 60UL + t.tm_sec;
  interrupts();
  Serial.printf("[NTP] %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
}

void connectToWifiBlocking() {
  String ssid = readFromFile(FILE_SSID);
  String pwd  = readFromFile(FILE_PWD);

  if (ssid == "" || pwd == "") {
    Serial.println("[WIFI] Keine gespeicherten Zugangsdaten");
    return;
  }

  Serial.printf("[WIFI] Verbinde mit '%s' ...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pwd.c_str());

  unsigned long deadline = millis() + 15000UL;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(200);
    yield();
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Verbunden!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
    syncTime();
  } else {
    Serial.println("[WIFI] Verbindung fehlgeschlagen - starte AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
  }
}

// -----------------------------
// Setup
// -----------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BinaryClock mit Ticker & Multiplexing ===");

  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS konnte nicht gemountet werden!");
  }

  // Pins initialisieren
  for (uint8_t g = 0; g < 4; g++) {
    for (uint8_t i = 0; i < 3; i++) {
      int8_t p = groups[g][i];
      if (p >= 0) {
        pinMode((uint8_t)p, OUTPUT);
        digitalWrite((uint8_t)p, LOW);
      }
    }
  }

  // Versuche vorhandene WiFi-Daten zu nutzen
  connectToWifiBlocking();

  // Falls nicht verbunden -> AP starten
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[AP] Starte Konfigurations-AP ...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.print("[AP] IP: ");
    Serial.println(WiFi.softAPIP());
  }

  // Webserver-Routen
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound([](){ server.send(404, "text/plain", "Seite nicht gefunden"); });
  server.begin();
  Serial.println("[WEB] Server gestartet");

  // Ticker starten
  sekundenTicker.attach(1.0, sekundenISR);     // jede Sekunde
  multiplexTicker.attach_ms(5, multiplexISR);  // alle 5 ms
}

// -----------------------------
// Hauptschleife
// -----------------------------
void loop() {
  // Webserver-Anfragen bedienen
  server.handleClient();

  // Handle Connect-Request (aus Web-Handler)
  if (connectRequest) {
    // einzige Stelle, die connectRequest verändert (sicher im loop)
    connectRequest = false;
    connectToWifiBlocking();
  }

  // Handle NTP-Sync-Request (aus sekundenISR)
  if (syncRequest) {
    syncRequest = false;
    // Versuche, WiFi (erneut) zu verbinden, falls nicht verbunden
    if (WiFi.status() != WL_CONNECTED) {
      connectToWifiBlocking();
    } else {
      syncTime();
    }
  }

  // Zeitwerte lokal kopieren (volatile -> lokal)
  unsigned long sec;
  uint8_t grp;
  noInterrupts();
  sec = Sekunden;
  grp = aktuelleGruppe;
  interrupts();

  // Korrektur bei Tagesüberlauf
  if (sec >= 24UL * 3600UL) {
    noInterrupts();
    Sekunden = sec % (24UL * 3600UL);
    sec = Sekunden;
    interrupts();
  }

  Minuten = (sec % 3600UL) / 60;
  Stunden = (sec / 3600UL) % 24;

  // Multiplex: zuerst alle LEDs löschen, dann die aktive Gruppe setzen
  clearAllGroupPins();
  setGroupOutputs(grp, Minuten, Stunden);

  // sehr kurze Pause verhindern (die Hauptarbeit ist hier sehr leicht),
  // wir wollen loop() aber nicht endlos schnell laufen lassen -> yield genügt
  yield();
}
