/*  *********************************************
    CardioKit_R10.cpp
    Code to run the CardioKit R01 board
    Run DC ECG DAC DMA closed-loop control
        Filter/process buffers to enable control loop
    Run Differential ADC DMA interrupt-driven sampling
    Run DC ECG MUX synchronously with DAC / ADC
    Offload data with SimpleTCP ESP8266

    Added "NVIC_ENABLE_IRQ(IRQ_PDB);" to enable pcb_isr()

    Nathan Volman
    Created: March 5, 2020
        Tested to stream properly at 240MHz fCPU
        Changed fCPU from 180MHz (default) to 240MHz consuming an extra 20mA
        Changed fCPU back to 180MHz, didn't need extra speed, not worth extra noise
    Development Environment Specifics:
    Atom + PlatformIO

    Hardware Specifications:
    See schematics for ESP-12E
    and CardioKit R01
 *  *********************************************/
#include <Arduino.h>
#include <IntervalTimer.h>
#include <ADC.h>
#include <RingBufferDMA.h>
#include "SimpleTCP.h"
#include "hwsettings.h"
#include "CardioKitDac.h"
#include "qcepMux.h"
#include "CardioKitLEDS.h"
#include "CardioKitCommandSpace.h"
#include <SparkFun_ADXL345.h>
#include <math.h>

ADC *adc = new ADC();

const uint8_t adc_dma_buffer_size = 1; // 128 is hard max, aliases down for higher numbers
DMAMEM static volatile int16_t __attribute__((aligned(adc_dma_buffer_size + 0))) bufferAdc0[adc_dma_buffer_size]; // allocate ADC0 DMA buffer
DMAMEM static volatile int16_t __attribute__((aligned(adc_dma_buffer_size + 0))) bufferAdc1[adc_dma_buffer_size]; // allocate ADC1 DMA buffer
RingBufferDMA *dmaBuffer0 = new RingBufferDMA(bufferAdc0, adc_dma_buffer_size, ADC_0); // use dma with ADC0
RingBufferDMA *dmaBuffer1 = new RingBufferDMA(bufferAdc1, adc_dma_buffer_size, ADC_1); // use dma with ADC1

//////////////////////////////////////////////
///////// INITIALIZE STATE VARIABLES /////////
//////////////////////////////////////////////
volatile static uint16_t samples[PINGPONG_BUFFER_COUNT][NUM_DATA_STREAMS][CHANNEL_BUFFER_LENGTH] = {0}; // Ping-pong Buffer 1 and 2
volatile static uint8_t  current_channel               =  0;     // Which ECG Channel is the ADC currently sampling
volatile static uint16_t samples_idx[NUM_DATA_STREAMS] = {0};    // For each Channel, what is the index into 'samples[CURR_BUF][CH_N][]'
volatile static uint8_t  buffer_num                    =  0;     // Which Ping-Pong Buffer is currently being used
volatile static uint8_t  buffer_num_pcg                =  0;     // Which Ping-Pong Buffer is currently being used by pcg
volatile static bool     buffer_ready_flag             =  false; // Signals that one of the Ping-Pong buffers is ready for processing
volatile static bool     pcg_buffer_ready_flag         =  false; // Signals that one of the Ping-Pong buffers is ready for processing
volatile static bool     accel_read_sample_flag        =  false; // Signals that the accelerometer should take another reading


//////////////////////////////////////////////
////////// ACCELEROMETER VARIABLES ///////////
//////////////////////////////////////////////
int16_t ACCELx,ACCELy,ACCELz;
ADXL345 adxl = ADXL345(pinADXL_CS); // SPI, ADXL345(CS_PIN);

// returns 0 to 359 degrees with 0 degrees being towards left shoulder, 90 towards feet, 270 towards head
FASTRUN uint16_t GetAxisAngle(int x, int y)
{
    double theta = double(y)/double(x);
    theta = atan(theta) * 180.0/ 3.14159265;
    theta = abs(theta);

    if( x >= 0 && y >= 0)
    { // both +ve
        theta = 180 + theta;
    }
    else if( x >= 0 && y <= 0)
    { // x +ve, y -ve
        theta = 180 - theta;
    }
    else if( x <= 0 && y <= 0)
    { // both -ve
        //theta = theta;
    }
    else
    { // x -ve, y +ve
        theta = 360 - theta;
    }
    uint16_t angle = (uint16_t) (theta);
    return (angle % 360);
}

