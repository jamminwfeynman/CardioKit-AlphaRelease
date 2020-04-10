/*
    SimpleTCP.cpp - Library for wrapping byte-wise data in packet and reliably
    transmitting it.
    Created by Nathan Volman, January 25, 2019
*/
#include "SimpleTCP.h"
#include <stdio.h>
#include <cstdlib>

// need this line as described here: stackoverflow.com/questions/9110487/undefined-reference-to-a-static-member
uint32_t SimpleTCP::interBufferTimeMicros = 58000;
volatile bool SimpleTCP::txReadyFlag = true;
bool SimpleTCP::usingTimer = true;
uint32_t SimpleTCP::packetHeaderSize = 12;
uint32_t SimpleTCP::microsToKeepPackets = 4000000; // keep packets for 4 seconds after transmission

const uint32_t txBufferSize = 730;
uint16_t txBufferLen = 718; // [This is ESP TX BUF Size - 12(HeaderSize)]/2 : github.com/jeelabs/esp-link/blob/master/serial/serbridge.h
uint8_t txBuffer[txBufferSize];

const uint32_t packetBufferSize = 730;
uint16_t packetBufferLen;
uint8_t packetBuffer[packetBufferSize];

const uint32_t ackCircBufferSize = 100;
uint8_t ackCircBuffer[ackCircBufferSize] = {0};
uint32_t acbHead           = 0;
uint32_t acbTail           = 0;
uint32_t validBytesCircBuf = 0;

const uint32_t outputPtrBufferSize = 100; // Sized to store 718*60 > 30kB ~ 3 seconds of full data rate sampled data
uint8_t * outputPtrBuffer[outputPtrBufferSize]; // Stores pointers to preallocated array of bytes to send out
uint16_t outputPtrLenBuffer[outputPtrBufferSize]; // for index i, holds length of bytes of array in outputPtrBuffer
uint32_t outputPtrTimeBuffer[outputPtrBufferSize]; // for index i, holds time in micros() of array in outputPtrBuffer
uint32_t outputPtrSequenceBuffer[outputPtrBufferSize]; // for index i, holds sequenceNumber of first byte of array in outputPtrBuffer
uint32_t outputPtrBufferHead = 0;
uint32_t outputPtrBufferTail = 0;

void PrintBufferState()
{
    Serial.println("Buffer State");
    for(uint32_t i=0;i<outputPtrBufferSize;i++)
    {
        Serial.print(outputPtrSequenceBuffer[i]);
        Serial.print("/");
        Serial.print(outputPtrLenBuffer[i]);
        Serial.print("/");
        Serial.print(outputPtrBufferHead);
        Serial.print("/");
        Serial.println(outputPtrBufferTail);
    }
}

// add an element to the output buffer at the back to be sent out last
void AddToOutputPtrBuffer(uint8_t * BufPtr, uint16_t len, uint32_t seqNum)
{
    if(((outputPtrBufferTail + 1) % outputPtrBufferSize) == outputPtrBufferHead)
    { // buffer Full
        Serial.println("Output Buffer Overflow!");
    } else {
        // buffer has space, add the element
        outputPtrBuffer[outputPtrBufferTail]         = BufPtr;
        outputPtrLenBuffer[outputPtrBufferTail]      = len;
        outputPtrSequenceBuffer[outputPtrBufferTail] = seqNum;
        outputPtrTimeBuffer[outputPtrBufferTail++]   = micros();
        outputPtrBufferTail %= outputPtrBufferSize;
    }
    //Serial.println("LowPri");
    //PrintBufferState();
}

