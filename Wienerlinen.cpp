#pragma once
#ifndef WIENER_LINIEN_CPP
#define WIENER_LINIEN_CPP
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 100

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <math.h>
#include "DSP800.h"

#include "ArduinoJson-v7.4.3.h"

const String NOINFO = String("No Information");
const String EMPTY = String("");    
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



    std::array<char, DSP800::LENGTH> render_into_system(std::array<const String*, 3> info) {
        
        std::array<char, DSP800::LENGTH> result;
        result.fill(' ');
        int index = 0;
        std::vector<char> identifier = DSP800::DSP800::to_character_table(*info[0]);
        std::vector<char> detail = DSP800::DSP800::to_character_table(*info[1]);
        std::vector<char> timing = DSP800::DSP800::to_character_table(*info[2]);
        for(;index<identifier.size();){
            result[index++] = identifier[index];
        }
        result[index++] = ' ';
        for(int i = 0; i<min(detail.size(), DSP800::LENGTH - identifier.size() - 2 - timing.size()); i++){
            result[index++] = detail[i];
        }
        index = DSP800::LENGTH - 1 - timing.size();
        result[index++] = ' ';
        for(int i = 0; i < timing.size(); i++){
            result[index++] = timing[i];
        }
        return result;
    }

    std::pair<std::vector<std::array<char, DSP800::LENGTH>>,bool> redner(int time = 0){
        bool no_info_banner = false;
        std::vector<std::array<char, DSP800::LENGTH>> dp;
        dp.reserve(stopidList.size()+1); 
        for(int stopid : stopidList){
            //if(!departures.contains(stopid) || departures[stopid].empty()) {String stopidstring = String(stopid);dp.push_back(render_into_system(length/2, {&NOINFO, &EMPTY, &stopidstring}));continue;};
            if(!departures.contains(stopid) || departures[stopid].empty()) {no_info_banner = true;continue;};
            int current_train = getCountdown(0,stopid);
            int next_train = getCountdown(1,stopid);
            String info = " ";
            
            if(current_train == 0){
                VehicleType vt = getType(0, stopid);
                if(vt == PTTRAM || vt == PTBUSCITY) info.concat((time%2 ==0)? char(220) : char(223));
                else info.concat(((time/2)%2 ==0)? "* " : " *");
            } else if (current_train == -1) {
                no_info_banner = true;
                continue;
            } else {
                info.concat(current_train);
            }
            if(next_train != -1){
                info.concat(String(char(179)) + next_train);
            }
            dp.push_back(render_into_system({&departures[stopid][0].lineName, &departures[stopid][0].towards, &info}));
        }
        sort(dp.begin(), dp.end());
        return {dp,no_info_banner};
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
