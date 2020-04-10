/*
    SimpleTCP.h - Library for wrapping byte-wise data in packet and reliably
    transmitting it.
    Created by Nathan Volman, January 25, 2019
*/
#ifndef SIMPLE_TCP_H
#define SIMPLE_TCP_H

#include <Arduino.h>

class SimpleTCP
{
    public:
        SimpleTCP();
        SimpleTCP(bool usingIntervalTimer);
        bool Transmit();
        void HandleFindingStartAck();
        void HandleNacks();
        void HandleSendingSamples();
        void HandleSendingSamplesTimer(uint8_t * data, uint32_t len);
        void EraseOldOutputBuffers();
        uint32_t ReadAlternateCommand();
        void ClearAlternateCommand();
        static uint32_t GetInterBufferTimeMicros();
        static void SetTxReadyFlag(); // Called by IntervalTimer - sets txReadyFlag to true

    private:
        struct packet
        {
            uint32_t sequenceNumber; // Sequence # of first byte in this packet
            uint32_t ackNumber; // Sequence # of last ack'd byte
            uint16_t byteLength;
            uint16_t checksum;
            uint8_t* data;
        } pack;

        struct nacket
        {
            uint8_t  nackSignifier;
            uint32_t sequenceNumber; // Sequence # of first byte in this packet
            uint16_t byteLength;
            uint8_t  checksum;
        } nack;

        struct acket
        {
            uint8_t  ackSignifier;
            uint32_t sequenceNumber; // Sequence # of first byte in this packet
            uint16_t byteLength;
            uint64_t timestamp;
            uint8_t  checksum;
        } ack;

        void MakePacket(uint8_t * dataIn, uint16_t len, uint8_t * packetOut, uint16_t * packetLen, uint32_t nextByteNo, bool resend);
        void ParsePacket(uint8_t * data,  uint16_t len);
        void ResendPacket(uint32_t sequenceNumber, uint16_t byteLength, uint32_t txBufLen);
        void ResendPacketTimer(uint32_t sequenceNumber, uint16_t byteLength);
        uint32_t GetNacketLength();
        uint32_t GetAcketLength();
        bool isNack(uint8_t * data);
        bool isNackSignifier(uint8_t data);
        bool isNackResetCommand(uint32_t NackSequenceNumber);
        bool isNackAlternateCommand(uint32_t NackSequenceNumber);
        void FlagNackAlternateCommand(uint32_t NackSequenceNumber);
        void ResetTeensy();
        void ParseNack(uint8_t * data, uint32_t txBufLen, bool toPrint);
        void PrintNack();
        bool isAckSignifier(uint8_t data);
        bool isAck(      uint8_t * data);
        bool isAckOrNack(uint8_t * data);
        void ParseAck(   uint8_t * data, struct acket * a);
        void ProcessAck(uint8_t * data);
        void UpdateMicroOffset(uint_least64_t nanoTime);
        void PrintMicroOffset();
        uint32_t GetNextByteNum();
        uint32_t lastAckdByte;
        uint32_t nextByteNum; // Currently unsupported
        // the first ack received has the send time in nanoseconds in its payload, this stores an offset from millis()
        int_least64_t  microsecondOffset;
        void     ResendPackets(uint32_t start, uint32_t stop);
        void     CalculateChecksum(struct packet * p);
        uint8_t  CalculateAckChecksum(uint8_t * data);
        uint8_t  CalculateNackChecksum(uint8_t * data);
        bool     StartAckReceived;
        uint32_t AltCommand;
        uint32_t lastBufSentMicros; // time in microseconds when the last buffer was sent
        static uint32_t interBufferTimeMicros; // time that must elapse between two buffer sends in micros
        static volatile bool txReadyFlag; // When this is true -> ready to send next packet, else wait
        static uint32_t packetHeaderSize;
        static bool usingTimer;
        static uint32_t microsToKeepPackets;

};

#endif //SIMPLE_TCP_H
