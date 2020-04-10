/*  *********************************************
    CardioKitLEDS.h
    Helper functions to control LEDs on CardioKit R01

    Nathan Volman
    Created: Feb 16, 2020
    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for CardioKit R01
 *  *********************************************/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef CARDIOKIT_LEDS_H
#define CARDIOKIT_LEDS_H
#include <WProgram.h>
#include "hwsettings.h"

typedef enum
{
    CKLED_NORTH,
    CKLED_EAST,
    CKLED_SOUTH,
    CKLED_WEST,
    CKLED_STATUS,
    CKLED_ALL,
    CKLED_ALL_NO_STAT
} CKLED_t;

// call this in setup to initialize CardioKit LEDs
void InitializeCkLeds();

// control a CardioKit LED
void ControlCkLed(CKLED_t led, int HIGH_OR_LOW);

#endif //CARDIOKIT_LEDS_H
#ifdef __cplusplus
}
#endif