FASTRUN void ReadAccelIntoArray()
{
    adxl.readAccel(&ACCELx, &ACCELy, &ACCELz);
    samples[buffer_num][ACCEL_STREAM_SLOT][samples_idx[ACCEL_STREAM_SLOT]] = GetAxisAngle(ACCELx,ACCELy);
    samples_idx[ACCEL_STREAM_SLOT] = (samples_idx[ACCEL_STREAM_SLOT] + 1) % CHANNEL_BUFFER_LENGTH; // Move samples buffer indices
}

/********************* ISR *********************/
FASTRUN void ADXL_ISR() {
    Serial.println("ADXL_ISR");
  // getInterruptSource clears all triggered actions after returning value
  // Do not call again until you need to recheck for triggered actions
  byte interrupts = adxl.getInterruptSource();

  // Free Fall Detection
  if(adxl.triggered(interrupts, ADXL345_FREE_FALL)){
    Serial.println("*** FREE FALL ***");
    //add code here to do when free fall is sensed
  }

  // Inactivity
  if(adxl.triggered(interrupts, ADXL345_INACTIVITY)){
    Serial.println("*** INACTIVITY ***");
     //add code here to do when inactivity is sensed
  }

  // Activity
  if(adxl.triggered(interrupts, ADXL345_ACTIVITY)){
    Serial.println("*** ACTIVITY ***");
     //add code here to do when activity is sensed
  }

  // Double Tap Detection
  if(adxl.triggered(interrupts, ADXL345_DOUBLE_TAP)){
    Serial.println("*** DOUBLE TAP ***");
     //add code here to do when a 2X tap is sensed
  }

  // Tap Detection
  if(adxl.triggered(interrupts, ADXL345_SINGLE_TAP)){
    Serial.println("*** TAP ***");
     //add code here to do when a tap is sensed
  }
}

FASTRUN void ADXL_ISR2() {}

