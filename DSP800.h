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

        DSP800(HardwareSerial &SerialPort) : SerialPort(SerialPort) {
            buffer.fill(' ');
        }

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
            buffer.fill(' ');
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

        static char to_character_table(const String& str, int &index){
            if (str.charAt(index) == 0xC3 && index + 1 < str.length()) {
                    index++;
                    switch ((unsigned char)str[index]) {
                        case 0xBC: return (char)0x7D; // ü
                        case 0xB6: return (char)0x7C; // ö
                        case 0xA4: return (char)0x7B; // ä
                        case 0x9F: return (char)0x7E; // ß
                        default:
                            index--;
                            return (char)0xC3;
                    }
                }
            return str.charAt(index);
        }

        static std::pair<std::array<char, LENGTH>, size_t> to_length_array_variable(const String& str) {
            int count = 0;
            std::array<char, LENGTH> result;
            for(int i = 0; i < min<int>(LENGTH, str.length()); i++){ 
                result[count] = to_character_table(str, i);
                count++;
            }
            return {result, count};
        }

        static std::array<char, LENGTH> to_length_array(const String& str){
            return to_length_array_variable(str).first;
        }

        void print(const std::array<char, LENGTH>& c){
            for(auto ch : c){
                buffer[cursor++%40]=ch;
                SerialPort.print(ch);
            }
            cursor%=LENGTH*ROW;
        }

        void update(const std::array<char, LENGTH>& c){
  
            for(auto ch : c){
                if(buffer[cursor%(LENGTH*ROW)] != ch){
                    buffer[cursor%(LENGTH*ROW)] = ch;
                    cursor_position(cursor);
                    print(ch, 1);
                } else {
                    cursor = (cursor+1) % (LENGTH * ROW);
                }
            }

            //cursor_position(cursor);
        }

        void print(String val){
            cursor_position(cursor);
            SerialPort.print(val);
            cursor = (cursor + val.length())%40;
        }
        
        template <typename T>
        void print(T val, size_t length) {
            cursor_position(cursor);
            SerialPort.print(val);
            cursor = (cursor + length)%40;
        }

};

}