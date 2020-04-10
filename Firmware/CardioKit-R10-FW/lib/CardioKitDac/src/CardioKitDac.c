/*  *********************************************
    CardioKitDac.c
    Closed Loop Control for DC ECG DAC

    Nathan Volman
    Created: Mar 7, 2019

    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for DC ECG RCEP R0.0
    and QuickCEP Prototype Rev.1
 *  *********************************************/
#include "CardioKitDac.h"
#include <Arduino.h>

#define MIN_DAC_VAL 0
#define MAX_DAC_VAL 4095
#define INITIAL_DAC_VAL 2048
#define SETTLED_RANGE_UPPER_BOUND 65000
#define SETTLED_RANGE_LOWER_BOUND 500
#define SLEW_END_RANGE_UPPER_BOUND 40000
#define SLEW_END_RANGE_LOWER_BOUND 26000
#define SETTLED_MIDRANGE 32768
#define MAX_SLEW_ITERATIONS 11
static uint8_t IsFastSlewing[NUM_ECG_CHANNELS] = {true};
static int8_t FastSlewIteration[NUM_ECG_CHANNELS] = {-1};
static uint8_t OverridingDacValues[NUM_ECG_CHANNELS] = {false};
static uint16_t SlewStepSize[MAX_SLEW_ITERATIONS] = {1024,512,256,128,64,32,16,8,4,2,1};

static uint16_t dacValues[NUM_ECG_CHANNELS] = {INITIAL_DAC_VAL};

uint16_t overrideDacValue(uint8_t channel_number, uint16_t new_dac_value)
{
    OverridingDacValues[channel_number] = true;
    dacValues[channel_number] = (uint16_t) new_dac_value;
    return dacValues[channel_number];
}

void disableDacValueOverride(uint8_t channel_number)
{
    OverridingDacValues[channel_number] = false;
}

void initializeDac()
{
    pinMode(pinDAC0, OUTPUT);
    analogWriteResolution(12); // 12-bit DAC mode (Does this set resolution for DAC0 and DAC1?)
    for(int i = 0; i < NUM_ECG_CHANNELS; i++)
    {
        dacValues[i] = INITIAL_DAC_VAL;
    }
}

void dacWriteNextChannel(uint8_t channel)
{
    analogWrite(pinDAC0, dacValues[channel]);
}

// helper function to determine if a sample is within the "settled" range
uint8_t SampleOutOfRange(uint16_t sample)
{
    if(sample >= SETTLED_RANGE_UPPER_BOUND) { return true;}
    if(sample <= SETTLED_RANGE_LOWER_BOUND) { return true;}
    return false;
}

// helper function to determine if a sample is within the "settled" range
uint8_t SampleOutOfSlewDoneRange(uint16_t sample)
{
    if(sample >= SLEW_END_RANGE_UPPER_BOUND) { return true;}
    if(sample <= SLEW_END_RANGE_LOWER_BOUND) { return true;}
    return false;
}

// helper function to restart fast slewing and bring channel back to original state
void ResetChannel(uint8_t channel)
{
    IsFastSlewing[channel] = true;
    dacValues[channel] = INITIAL_DAC_VAL;
    FastSlewIteration[channel] = -1;
}

// Pass in a new sample for the given channel for handling slewing
// This should be called before the next time the channel needs to be written
void HandleNewEcgSampleDac(uint8_t channel, uint16_t sample)
{
    if(IsFastSlewing[channel])
    {
        FastSlewIteration[channel]++;
        if(FastSlewIteration[channel] >= MAX_SLEW_ITERATIONS) // check this for off by one
        {
            if(SampleOutOfSlewDoneRange(sample))
            {
                ResetChannel(channel); // This is where the looping happens
            } else {
                IsFastSlewing[channel] = false;
                FastSlewIteration[channel] = -1;
            }
        }
        else if(SampleOutOfSlewDoneRange(sample))
        {
            if(sample >= SETTLED_MIDRANGE)
            {
                dacValues[channel] -= SlewStepSize[FastSlewIteration[channel]];
            } else {
                dacValues[channel] += SlewStepSize[FastSlewIteration[channel]];
            }
        } else {
            IsFastSlewing[channel] = false;
            FastSlewIteration[channel] = -1;
        }
    }
    else if(SampleOutOfRange(sample))
    {
        ResetChannel(channel);
    }
}
