#include "wifi_mqtt.h"
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

// JSON数据格式定义 - 修改为心率血氧传感器
#define JSON_DATA_HEART_SPO2 "{\"bpm\":%.1f,\"SpO2\":%.1f}"

// 心率血氧数据变量
float heart_rate = 0.0;
float spo2_value = 0.0;
float heart_rate_valid = 0.0;  // 存储最后一次有效的心率数据
float spo2_value_valid = 0.0;  // 存储最后一次有效的血氧数据
float heart_rate_display = 0.0;  // 当前显示的心率数据
float spo2_value_display = 0.0;  // 当前显示的血氧数据
bool mqtt_sent_this_second = false;  // 防止同一秒内重复发送MQTT
bool display_initialized = false;  // 显示是否已初始化

// 数据有效性检查函数
bool isValidHeartRate(float hr) {
  // 0值表示等待数据，不算错误数据
  if (hr == 0.0) return true;
  return (hr >= 40.0 && hr <= 200.0);  // 心率范围：40-200 bpm
}

bool isValidSpO2(float spo2) {
  // 0值表示等待数据，不算错误数据
  if (spo2 == 0.0) return true;
  return (spo2 >= 40.0 && spo2 <= 100.0);  // 血氧范围：40-100%
}

// 初始化心率血氧传感器（串口2）
void HeartSpO2_init() {
  Serial2.begin(115200);
  Serial.println("Heart rate and SpO2 sensor initialized on Serial2 (115200 baud)");
  
  // 等待传感器稳定
  delay(1000);
  
  // 检查传感器是否响应
  Serial.println("Checking sensor connection...");
  Serial2.println("AT");  // 发送测试命令
  delay(500);
  
  if (Serial2.available()) {
    String response = Serial2.readString();
    Serial.print("Sensor response: ");
    Serial.println(response);
  } else {
    Serial.println("Warning: No response from sensor, check connection");
  }
}

// 读取心率血氧数据 - 优化版本，直接解析并更新全局变量
bool readHeartSpO2Data() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    data.trim();
    if (data.length() > 0) {
      Serial.print("Received from Serial2: ");
      Serial.println(data);
      
      // 检查数据格式是否正确
      if (data.indexOf("Heart rate:") != -1 && data.indexOf("SpO2:") != -1) {
        float temp_heart_rate = 0.0;
        float temp_spo2_value = 0.0;
        
        // 直接解析心率数据 - 适配实际数据格式 "Heart rate:0.00bpm"
        int heart_start = data.indexOf("Heart rate:");
        if (heart_start != -1) {
          int heart_end = data.indexOf("bpm", heart_start);
          if (heart_end != -1) {
            String heart_str = data.substring(heart_start + 11, heart_end);
            temp_heart_rate = heart_str.toFloat();
            Serial.print("Parsed heart rate: ");
            Serial.println(temp_heart_rate);
          }
        }
        
        // 直接解析血氧数据 - 适配实际数据格式 "SpO2:0%"
        int spo2_start = data.indexOf("SpO2:");
        if (spo2_start != -1) {
          int spo2_end = data.indexOf("%", spo2_start);
          if (spo2_end != -1) {
            String spo2_str = data.substring(spo2_start + 5, spo2_end);
            temp_spo2_value = spo2_str.toFloat();
            Serial.print("Parsed SpO2: ");
            Serial.println(temp_spo2_value);
          }
        }
        
        // 数据有效性检查和错误处理
        bool heart_rate_valid_flag = isValidHeartRate(temp_heart_rate);
        bool spo2_valid_flag = isValidSpO2(temp_spo2_value);
        
        // 处理心率数据
        if (heart_rate_valid_flag) {
          heart_rate = temp_heart_rate;
          // 只有当数据不为0时才更新有效数据
          if (temp_heart_rate > 0) {
            heart_rate_valid = temp_heart_rate;
          }
          Serial.print("Heart rate updated to: ");
          Serial.println(heart_rate);
        } else {
          // 使用上一次的有效数据
          heart_rate = heart_rate_valid;
          Serial.print("Invalid heart rate: ");
          Serial.print(temp_heart_rate);
          Serial.print(" bpm, using previous valid value: ");
          Serial.print(heart_rate_valid);
          Serial.println(" bpm");
        }
        
        // 处理血氧数据
        if (spo2_valid_flag) {
          spo2_value = temp_spo2_value;
          // 只有当数据不为0时才更新有效数据
          if (temp_spo2_value > 0) {
            spo2_value_valid = temp_spo2_value;
          }
          Serial.print("SpO2 updated to: ");
          Serial.println(spo2_value);
        } else {
          // 使用上一次的有效数据
          spo2_value = spo2_value_valid;
          Serial.print("Invalid SpO2: ");
          Serial.print(temp_spo2_value);
          Serial.print("%, using previous valid value: ");
          Serial.print(spo2_value_valid);
          Serial.println("%");
        }
        
        // 返回是否成功解析数据（包括0值）
        return true;
      }
    }
  }
  return false;
}


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
  TFT_ShowString(10, 35, "（心率）系统启动中...");
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
      Serial.println("Heart rate and SpO2 sensor init...");
      HeartSpO2_init();
    } 
    TFT_ShowProgressBar(10, 150, 200, 15, 100, GREEN, BLACK);
    delay(1000);
    TFT_ClearScreen();
    // 直接显示心率血氧监测界面，显示等待数据
    ShowHeartSpO2Dashboard(0.0, 0.0, wifi_con_flag, mqttstate, groupid);
    display_initialized = true;
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

    // 每1秒执行一次，second每1秒增加1
    if (millis() - lastMsMain >= 1000) {
      lastMsMain = millis();
      second++; // 每1秒second加1
      if (second > 30000)
        second = 0;
      mqtt_sent_this_second = false;  // 重置MQTT发送标志
    }

      mainFunction();  //主功能函数
    }
  }



