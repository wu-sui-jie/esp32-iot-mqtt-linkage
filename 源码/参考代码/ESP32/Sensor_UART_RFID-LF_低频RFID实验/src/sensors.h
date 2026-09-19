#ifndef _CONFIG_H
#define _CONFIG_H

#include "Arduino.h"
#include <HardwareSerial.h>

// RFID读卡命令 - 低频LF
#define RFID_READ_CMD "AA 00 03 29 26 00 0C BB"
#define RFID_RESPONSE_HEADER "AA 00 07"
#define RFID_CARD_DATA_START 6  // 卡号数据开始位置（字节索引）

void RFID_init();
String readRFIDCard(void);

#endif
