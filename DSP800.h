#include <Arduino.h>
#include <string>

class DSP800 {
    private:
        HardwareSerial &SerialPort;
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
            return 0;
        }

        int save(int layer = 0) {
            if(layer > 2 || layer < 0) return -1;
            byte buf[] = {0x04, 0x01,0x53,0x31 + (layer),0x17};
            SerialPort.write(buf,5);
            return 0;
        }

        int cursor_position(int position = 0) {
            if(position >= 39 || position < 0) return -1;
            byte buf[] = {0x04, 0x01,0x50,0x31 + (position),0x17};
            SerialPort.write(buf,5);
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

        void print(auto str = "") {
            SerialPort.print(str);
        }

};