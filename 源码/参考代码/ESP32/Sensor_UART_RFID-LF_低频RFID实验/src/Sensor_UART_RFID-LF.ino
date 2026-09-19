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

//String prefixDeviceID = "LDEH00";

const int KeyPin = 39;  //设置重置按键引脚,用于删除WiFi信息

int connectTimeOut_s = 15;  //WiFi连接超时时间，单位秒
int mqttstate=0;

#define BUF_DATA_LEN 150
char ATdata[BUF_DATA_LEN];
String strdata = "";
int len = 0;

// JSON数据格式定义
#define JSON_DATA_RFID "{\"LFRFIDCARD\":%s}"




void setup() {
  Serial.begin(115200);  //波特率
  // Serial2.begin(115200);
  delay(1000);
  while (!Serial)
    ;
  Serial.println("start...");
  delay(1000);
  LEDinit();               //LED用于显示WiFi状态
  pinMode(KeyPin, INPUT);  //按键上拉输入模式(默认高电平输入,按下时下拉接到低电平)
    // 初始化IO
  IO_init();
  
  // 初始化TFT
  TFT_init();
  TFT_clear();

  
  // 检查字库
  if(CHECK_FALSH()) {
    Serial.println("字库检查通过");
  } else {
    Serial.println("字库检查失败");
  }
  
  Serial.println("TFT液晶屏初始化完成");
  TFT_ClearScreen();
  TFT_SetTextColor(WHITE, BLACK);
  TFT_SetFontSize(SONG_STYLE20);
  TFT_ShowString(10, 35, "（低频）系统启动中...");
  TFT_ShowProgressBar(10, 150, 200, 15, 0, GREEN, BLACK);
  delay(1000);
  //oled_munu_init();
  //menu2_0_show("start...");
  

  
  TFT_ClearArea(0,0,240,140);
  TFT_ShowString(10, 35, "存储信息查询中...");
  Serial.println("存储信息查询:");
  prefs.begin("smarthome");
  wifi_ssid = prefs.getString("wifi_ssid");  //wifi账号密码
  wifi_pass = prefs.getString("wifi_pass");  //wifi账号密码
  groupid = prefs.getString("groupid");      //groupid
  mqttip = prefs.getString("mqttip");        //sensortype
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
  //menu2_0_show("If you want to reconfigure the network, please long press the KEY button");
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
     // menu_2_show("KEY Button Press", j);
  
    }
    if (!digitalRead(KeyPin)) {
      Serial.println("\n按键已长按3秒,正在清空网络连保存接信息.");
      TFT_ClearScreen();
      TFT_ShowString(10, 35, "按键已长按3秒,正在清空网络连保存接信息.");
      restoreWiFi();  //删除保存的wifi信息
      //删除保存的配置信息
      prefs.begin("smarthome");
      prefs.clear();
      prefs.end();
     // menu2_0_show("config clear,reboot...");
      TFT_ClearScreen();
      TFT_ShowString(10, 35, "配网信息已清除,正在重启...");
      Serial.println("正在重启设备.");
      ESP.restart();  //重启复位esp32
    }
  }
  if (wifi_ssid == "" || wifi_pass == "") {
    //menu2_0_show("wifiConfig...");
    TFT_ClearScreen();
    wifiConfig();  //开始配网功能
  } else {
    connectToWiFi(connectTimeOut_s);  //连接wifi，传入的是wifi连接等待时间15s

  }

  if (wifi_con_flag == 1) {
    //***************开始执行******************
    TFT_ShowProgressBar(10, 150, 200, 15, 50, GREEN, BLACK);
    delay(1000);
    Serial.println("MQTT连接中...");
    //menu2_0_show("client init...");
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "MQTT连接中...");
    clientInit();
     // SSID_init();
    delay(1000);
    //Topic_init(prefixDeviceID + groupid);
    Topic_init(groupid);
    // Serial.println("wifi初始化中...");
    // setup_wifi();
    TFT_ShowProgressBar(10, 150, 200, 15, 75, GREEN, BLACK);
    
    Serial.println("传感器初始化中...");
    //menu2_0_show("sensor init...");
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "传感器初始化中...");

    if (sensortype == "01") {
      Serial.println("RFID init...");
      RFID_init();
    } 
    TFT_ShowProgressBar(10, 150, 200, 15, 100, GREEN, BLACK);
    delay(3000);
    TFT_ClearScreen();
    TFT_FillRect(0, 0, 240, 40, BLUE);
    }
   else
   {
      TFT_ClearScreen();
      TFT_ShowString(10, 35, "请用手机或电脑WiFi连接IOT-4-1开头的无线网络后登录网页进行无线账号密码配置...");
   }

  }


