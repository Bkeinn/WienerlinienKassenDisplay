#include <Arduino.h>
#include "DSP800.h"
#include "Wienerlinen.cpp"
#include <WiFi.h>
#include <HTTPClient.h>
#include <array>

DSP800 DSP(Serial);
WienerLinienStation Zip({4940, 4430, 4264});
uint32_t ttime = 0;
uint32_t star_flip = false;
std::vector<String> dp;
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
  ttime = millis() - 15'000;
}

void fetchData(void * paramter) {
  while(true){
    HTTPClient http;
    http.begin(Zip.getQueryUrl().c_str());
    int httpCode = http.GET();
    if(httpCode != HTTP_CODE_OK){
      DSP.clear();
      DSP.cursor_position(0);
      DSP.print(http.errorToString(httpCode));
    } else {
      while(rendering) vTaskDelay(100 / portTICK_PERIOD_MS);
      updating = true;
      if(!Zip.parseResponse(http.getString())){
        DSP.print("Error while Parsing json :(");
      }
      updating = false;
    }
    http.end();
    vTaskDelay(15000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  //Serial.begin(9600, SERIAL_8N1, 14,12);
  Serial.begin(9600);

  WiFi.begin("Science Pool Lidl", "Moebia31415");

  while (WiFi.status() != WL_CONNECTED) {
    DSP.print('.');
    delay(300);
  }
  DSP.clear();
  DSP.print("Connected");
  DSP.setLanguage(DSP800::GERMANY);

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
  dp = Zip.redner(40,star_flip);
  rendering = false;

  WiFiClient client = server.available();
  if(client){
    while(client.connected()){
      if(client.available()){

      }
    }
  }

  //DSP.clear();
  DSP.cursor_position(0);
  DSP.print(dp[(star_flip/10)%dp.size()]);
  DSP.print(dp[(1+(star_flip/10))%dp.size()]);

  
  star_flip++;
  delay(1'000);
}