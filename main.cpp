#pragma once
#include <Arduino.h>
#include "DSP800.h"
#include "Wienerlinen.cpp"
#include "Webserver.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <array>
#include <tuple>
#include <mutex>
#include <sstream>
#include <WebServer.h>
#include "Sound.h"
#include <time.h>
#include <set>

#define GreenButton 22
bool lastGreenState = LOW;
#define RedButton 23
bool lastRedState = LOW;

#define Buzzer_port 13

DSP800::DSP800 DSP(Serial1);
WienerLinienStation Zip({});
std::pair<uint32_t,uint32_t> star_flip = {0,0};
std::mutex mutex;
std::vector<std::array<char, DSP800::LENGTH>> dp;
WebServer server(80);
TaskHandle_t FetchTaskHandle = NULL;
std::pair<bool, bool> configuration_mode = {false,false};
unsigned long last_update = millis();

Buzzer buzzer = Buzzer(Buzzer_port);

// ── Alert tracking ───────────────────────────────────────────────
int lastFetchVersion = 0;
std::set<int> beepedAlready;  // tracks (stopid << 8 | offset) already beeped this fetch cycle
unsigned long lastAlertCheck = 0;

int get_num_length(int n){
  if(n==0) return 1;
  int l = 0;
  while(n > 0){
    l++;
    n /= 10;
  }
  return l;
}

// ── Fetch task (unchanged logic) ─────────────────────────────────

void fetchData(void * paramter) {
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  
  while(true){
    { std::lock_guard<std::mutex> lock(mutex); http.begin(Zip.getQueryUrl().c_str()); }
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      Serial.println(httpCode);
      Serial.println(String(http.errorToString(httpCode)));
    } else {
      while(!mutex.try_lock()) vTaskDelay((portTICK_PERIOD_MS != 0)? 100 / portTICK_PERIOD_MS : 0);
      if(!Zip.parseResponse(http.getString())){
        Serial1.println(String("Error while Parsing json :("));
      }
      mutex.unlock();
    }
    http.end();
    vTaskDelay((portTICK_PERIOD_MS != 0)? 15000 / portTICK_PERIOD_MS : 0);
  }
}

// ── Alert checking ───────────────────────────────────────────────
//
// Parses the alert JSON stored in EEPROM, checks each rule against
// the current time and the current + next departure countdown for
// each watched LineStop. Triggers the buzzer when a match is found,
// but only once per (stopid, departure_offset) per fetch cycle.

void checkAlerts() {
    // Only check once per second
    if (millis() - lastAlertCheck < 1000) return;
    lastAlertCheck = millis();

    // On new fetch data, clear the "already beeped" set
    if (Zip.fetchVersion != lastFetchVersion) {
        beepedAlready.clear();
        lastFetchVersion = Zip.fetchVersion;
    }

    // Get current local time (use short timeout to avoid blocking)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return;  // NTP not synced yet
    int nowWeekday = timeinfo.tm_wday;  // 0=Sun, 1=Mon, ... 6=Sat
    // Convert to our bitmask convention: 0=Mon ... 6=Sun
    int weekdayBit = (nowWeekday == 0) ? 6 : (nowWeekday - 1);
    int nowMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    // Load alert configs
    String alertsJSON = EEPROMWIENERLINIEN::loadAlertsJSON();
    if (alertsJSON.length() <= 2) return;  // "[]" or empty

    JsonDocument alertDoc;
    DeserializationError err = deserializeJson(alertDoc, alertsJSON);
    if (err) return;  // malformed JSON, skip

    JsonArray alertList = alertDoc.as<JsonArray>();

    for (auto& ls : Zip.linestopList) {
        // Find alert entry for this stopid
        JsonObject entry;
        for (JsonObject e : alertList) {
            if (e["stopid"].as<int>() == ls.stopid) {
                entry = e;
                break;
            }
        }
        if (entry.isNull()) continue;

        JsonArray rules = entry["rules"].as<JsonArray>();
        if (!rules || rules.size() == 0) continue;

        // Check current (offset 0) and next (offset 1) departures
        for (int offset = 0; offset < 2; offset++) {
            int cd = Zip.getCountdown(offset, ls);
            if (cd <= 0) continue;

            int beepKey = (ls.stopid << 8) | offset;
            if (beepedAlready.count(beepKey)) continue;  // already beeped this cycle

            for (JsonObject rule : rules) {
                int wd  = rule["wd"]  | 0;    // weekday bitmask
                int sh  = rule["sh"]  | 0;    // start hour
                int sm  = rule["sm"]  | 0;    // start minute
                int eh  = rule["eh"]  | 23;   // end hour
                int em  = rule["em"]  | 59;   // end minute
                int mb  = rule["mb"]  | 5;    // minutes before

                // Check weekday
                if (!(wd & (1 << weekdayBit))) continue;

                // Check time window
                int windowStart = sh * 60 + sm;
                int windowEnd   = eh * 60 + em;
                if (nowMinutes < windowStart || nowMinutes > windowEnd) continue;

                // Check countdown
                if (cd == mb) {
                    buzzer.blink();
                    beepedAlready.insert(beepKey);
                    break;  // one beep per departure per cycle
                }
            }
        }
    }
}