void InitADXL()
{
    pinMode(pinADXL_INT1, INPUT); // Setup interrupt pins for ADXL
    pinMode(pinADXL_INT2, INPUT); // Setup interrupt pins for ADXL

    adxl.powerOn();                     // Power on the ADXL345

    adxl.setRangeSetting(2);            // Give the range settings
                                      // Accepted values are 2g, 4g, 8g or 16g
                                      // Higher Values = Wider Measurement Range
                                      // Lower Values = Greater Sensitivity (false, sensitivity is fixed at full resolution)

    adxl.setSpiBit(0);                  // Configure the device to be in 4 wire SPI mode when set to '0' or 3 wire SPI mode when set to 1
                                      // Default: Set to 1
                                      // SPI pins on the ATMega328: 11, 12 and 13 as reference in SPI Library

    adxl.setActivityXYZ(1, 0, 0);       // Set to activate movement detection in the axes "adxl.setActivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
    adxl.setActivityThreshold(75);      // 62.5mg per increment   // Set activity   // Inactivity thresholds (0-255)

    adxl.setInactivityXYZ(1, 0, 0);     // Set to detect inactivity in all the axes "adxl.setInactivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
    adxl.setInactivityThreshold(75);    // 62.5mg per increment   // Set inactivity // Inactivity thresholds (0-255)
    adxl.setTimeInactivity(10);         // How many seconds of no activity is inactive?

    adxl.setTapDetectionOnXYZ(0, 0, 1); // Detect taps in the directions turned ON "adxl.setTapDetectionOnX(X, Y, Z);" (1 == ON, 0 == OFF)

    // Set values for what is considered a TAP and what is a DOUBLE TAP (0-255)
    adxl.setTapThreshold(50);           // 62.5 mg per increment
    adxl.setTapDuration(15);            // 625 μs per increment
    adxl.setDoubleTapLatency(80);       // 1.25 ms per increment
    adxl.setDoubleTapWindow(200);       // 1.25 ms per increment

    // Set values for what is considered FREE FALL (0-255)
    adxl.setFreeFallThreshold(7);       // (5 - 9) recommended - 62.5mg per increment
    adxl.setFreeFallDuration(30);       // (20 - 70) recommended - 5ms per increment

    // Setting all interupts to take place on INT1 pin
    //adxl.setImportantInterruptMapping(1, 1, 1, 1, 1);     // Sets "adxl.setEveryInterruptMapping(single tap, double tap, free fall, activity, inactivity);"
                                                        // Accepts only 1 or 2 values for pins INT1 and INT2. This chooses the pin on the ADXL345 to use for Interrupts.
                                                        // This library may have a problem using INT2 pin. Default to INT1 pin.

    // Turn on Interrupts for each mode (1 == ON, 0 == OFF)
    adxl.InactivityINT(0);
    adxl.ActivityINT(0);
    adxl.FreeFallINT(0);
    adxl.doubleTapINT(1);
    adxl.singleTapINT(0);
    adxl.watermarkINT(0); // enable watermark interrupt
    adxl.setInterruptMapping(ADXL345_INT_WATERMARK_BIT, ADXL345_INT2_PIN);

    //attachInterrupt(digitalPinToInterrupt(pinADXL_INT2), ADXL_ISR2, RISING);   // Attach Interrupt
    attachInterrupt(digitalPinToInterrupt(pinADXL_INT1), ADXL_ISR, RISING);   // Attach Interrupt
    // put device in FIFO mode, attach an interrupt to watermark bit to service FIFO
    // put FIFO servicing code into ISR
    //adxl.setFifoMode(ADXL345_FIFO_FIFO, ADXL_FIFO_THR, ADXL345_FIFO_INT2);
    //adxl.setRate(3200);//6.25);
    adxl.setFullResBit(true); // seems not to do anything in 2G mode which makes sense given datasheet
    delay(10);
    Serial.print("Rate: ");
    Serial.println(adxl.getRate());
    Serial.print("FIFOMODE: ");
    Serial.println(adxl.getFifoMode(),HEX);
    Serial.print("LOW POWER: ");
    if(adxl.isLowPower())
        Serial.println("ON");
    else
        Serial.println("OFF");
    delay(10);
    if(adxl.isInterruptEnabled(ADXL345_INT_WATERMARK_BIT)){
        Serial.println("WATERMARK ENABLED");
    } else {
        Serial.println("WATERMARK DISABLED");
    }
    Serial.print("INT SOURCE: ");
    Serial.println(adxl.getInterruptSource(),HEX);
    Serial.print("INT MAPPING: ");
    Serial.println(adxl.getInterruptMapping(ADXL345_INT_WATERMARK_BIT),HEX);
    //adxl345 is by default in full power mode,
    //watermark interrupt is enabled on INT2

    // read the ADXL345 FIFO until it's empty to prime the loop. Otherwise doesn't work
    // due to rising edge Interrupt race condition
    int16_t x_arr[32];
    int16_t y_arr[32];
    int16_t z_arr[32];

    for(int i = 0; i < 32; i++){
        adxl.readAccel(&x_arr[0], &y_arr[0], &z_arr[0]);
    }
}

//////////////////////////////////////////////
///////// INTERRUPT SERVICE ROUTINES /////////
//////////////////////////////////////////////
FASTRUN void adc0_isr()
{ // this triggers when conversion is complete
    adc->adc0->readSingle(); // readSingle clears interrupt
}

FASTRUN void adc1_isr()
{ // this triggers when conversion is complete on ADC1
    adc->adc1->readSingle(); // clear interrupt
}

FASTRUN void pdb_isr()
{   // pdb interrupt is enabled in case you need it.
    PDB0_SC &= ~PDB_SC_PDBIF; // clears interrupt
}