// add an element to the output buffer at the front to be sent out first
// this does not currently work, overwrites the head and causes it to never be transmitted
void AddToOutputPtrBufferHighPriority(uint8_t * BufPtr, uint16_t len, uint32_t seqNum)
{
    //Serial.println("HighPriBefore");
    //PrintBufferState();
    if(((outputPtrBufferTail + 1) % outputPtrBufferSize) == outputPtrBufferHead)
    { // buffer Full
        Serial.println("Output Buffer Overflow!");
    } else {
        // buffer has space, add the element to the front of the circular buffer
        if(outputPtrBufferHead==outputPtrBufferTail)
        {
            outputPtrBuffer[outputPtrBufferTail]         = BufPtr;
            outputPtrLenBuffer[outputPtrBufferTail]      = len;
            outputPtrSequenceBuffer[outputPtrBufferTail] = seqNum;
            outputPtrTimeBuffer[outputPtrBufferTail++]   = micros();
            outputPtrBufferTail %= outputPtrBufferSize;
        } else {
            outputPtrBufferHead += outputPtrBufferSize - 1;
            outputPtrBufferHead %= outputPtrBufferSize;
            // first move the element to be overwritten
            uint32_t SwapIndex = outputPtrBufferHead + ((uint32_t)(outputPtrBufferSize/2));
            SwapIndex %= outputPtrBufferSize;

            outputPtrBuffer[SwapIndex]         = outputPtrBuffer[outputPtrBufferHead];
            outputPtrLenBuffer[SwapIndex]      = outputPtrLenBuffer[outputPtrBufferHead];
            outputPtrSequenceBuffer[SwapIndex] = outputPtrSequenceBuffer[outputPtrBufferHead];
            outputPtrTimeBuffer[SwapIndex]     = outputPtrTimeBuffer[outputPtrBufferHead];

            // now add the element to the front of the buffer
            outputPtrBuffer[outputPtrBufferHead]         = BufPtr;
            outputPtrLenBuffer[outputPtrBufferHead]      = len;
            outputPtrSequenceBuffer[outputPtrBufferHead] = seqNum;
            outputPtrTimeBuffer[outputPtrBufferHead]     = micros();
        }
    }
    //Serial.println("HighPriAfter");
    //PrintBufferState();
}

void flattenCircularBuffer(uint8_t * circ, uint32_t circHead, uint32_t bytesToCopy,
                           uint32_t bufSize, uint8_t * flattened)
{
    for(uint32_t i = 0; i < bytesToCopy; i++)
        flattened[i] = circ[((circHead + i) % bufSize)];
}

SimpleTCP::SimpleTCP()
{
    this->lastAckdByte      = 0;
    this->nextByteNum       = 0;
    this->microsecondOffset = 0;
    this->StartAckReceived  = false;
    this->lastBufSentMicros = 0;
    this->AltCommand        = 0;
    SimpleTCP::interBufferTimeMicros = 58000;
    SimpleTCP::usingTimer = true;
    /*
        With txBufferLen = 718:
        40000 gives 12.30 kBps with ~1 drop/second but successful retransmit
        35000 fails barely giving 11.82 kBps
        50000 gives 10.79 kBps with .5 drop/second but successful retransmit (drops spaced evenly in time)
        55000 gives 10.14 kBps with .25 drop/second but successful retransmit (all drops happen near start)
        56000 gives 9.81  kBps with 1 to 2 drops at start with successful retransmit
        56000 gives 10.14 kBps with occassional drops throughout over 4 minute run
        58000 gives 9.71  kBps with 1 drop at start with successful retransmit
        58000 gives 9.90  kBps with 3 drops in 4 minutes. Very stable
        60000 gives 9.64  kBps with 1 random drop somewhere in the middle, like not its fault
        61000 gives 9.45  kBps with 2 random drops somewhere in the middle, like not its fault
    */
};

SimpleTCP::SimpleTCP(bool usingIntervalTimer)
{
    this->lastAckdByte      = 0;
    this->nextByteNum       = 0;
    this->microsecondOffset = 0;
    this->StartAckReceived  = false;
    this->lastBufSentMicros = 0;
    this->AltCommand        = 0;
    SimpleTCP::interBufferTimeMicros = 58000;
    SimpleTCP::usingTimer = usingIntervalTimer;
    /*
        With txBufferLen = 718:
        40000 gives 12.30 kBps with ~1 drop/second but successful retransmit
        35000 fails barely giving 11.82 kBps
        50000 gives 10.79 kBps with .5 drop/second but successful retransmit (drops spaced evenly in time)
        55000 gives 10.14 kBps with .25 drop/second but successful retransmit (all drops happen near start)
        56000 gives 9.81  kBps with 1 to 2 drops at start with successful retransmit
        56000 gives 10.14 kBps with occassional drops throughout over 4 minute run
        58000 gives 9.71  kBps with 1 drop at start with successful retransmit
        58000 gives 9.90  kBps with 3 drops in 4 minutes. Very stable
        60000 gives 9.64  kBps with 1 random drop somewhere in the middle, like not its fault
        61000 gives 9.45  kBps with 2 random drops somewhere in the middle, like not its fault
    */
};

void SimpleTCP::SetTxReadyFlag()
{
    SimpleTCP::txReadyFlag = true;
}

