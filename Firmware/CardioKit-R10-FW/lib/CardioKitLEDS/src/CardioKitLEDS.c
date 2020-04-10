/*  *********************************************
    CardioKitLEDS.c
    Helper functions to control LEDs on CardioKit R01

    Nathan Volman
    Created: Feb 16, 2020
    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for CardioKit R01
 *  *********************************************/
#include "CardioKitLEDS.h"
#include <Arduino.h>

#define LED_DELAY_TIME (250) // Delay time for startup led display

// call this in setup to initialize CardioKit LEDs
void InitializeCkLeds()
{
    // First enable 3V3_AUX rail
    pinMode(pin3V3_AUX_EN, OUTPUT);
    digitalWrite(pin3V3_AUX_EN, HIGH);

    pinMode(pinLED_N, OUTPUT);
    pinMode(pinLED_E, OUTPUT);
    pinMode(pinLED_S, OUTPUT);
    pinMode(pinLED_W, OUTPUT);
    pinMode(pinSTATLED, OUTPUT);

    digitalWriteFast(pinSTATLED, HIGH);
    delay(LED_DELAY_TIME);
    digitalWriteFast(pinLED_N, HIGH);
    delay(LED_DELAY_TIME);
    digitalWriteFast(pinLED_E, HIGH);
    delay(LED_DELAY_TIME);
    digitalWriteFast(pinLED_S, HIGH);
    delay(LED_DELAY_TIME);
    digitalWriteFast(pinLED_W, HIGH);
    delay(LED_DELAY_TIME);
    ControlCkLed(CKLED_ALL, LOW);
}

// control a CardioKit LED
void ControlCkLed(CKLED_t led, int HIGH_OR_LOW)
{
    switch (led)
    {
        case CKLED_NORTH:
            digitalWriteFast(pinLED_N, HIGH_OR_LOW);
            break;
        case CKLED_EAST:
            digitalWriteFast(pinLED_E, HIGH_OR_LOW);
            break;
        case CKLED_SOUTH:
            digitalWriteFast(pinLED_S, HIGH_OR_LOW);
            break;
        case CKLED_WEST:
            digitalWriteFast(pinLED_W, HIGH_OR_LOW);
            break;
        case CKLED_STATUS:
            digitalWriteFast(pinSTATLED, HIGH_OR_LOW);
            break;
        case CKLED_ALL:
            digitalWriteFast(pinLED_N,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_E,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_S,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_W,   HIGH_OR_LOW);
            digitalWriteFast(pinSTATLED, HIGH_OR_LOW);
            break;
        case CKLED_ALL_NO_STAT:
            digitalWriteFast(pinLED_N,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_E,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_S,   HIGH_OR_LOW);
            digitalWriteFast(pinLED_W,   HIGH_OR_LOW);
            break;
        default:
            break;
    }
}
