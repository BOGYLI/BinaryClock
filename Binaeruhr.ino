#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <time.h>

// ----------------------------------------------------
//        KONSTANTEN & VARIABLEN
// ----------------------------------------------------

const char* apSSID     = "BinaryClock"; // Name des eigenen WLAN Access Points des Microcontrollers
const char* apPassword = "BC123456"; // Passwort des Access Points

const char* ntpServer = "pool.ntp.org"; // URL des Zeitservers
const char* TZ_DE = "CET-1CEST,M3.5.0,M10.5.0/3"; // Zeitzone für Deutschland

ESP8266WebServer server(80);

// Dateien im Flash
const char* FILE_SSID = "/wifi_ssid.txt";
const char* FILE_PWD  = "/wifi_pwd.txt";

// Zeitvariablen
unsigned long Sekunden = 0;
int Minuten = 0;
int Stunden = 0;

// LED-PINs müssen umnummeriert werden, weil sich ESP und Arduino (bei tinkercad) unterscheiden.
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
  configTzTime(TZ_DE, ntpServer);

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

  int PINs[] = {PIN0,PIN1,PIN2,PIN3,PIN4,PIN5,PIN6,PIN7,PIN8,PIN9,PIN10};
  for (int p : PINs) pinMode(p, OUTPUT);
}

// ----------------------------------------------------
//        HAUPTSCHLEIFE – LED-BINÄRUHR
// ----------------------------------------------------

void loop() {
  server.handleClient();

  // Nach einem Tag werden sie Sekunden zurückgesetzt.
  if (Sekunden >= 24 * (60 * 60)) {
    Sekunden = 0;
  }
  // Um 3 Uhr nachts wird die Zeit mit dem Server synchronisiert.
  if  (Sekunden == 3 * (60 * 60)) {
    connectToWifi();
  }
  Minuten = ((Sekunden % 3600) / 60);
  Stunden = (Sekunden / 3600);

  // Jede Lampengruppe (3 LED's) leuchtet 5ms => zusammen 20ms.
  // Das wird 50 mal wiederholt => Gesamtzeit 1s.
  for (int i=0; i<50; i++) {
    if (Minuten % 2 >= 1) {
      digitalWrite(PIN0, HIGH);
    } else {
      digitalWrite(PIN0, LOW);
    }
    if (Minuten % 4 >= 2) {
      digitalWrite(PIN1, HIGH);
    } else {
      digitalWrite(PIN1, LOW);
    }
    if (Minuten % 8 >= 4) {
      digitalWrite(PIN2, HIGH);
    } else {
      digitalWrite(PIN2, LOW);
    }
    delay(5); // Warte 5 Millisekunden
    digitalWrite(PIN0, LOW);
    digitalWrite(PIN1, LOW);
    digitalWrite(PIN2, LOW);
    if (Minuten % 16 >= 8) {
      digitalWrite(PIN3, HIGH);
    } else {
      digitalWrite(PIN3, LOW);
    }
    if (Minuten % 32 >= 16) {
      digitalWrite(PIN4, HIGH);
    } else {
      digitalWrite(PIN4, LOW);
    }
    if (Minuten % 64 >= 32) {
      digitalWrite(PIN5, HIGH);
    } else {
      digitalWrite(PIN5, LOW);
    }
    delay(5); // Warte 5 Millisekunden
    digitalWrite(PIN3, LOW);
    digitalWrite(PIN4, LOW);
    digitalWrite(PIN5, LOW);
    if (Stunden % 2 >= 1) {
      digitalWrite(PIN6, HIGH);
    } else {
      digitalWrite(PIN6, LOW);
    }
    if (Stunden % 4 >= 2) {
      digitalWrite(PIN7, HIGH);
    } else {
      digitalWrite(PIN7, LOW);
    }
    if (Stunden % 8 >= 4) {
      digitalWrite(PIN8, HIGH);
    } else {
      digitalWrite(PIN8, LOW);
    }
    delay(5); // Warte 5 Millisekunden
    digitalWrite(PIN6, LOW);
    digitalWrite(PIN7, LOW);
    digitalWrite(PIN8, LOW);
    if (Stunden % 16 >= 8) {
      digitalWrite(PIN9, HIGH);
    } else {
      digitalWrite(PIN9, LOW);
    }
    if (Stunden % 32 >= 16) {
      digitalWrite(PIN10, HIGH);
    } else {
      digitalWrite(PIN10, LOW);
    }
    delay(5); // Warte 5 Millisekunden
    digitalWrite(PIN9, LOW);
    digitalWrite(PIN10, LOW);
  }
  Sekunden += 1;
}
