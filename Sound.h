#pragma once
#include <Arduino.h>

class Buzzer {
    int port;

    public:

    Buzzer(int port) : port(port) {};

    void sound(int length) {
        digitalWrite(port, 1);    
        delay(length);
        digitalWrite(port, 0);
    }

    void blink() {
        sound(50);
        delay(20);
        sound(50);
    }
};