FASTRUN void dmaBuffer0_isr()
{
    if(current_channel == 0)
    {
        accel_read_sample_flag = true;
    }
    volatile uint16_t adc_sample_in = (uint16_t) dmaBuffer0->buffer()[0];
    samples[buffer_num][current_channel][samples_idx[current_channel]] = adc_sample_in;// Store most recent ADC val into Ping-Pong Buffer
    HandleNewEcgSampleDac(current_channel, adc_sample_in);
    samples_idx[current_channel] = (samples_idx[current_channel] + 1) % CHANNEL_BUFFER_LENGTH; // Move samples buffer indices

    // If an entire buffer has just filled up then switch buffers and set the flag to process the full buffer
    if ( (current_channel == (NUM_ECG_CHANNELS - 1)) && (samples_idx[current_channel] == 0))
    {
        buffer_num = (buffer_num == 1) ? 0 : 1; // Switch buffer_num between 0 and 1
        if(buffer_ready_flag){ Serial.println("BUFFER OVERRUN!"); }
        buffer_ready_flag = true;
    }
    current_channel = (current_channel + 1) % NUM_ECG_CHANNELS;
    switchNextMuxChannel(current_channel); // this used to be at the end of dmaBuffer0_isr but don't know why it wasnt earlier
    dacWriteNextChannel(current_channel); // Write the drive value for the next channel to DAC0
    dmaBuffer0->dmaChannel->clearInterrupt(); // Update the internal buffer positions
}

volatile static uint8_t  downsamplingCounter =  0; // only save every NUM_ECG_CHANNELS'th sample
volatile static uint32_t downsampleAverage = 0;
FASTRUN void dmaBuffer1_isr()
{ // ISR for reading DMA from ADC1
    downsampleAverage += (uint16_t) dmaBuffer1->buffer()[0];
    if( downsamplingCounter == 0 )
    {
        samples[buffer_num_pcg][PCG_STREAM_SLOT][samples_idx[PCG_STREAM_SLOT]] = downsampleAverage/NUM_ECG_CHANNELS; // Store most recent ADC val into Ping-Pong Buffer
        downsampleAverage = 0;
        samples_idx[PCG_STREAM_SLOT] = (samples_idx[PCG_STREAM_SLOT] + 1) % CHANNEL_BUFFER_LENGTH;                   // Move samples buffer indices
        // If an entire buffer has just filled up then switch buffers and set the flag to process the full buffer
        if( samples_idx[PCG_STREAM_SLOT] == 0 )
        {
            buffer_num_pcg = (buffer_num_pcg == 1) ? 0 : 1; // Switch buffer_num_pcg between 0 and 1
            //if(pcg_buffer_ready_flag){ Serial.println("BUFFER OVERRUN! PCG"); }
            pcg_buffer_ready_flag = true;
        }
    }
    downsamplingCounter = (downsamplingCounter + 1) % NUM_ECG_CHANNELS;
    dmaBuffer1->dmaChannel->clearInterrupt(); // Update the internal buffer positions
}

FASTRUN void batteryLow_isr()
{
    // This is triggered when the VSYS voltage goes under 3.5V, the system should not be used under this voltage and must be charged
    ControlCkLed(CKLED_STATUS, !digitalRead(pinBAT_LO));
}

SimpleTCP stcp;
IntervalTimer tcpTimer;

