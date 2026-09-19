#include "wifi_mqtt.h"
#include "sensors.h"
#include "WiFiUser.h"
#include "TFT_Library.h"
#include "UTF8ToGB2312.h"
#include <Preferences.h>
#include <ArduinoJson.h>


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

// JSON数据格式定义
#define JSON_DATA_LORA "{\"commType\":\"LoRa\",\"sensorType\":\"LORA_COLLECTOR\",\"data\":\"%s\"}"

void setup() {
  Serial.begin(115200);  //波特率
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(1000);
  while (!Serial)
    ;
  Serial.println("LORA Collector starting...");
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
    Serial.println("Font check passed");
  } else {
    Serial.println("Font check failed");
  }
  
  Serial.println("TFT LCD initialized");
  TFT_ClearScreen();
  TFT_SetTextColor(WHITE, BLACK);
  TFT_SetFontSize(SONG_STYLE20);
  TFT_ShowString(10, 35, "(LoRa)系统启动中...");
  TFT_ShowProgressBar(10, 150, 200, 15, 0, GREEN, BLACK);
  delay(1000);
  

  
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
  TFT_ClearArea(0,0,240,140);
  TFT_ShowString(10, 35, "无线网络连接中...");
  TFT_ShowString(10, 70, "如果要重新配网,请长按KEY2按键3秒...");

  delay(2000);
  if (!digitalRead(KeyPin))  //长按3秒(KEY)清除网络配置信息
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
      restoreWiFi();  //删除保存的wifi信息
      //删除保存的配置信息
      prefs.begin("smarthome");
      prefs.clear();
      prefs.end();
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
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "MQTT连接中...");
    clientInit();
    delay(1000);
    Topic_init(groupid);
    TFT_ShowProgressBar(10, 150, 200, 15, 75, GREEN, BLACK);
    
    Serial.println("传感器初始化中...");
    TFT_ClearArea(0,0,240,140);
    TFT_ShowString(10, 35, "传感器初始化中...");

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
String lora_data = "";
String lora_data_prev = "";
bool lora_data_flag = false;

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
  // 检查串口2是否有LORA数据
  String raw_lora_data = readLORAData();
  String display_data = "";
  
  if (raw_lora_data != "") {
    Serial.print("接收到LoRa数据: ");
    Serial.println(raw_lora_data);
    
    // 解析JSON数据，提取data字段的值用于显示
    String data_value = extractDataFromJSON(raw_lora_data);
    if (data_value != "") {
      Serial.print("使用提取的data值显示: ");
      Serial.println(data_value);
      display_data = data_value; // 显示时只显示data字段的值
    } else {
      Serial.println("JSON解析失败，显示原始数据");
      display_data = raw_lora_data;
    }
    
    // MQTT发送原始数据（每次收到数据都转发）
    Serial.print("MQTT发送原始数据: ");
    Serial.println(raw_lora_data);
    postStr(raw_lora_data.c_str());
  } else {
    display_data = "";
  }
  
  ShowLORADashboard(display_data, wifi_con_flag, mqttstate, groupid);
}


void cleanBuffer(char* buf, int len) {
  for (int i = 0; i < len; i++) {
    buf[i] = '\0';
  }
}

// 从JSON字符串中提取data字段的值
String extractDataFromJSON(String jsonString) {
  Serial.print("解析JSON: ");
  Serial.println(jsonString);
  
  // 查找"data":"后面的内容
  int dataStart = jsonString.indexOf("\"data\":\"");
  if (dataStart == -1) {
    Serial.println("未找到data字段");
    return ""; // 没有找到data字段
  }
  
  // 跳过"data":"，找到实际数据的开始位置
  dataStart += 8; // "data":"的长度是8
  
  // 查找data字段结束的引号（需要处理转义引号）
  int dataEnd = -1;
  int searchPos = dataStart;
  
  while (true) {
    int nextQuote = jsonString.indexOf("\"", searchPos);
    if (nextQuote == -1) {
      Serial.println("未找到data字段结束引号");
      return ""; // 没有找到结束引号
    }
    
    // 检查这个引号是否是转义的（前面是否有反斜杠）
    if (nextQuote > 0 && jsonString.charAt(nextQuote - 1) == '\\') {
      // 这是转义引号，继续查找下一个
      searchPos = nextQuote + 1;
    } else {
      // 这是真正的结束引号
      dataEnd = nextQuote;
      break;
    }
  }
  
  // 提取data字段的值
  String result = jsonString.substring(dataStart, dataEnd);
  Serial.print("提取的data值: ");
  Serial.println(result);
  return result;
}


// 显示LORA采集器信息界面
void ShowLORADashboard(String lora_data, bool wifi_online, bool mqtt_connected, String group_id) {
  TFT_SetTextColor(WHITE, BLUE);
  TFT_SetFontSize(SONG_STYLE24);
  TFT_ShowString(30, 6, "LoRa采集器");
  TFT_ShowWiFi(220, 15, wifi_online);
  TFT_SetFontSize(SONG_STYLE12);
  TFT_SetTextColor(wifi_online ? WHITE : RED, BLUE);
  TFT_ShowString(195, 20, wifi_online ? "在线" : "离线");
  TFT_SetTextColor(WHITE, BLUE);
  TFT_ShowString(195, 4, mqtt_connected ? "MQTT" : " ");
  TFT_SetFontSize(SONG_STYLE24);
  TFT_SetTextColor(BLUE, BLACK);
  TFT_ShowString(20, 80, "数据:");
  TFT_ClearArea(0, 104, 240, 96);
  if (lora_data == "") {
    // 无数据时不显示任何内容
  } else {
    // 如果数据太长，截断显示
    TFT_SetTextColor(GREEN, BLACK);
    String display_data = lora_data;
    if (display_data.length() > 70) {
      display_data = display_data.substring(0, 70);
    }
    TFT_ShowString(20, 110, display_data.c_str());
  }
  TFT_SetFontSize(SONG_STYLE16);
  TFT_SetTextColor(YELLOW, BLACK);
  char group_str[20];
  sprintf(group_str, "设备ID: %s", group_id.c_str());
  TFT_ShowString(50, 200, group_str);
}


