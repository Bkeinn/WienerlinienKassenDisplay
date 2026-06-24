#pragma once
#ifndef WIENER_LINIEN_CPP
#define WIENER_LINIEN_CPP
#define ARDUINOJSON_DEFAULT_NESTING_LIMIT 100

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <math.h>
#include <utility>
#include "DSP800.h"

#include "ArduinoJson-v7.4.3.h"

const String NOINFO = String("No Information");
const String EMPTY = String("");    
class WienerLinienStation {
public:
    struct LineStop {
        int stopid;
        int linecode = -1;
        std::array<char, 3> name;

        LineStop() : stopid(-1), name{'\0', '\0', '\0'} {};

        LineStop(int stopid_v, const String& s) {
            stopid = stopid_v;
            for (size_t i = 0; i < name.size(); ++i) {
                name[i] = (i < s.length()) ? s.charAt(i) : '\0';
            }
        }
        bool operator==(const LineStop& other) const {return (stopid == other.stopid && name == other.name);}
    };
    struct LineStopHasher {
        std::size_t operator()(const WienerLinienStation::LineStop& ls) const {
        std::size_t h1 = std::hash<int>{}(ls.stopid);
        for (char c : ls.name) h1 ^= std::hash<char>{}(c);
        return h1;
        }
    };
    enum VehicleType {
        PTTRAM,
        UNKNOWN,
        PTMETRO,
        PTBUSCITY,
    };
    struct Departure {
        String towards;
        VehicleType type;
        LineStop linestop;
        int countdown;
    };
    

    std::vector<LineStop> linestopList;
    std::vector<LineStop> linestopActive;
    std::vector<LineStop> linestopInactive;
    std::unordered_map<int, String> stopName;      // stopid → human station name

    String getStopName(int id) const {
        auto it = stopName.find(id);
        return it != stopName.end() ? it->second : "";
    }
    

    WienerLinienStation(const std::vector<LineStop> stopid) 
        : linestopList(stopid) {}

    std::string getQueryUrl() const {
        std::string url = "https://www.wienerlinien.at/ogd_realtime/monitor?";
        for (size_t i = 0; i < linestopList.size(); ++i) {
            url += "stopId=" + std::to_string(linestopList[i].stopid);
            if (i < linestopList.size() - 1) url += "&";
        }
        return url;
    }

    bool parseResponse(const String& json) {
        JsonDocument filter;
        filter["data"]["monitors"][0]["locationStop"]["properties"]["attributes"]["rbl"] = true;
        filter["data"]["monitors"][0]["lines"][0]["name"] = true;
        filter["data"]["monitors"][0]["lines"][0]["towards"] = true;
        filter["data"]["monitors"][0]["lines"][0]["type"] = true;
        filter["data"]["monitors"][0]["lines"][0]["departures"]["departure"][0]["departureTime"]["countdown"] = true;

        JsonDocument doc; 

        DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return false;
        }

        for(LineStop linestop : linestopList) departures[linestop] = {};

        JsonArray monitors = doc["data"]["monitors"];
        for (int i = 0; i < monitors.size(); i++) {
            JsonArray lines = monitors[i]["lines"];
            int stopid = monitors[i]["locationStop"]["properties"]["attributes"]["rbl"];
            for (JsonObject line : lines) {
                LineStop linestop = LineStop(stopid, String(line["name"]  | "Unknown"));
                String towards = line["towards"] | "Unknown";

                VehicleType type = UNKNOWN;
                const char* typeStr = line["type"];
                if (typeStr) {
                    if (strcmp(typeStr, "ptTram") == 0) type = PTTRAM;
                    else if (strcmp(typeStr, "ptMetro") == 0) type = PTMETRO;
                    else if (strcmp(typeStr, "ptBusCity") == 0) type = PTBUSCITY;
                }

                JsonArray lineDepartures = line["departures"]["departure"];
                std::vector<Departure> LineDepartures;
                for (JsonObject dep : lineDepartures) {
                    Departure d;
                    d.towards = towards;
                    d.type = type;
                    d.countdown = dep["departureTime"]["countdown"] | 999;
                    d.linestop = linestop;
                    Serial.print("LineDepartures: ");
                    Serial.println(stopid);
                    Serial.println(linestop.stopid);
                    Serial.println(linestop.name.data());
                    auto it = departures.find(linestop);
                    if(it != departures.end()) it->second.push_back(d);
                }
            }
        }


        for (auto& stationEntry : departures) {
            std::sort(stationEntry.second.begin(), stationEntry.second.end(), 
                [](const Departure& a, const Departure& b) {
                    return a.countdown < b.countdown;
                }
            );
        }

