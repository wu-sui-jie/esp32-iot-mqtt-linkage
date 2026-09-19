#ifndef _CONFIG_H
#define _CONFIG_H

#include "Arduino.h"
#include <ArduinoJson.h>

#define SR602_PIN 14

void SR602_init();
bool readSR602();

#endif
