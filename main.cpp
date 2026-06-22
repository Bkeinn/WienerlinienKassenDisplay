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

#define GreenButton 22
bool lastGreenState = LOW;
#define RedButton 23
bool lastRedState = LOW;

DSP800::DSP800 DSP(Serial1);
WienerLinienStation Zip({4940, 4430, 4264});
//WienerLinienStation Zip({4940});
std::pair<uint32_t,uint32_t> star_flip = {0,0};
std::mutex mutex;
std::vector<std::array<char, DSP800::LENGTH>> dp;
WebServer server(80);
TaskHandle_t FetchTaskHandle = NULL;
std::pair<bool, bool> configuration_mode = {false,false};
unsigned long last_update = millis();

int get_num_length(int n){
  if(n==0) return 1;
  int l = 0;
  while(n > 0){
    l++;
    n /= 10;
  }
  return l;
}

void fetchData(void * paramter) {
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  
  while(true){
    http.begin(Zip.getQueryUrl().c_str());
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

void setup() {
  Serial1.begin(9600, SERIAL_8N1, 14,4);
  Serial.begin(9600);

  pinMode(GreenButton, INPUT_PULLDOWN);
  pinMode(RedButton, INPUT_PULLDOWN);

  WiFi.begin("Science Pool Lidl", "Moebia31415");

  while (WiFi.status() != WL_CONNECTED) {
    DSP.print(String('.'));
    delay(300);
  }

  server.on("/", HTTP_GET, []() {
    handleRoot(server, Zip.stopidList);
  });

  server.on("/submit", HTTP_POST, []() {
    handleSubmit(server, Zip.stopidList);
  });

  server.begin();

  DSP.clear();
  DSP.print(String("Connected"));
  DSP.setLanguage(DSP800::DSP800::GERMANY);

  delay(5000);

  xTaskCreatePinnedToCore(
    fetchData,
    "FetchTask",
    10000,
    NULL,
    1,
    &FetchTaskHandle,
    1                   /* Core 0 (WiFi usually runs here, loop runs on 1) */
  );
  DSP.clear();
}

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

  if (digitalRead(RedButton) && !lastRedState) star_flip.first += 10;
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

  delay(50);
}