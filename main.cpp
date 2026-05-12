#pragma once
#include <Arduino.h>
#include "DSP800.h"
#include "Wienerlinen.cpp"
#include <WiFi.h>
#include <HTTPClient.h>
#include <array>
#include <tuple>

DSP800::DSP800 DSP(Serial);
WienerLinienStation Zip({4940, 4430, 4264, 12345});
uint32_t star_flip = false;
std::vector<std::array<char, DSP800::LENGTH>> dp;
WiFiServer server(80);
TaskHandle_t FetchTaskHandle = NULL;
bool updating = false;
bool rendering = false;

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
  while(true){
    HTTPClient http;
    http.begin(Zip.getQueryUrl().c_str());
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      DSP.clear();
      DSP.cursor_position(0);
      DSP.print(String(http.errorToString(httpCode)));
    } else {
      while(rendering) vTaskDelay((portTICK_PERIOD_MS != 0)? 100 / portTICK_PERIOD_MS : 0);
      updating = true;
      if(!Zip.parseResponse(http.getString())){
        DSP.print(String("Error while Parsing json :("));
      }
      updating = false;
    }
    http.end();
    vTaskDelay((portTICK_PERIOD_MS != 0)? 15000 / portTICK_PERIOD_MS : 0);
  }
}

void setup() {
  //Serial.begin(9600, SERIAL_8N1, 14,12);
  Serial.begin(9600);

  WiFi.begin("Science Pool Lidl", "Moebia31415");

  while (WiFi.status() != WL_CONNECTED) {
    DSP.print(String('.'));
    delay(300);
  }
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
    0                   /* Core 0 (WiFi usually runs here, loop runs on 1) */
  );
}



void loop() {
  while(updating) delay(100);
  rendering = true;
  bool no_info = false;
  std::tie(dp, no_info) = Zip.redner(star_flip);
  rendering = false;

  WiFiClient client = server.available();
  if(client){
    while(client.connected()){
      if(client.available()){

      }
    }
  }

  DSP.cursor_position(0);
  if(dp.size() == 0 || (no_info && (1+(star_flip/10))%dp.size() == dp.size()-1)){
    DSP.print("Teilweise keine     ");
    DSP.print("Echtzeitinfos       ");
  } else {
    DSP.print(dp[(star_flip/10)%dp.size()]);
    DSP.print(dp[(1+(star_flip/10))%dp.size()]); 
  }

  
  star_flip++;
  delay(1'000);
}