/*  *********************************************
    qcep_mux.h
    Fixed HW Definitions and functions for the QuickCEP
    DC ECG MUX

    Nathan Volman
    Created: Nov 30, 2018
    Updated: Mar 7, 2019

    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for DC ECG RCEP R0.0
    and QuickCEP Prototype Rev.1
 *  *********************************************/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef QCEP_MUX_H
#define QCEP_MUX_H
#include <WProgram.h>

// Setup pins, enable MUXs, choose initial channel
void initializeMux();

// Switch MUXs to next channel
void switchNextMuxChannel(uint8_t channel);

// Read the current Channel
uint8_t GetCurrentMuxChannel();

#endif //QCEP_MUX_H
#ifdef __cplusplus
}
#endif