uint32_t SimpleTCP::GetInterBufferTimeMicros()
{
    return SimpleTCP::interBufferTimeMicros;
}

uint32_t SimpleTCP::GetNextByteNum()
{
    return this->nextByteNum;
}

bool SimpleTCP::isNack(uint8_t * data)
{
    // check nack signifier
    if(data[0] != 0xFA)
    {
        return false;
    }
    // check checksum
    if( ((uint8_t)this->CalculateNackChecksum(data)) != ((uint8_t)data[7]))
    {
        return false;
    }
    return true;
}

bool SimpleTCP::isNackSignifier(uint8_t data)
{
    if(data != 0xFA)
    {
        return false;
    }
    return true;
}

bool SimpleTCP::isNackResetCommand(uint32_t NackSequenceNumber)
{
    // check nack sequenceNumber for RESET signal
    if(NackSequenceNumber == 0xFFFFDEAD)
    {
        return true;
    }
    return false;
}

// 0xFE in the high 8 bits of NackSequenceNumber signals ALT command space
bool SimpleTCP::isNackAlternateCommand(uint32_t NackSequenceNumber)
{
    uint8_t NackSignal = (uint8_t)((NackSequenceNumber >> 24) & 0xFF);
    // check nack sequenceNumber for ALT signal
    if( NackSignal == 0xFE)
    {
        return true;
    }
    return false;
}

// SimpleTCP allows for 24 bits of ALT command space in the low 24 bits of NackSequenceNumber
// Return 0 : No Command Waiting
// Else: User can define list of commands as needed with 24 bits of space
uint32_t SimpleTCP::ReadAlternateCommand()
{
    return (uint32_t) this->AltCommand;
}

// Call this in loop after ReadNackAlternateCommand to acknowledge command execution
// TODO: Handle command incoming while outstanding command present
void SimpleTCP::ClearAlternateCommand()
{
    this->AltCommand = 0;
}

// Call this to store the Alternate Command to be read by user
void SimpleTCP::FlagNackAlternateCommand(uint32_t NackSequenceNumber)
{
    this->AltCommand = (NackSequenceNumber & 0x00FFFFFF);
}

#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);

void SimpleTCP::ResetTeensy()
{
    Serial.println("NACK RESET RECEIVED");
    CPU_RESTART;
}

void SimpleTCP::ParseNack(uint8_t * data, uint32_t txBufLen, bool toPrint)
{
    this->nack.nackSignifier   = (byte)(data[0] & 0xFF);
    this->nack.sequenceNumber  = ((data[1]<<24) & 0xFF000000);
    this->nack.sequenceNumber |= ((data[2]<<16) & 0x00FF0000);
    this->nack.sequenceNumber |= ((data[3]<< 8) & 0x0000FF00);
    this->nack.sequenceNumber |= ((data[4]    ) & 0x000000FF);
    this->nack.byteLength      = (data[5]<< 8) & 0xFF00;
    this->nack.byteLength     |= (data[6]    ) & 0x00FF;
    this->nack.checksum        =  data[7];

    // calculate checksum of nack
    uint8_t check = this->CalculateNackChecksum(data);

    // if the checksum is valid, process the nack
    if(((byte)this->nack.checksum) == ((byte)check))
    {
        if(isNackResetCommand(this->nack.sequenceNumber))
        { // Reset the Teensy
            ResetTeensy();
        } else if (isNackAlternateCommand(this->nack.sequenceNumber)) {
            FlagNackAlternateCommand(this->nack.sequenceNumber);
        } else {
            this->ResendPacketTimer(this->nack.sequenceNumber, this->nack.byteLength);
            if(toPrint)
                this->PrintNack();
        }
    }
}

void SimpleTCP::PrintNack()
{
    Serial.print("Nack:Seq/Byt - ");
    Serial.print(this->nack.sequenceNumber);
    Serial.print("/");
    Serial.println(this->nack.byteLength);
}

