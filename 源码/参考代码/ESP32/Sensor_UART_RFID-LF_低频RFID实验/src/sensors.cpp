#include "Arduino.h"
#include "sensors.h"

// 将十六进制字符串转换为字节数组
void hexStringToBytes(const String& hexString, byte* bytes, int maxLength) {
  String cleanHex = hexString;
  cleanHex.replace(" ", ""); // 移除空格
  
  int length = (cleanHex.length() / 2) < maxLength ? (cleanHex.length() / 2) : maxLength;
  for (int i = 0; i < length; i++) {
    String hexByte = cleanHex.substring(i * 2, i * 2 + 2);
    bytes[i] = (byte)strtol(hexByte.c_str(), NULL, 16);
  }
}

// 将字节数组转换为十六进制字符串
String bytesToHexString(const byte* bytes, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    if (bytes[i] < 16) {
      result += "0";
    }
    result += String(bytes[i], HEX);
  }
  // 手动转换为大写
  for (int i = 0; i < result.length(); i++) {
    if (result[i] >= 'a' && result[i] <= 'f') {
      result[i] = result[i] - 32;
    }
  }
  return result;
}

// 将十六进制字符串转换为10位十进制数
String hexToDecimal10Digit(const String& hexString) {
  // 移除空格
  String cleanHex = hexString;
  cleanHex.replace(" ", "");
  
  // 转换为长整型
  unsigned long decimalValue = strtoul(cleanHex.c_str(), NULL, 16);
  
  // 转换为10位字符串，不足前面补0
  String result = String(decimalValue);
  while (result.length() < 10) {
    result = "0" + result;
  }
  
  return result;
}

void RFID_init() {
  Serial2.begin(9600); // 初始化串口2，波特率9600
  Serial.println("RFID LF module init complete!");
}

String readRFIDCard(void) {
  // 发送读卡命令
  String command = RFID_READ_CMD;
  command.replace(" ", ""); // 移除空格
  
  // 将命令转换为字节数组并发送
  byte cmdBytes[8];
  hexStringToBytes(command, cmdBytes, 8);
  Serial.print("发送命令: ");
  Serial.println(command);
  Serial2.write(cmdBytes, 8);
  
  // 等待响应
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < 1000) { // 等待1秒
    if (Serial2.available()) {
      byte incomingByte = Serial2.read();
      if (incomingByte < 16) {
        response += "0";
      }
      response += String(incomingByte, HEX);
    }
    delay(1);
  }
  
  // 检查响应长度和格式
  if (response.length() < 6) {
    return ""; // 响应太短，无效
  }
  
  // 检查响应头 AA 00 07
  String responseHeader = response.substring(0, 6);
  if (responseHeader != "aa0007") {
    return ""; // 响应头不匹配
  }
  
  // 检查完整响应格式：AA 00 07 00 30 4F 00 7A 37 11 24 BB (24个字符)
  if (response.length() == 24) {
    // 打印完整的响应数据
    Serial.print("完整响应数据: ");
    Serial.println(response);
    
    // 提取卡号数据：00 7A 37 11 (位置12-19)，跳过厂商编码4F
    String cardHexData = response.substring(12, 20);
    Serial.print("提取的卡号十六进制: ");
    Serial.println(cardHexData);
    
    // 转换为10位十进制数
    String cardDecimal = hexToDecimal10Digit(cardHexData);
    Serial.print("转换后的十进制卡号: ");
    Serial.println(cardDecimal);
    
    return cardDecimal;
  }
  
  return ""; // 其他情况返回空字符串
}

