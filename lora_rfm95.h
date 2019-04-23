#ifndef ___LORA_RFM95_H___
#define ___LORA_RFM95_H___ 1

// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <stdint.h>
#include <stdbool.h>
#define LORA_FREQUENCY_EU                          866000
#define LORA_FREQUENCY_US                          915000
#define LORA_FREQUENCY_AS                          433000

#define LORA_FREQUENCY LORA_FREQUENCY_EU

#define PA_OUTPUT_RFO_PIN          0
#define PA_OUTPUT_PA_BOOST_PIN     1

int loraInit(void);
int loraBegin(long frequency);
void loraEnd();

int beginPacket(int implicitHeader);
int endPacket(bool async);

int parsePacket(int size);
int packetRssi();
float packetSnr();
long packetFrequencyError();

// from Print
size_t loraWriteByte(uint8_t byte);
size_t loraWriteBuf(const uint8_t *buffer, size_t size);

// from Stream
int  loraAvailable();
int  loraRead();
int  loraPeek();
void loraFlush();

void loraIdle();
void loraSleep();

void setTxPower(int level, int outputPin);
void setFrequency(long frequency);
void setSpreadingFactor(int sf);
void setSignalBandwidth(long sbw);
void setCodingRate4(int denominator);
void setPreambleLength(long length);
void setSyncWord(int sw);
void enableCrc();
void disableCrc();
void enableInvertIQ();
void disableInvertIQ();

void setOCP(uint8_t mA); // Over Current Protection control

// deprecated
void crc();
void noCrc();

uint8_t readRssi();

void setSPIFrequency(uint32_t frequency);

void dumpRegisters();

void explicitHeaderMode();
void implicitHeaderMode();

void handleDio0Rise();
bool isTransmitting();

int getSpreadingFactor();
long getSignalBandwidth();

void setLdoFlag();

void dumpPayload(char *buf, int len);

#endif /* ___LORA_H___ */
