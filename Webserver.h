#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>

String getHTML(std::vector<int> &current) {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 Vector Input</title></head>
<body><h1>ESP32 Integer Collector</h1>
  
<form action='/submit' method='POST'>
<input type='text' name='numbers' placeholder="1, 2, 3">
<input type='submit' value='Send to ESP32'>
</form>)rawliteral";
  
  html += "<h3>Current Vector:</h3><ul>";
  for(int x : current) {
    html += "<li>" + String(x) + "</li>";
  }
  html += "</ul></body></html>";
  return html;
}


bool stopid_validator(String &numbers) {
    if(numbers.length() <= 0) return false;
    for(char c : numbers) {
        if(!((c >= '0' && c <= '9') || c == ',' || c == ' ')) return false;
    }
    return true;
}


std::vector<int> parse_num_vec(String &numbers) {
    std::vector<int> stopid;
    int cur = 0;
    bool hasDigit = false;

    for(char c : numbers){
        if(c == ' ') {
            continue;
        }
        if(c == ',') {
            if (hasDigit) {
                stopid.push_back(cur);
                cur = 0;
                hasDigit = false;
            }
        } else {
            cur *= 10;
            cur += (int)c - '0';
            hasDigit = true;
        }
    }
    if (hasDigit) {
        stopid.push_back(cur);
    }
    return stopid;
}

void handleRoot(WebServer &server, std::vector<int> &current) {
    server.send(200, "text/html", getHTML(current));
}

void handleSubmit(WebServer &server, std::vector<int> &stopidList) {
    if(server.hasArg("numbers")) {
        String rawInput = server.arg("numbers");
        if(stopid_validator(rawInput)) {
            stopidList = parse_num_vec(rawInput);
            
            // Redirect the browser back to the root page to show the updated list
            server.sendHeader("Location", "/");
            server.send(303, "text/plain", "Redirecting...");
        }
        else {
            String response = "<h3>Invalid stopid data</h3><img src='https://http.cat/images/406.jpg'>";
            server.send(406, "text/html", response);
        }
    } else {
        String response = "<h3>No stopid data</h3><img src='https://http.cat/images/400.jpg'>";
        server.send(400, "text/html", response);
    }
}