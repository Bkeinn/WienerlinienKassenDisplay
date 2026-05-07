#ifndef WIENER_LINIEN_CPP
#define WIENER_LINIEN_CPP
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 100

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "ArduinoJson-v7.4.3.h"

class WienerLinienStation {
public:
    enum VehicleType {
        PTTRAM,
        UNKNOWN,
        PTMETRO,
        PTBUSCITY,
    };
    struct Departure {
        String lineName;
        String towards;
        VehicleType type;
        int stopid;
        int countdown;
    };
    

    WienerLinienStation(const std::vector<int> stopid) 
        : stopidList(stopid) {}

    std::string getQueryUrl() const {
        std::string url = "https://www.wienerlinien.at/ogd_realtime/monitor?";
        for (size_t i = 0; i < stopidList.size(); ++i) {
            url += "stopId=" + std::to_string(stopidList[i]);
            if (i < stopidList.size() - 1) url += "&";
        }
        return url;
    }

    bool parseResponse(const String& json) {
        
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, json);
        if (error) {
            //std::cout << "Error: " << error << std::endl;
            //Serial.print(error.c_str());
            return false;
        }
        departures.clear();

        JsonArray monitors = doc["data"]["monitors"];
        // for (JsonObject monitor : monitors) {
        for(int i = 0; i<monitors.size();i++){
            JsonArray lines = monitors[i]["lines"];
            for (JsonObject line : lines) {
                String lineName = line["name"] | "Unknown";
                String towards = line["towards"] | "Unknown";
                VehicleType type = UNKNOWN;
                if(line["type"] == "ptTram"){
                    type = PTTRAM;
                } else if(line["type"] == "ptMetro"){
                    type = PTMETRO;
                } else if(line["type"] == "ptBusCity"){
                    type = PTBUSCITY;
                }
                
                JsonArray lineDepartures = line["departures"]["departure"];
                for (JsonObject dep : lineDepartures) {
                        Departure d;
                        d.lineName = lineName;
                        d.towards = towards;
                        d.type = type;
                        d.countdown = dep["departureTime"]["countdown"] | 999; // If it takes really long it has no countdown and I suppose there will always be a faster train/bus than 999
                        d.stopid = stopidList[i];
                        departures[stopidList[i]].push_back(d);
                }
            }
        }

        for(auto& Stations : departures){
          std::sort(Stations.second.begin(), Stations.second.end(), [](const Departure& a, const Departure& b) {
              return a.countdown < b.countdown;
          });
        }

        return true;
    }

    size_t getDepartureCount() const {
        return departures.size();
    }

    int getCountdown(size_t offset = 0, int stopid = 0) {
        if (!departures.contains(stopid) || offset >= departures[stopid].size()) return -1;
        return departures[stopid][offset].countdown;
    }

    VehicleType getType(size_t offset = 0, int stopid = 0) {
        if (!departures.contains(stopid) || offset >= departures[stopid].size()) return UNKNOWN;
        return departures[stopid][offset].type;
    }

    Departure getDeparture(size_t offset = 0, int stopid = 0) {
        Departure d;
        if (!departures.contains(stopid) || offset >= departures[stopid].size()) return d;
        return departures[stopid][offset];
    }

    String getTowards(size_t offset = 0, int stopid = 0) {
        if (!departures.contains(stopid) || offset >= departures.size()) return "";
        return departures[stopid][offset].towards;
    }

    int set_stopids(std::vector<int> stopids){
        stopidList = stopids;
        departures.clear();
        return 0;
    }

    std::vector<int> get_stopids() {
        return stopidList;
    }

    std::vector<String> redner(int length = 40, int time = 0){
        std::vector<String> dp;
        dp.reserve(stopidList.size()); 
        for(int stopid : stopidList){
            if(!departures.contains(stopid) || departures[stopid].empty()) {dp.push_back(String("No information: ") + String(stopid));dp[dp.size()-1].concat("    "); dp[dp.size()-1].substring(0,20);continue;};
            String s = (departures[stopid][0].lineName + String(" ") + departures[stopid][0].towards);
            s.replace("ü", String(char(0x7d)));
            s.replace("ö", String(char(0x7c)));
            s.replace("ä", String(char(0x7b)));
            s.replace("ß", String(char(0x7e)));
            
            int current_train = getCountdown(0,stopid);
            int next_train = getCountdown(1,stopid);
            String info = " ";
            
            if(current_train == 0){
                VehicleType vt = getType(0, stopid);
                if(vt == PTTRAM || vt == PTBUSCITY) info.concat((time%2 ==0)? char(220) : char(223));
                else info.concat(((time/2)%2 ==0)? "* " : " *");
            } else {
                info.concat(current_train);
            }
            info.concat(String(char(179)) + next_train);
            s.concat("                    ");
            s = s.substring(0, 20 - (info.length()));
            s.concat(info);
            dp.push_back(s);
        }
        sort(dp.begin(), dp.end());
        return dp;
    }

    void debug() {
        Serial.print("Debug: \n");
        for(auto a : departures){
            Serial.print(a.first + "\n\t");
            for(Departure b : a.second){
                Serial.print(b.lineName.c_str());
                Serial.print("-");
                Serial.print(b.countdown);
                Serial.print(" ");
            }
        }
        Serial.println();
    }

private:
    std::vector<int> stopidList;
    // Stopid to departures
    std::unordered_map<int, std::vector<Departure>> departures;
};

#endif