unsigned long lastMsMain = 0;
unsigned int second = 0;  //秒计数
unsigned int ms_50 = 0;   //50毫秒
String rfid_card = "";
String rfid_card_prev = "";
bool rfid_card_flag = false;

void loop() {

  if (wifi_con_flag != 1) {
    checkDNS_HTTP();     //检测客户端DNS&HTTP请求，也就是检查配网页面那部分
    checkConnect(true);  //检测网络连接状态，参数true表示如果断开重新连接
    delay(30);
  } else if (wifi_con_flag == 1) {  //1 Wifi联网成功

    if (!clientIsConnected()) {
      reconnect();
    }
    clientLoop();

    if (millis() - lastMsMain >= 1000) {
      lastMsMain = millis();
      second++;
      if (second > 30000)
        second = 0;

      mainFunction();  //主功能函数
    }
  }
}


void mainFunction() {
  if (sensortype == "01") {
    rfid_card = readRFIDCard();

    ShowRFIDDashboard(rfid_card, wifi_con_flag, mqttstate, groupid);
    rfid_card_flag = (rfid_card != rfid_card_prev);  //卡号有变化（包括有卡到无卡）
    if (rfid_card_flag) {
      cleanBuffer(ATdata, BUF_DATA_LEN);
      if (rfid_card != "") {
      len = snprintf(ATdata, BUF_DATA_LEN, JSON_DATA_RFID, rfid_card.c_str());
      }
      else if(rfid_card== "")
      {
        len = snprintf(ATdata, BUF_DATA_LEN, JSON_DATA_RFID, "0");
      }
      Serial.println(ATdata);
      postMsg("Wifi", "LFRFID", ATdata);
    }
    rfid_card_prev = rfid_card;
  }
}


void cleanBuffer(char* buf, int len) {
  for (int i = 0; i < len; i++) {
    buf[i] = '\0';
  }
}


// 显示RFID读卡信息界面
void ShowRFIDDashboard(String card_id, bool wifi_online, bool mqtt_connected, String group_id) {
  TFT_SetTextColor(WHITE, BLUE);
  TFT_SetFontSize(SONG_STYLE24);
  TFT_ShowString(30, 6, "低频 RFID读卡");
  TFT_ShowWiFi(220, 15, wifi_online);
  TFT_SetFontSize(SONG_STYLE12);
  TFT_SetTextColor(wifi_online ? WHITE : RED, BLUE);
  TFT_ShowString(195, 20, wifi_online ? "在线" : "离线");
  TFT_SetTextColor(WHITE, BLUE);
  TFT_ShowString(195, 4, mqtt_connected ? "MQTT" : " ");
  TFT_SetFontSize(SONG_STYLE24);
  TFT_SetTextColor(RED, BLACK);
  TFT_ShowString(30, 80, "卡号:");
  TFT_ClearArea(100, 80, 120, 32);
  if (card_id == "") {
    TFT_ShowString(100, 80, "无卡");
  } else {
    TFT_ShowString(100, 80, card_id.c_str());
  }
  TFT_SetFontSize(SONG_STYLE16);
  TFT_SetTextColor(YELLOW, BLACK);
  char group_str[20];
  sprintf(group_str, "设备ID: %s", group_id.c_str());
  TFT_ShowString(50, 200, group_str);
}


