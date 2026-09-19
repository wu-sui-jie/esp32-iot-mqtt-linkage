#include "wifi_mqtt.h"
#include "sensors.h"
#include "WiFiUser.h"
#include "TFT_Library.h"
#include "UTF8ToGB2312.h"
#include <Preferences.h>

int wifi_con_flag = 0;  //Wifi连接状态,1:Wifi联网成功,其余都是失败

Preferences prefs;                 //使用Preferences保存数据
String wifi_ssid = "";     //wifi账号密码
String wifi_pass = "";     //wifi账号密码
String groupid = "";             //groupid
String sensortype = "";          //sensortype
String mqttip = "";  //mqttip

const int KeyPin = 39;  //设置重置按键引脚,用于删除WiFi信息

int connectTimeOut_s = 15;  //WiFi连接超时时间，单位秒
int mqttstate=0;

#define BUF_DATA_LEN 150
char ATdata[BUF_DATA_LEN];
String strdata = "";
int len = 0;
#define JSON_DATA_FLAME "{\"analog\":%d,\"fire\":%d}"

void setup() {
  Serial.begin(115200);  //波特率
  delay(1000);
  while (!Serial);
  Serial.println("start...");
  delay(1000);
  LEDinit();               //LED用于显示WiFi状态
  pinMode(KeyPin, INPUT);  //按键上拉输入模式(默认高电平输入,按下时下拉接到低电平)
  IO_init();
  TFT_init();
  TFT_clear();
  TFT_BacklightOn();
  if(CHECK_FALSH()) {
    Serial.println("字库检查通过");
  } else {
    Serial.println("字库检查失败");
  }
  Serial.println("TFT液晶屏初始化完成");
  TFT_ClearScreen();
  TFT_SetTextColor(WHITE, BLACK);
  TFT_SetFontSize(SONG_STYLE20);
  TFT_ShowString(10, 35, "系统启动中...");
  TFT_ShowProgressBar(10, 150, 200, 15, 0, GREEN, BLACK);
  delay(1000);
  TFT_ClearArea(0,0,240,140);
  TFT_ShowString(10, 35, "存储信息查询中...");
  Serial.println("存储信息查询:");
  prefs.begin("smarthome");
  wifi_ssid = prefs.getString("wifi_ssid");
  wifi_pass = prefs.getString("wifi_pass");
  groupid = prefs.getString("groupid");
  mqttip = prefs.getString("mqttip");
  sensortype = prefs.getString("sensortype");
  prefs.end();
  Serial.print("wifi_ssid:");
  Serial.println(wifi_ssid);
  Serial.print("wifi_pass:");
  Serial.println(wifi_pass);
  Serial.print("groupid:");
  Serial.println(groupid);
  Serial.print("mqttip:");
  Serial.println(mqttip);
  Serial.print("sensortype:");
  Serial.println(sensortype);
  delay(1000);
  TFT_ShowProgressBar(10, 150, 200, 15, 25, GREEN, BLACK);
  Serial.println("如果要重新配网,请长按KEY2按键3秒...");
  TFT_ClearArea(0,0,240,140);
  TFT_ShowString(10, 35, "无线网络连接中...");
  TFT_ShowString(10, 70, "如果要重新配网,请长按KEY2按键3秒...");
  delay(2000);
  if (!digitalRead(KeyPin))  //长按5秒(KEY)清除网络配置信息
   {
    for (int j = 1; j <= 3; j++) {
      delay(1000);
      Serial.print(j);
      Serial.print(". ");
    }
    if (!digitalRead(KeyPin)) {
      Serial.println("\n按键已长按3秒,正在清空网络连保存接信息.");
      TFT_ClearScreen();
      TFT_ShowString(10, 35, "按键已长按3秒,正在清空网络连保存接信息.");
      restoreWiFi();
      prefs.begin("smarthome");
      prefs.clear();
      prefs.end();
      TFT_ClearScreen();
      TFT_ShowString(10, 35, "配网信息已清除,正在重启...");
      Serial.println("正在重启设备.");
      ESP.restart();
    }
  }
  if (wifi_ssid == "" || wifi_pass == "") {
    TFT_ClearScreen();
    wifiConfig();
  } else {
    connectToWiFi(connectTimeOut_s);
  }
  if (wifi_con_flag == 1) {
    TFT_ShowProgressBar(10, 150, 200, 15, 50, GREEN, BLACK);
    delay(1000);
    Serial.println("MQTT连接中...");
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "MQTT连接中...");
    clientInit();
    delay(1000);
    Topic_init(groupid);
    TFT_ShowProgressBar(10, 150, 200, 15, 75, GREEN, BLACK);
    Serial.println("传感器初始化中...");
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "传感器初始化中...");
    FlameSensor_init();
    TFT_ShowProgressBar(10, 150, 200, 15, 100, GREEN, BLACK);
    delay(3000);
    TFT_ClearScreen();
    TFT_FillRect(0, 0, 240, 40, BLUE);
  } else {
    TFT_ClearScreen();
    TFT_ShowString(10, 35, "请用手机或电脑WiFi连接IOT-4-1开头的无线网络后登录网页进行无线账号密码配置...");
  }
}