void SimpleTCP::HandleFindingStartAck()
{
    // wait until Start Ack is received to send data
    // Start Ack also synchronizes world time to internal micros() counter
    // via this->microsecondOffset
    while(!this->StartAckReceived)
    {
        //Serial.println("Looking for Start ACK");
        // fill the buffer up to 16 bytes
        while(validBytesCircBuf < 16)
        {
            if(Serial4.available())
            {
                ackCircBuffer[acbTail] = Serial4.read();
                //Serial.println(ackCircBuffer[acbTail], HEX);
                acbTail = (acbTail + 1) % ackCircBufferSize;
                validBytesCircBuf++;
            }
        }
        // the circularbuffer has exactly 16 bytes in it check for ack signifier
        if(this->isAckSignifier(ackCircBuffer[acbHead]))
        {
            // unroll 16 samples of the circular buffer into a flattened buffer
            uint8_t flattenedBuf[16] = {0};
            flattenCircularBuffer(ackCircBuffer,acbHead,16,ackCircBufferSize,flattenedBuf);
            if(this->isAck(flattenedBuf))
            {
                this->ProcessAck(flattenedBuf);
                Serial.println("StartAck Found!");
                // reset circular buffer for nacks below
                validBytesCircBuf = 0;
                acbHead           = 0;
                acbTail           = 0;
            } else {
                // skip past this byte and restart loop
                acbHead = (acbHead + 1) % ackCircBufferSize;
                validBytesCircBuf--;
            }
        } else {
            // skip past this byte and restart loop
            acbHead = (acbHead + 1) % ackCircBufferSize;
            validBytesCircBuf--;
        }
    }
    this->PrintMicroOffset();
}

// process incoming nacks
void SimpleTCP::HandleNacks()
{
    /*
    if( enough bytes for nack between buffer and incoming stream)
        pull in as many bytes as possible

        hop over bytes until you find nack signifier or have less than nack size bytes

        found nack sig?
            check if it's a nack
            if it is -> process it, repackage data and send it out
            if it isnt -> hop over this signifier and exit the loop
    */
    if((validBytesCircBuf + Serial4.available()) >= this->GetNacketLength())
    {
        // while there are bytes in the stream and the buffer isn't full
        // take in bytes
        while(Serial4.available() && (validBytesCircBuf < ackCircBufferSize))
        {
            ackCircBuffer[acbTail++] = Serial4.read();
            acbTail %= ackCircBufferSize; // make sure it doesn't overrun the end of the buffer
            validBytesCircBuf++;
        }
        // The buffer is now guaranteed to have at least NacketLength bytes in it
        // Look for a nack signifier
        while(validBytesCircBuf >= this->GetNacketLength())
        {
            if(this->isNackSignifier(ackCircBuffer[acbHead]))
            {   // unroll 8 samples of the circular buffer into a flattened buffer
                uint8_t flattenedBuf[8];
                flattenCircularBuffer(ackCircBuffer,acbHead,8,ackCircBufferSize,flattenedBuf);
                if(this->isNack(flattenedBuf))
                {
                    this->ParseNack(flattenedBuf, txBufferLen, true);
                    //Serial.println("Nack Found!");
                    acbHead = (acbHead + 8) % ackCircBufferSize;
                    validBytesCircBuf -= 8;
                    return; // only handle one Nack per call to HandleNacks
                } else {
                    // skip past this byte and restart loop
                    acbHead = (acbHead + 1) % ackCircBufferSize;
                    validBytesCircBuf--;
                }
            } else {
                // first byte in circular buffer is not nack signifier, skip it
                // skip past this byte and restart loop
                acbHead = (acbHead + 1) % ackCircBufferSize;
                validBytesCircBuf--;
            }
        }
    }
}

// Transmit the data stored in the head of the output buffer
// Return true if data sent, else false
bool SimpleTCP::Transmit()
{
    if(SimpleTCP::txReadyFlag && (outputPtrBufferHead != outputPtrBufferTail))
    { // Ready to transmit and buffer isn't empty
        // Transmit the data stored in the head of the output buffer
        Serial4.write(outputPtrBuffer[outputPtrBufferHead],outputPtrLenBuffer[outputPtrBufferHead]);

        // Reset the transmit ready flag
        SimpleTCP::txReadyFlag = false;

        // Record the Transmit time
        outputPtrTimeBuffer[outputPtrBufferHead] = micros();

        // Increment the head
        outputPtrBufferHead++;
        outputPtrBufferHead %= outputPtrBufferSize;
        return true;
    }
    return false;
}

// free the buffers pointed to in the output buffer is they are stale
void SimpleTCP::EraseOldOutputBuffers()
{
    uint32_t cutoffTime = micros();
    for(uint32_t i=0;i<outputPtrBufferSize;i++)
    {
        if( (outputPtrLenBuffer[i] > 0) &&
            ((outputPtrTimeBuffer[i] + SimpleTCP::microsToKeepPackets) <= cutoffTime))
        {   // there is a valid packet in this entry and it is stale
            free((void*)outputPtrBuffer[i]);
            outputPtrLenBuffer[i] = 0;
        }
    }
}

