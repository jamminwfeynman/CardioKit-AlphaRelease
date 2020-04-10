/*  *********************************************
    hwSettings.h
    Fixed HW Definitions for CardioKit R01

    Nathan Volman
    Created: Feb 12, 2020

    Development Environment Specifics:
    None

    Hardware Specifications:
    See schematics for CardioKit R01
 *  *********************************************/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef HW_SETTINGS_H
#define HW_SETTINGS_H

/*  *********************************************
    PINS OF INTEREST
    Teensy Pin ----------- Function
    1                  | MUX0_A0 (U4 is MUX0)
    2                  | MUX0_A1
    3                  | MUX0_A2
    4                  | MUX1_A0
    5                  | MUX1_A1
    6                  | MUX1_A2
    0                  | ESP_PWRON
    35                 | Status LED
    22                 | CM_ADC
    DAC0               | OFFSET_CANCEL
    DAC1               | RLD DRIVE (Optional)
    A10                | OUT_POS
    A11                | OUT_REF
 *  *********************************************/
//#define pinMUX_EN     0 // There is no MUX_EN anymore
#define pinMUX0_A0    1
#define pinMUX0_A1    2
#define pinMUX0_A2    3
#define pinMUX1_A0    4
#define pinMUX1_A1    5
#define pinMUX1_A2    6
#define pinESP_OFF    0
#define pin3V3_AUX_EN 53
#define pinSTATLED    40

#define pinADXL_CS    30
#define pinADXL_INT1  33
#define pinADXL_INT2  41
#define ADXL_FIFO_THR 31 // Must be less than 32

#define pinBAT_LO     18

#define pinDAC0      A21
#define pinDAC1      A22
#define pinADC_ECG   A10
#define pinADC_PCG   A11

#define pinLED_N     22
#define pinLED_E     15
#define pinLED_S     23
#define pinLED_W     9

#define pinVIBM_EN   8
#define pinVIBM_PWM  7

#define NUM_ECG_CHANNELS   (5)    // Number of MUX Channels on DC ECG
#define PCG_PRESENT        (1)    // 1 if PCG Stream present, 0 else
#define ACCEL_PRESENT      (1)    // 1 if ACCEL Stream present, 0 else
#define NUM_DATA_STREAMS   ((NUM_ECG_CHANNELS) + (PCG_PRESENT) + (ACCEL_PRESENT))
#define PCG_STREAM_SLOT    (NUM_ECG_CHANNELS)    // buffer[PCG_STREAM_SLOT],   ECG 0-3 go into slots 0,1,2,3
#define ACCEL_STREAM_SLOT  ((NUM_ECG_CHANNELS) + (PCG_PRESENT))    // buffer[ACCEL_STREAM_SLOT],   ECG 0-3 go into slots 0,1,2,3

#define ADC_RESOLUTION  (16)
#define ADC_AVERAGING   (32)  // Can be 0, 4, 8, 16 or 32.
#define CORE_SAMPLE_FREQ 400
#define ADC_PDB_FREQ_HZ  ((CORE_SAMPLE_FREQ) * (NUM_ECG_CHANNELS)) //NUM_ECG_CHANNELS * 800 per channel   // 6000 just barely works over SimpleTCP, total sample rate for all ECG channels combined

#define CHANNEL_BUFFER_LENGTH ((359)/(NUM_DATA_STREAMS)) //(89//179//359 // (718/2) because of 16 bit samples
#define PINGPONG_BUFFER_COUNT 2

#endif //HW_SETTINGS_H
#ifdef __cplusplus
}
#endif
