#ifndef _CONFIG_H
#define _CONFIG_H

#include "Arduino.h"
#include <HardwareSerial.h>

// RFID读卡命令
#define RFID_READ_CMD "AA BB 00 00 00 02 20 01 23"
#define RFID_NO_CARD_RESPONSE "AA BB CC CC 00 02 20 01 23"
#define RFID_RESPONSE_HEADER "AA BB CC CC"

void RFID_init();
String readRFIDCard(void);

#endif