void setup()
{
    Serial.begin(2000000);
    Serial4.begin(460800);

    InitADXL();

    stcp = SimpleTCP();
    tcpTimer.begin(SimpleTCP::SetTxReadyFlag, SimpleTCP::GetInterBufferTimeMicros());
    tcpTimer.priority(255); // lowest priority

    InitializeCkLeds();
    ControlCkLed(CKLED_STATUS, HIGH);
    pinMode(pinBAT_LO, INPUT);
    attachInterrupt(digitalPinToInterrupt(pinBAT_LO), batteryLow_isr, CHANGE); // Handle Vbat Low shutdown

    // ECG-Related
    pinMode(pinESP_OFF, OUTPUT);
    delay(5);
    digitalWriteFast(pinESP_OFF, LOW); // Enable ESP12

    pinMode(pinADC_ECG, INPUT); // ECG ADC input pin

    initializeMux(); // Enable MUXs and set to initial channel
    initializeDac(); // Enable DAC0 pin and set resolution to 12-bits

    // Setup ADC0 with PDB (Programmable Delay Block) and DMA (Direct Memory Access)
    //adc->setReference(ADC_REFERENCE::REF_EXT, ADC_0); // This dramatically increases noise, do not do it, leave on REF_3V3
    adc->setAveraging(ADC_AVERAGING); // set number of averages
    adc->setResolution(ADC_RESOLUTION); // set bits of resolution
    adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS);//ADC_CONVERSION_SPEED::HIGH_SPEED_16BITS);
    adc->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);//ADC_SAMPLING_SPEED::HIGH_SPEED);
    adc->enableDMA(ADC_0); // Enable DMA on ADC0
    dmaBuffer0->start(&dmaBuffer0_isr);
    adc->adc0->stopPDB(); // Call stop to ensure known stopped state
    adc->adc0->startSingleRead(pinADC_ECG); // get 16-bit single-ended values 0x0 [0V] to 0xFFFF [3.3V]
        //adc->startSingleDifferential(pinADC_ECG, pinADC_n); // Call this to setup everything before the pdb starts
    adc->enableInterrupts(ADC_0);         // As in adc_pdb example
    adc->adc0->startPDB(ADC_PDB_FREQ_HZ); // As in adc_pdb example
    //NVIC_ENABLE_IRQ(IRQ_PDB); // Enables pdb_isr, without this it doesn't get called, other effects unknown

    // Setup ADC1 with PDB (Programmable Delay Block) and DMA (Direct Memory Access)
    adc->setAveraging(ADC_AVERAGING, ADC_1); // set number of averages
    adc->setResolution(ADC_RESOLUTION, ADC_1); // set bits of resolution
    adc->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED, ADC_1); // change the conversion speed
    adc->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED, ADC_1); // change the sampling speed
    adc->enableDMA(ADC_1); // Enable DMA on ADC1
    dmaBuffer1->start(&dmaBuffer1_isr);
    adc->adc1->stopPDB();
    adc->adc1->startSingleRead(pinADC_PCG); // call this to setup everything before the pdb starts
    adc->enableInterrupts(ADC_1);
    adc->adc1->startPDB(ADC_PDB_FREQ_HZ);

    adc->printError(); // Print errors, if any.
    adc->resetError(); // Print errors, if any.

    //*****************************//
    // SET DAC REFERENCE TO 1.2V Internal
    //DAC0_C0 &= ~DAC_C0_DACRFS; // 1.2V
	//DAC1_C0 &= ~DAC_C0_DACRFS;

    // wait until Start Ack is received to send data
    // Start Ack also synchronizes world time to internal micros() counter
    // via stcp.microsecondOffset
    stcp.HandleFindingStartAck();
    // at this point, the start Ack has been received
    // and stcp.microsecondOffset is established
    // proceed with data transfer
    //Serial.println("Code Proceeding to TX");
}

void loop()
{
    if(ACCEL_PRESENT && accel_read_sample_flag) {
        ReadAccelIntoArray();
        accel_read_sample_flag = false;
    }

    if(buffer_ready_flag && pcg_buffer_ready_flag)
    { // A buffer is ready to transmit, add it to the outbound queue
        buffer_ready_flag = false; // clear the flag
        pcg_buffer_ready_flag = false; // clear the flag

        // samples stores 16-bit values, send it a twice-as-long 8-bit buffer
        // function can only handle sending max 718 bytes at a time currently
        stcp.HandleSendingSamplesTimer((uint8_t*) &samples[(buffer_num + 1) % 2][0][0], CHANNEL_BUFFER_LENGTH * NUM_DATA_STREAMS * 2);
    }

    // Check and handle an incoming command from cloud host
    uint32_t cmd_in = stcp.ReadAlternateCommand();
    if(cmd_in != 0)
    {
        stcp.ClearAlternateCommand();
        HandleCloudCommand(cmd_in);
    }

    stcp.HandleNacks(); // process incoming nacks
    stcp.Transmit(); // try to send data out via stcp
    stcp.EraseOldOutputBuffers(); // clean up output buffers that are stale
}
