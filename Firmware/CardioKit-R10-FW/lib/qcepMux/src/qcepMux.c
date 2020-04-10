/*  *********************************************
    qcep_mux.c
    Fixed HW Definitions and functions for the QuickCEP
    DC ECG MUX

    Nathan Volman
    Created: Nov 30, 2018

    Development Environment Specifics:
    None

    Hardware Specifications:
    See schematics for DC ECG RCEP R0.0
    and QuickCEP Prototype Rev.1
 *  *********************************************/
#include "qcepMux.h"
#include "hwsettings.h"

typedef enum
{
    U6_LEAD_THB = 0,
    U6_LEAD_1   = 1,
    U6_LEAD_2   = 2,
    U6_LEAD_3   = 3,
    U6_LEAD_4   = 4,
    U6_LEAD_5   = 5,
    U6_LEAD_6   = 6,
    U6_LEAD_7   = 7,

    U7_LEAD_0 = 0,
    U7_LEAD_1 = 1,
    U7_LEAD_2 = 2,
    U7_LEAD_3 = 3,
    U7_LEAD_4 = 4,
    U7_LEAD_5 = 5,
    U7_LEAD_6 = 6,
    U7_LEAD_7 = 7
} MUX_LEADS;

typedef enum
{
    U6 = 0,
    U7 = 1
} MUX;

static uint8_t ChannelTableIndex = 0;
// Stores the lead pairs as channels
// Can access ChannelTable[channelNum][U6=0/U7=1]
// "U6" entries are - and "U7" are + with regards to vector direction of signal
// due to signal inversion in stage 2
// Signals have vector U6 -> U7
static MUX_LEADS ChannelTable[8][2] =
{ // Both Bit 0's on the muxs cross all leads
    {U6_LEAD_THB,U7_LEAD_2},//{U6_LEAD_THB,U7_LEAD_2},
    {U6_LEAD_4,U7_LEAD_0},//{U6_LEAD_4,U7_LEAD_0},
    {U6_LEAD_5,U7_LEAD_1},//{U6_LEAD_5,U7_LEAD_1},
    {U6_LEAD_6,U7_LEAD_2},//{U6_LEAD_6,U7_LEAD_2},
    {U6_LEAD_7,U7_LEAD_3},//{U6_LEAD_7,U7_LEAD_3},
    {U6_LEAD_1,U7_LEAD_5},
    {U6_LEAD_1,U7_LEAD_6},
    {U6_LEAD_1,U7_LEAD_7},
};

uint32_t SetMuxLead(MUX mux, MUX_LEADS lead)
{
    if(mux == U6)
    {
        digitalWriteFast(pinMUX1_A0, lead & 0x1);
        digitalWriteFast(pinMUX1_A1, lead & 0x2);
        digitalWriteFast(pinMUX1_A2, lead & 0x4);
    } else {
        digitalWriteFast(pinMUX0_A0, lead & 0x1); // pins 0 are U4 on the right
        digitalWriteFast(pinMUX0_A1, lead & 0x2);
        digitalWriteFast(pinMUX0_A2, lead & 0x4);
    }
    return lead;
}

void initializeMux()
{
    pinMode(pinMUX0_A0 , OUTPUT);
    pinMode(pinMUX0_A1 , OUTPUT);
    pinMode(pinMUX0_A2 , OUTPUT);
    pinMode(pinMUX1_A0 , OUTPUT);
    pinMode(pinMUX1_A1 , OUTPUT);
    pinMode(pinMUX1_A2 , OUTPUT);

    SetMuxLead(U6, ChannelTable[ChannelTableIndex][U6]);
    SetMuxLead(U7, ChannelTable[ChannelTableIndex][U7]);
}

void switchNextMuxChannel(uint8_t channel)
{ // Switch MUXs to next channel
    ChannelTableIndex = (ChannelTableIndex + 1) % (NUM_ECG_CHANNELS);
    SetMuxLead(U6, ChannelTable[channel][U6]);
    SetMuxLead(U7, ChannelTable[channel][U7]);
}

// Read the current Channel
uint8_t GetCurrentMuxChannel()
{
    return ChannelTableIndex;
}
