#ifndef _WIFI_MQTT_H
#define _WIFI_MQTT_H

#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

bool clientIsConnected();
void clientLoop();
void clientInit();
void setup_wifi();
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
void parseMqttResponse(char *payload);
// void postMsg(String  msg);
void postMsg(String commType,String sensorType, String msg);
void postStr(String str);
void Topic_init(String id);
void SSID_init();


#endif