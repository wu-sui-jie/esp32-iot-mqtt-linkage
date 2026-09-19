#ifndef _CONFIG_H
#define _CONFIG_H

#include "Arduino.h"
#include <ArduinoJson.h>

#define IR_BARRIER_PIN 14

void IRBarrier_init();
bool readIRBarrier();

#endif
