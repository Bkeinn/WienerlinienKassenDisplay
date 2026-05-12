#pragma once
#include <Arduino.h>
#include <string>
#include <array>

namespace DSP800 {
    inline constexpr int LENGTH = 20;
    inline constexpr int ROW = 2;


class DSP800 {
    private:
        HardwareSerial &SerialPort;
        std::array<char, 40> buffer;
        int cursor = 0;
    public:
        enum DisplayMode {
            HORIZONTAL,
            VERTICAL,
        };

        enum Language {
            USA,
            GERMANY,
            UK,
        };

        DSP800(HardwareSerial &SerialPort) : SerialPort(SerialPort) {}

        int setLanguage(Language l){
            byte buf[] = {0x04, 0x01, 0x49, 0x00, 0x17};
            if(l == USA){
                buf[3] = 0x30;
            } else if(l == GERMANY) {
                buf[3] = 0x32;
            } else if(l == UK){
                buf[3] = 0x33;
            }
            SerialPort.write(buf, 5);
            return 0;
        }
        
        int clear(int start = 0, int end = 39) {
            if(end < start || start < 0 || end > 39) return -1;
            byte buf[] = {0x04, 0x01,0x43,0x31 + start,0x31+end,0x17};
            SerialPort.write(buf,6);
            cursor = 0;
            return 0;
        }

        int save(int layer = 0) {
            if(layer > 2 || layer < 0) return -1;
            byte buf[] = {0x04, 0x01,0x53,0x31 + (layer),0x17};
            SerialPort.write(buf,5);
            return 0;
        }

        int cursor_position(int position = 0) {
            if(position > 39 || position < 0) return -1;
            byte buf[] = {0x04, 0x01,0x50,0x31 + (position),0x17};
            SerialPort.write(buf,5);
            cursor = position;
            return 0;
        }

        int display(std::array<bool, 3> layers, DisplayMode dpmode) {
            byte buf[] = {0x04, 0x01,0x44,0x31, (dpmode == HORIZONTAL)? 0x31 : 0x32 ,0x17};
            if(layers[0] && !layers[1] && !layers[2]){
                
            } else if(!layers[0] && layers[1] && !layers[2]) {
                buf[3] = 0x32;
            } else if(layers[0] && layers[1] && !layers[2]) {
                buf[3] = 0x33;
            }else if(!layers[0] && !layers[1] && layers[2]) {
                buf[3] = 0x34;
            }else if(layers[0] && !layers[1] && layers[2]) {
                buf[3] = 0x35;
            }else if(!layers[0] && layers[1] && layers[2]) {
                buf[3] = 0x36;
            } else {
                buf[3] = 0x37;
            }
            SerialPort.write(buf,6);
            return 0;
        }

        static std::vector<char> to_character_table(const String& str){
            std::vector<char> result;
            result.reserve(str.length());
            for (size_t i = 0; i < str.length(); i++) {
                unsigned char c = str[i];
                if (c == 0xC3 && i + 1 < str.length()) {
                    i++;
                    switch ((unsigned char)str[i]) {
                        case 0xBC: result.push_back((char)0x7D); break; // ü
                        case 0xB6: result.push_back((char)0x7C); break; // ö
                        case 0xA4: result.push_back((char)0x7B); break; // ä
                        case 0x9F: result.push_back((char)0x7E); break; // ß
                        default:
                            result.push_back((char)0xC3);
                            result.push_back(str[i]);
                            break;
                    }
                } else {
                    result.push_back((char)c);
                }
            }
            return result;
        }

        void print(std::array<char, LENGTH>& c){
            for(auto ch : c){
                buffer[cursor++%40]=ch;
                SerialPort.print(ch);
            }
        }

        void update(const std::array<char, LENGTH>& c){
  
            for(auto ch : c){
                if(buffer[cursor%LENGTH] != ch){
                    buffer[cursor%LENGTH] = ch;
                    cursor_position(cursor);
                    print(ch, 1);
                } else {
                    cursor++;
                }
            }
            cursor_position(cursor);
        }

        void print(String val){
            SerialPort.print(val);
            cursor = (cursor + val.length())%40;
        }
        
        template <typename T>
        void print(T val, size_t length) {
            SerialPort.print(val);
            cursor = (cursor + length)%40;
        }

};

}