void SimpleTCP::HandleSendingSamplesTimer(uint8_t * data, uint32_t dataLen)
{
    uint8_t * out;
    uint16_t outLen;

    // allocate an appropriately sized array to store data while it's being sent out
    if((dataLen > 0) && (dataLen <= txBufferLen)) {
        out = (uint8_t*) malloc(dataLen + SimpleTCP::packetHeaderSize);
    } else {
        Serial.print("HandleSendingSamplesTimer: Currently Unsupported Length - ");
        Serial.println(dataLen);
    }

    // packetize the data
    uint32_t packetSequenceNumber = this->GetNextByteNum();
    this->MakePacket(data, dataLen, out, &outLen, packetSequenceNumber, false);

    // add it to the transmit buffer
    AddToOutputPtrBuffer(out, outLen, packetSequenceNumber);
}

void SimpleTCP::HandleSendingSamples()
{
    // Packetize and send over data as fast as needed
    for(uint32_t ii=0;ii<txBufferLen;ii++)
        txBuffer[ii] = (uint8_t)random(256);

    this->MakePacket(txBuffer,txBufferLen,packetBuffer,&packetBufferLen,this->GetNextByteNum(), false);
    if( (micros() - this->lastBufSentMicros) < SimpleTCP::interBufferTimeMicros)
    {
        uint32_t toDelay = SimpleTCP::interBufferTimeMicros - micros() + this->lastBufSentMicros;
        delayMicroseconds(toDelay);
    }
    Serial4.write(packetBuffer,packetBufferLen);
    this->lastBufSentMicros = micros();
}

uint32_t SimpleTCP::GetNacketLength()
{
    return (uint32_t) 8;
}
uint32_t SimpleTCP::GetAcketLength()
{
    return (uint32_t) 16;
}

bool SimpleTCP::isAckSignifier(uint8_t data)
{
    if(data != 0xBF)
    {
        return false;
    }
    return true;
}

bool SimpleTCP::isAck(uint8_t * data)
{
    // check ack signifier
    if(data[0] != 0xBF)
    {
        return false;
    }
    // check checksum
    if( ((uint8_t)this->CalculateAckChecksum(data)) != ((uint8_t)data[15]))
    {
        return false;
    }
    return true;;
}

bool SimpleTCP::isAckOrNack(uint8_t * data)
{
    if((data[0] == 0xBF) || (data[0] == 0xFA))
    {
        return true;
    }
    return false;
}

// Assumed that struct acket "a" is preallocated
void SimpleTCP::ParseAck(uint8_t * data, struct acket * a)
{
    a->ackSignifier    =  data[0];
    a->sequenceNumber  = (data[1] << 24)  & 0xFF000000;
    a->sequenceNumber |= (data[2] << 16)  & 0x00FF0000;
    a->sequenceNumber |= (data[3] <<  8)  & 0x0000FF00;
    a->sequenceNumber |= (data[4]      )  & 0x000000FF;
    a->byteLength      = (data[5] <<  8)  & 0xFF00;
    a->byteLength     |= (data[6]      )  & 0x00FF;
    a->timestamp       = ( ((uint_least64_t) data[7] ) << 56) & 0xFF00000000000000;
    a->timestamp      |= ( ((uint_least64_t) data[8] ) << 48) & 0x00FF000000000000;
    a->timestamp      |= ( ((uint_least64_t) data[9] ) << 40) & 0x0000FF0000000000;
    a->timestamp      |= ( ((uint_least64_t) data[10]) << 32) & 0x000000FF00000000;
    a->timestamp      |= ( ((uint_least64_t) data[11]) << 24) & 0x00000000FF000000;
    a->timestamp      |= ( ((uint_least64_t) data[12]) << 16) & 0x0000000000FF0000;
    a->timestamp      |= ( ((uint_least64_t) data[13]) <<  8) & 0x000000000000FF00;
    a->timestamp      |= ( ((uint_least64_t) data[14])      ) & 0x00000000000000FF;
    a->checksum        =  data[15];
}