// ── AP mode: WiFi setup portal ───────────────────────────────────
//
// Runs when no WiFi credentials are stored or connection fails.
// Creates a softAP, shows the IP on the VFD, and serves a simple
// page for entering SSID + password. On successful submit, saves
// to EEPROM and reboots.

void startAPMode() {
    Serial.println("Entering WiFi setup AP mode...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("WienerLinien-Setup");

    IPAddress apIP = WiFi.softAPIP();
    String ipStr = apIP.toString();
    DSP.clear();
    DSP.print(String("WiFi Setup"));
    // Show AP IP on second row
    {
        auto ipArr = DSP800::DSP800::to_length_array(ipStr);
        // We show it by rendering into dp
        std::vector<std::array<char, DSP800::LENGTH>> apDisplay;
        apDisplay.push_back(DSP800::DSP800::to_length_array(String("Connect to AP:")));
        apDisplay.push_back(DSP800::DSP800::to_length_array(String("WienerLinien-Setup")));
        apDisplay.push_back(ipArr);
        if (apDisplay.size() > 0) {
            DSP.clear();
            DSP.update(apDisplay[0]);
            if (apDisplay.size() > 1) DSP.update(apDisplay[1]);
            if (apDisplay.size() > 2) DSP.update(apDisplay[2]);
        }
    }

    // Set up web server for setup portal
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", getWifiSetupHTML());
    });
    server.on("/", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("password")) {
            String ssid = server.arg("ssid");
            String pass = server.arg("password");
            if (ssid.length() > 0) {
                EEPROMWIENERLINIEN::saveWiFiCredentials(ssid, pass);
                server.send(200, "text/html",
                    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Saved</title>"
                    "<meta http-equiv='refresh' content='3;url=/'></head>"
                    "<body><h2>Credentials saved!</h2><p>Rebooting... please reconnect to your WiFi network.</p></body></html>");
                delay(1000);
                ESP.restart();
                return;
            }
        }
        server.send(400, "text/html", "<h2>Error: SSID must not be empty</h2>");
    });
    // Catch-all for captive portal compatibility
    server.onNotFound([]() {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
    });

    server.begin();

    // Block until credentials are saved (loop handles clients)
    while (true) {
        server.handleClient();
        delay(10);
    }
}

// ── Main setup ───────────────────────────────────────────────────