void mainFunction() {
  if (sensortype == "01") {
    bool dataReceived = readHeartSpO2Data();
    if (dataReceived) {
      Serial.print("检测到心率血氧数据: 心率=");
      Serial.print(heart_rate);
      Serial.print(" bpm, 血氧=");
      Serial.print(spo2_value);
      Serial.println("%");
    }
    
    // 只有在显示初始化完成后才开始更新显示
    if (display_initialized) {
      // 检查显示数据是否需要更新
      bool need_update_display = false;
      if (heart_rate != heart_rate_display || spo2_value != spo2_value_display) {
        heart_rate_display = heart_rate;
        spo2_value_display = spo2_value;
        need_update_display = true;
      }
      
      // 首次运行或数据变化时更新显示
      static bool first_run = true;
      
      if (need_update_display || first_run) {
        Serial.print("Updating display - heart_rate: ");
        Serial.print(heart_rate_display);
        Serial.print(", spo2: ");
        Serial.println(spo2_value_display);
        
        ShowHeartSpO2Dashboard(heart_rate_display, spo2_value_display, wifi_con_flag, mqttstate, groupid);
        first_run = false;
      }
    }
    
    // 每5秒发送一次MQTT数据
    if (second % 5 == 0 && !mqtt_sent_this_second) {
      cleanBuffer(ATdata, BUF_DATA_LEN);
      len = snprintf(ATdata, BUF_DATA_LEN, JSON_DATA_HEART_SPO2, heart_rate, spo2_value);
      Serial.print("Sending MQTT data every 5 seconds: ");
      Serial.println(ATdata);
      postMsg("Wifi", "HeartRate", ATdata);
      mqtt_sent_this_second = true;  // 标记已发送
    }
  }
}


void cleanBuffer(char* buf, int len) {
  for (int i = 0; i < len; i++) {
    buf[i] = '\0';
  }
}