        return true;
    }

    size_t getDepartureCount() const {
        return departures.size();
    }

    int getCountdown(size_t offset, LineStop linestop) {
        if (!departures.contains(linestop) || offset >= departures[linestop].size()) return -1;
        return departures[linestop][offset].countdown;
    }

    VehicleType getType(size_t offset, LineStop linestop) {
        if (!departures.contains(linestop) || offset >= departures[linestop].size()) return UNKNOWN;
        return departures[linestop][offset].type;
    }

    Departure getDeparture(size_t offset, LineStop linestop) {
        Departure d;
        if (!departures.contains(linestop) || offset >= departures[linestop].size()) return d;
        return departures[linestop][offset];
    }

    String getTowards(size_t offset, LineStop linestop) {
        if (!departures.contains(linestop) || offset >= departures[linestop].size()) return "";
        return departures[linestop][offset].towards;
    }

    int set_stopids(std::vector<LineStop> linestop){
        linestopList = linestop;
        departures.clear();
        return 0;
    }

    void partition_stopids(){
        linestopActive.clear();
        linestopInactive.clear();
        for(LineStop linestop : linestopList){
            if(!departures.contains(linestop) || departures[linestop].empty() || getCountdown(0,linestop) == -1) linestopInactive.push_back(linestop);
            else linestopActive.push_back(linestop);
        }
    }

    template <std::size_t N>
    std::array<char, DSP800::LENGTH> render_into_system(std::array<char, N>& name, const String& towards, const String& info) {
        
        std::array<char, DSP800::LENGTH> result;
        result.fill(' ');
        int index = 0;
        auto [identifier, identifier_length] = DSP800::DSP800::to_length_array_variable(name);
        auto [detail, detail_length] = DSP800::DSP800::to_length_array_variable(towards);
        auto [timing, timing_length] = DSP800::DSP800::to_length_array_variable(info);
        for(;index<min<int>(identifier_length, DSP800::LENGTH - 1); index++){
            result[index] = identifier[index];
        }
        result[index++] = ' ';
        for(int i = 0; i<min(detail_length, DSP800::LENGTH - identifier_length - 2 - timing_length); i++){
            result[index++] = detail[i];
        }
        index = max<int>(DSP800::LENGTH - 1 - timing_length,0);
        result[index++] = ' ';
        for(int i = 0; i < timing_length; i++){
            result[index++] = timing[i];
        }
        return result;
    }

    int redner_active(int time, std::vector<std::array<char, DSP800::LENGTH>> &dp){
        dp.resize(0);
        partition_stopids();
        dp.reserve(linestopActive.size()+1);
        for(LineStop linestop : linestopActive){

            int current_train = getCountdown(0, linestop);
            int next_train = getCountdown(1, linestop);

            String info = " ";
            if(current_train == 0){
                VehicleType vt = getType(0, linestop);
                if(vt == PTTRAM || vt == PTBUSCITY) info.concat((time%2 ==0)? char(220) : char(223));
                else info.concat(((time/2)%2 ==0)? "* " : " *");
            } else {
                info.concat(current_train);
            }
            if(next_train != -1){
                info.concat(String(char(179)) + next_train);
            }
            dp.push_back(render_into_system(linestop.name, departures[linestop][0].towards, info));
        }
        sort(dp.begin(), dp.end());
        return 0;
    }

    int redner_inactive(std::vector<std::array<char, DSP800::LENGTH>> &dp, std::array<char, DSP800::LENGTH> ipa) {
        dp.resize(0);
        partition_stopids();
        dp.reserve(linestopInactive.size()+1);
        dp.push_back(ipa);
        auto [info, info_length] = DSP800::DSP800::to_length_array_variable(String("Kein Info für:"));
        for(LineStop linestop : linestopInactive){
            auto [line, length] = DSP800::DSP800::to_length_array_variable(String(linestop.stopid));
            std::copy(line.begin(), line.begin()+length, line.end()-length-1);
            std::copy(info.begin(), info.begin()+info_length, line.begin());
            dp.push_back(line);
        }
        return 0;
    }   

    void debug() {
        Serial.print("Debug: \n");
        for(auto a : departures){
            Serial.print(a.first.stopid);
            Serial.print(" ");
            Serial.print(String(a.first.name.data()));
            Serial.print("\n\t");
            for(Departure b : a.second){
                Serial.print(b.towards.c_str());
                Serial.print("-");
                Serial.print(b.countdown);
                Serial.print(" ");
            }
        }
        Serial.println();
    }


private:
    // LineStop to departures
    std::unordered_map<LineStop, std::vector<Departure>, LineStopHasher> departures;
};

#endif
