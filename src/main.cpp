/*
main.cpp
*/

#include <Arduino.h>
#include <Keyboard.h>

void setup() {
    DDRB |= (1 << PB0);
    Keyboard.begin();
}

void loop() {
    PORTB |= (1 << PB0);
    delay(1400);
    PORTB &= ~(1 << PB0);
    delay(4);
    Keyboard.press('h');
    delay(4);
    Keyboard.release('h');
}