// Call this once you're sure you have a valid ack to handle everything the ack
// needs to handle
void SimpleTCP::ProcessAck(uint8_t * data)
{
    this->ack.timestamp  = ( ((uint_least64_t) data[7] ) << 56) & 0xFF00000000000000;
    this->ack.timestamp |= ( ((uint_least64_t) data[8] ) << 48) & 0x00FF000000000000;
    this->ack.timestamp |= ( ((uint_least64_t) data[9] ) << 40) & 0x0000FF0000000000;
    this->ack.timestamp |= ( ((uint_least64_t) data[10]) << 32) & 0x000000FF00000000;
    this->ack.timestamp |= ( ((uint_least64_t) data[11]) << 24) & 0x00000000FF000000;
    this->ack.timestamp |= ( ((uint_least64_t) data[12]) << 16) & 0x0000000000FF0000;
    this->ack.timestamp |= ( ((uint_least64_t) data[13]) <<  8) & 0x000000000000FF00;
    this->ack.timestamp |= ( ((uint_least64_t) data[14])      ) & 0x00000000000000FF;
    this->UpdateMicroOffset((uint_least64_t)this->ack.timestamp);
    this->StartAckReceived = true;
}

// To Do: Fix "15" magic number (Hint: don't use sizeof due to packing irregularities)
uint8_t SimpleTCP::CalculateAckChecksum(uint8_t * data)
{
    uint8_t checksum = 0;
    for(uint8_t i=0;i<15;i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

// To Do: Fix "7" magic number (Hint: don't use sizeof due to packing irregularities)
uint8_t SimpleTCP::CalculateNackChecksum(uint8_t * data)
{
    uint8_t checksum = 0;
    for(uint8_t i=0;i<7;i++)
    {
        checksum ^= data[i];
    }
    return checksum;
}

// Wrap a byte array dataIn of length len in the SimpleTCP packet header
// Result stored in packetOut of length packetLen bytes
// Pass in nextByteNum which is the index of the first byte in dataIn to be sent out
void SimpleTCP::MakePacket(uint8_t * dataIn, uint16_t len, uint8_t * packetOut, uint16_t * packetLen, uint32_t nextByteNo, bool resend)
{
    // packetOut[0] will now be a packet Signifier 0xAA
    packetOut[0] = (uint8_t) 0xAA;
    //packetOut[0] = (uint8_t)(nextByteNo >> 24) & 0x000000FF;
    packetOut[1] = (uint8_t)(nextByteNo >> 16) & 0x000000FF;
    packetOut[2] = (uint8_t)(nextByteNo >>  8) & 0x000000FF;
    packetOut[3] = (uint8_t)(nextByteNo      ) & 0x000000FF;

    //packetOut[4] will also be signifier 0x55
    packetOut[4] = (uint8_t) 0x55;
    //packetOut[4] = (uint8_t)(this->lastAckdByte >> 24) & 0x000000FF;
    packetOut[5] = (uint8_t)(this->lastAckdByte >> 16) & 0x000000FF;
    packetOut[6] = (uint8_t)(this->lastAckdByte >>  8) & 0x000000FF;
    packetOut[7] = (uint8_t)(this->lastAckdByte      ) & 0x000000FF;

    packetOut[8] = (uint8_t)((len >> 8) & 0x00FF);
    packetOut[9] = (uint8_t)((len     ) & 0x00FF);

    // Checksum take 2
    uint8_t dataChecksum = 0;
    for(uint16_t i=0;i<len;i++)
    {
        dataChecksum ^= dataIn[i];
    }
    packetOut[10] = (uint8_t)(dataChecksum & 0xFF);

    uint8_t headerChecksum = 0;
    for(uint16_t i=0;i<10;i++)
    {
        headerChecksum ^= packetOut[i];
    }

    packetOut[11] = (uint8_t)(headerChecksum & 0xFF);

    memcpy((void*) &packetOut[12], (void*) dataIn, len);

    *packetLen = len + SimpleTCP::packetHeaderSize;
    // if this is not a retransmission of the packet, increment nextByteNum
    if(!resend) {this->nextByteNum += len;}
}

// calculates the time offset between the processors internal micros and
// client PC's nanosecond timer
void SimpleTCP::UpdateMicroOffset(uint_least64_t nanoTime)
{
    this->microsecondOffset = (int_least64_t)(((float)nanoTime)/1000) - micros();
}

void SimpleTCP::PrintMicroOffset()
{
    Serial.print("micros: 0x");
    Serial.println(micros(),HEX);
    Serial.print("microsecondOffset: 0x");
    Serial.print((uint32_t)(this->microsecondOffset>>32),HEX);
    Serial.println((uint32_t)this->microsecondOffset,HEX);
}

// Find the data at sequenceNumber and length byteLength in outputPtrBuffer
// Set it to be retransmit at soon as possible
void SimpleTCP::ResendPacketTimer(uint32_t sequenceNumber, uint16_t byteLength)
{
    uint32_t firstByteLeftToRetransmit = sequenceNumber;

    //Serial.print("RPT: seq/len - ");
    //Serial.print(sequenceNumber);
    //Serial.print("/");
    //Serial.println(byteLength);

    // Search for the data requested in the output buffer
    for(uint32_t i = 0; i < outputPtrBufferSize; i++)
    {
        if( (outputPtrLenBuffer[i] != 0                                                     ) &&
            (firstByteLeftToRetransmit >= outputPtrSequenceBuffer[i]                        ) &&
            (firstByteLeftToRetransmit  < (outputPtrSequenceBuffer[i] + outputPtrLenBuffer[i]) - SimpleTCP::packetHeaderSize))
        { // the packet in this entry of the output buffer has data to retransmit
            // set this packet to be high priority retransmit

            //Serial.print("HighPrio: seq/len - ");
            //Serial.print(outputPtrSequenceBuffer[i]);
            //Serial.print("/");
            //Serial.println(outputPtrLenBuffer[i]);
            AddToOutputPtrBuffer(outputPtrBuffer[i], outputPtrLenBuffer[i], outputPtrSequenceBuffer[i]);
            // update firstByteLeftToRetransmit to be the byte after the last byte in the found packet
            firstByteLeftToRetransmit = outputPtrSequenceBuffer[i] + outputPtrLenBuffer[i];
            // mark the prior entry in the output buffer to be empty
            outputPtrLenBuffer[i] = 0;

            // check if we have retransmitted everything already
            if(firstByteLeftToRetransmit >= (sequenceNumber + byteLength)) {return;}
        }
    }
    // couldn't find the packet asked to retransmit, this must be handled in the future with SD storage
    Serial.print("ERROR: Can't Find Nack Data - ");
    Serial.println(firstByteLeftToRetransmit);
    PrintBufferState();
}

// Resend the data at sequenceNumber of length byteLength
// In a packetized way, each packet has maxlength txBufLen
void SimpleTCP::ResendPacket(uint32_t sequenceNumber, uint16_t byteLength, uint32_t txBufLen)
{
    uint16_t packetBufSize = txBufLen + 12; //12-byte header
    uint8_t  packetBuf[packetBufSize];
    uint8_t  txBuf[txBufLen];
    uint16_t thisPacketLen = 0;
    uint16_t bytesRemaining = byteLength;
    uint32_t currentByteIndex = sequenceNumber;

    // while there are bytes left to retransmit, keep transmitting
    while(bytesRemaining > 0)
    {
        // if there are more bytes left than can fit in the buffer, send a full buffer
        if(bytesRemaining >= txBufLen){
            thisPacketLen = txBufLen;
        } else {
        // if there are less than a full buffer's worth of bytes, just send the remainder
            thisPacketLen = bytesRemaining;
        }

        // fill the buffer with thisPacketLen bytes
        for(uint32_t ii=0;ii<thisPacketLen;ii++)
        {
            txBuf[ii] = (uint8_t)random(256);
        }

        // packetize the data and form the header
        this->MakePacket(&txBuf[0],(uint16_t)thisPacketLen,&packetBuf[0],(uint16_t*)&packetBufSize,currentByteIndex,true);

        Serial.print("Rsnt Byt/Len: ");
        //Serial.print(  "  ByteIndx : ");
        Serial.print(currentByteIndex);
        Serial.print("/");
        Serial.println(thisPacketLen);

        // send the packet out via serial4
        if( (micros() - this->lastBufSentMicros) < SimpleTCP::interBufferTimeMicros)
        {
            uint32_t toDelay = SimpleTCP::interBufferTimeMicros - micros() + this->lastBufSentMicros;
            delayMicroseconds(toDelay);
        }
        Serial4.write(packetBuf,packetBufSize);
        this->lastBufSentMicros = micros();

        // update the necessary variables
        bytesRemaining   -= thisPacketLen;
        currentByteIndex += thisPacketLen;
    }
}
