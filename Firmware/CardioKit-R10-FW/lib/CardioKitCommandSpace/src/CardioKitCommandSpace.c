/*
    CardioKitCommandSpace.c
    Library for Handling User-Defined Commands from Cloud Host
    Created by Nathan Volman, Feb 24, 2020
*/
#include "CardioKitCommandSpace.h"
#include <Arduino.h>
#include "CardioKitLEDS.h"

typedef enum
{
    NO_OP = 0x00,
    CKCMD_LED_ON = 0x01,
    CKCMD_LED_OFF = 0x02,
    CKCMD_LED_FLASH = 0x03
} HostCommand_t;

void HandleCloudCommand(uint32_t cmd)
{
    uint32_t opcode = (cmd >> 16) & 0xFF;
    switch(opcode)
    {
        case NO_OP:
            // this should never be reached
            break;
        case CKCMD_LED_ON:
            ControlCkLed(CKLED_ALL, HIGH);
            break;
        case CKCMD_LED_OFF:
            ControlCkLed(CKLED_ALL, LOW);
            break;
        default:
            break;
    }
}
