/*  *********************************************
    CardioKitDac.h
    Closed Loop Control for DC ECG DAC

    Nathan Volman
    Created: Feb 12, 2020
    Branched from qcepDac.h
    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for CardioKit R01
 *  *********************************************/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef CARDIOKIT_DAC_H
#define CARDIOKIT_DAC_H
#include <WProgram.h>
#include "hwsettings.h"

// Enable DAC0 pin and set resolution to 12-bits
void initializeDac();

// Write the drive value for the next channel to DAC0
void dacWriteNextChannel(uint8_t channel);

// Pass in a new sample for the given channel for handling slewing
void HandleNewEcgSampleDac(uint8_t channel, uint16_t sample);

// Pass a sample buffer into this function every time it's available for the DAC to
// update it's drive values for each channel
void updateDacControlValues(uint16_t * adc_samples, uint32_t num_samples_per_channel);
uint16_t updateDacControlValues2(uint16_t * adc_samples, uint32_t num_samples_per_channel);
uint16_t updateDacControlValuesFastSlew(uint16_t * adc_samples, uint32_t num_samples_per_channel);

uint16_t getDacValue(uint8_t channel_number);

// Return the current DAC Channel
uint8_t getCurrentDacChannel();

// Get current DAC Control value for given channel
uint16_t getDacControlValue(uint8_t channel);

// Use this to test the DAC and force a certain DAC value
uint16_t overrideDacValue(uint8_t channel_number, uint16_t new_dac_value);

// Re-enable slewing servo algorithm
void disableDacValueOverride(uint8_t channel_number);

#endif //CARDIOKIT_DAC_H
#ifdef __cplusplus
}
#endif
