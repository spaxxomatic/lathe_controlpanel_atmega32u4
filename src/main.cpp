/*
main.cpp
*/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <usb.h>

#define BUTTON_PIN 9
#define DEBOUNCE_MS 50

// Encoder on PB4 and PB6 via PCINT0
// State machine: bits [prev_B, prev_A, cur_B, cur_A]
// +1 for CW transitions, -1 for CCW
static const int8_t enc_table[16] = {
    0, +1, -1,  0,
   -1,  0,  0, +1,
   +1,  0,  0, -1,
    0, -1, +1,  0
};

static volatile int16_t enc_delta = 0;
static uint8_t enc_state = 0;

ISR(PCINT0_vect) {
    // Read PB4 (bit4) and PB6 (bit6), remap to bits 0 and 1
    uint8_t pb = PINB;
    uint8_t cur = ((pb >> 4) & 0x01) | ((pb >> 5) & 0x02);  // PB4→bit0, PB6→bit1
    enc_state = ((enc_state << 2) | cur) & 0x0F;
    enc_delta += enc_table[enc_state];
}

hd44780_I2Cexp lcd(0x27);

static bool lastButton = HIGH;
static uint32_t lastBlink = 0;
static bool ledOn = true;

void setup() {
    // LED
    DDRB |= (1 << PB0);
    PORTB &= ~(1 << PB0);

    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Encoder inputs with pull-ups on PB4 and PB6
    DDRB  &= ~((1 << PB4) | (1 << PB6));
    PORTB |=  ((1 << PB4) | (1 << PB6));

    // PCINT0: enable interrupts on PB4 and PB6
    PCMSK0 = (1 << PCINT4) | (1 << PCINT6);
    PCICR  = (1 << PCIE0);

    // LCD
    Wire.begin();
    Wire.setWireTimeout(3000, true);
    delay(100);
    int status = lcd.begin(16, 2);
    if (status == 0) {
        lcd.backlight();
        lcd.clear();
        delay(10);
        lcd.setCursor(0, 0);
        lcd.print("Hello World");
        lcd.setCursor(0, 1);
        lcd.print("Line 2 test");
    } else {
        // lcd.begin() failed — blink LED rapidly as error indicator
        while (true) {
            PORTB ^= (1 << PB0);
            delay(100);
        }
    }

    usb_init();
    delay(2000);
}

void loop() {
    // Blink LED
    uint32_t now = millis();
    if (ledOn && now - lastBlink >= 1400) {
        PORTB &= ~(1 << PB0);
        ledOn = false;
        lastBlink = now;
    } else if (!ledOn && now - lastBlink >= 10) {
        PORTB |= (1 << PB0);
        ledOn = true;
        lastBlink = now;
    }

    // Drain encoder delta — clamp to int8 range, drop excess (intentional)
    cli();
    int16_t delta = enc_delta;
    enc_delta = 0;
    sei();
    if (delta > 127)  delta = 127;
    if (delta < -128) delta = -128;

    static int32_t totalSteps = 0;
    totalSteps += delta;

    // Button
    bool btn = digitalRead(BUTTON_PIN);
    if (lastButton == HIGH && btn == LOW) {
        delay(DEBOUNCE_MS);
        btn = digitalRead(BUTTON_PIN);
    }

    uint8_t buttons = (btn == LOW) ? 0x01 : 0x00;
    bool btnChanged = (btn != lastButton);
    lastButton = btn;

    if (delta || btnChanged)
        send_report(buttons, (int8_t)delta);

    static uint32_t lastLcd = 0;
    if (now - lastLcd >= 200) {
        lastLcd = now;
        lcd.setCursor(0, 0);
        lcd.print("Steps:");
        lcd.print(totalSteps);
        lcd.print("    ");
    }


}