unsigned long lastMsMain = 0;
unsigned int second = 0;  //秒计数
unsigned int ms_50 = 0;   //50毫秒
bool lastFlameState = false;  // 记录上一次火焰状态

void loop() {
  if (wifi_con_flag != 1) {
    checkDNS_HTTP();
    checkConnect(true);
    delay(30);
  } else if (wifi_con_flag == 1) {
    if (!clientIsConnected()) {
      reconnect();
    }
    clientLoop();
    if (millis() - lastMsMain >= 1000) {
      lastMsMain = millis();
      second++;
      if (second > 30000)
        second = 0;
      mainFunction();
    }
  }
}

void mainFunction() {
  int flameAnalog = readFlameAnalog();
  bool flameDetected = readFlameDigital();
  Serial.print("Flame Analog Value: ");
  Serial.println(flameAnalog);
  Serial.print("Flame Detected: ");
  Serial.println(flameDetected ? "YES" : "NO");
  ShowFlameDashboard(flameAnalog, flameDetected, wifi_con_flag, mqttstate, groupid);
  
  // 检测火焰状态变化
  bool flameStateChanged = (flameDetected != lastFlameState);
  
  // 火焰状态变化时立即发送数据，或者每10秒定时发送
  if (flameStateChanged || (second % 10 == 5)) {
    cleanBuffer(ATdata, BUF_DATA_LEN);
    len = snprintf(ATdata, BUF_DATA_LEN, JSON_DATA_FLAME, flameAnalog, flameDetected ? 1 : 0);
    Serial.println(ATdata);
    if (flameStateChanged) {
      Serial.println("火焰状态变化，立即发送数据");
    }
    postMsg("Wifi", "FLAME", ATdata);
  }
  
  // 更新上一次火焰状态
  lastFlameState = flameDetected;
}

void cleanBuffer(char* buf, int len) {
  for (int i = 0; i < len; i++) {
    buf[i] = '\0';
  }
}

void ShowFlameDashboard(int analogValue, bool fireDetected, bool wifi_online, bool mqtt_connected, String group_id) {
  TFT_SetTextColor(WHITE, BLUE);
  TFT_SetFontSize(SONG_STYLE24);
  TFT_ShowString(30, 6, "火焰监测");
  TFT_ShowWiFi(220, 15, wifi_online);
  TFT_SetFontSize(SONG_STYLE12);
  TFT_SetTextColor(wifi_online ? WHITE : RED, BLUE);
  TFT_ShowString(195, 20, wifi_online ? "在线" : "离线");
  TFT_SetTextColor(WHITE, BLUE);
  TFT_ShowString(195, 4, mqtt_connected ? "MQTT" : " ");
  TFT_SetFontSize(SONG_STYLE24);
  TFT_SetTextColor(RED, BLACK);
  TFT_ShowString(30, 80, "模拟值:");
  TFT_ShowNumber(110, 80, analogValue, 4);
  TFT_SetTextColor(BLUE, BLACK);
  TFT_ShowString(30, 130, "火焰:");
  TFT_ShowString(110, 130, fireDetected ? "有火" : "无火");
  TFT_SetFontSize(SONG_STYLE16);
  TFT_SetTextColor(YELLOW, BLACK);
  char group_str[20];
  sprintf(group_str, "设备ID: %s", group_id.c_str());
  TFT_ShowString(50, 200, group_str);
} 