// 显示心率血氧传感器信息界面
void ShowHeartSpO2Dashboard(float heart_rate, float spo2_value, bool wifi_online, bool mqtt_connected, String group_id) {
  static bool first_draw = true;
  static float last_heart_rate = -1;
  static float last_spo2_value = -1;
  static bool last_wifi_online = false;
  static bool last_mqtt_connected = false;
  
  // 首次绘制或状态变化时绘制固定元素
  if (first_draw || (wifi_online != last_wifi_online) || (mqtt_connected != last_mqtt_connected)) {
    Serial.println("Drawing dashboard interface...");
    
    // 先清屏，确保背景干净
    TFT_ClearScreen();
    
    // 绘制蓝色背景条 - 确保覆盖整个顶部区域
    TFT_FillRect(0, 0, 240, 45, BLUE);
    Serial.println("Blue background bar drawn");
    
    // 在蓝色背景上显示文字
    TFT_SetTextColor(WHITE, BLUE);
    TFT_SetFontSize(SONG_STYLE24);
    TFT_ShowString(30, 8, "心率血氧监测");
    TFT_ShowWiFi(220, 15, wifi_online);
    TFT_SetFontSize(SONG_STYLE12);
    TFT_SetTextColor(wifi_online ? WHITE : RED, BLUE);
    TFT_ShowString(195, 22, wifi_online ? "在线" : "离线");
    TFT_SetTextColor(WHITE, BLUE);
    TFT_ShowString(195, 6, mqtt_connected ? "MQTT" : " ");
    
    // 绘制标签
    TFT_SetFontSize(SONG_STYLE24);
    TFT_SetTextColor(RED, BLACK);
    TFT_ShowString(30, 70, "心率:");
    TFT_SetTextColor(GREEN, BLACK);
    TFT_ShowString(30, 110, "血氧:");
    
    // 绘制设备ID
    TFT_SetFontSize(SONG_STYLE16);
    TFT_SetTextColor(YELLOW, BLACK);
    char group_str[20];
    sprintf(group_str, "设备ID: %s", group_id.c_str());
    TFT_ShowString(50, 200, group_str);
    
    first_draw = false;
    last_wifi_online = wifi_online;
    last_mqtt_connected = mqtt_connected;
    
    // 重置数据显示状态，确保首次绘制时显示数据
    last_heart_rate = -1;
    last_spo2_value = -1;
  }
  
  // 只在心率数据变化时更新心率显示
  if (heart_rate != last_heart_rate || first_draw) {
    Serial.print("Updating heart rate display: ");
    Serial.println(heart_rate);
    TFT_ClearArea(100, 70, 120, 32);
    TFT_SetFontSize(SONG_STYLE24);
    TFT_SetTextColor(RED, BLACK);
    if (heart_rate > 0) {
      char heart_str[20];
      sprintf(heart_str, "%.1f bpm", heart_rate);
      TFT_ShowString(100, 70, heart_str);
      Serial.println("Displayed heart rate value");
    } else {
      TFT_ShowString(100, 70, "等待数据");
      Serial.println("Displayed waiting for heart rate data");
    }
    last_heart_rate = heart_rate;
  }
  
  // 只在血氧数据变化时更新血氧显示
  if (spo2_value != last_spo2_value || first_draw) {
    Serial.print("Updating SpO2 display: ");
    Serial.println(spo2_value);
    TFT_ClearArea(100, 110, 120, 32);
    TFT_SetFontSize(SONG_STYLE24);
    TFT_SetTextColor(GREEN, BLACK);
    if (spo2_value > 0) {
      char spo2_str[20];
      sprintf(spo2_str, "%.1f %%", spo2_value);
      TFT_ShowString(100, 110, spo2_str);
      Serial.println("Displayed SpO2 value");
    } else {
      TFT_ShowString(100, 110, "等待数据");
      Serial.println("Displayed waiting for SpO2 data");
    }
    last_spo2_value = spo2_value;
  }
}