void setup() {
  Serial1.begin(9600, SERIAL_8N1, 14, 4);
  Serial.begin(9600);

  pinMode(GreenButton, INPUT_PULLDOWN);
  pinMode(RedButton, INPUT_PULLDOWN);

  pinMode(Buzzer_port, OUTPUT);

  DSP.clear();
  DSP.print(String("Starting..."));

  // ── WiFi: check for stored credentials ────────────────────────
  bool wifiOk = false;
  if (EEPROMWIENERLINIEN::isWiFiConfigured()) {
    String ssid = EEPROMWIENERLINIEN::loadWiFiSSID();
    String pass = EEPROMWIENERLINIEN::loadWiFiPassword();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long startAttempt = millis();
    DSP.clear();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 30000) {
      DSP.print(String('.'));
      delay(300);
    }
    wifiOk = (WiFi.status() == WL_CONNECTED);
  }

  if (!wifiOk) {
    startAPMode();
    // startAPMode restarts the ESP, so we never reach here
    return;
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ── Load persisted stops ──────────────────────────────────────

  Serial.println(EEPROMWIENERLINIEN::read().size());
  for(auto &linestop : EEPROMWIENERLINIEN::read()){
    Zip.linestopList.push_back(WienerLinienStation::LineStop(linestop));
    for(auto a : linestop) Serial.print(a);
    Serial.println("");
  }
  Zip.loadStationNamesFromEeprom();

  // ── Web server routes ─────────────────────────────────────────

  server.on("/", HTTP_GET, []() {
    handleRoot(server);
  });

  server.on("/api/rbl-lines", HTTP_GET, []() {
    handleApiRblLines(server);
  });

  server.on("/api/rbl-stations", HTTP_GET, []() {
    handleApiRblStations(server);
  });

  server.on("/api/stopids", HTTP_GET, [&mutex]() {
    std::lock_guard<std::mutex> lock(mutex);
    handleApiGetStopids(server, Zip);
  });

  server.on("/api/add-stop", HTTP_POST, [&mutex]() {
    std::lock_guard<std::mutex> lock(mutex);
    handleApiAddStop(server, Zip);
  });

  server.on("/api/delete-stop", HTTP_POST, [&mutex]() {
    std::lock_guard<std::mutex> lock(mutex);
    handleApiDeleteStop(server, Zip);
  });

  server.on("/api/delete-all", HTTP_POST, [&mutex]() {
    std::lock_guard<std::mutex> lock(mutex);
    handleApiDeleteAllStop(server, Zip);
  });

  // ── Alert API routes ──────────────────────────────────────────

  server.on("/api/alerts", HTTP_GET, []() {
    handleApiGetAlerts(server);
  });

  server.on("/api/alerts", HTTP_POST, []() {
    handleApiSaveAlerts(server);
  });

  // ── WiFi reset ────────────────────────────────────────────────

  server.on("/api/reset-wifi", HTTP_POST, []() {
    EEPROMWIENERLINIEN::clearWiFiCredentials();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"WiFi credentials cleared. Rebooting...\"}");
    delay(500);
    ESP.restart();
  });

  server.begin();

  // ── Show IP on VFD ────────────────────────────────────────────
  DSP.clear();
  {
    String ipStr = WiFi.localIP().toString();
    auto ipArr = DSP800::DSP800::to_length_array(ipStr);
    DSP.update(DSP800::DSP800::to_length_array(String("Connected")));
    DSP.update(ipArr);
  }
  DSP.setLanguage(DSP800::DSP800::GERMANY);

  // ── NTP time sync (server already running) ────────────────────
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync...");
  {
    struct tm timeinfo;
    int ntpRetries = 0;
    while (!getLocalTime(&timeinfo) && ntpRetries < 20) {
      Serial.print('.');
      // Serve clients during NTP wait
      for (int i = 0; i < 5; i++) { server.handleClient(); delay(100); }
      ntpRetries++;
    }
    if (getLocalTime(&timeinfo)) {
      Serial.println("\nNTP synced!");
    } else {
      Serial.println("\nNTP sync failed, alerts will not work until synced.");
    }
  }

  // ── Start fetch task (serve clients during initial delay) ─────

  xTaskCreatePinnedToCore(
    fetchData,
    "FetchTask",
    10000,
    NULL,
    1,
    &FetchTaskHandle,
    1
  );

  // Brief warm-up: let the server handle any pending requests
  for (int i = 0; i < 50; i++) {
    server.handleClient();
    delay(20);
  }

  DSP.clear();
}

// ── Main loop ───────────────────────────────────────────────────

void loop() {

  server.handleClient();
  if(!configuration_mode.first) {
    
    if(mutex.try_lock()){
      Zip.redner_active(star_flip.first, dp);
      mutex.unlock();
    }

    if (digitalRead(GreenButton) && !lastGreenState) configuration_mode.first = true;
  } else if(configuration_mode.first){
    if(mutex.try_lock()){
      Zip.redner_inactive(dp, DSP800::DSP800::to_length_array(WiFi.localIP().toString()));
      mutex.unlock();
    }
    if (digitalRead(GreenButton) && !lastGreenState) configuration_mode.first = false;
  }

  lastGreenState = digitalRead(GreenButton); 

  if (digitalRead(RedButton) && !lastRedState) star_flip = {star_flip.first+10, star_flip.second+10};
  lastRedState = digitalRead(RedButton);

  if(millis() - last_update >= 1'000) {star_flip.first++; last_update = millis();};

  if(star_flip.first != star_flip.second || configuration_mode.first != configuration_mode.second)[[unlikely]]{
    if(dp.size() == 0){
      configuration_mode.first = true;
    } else [[likely]] {
      if((star_flip.first/10) != ((star_flip.first-1)/10)) DSP.clear();
      DSP.update(dp[(star_flip.first/10)%dp.size()]);
      DSP.update(dp[(1+(star_flip.first/10))%dp.size()]); 
    }
    configuration_mode.second = configuration_mode.first;
    star_flip.second = star_flip.first;
  }

  // ── Check alert rules ─────────────────────────────────────────
  if (mutex.try_lock()) {
    checkAlerts();
    mutex.unlock();
  }

  //Zip.debug();
  delay(50);
}