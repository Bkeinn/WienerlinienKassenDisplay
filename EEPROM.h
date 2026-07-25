#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <array>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace EEPROMWIENERLINIEN {
    static constexpr size_t ENTRY_SIZE = 11;

    inline std::vector<std::array<byte, ENTRY_SIZE>> read() {
        Preferences prefs;
        prefs.begin("wiener_ln", true);

        size_t entries_length = prefs.getBytesLength("entries");
        if(entries_length == 0 || entries_length % ENTRY_SIZE != 0){
            prefs.end();
            return {};
        }
        std::vector<std::array<byte, ENTRY_SIZE>> vec(entries_length / ENTRY_SIZE);
        prefs.getBytes("entries", vec.data(), entries_length);
        prefs.end();
        return vec;
    }

    inline int add(const std::array<byte, ENTRY_SIZE>& save) {
        auto vec = read();
        vec.push_back(save);
        Preferences prefs;
        if(!prefs.begin("wiener_ln", false)) return -2;
        size_t written_bytes = prefs.putBytes("entries", vec.data(), vec.size() * ENTRY_SIZE);
        prefs.end();
        return (written_bytes == vec.size() * ENTRY_SIZE) ? vec.size() : -1;
    }
    
    inline int remove(const std::array<byte, ENTRY_SIZE>& save){
        auto vec = read();
        auto it = std::find(vec.begin(), vec.end(), save);
        if(it == vec.end()) return -1;
        vec.erase(it);
        Preferences prefs;
        if(!prefs.begin("wiener_ln", false)) return -2;

        prefs.putBytes("entries", vec.data(), vec.size() * ENTRY_SIZE);
        prefs.end();

        return 0;
    }

    inline int clear() {
        Preferences prefs;
        if(!prefs.begin("wiener_ln", false)) return -1;
        prefs.remove("entries");
        prefs.end();
        return 0;
    }

    // ── Station name persistence ─────────────────────────────────
    //
    // Binary format:
    //   uint16 count (big-endian)
    //   for each entry:
    //     int32  stopid    (big-endian)
    //     uint16 name_len  (big-endian)
    //     char   name_bytes[name_len]  (UTF-8)

    inline std::unordered_map<int, String> loadAllStationNames() {
        std::unordered_map<int, String> result;
        Preferences prefs;
        prefs.begin("wiener_ln", true);
        size_t len = prefs.getBytesLength("stop_names");
        if (len < 2) { prefs.end(); return result; }

        std::vector<byte> data(len);
        prefs.getBytes("stop_names", data.data(), len);
        prefs.end();

        size_t pos = 0;
        uint16_t count = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
        pos += 2;

        for (uint16_t i = 0; i < count && pos + 6 <= len; i++) {
            int32_t stopid = (static_cast<int32_t>(data[pos])     << 24)
                           | (static_cast<int32_t>(data[pos + 1]) << 16)
                           | (static_cast<int32_t>(data[pos + 2]) << 8)
                           |  static_cast<int32_t>(data[pos + 3]);
            pos += 4;

            uint16_t namelen = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
            pos += 2;

            if (pos + namelen > len) break;

            String s;
            s.reserve(namelen);
            for (uint16_t j = 0; j < namelen; j++) {
                s += static_cast<char>(data[pos++]);
            }
            result[stopid] = s;
        }
        return result;
    }

    inline void clearStationNames() {
        Preferences prefs;
        if (!prefs.begin("wiener_ln", false)) return;
        prefs.remove("stop_names");
        prefs.end();
    }

    // Internal helper — static gives internal linkage, safe for header
    static inline bool writeAllStationNames(const std::unordered_map<int, String>& names) {
        // Calculate total blob size
        size_t size = 2; // count
        for (const auto& pair : names) {
            size += 4 + 2 + pair.second.length();
        }

        std::vector<byte> data(size);
        size_t pos = 0;
        uint16_t count = names.size();
        data[pos++] = (count >> 8) & 0xFF;
        data[pos++] = count & 0xFF;

        for (const auto& pair : names) {
            int32_t sid = pair.first;
            data[pos++] = (sid >> 24) & 0xFF;
            data[pos++] = (sid >> 16) & 0xFF;
            data[pos++] = (sid >> 8) & 0xFF;
            data[pos++] = sid & 0xFF;

            const String& name = pair.second;
            uint16_t namelen = name.length();
            data[pos++] = (namelen >> 8) & 0xFF;
            data[pos++] = namelen & 0xFF;

            for (size_t j = 0; j < namelen; j++) {
                data[pos++] = static_cast<byte>(name[j]);
            }
        }

        Preferences prefs;
        if (!prefs.begin("wiener_ln", false)) return false;
        size_t written = prefs.putBytes("stop_names", data.data(), data.size());
        prefs.end();
        return written == data.size();
    }

    inline bool saveStationName(int stopid, const String& name) {
        auto names = loadAllStationNames();
        names[stopid] = name;
        return writeAllStationNames(names);
    }

    inline void removeStationName(int stopid) {
        auto names = loadAllStationNames();
        names.erase(stopid);
        writeAllStationNames(names);
    }

    // ── WiFi credential persistence ───────────────────────────────

    inline String loadWiFiSSID() {
        Preferences prefs;
        prefs.begin("wiener_ln", true);
        String s = prefs.getString("wifi_ssid", "");
        prefs.end();
        return s;
    }

    inline String loadWiFiPassword() {
        Preferences prefs;
        prefs.begin("wiener_ln", true);
        String s = prefs.getString("wifi_pass", "");
        prefs.end();
        return s;
    }

    inline bool saveWiFiCredentials(const String& ssid, const String& password) {
        Preferences prefs;
        if (!prefs.begin("wiener_ln", false)) return false;
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", password);
        // Set a flag so we know WiFi has been configured
        prefs.putBool("wifi_cfg", true);
        prefs.end();
        return true;
    }

    inline bool isWiFiConfigured() {
        Preferences prefs;
        prefs.begin("wiener_ln", true);
        bool cfg = prefs.getBool("wifi_cfg", false);
        prefs.end();
        return cfg;
    }

    inline void clearWiFiCredentials() {
        Preferences prefs;
        if (!prefs.begin("wiener_ln", false)) return;
        prefs.remove("wifi_ssid");
        prefs.remove("wifi_pass");
        prefs.remove("wifi_cfg");
        prefs.end();
    }

    // ── Alert rule persistence ────────────────────────────────────
    //
    // Stored as JSON string under key "alerts".
    // Structure: [{"stopid":NNN,"rules":[{...},...]}, ...]
    // Each rule: {"wd":BITMASK,"sh":H,"sm":M,"eh":H,"em":M,"mb":MIN}
    //   wd  = weekday bitmask (1<<0=Mon ... 1<<6=Sun)
    //   sh  = start hour (0-23)
    //   sm  = start minute (0-59)
    //   eh  = end hour (0-23)
    //   em  = end minute (0-59)
    //   mb  = minutes before departure to beep

    inline String loadAlertsJSON() {
        Preferences prefs;
        prefs.begin("wiener_ln", true);
        String s = prefs.getString("alerts", "[]");
        prefs.end();
        return s;
    }

    inline bool saveAlertsJSON(const String& json) {
        Preferences prefs;
        if (!prefs.begin("wiener_ln", false)) return false;
        prefs.putString("alerts", json);
        prefs.end();
        return true;
    }
};
