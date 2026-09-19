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

void RFID_init() {
  Serial2.begin(115200); // 初始化串口2，波特率115200
  Serial.println("RFID模块初始化完成!");
}

String readRFIDCard(void) {
  // 发送读卡命令
  String command = RFID_READ_CMD;
  command.replace(" ", ""); // 移除空格
  
  // 将命令转换为字节数组并发送
  byte cmdBytes[9];
  hexStringToBytes(command, cmdBytes, 9);
  Serial2.write(cmdBytes, 9);
  
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
  if (response.length() < 8) {
    return ""; // 响应太短，无效
  }
  
  // 检查响应头
  String responseHeader = response.substring(0, 8);
  if (responseHeader != "aabbcccc") {
    return ""; // 响应头不匹配
  }
  
  // 检查是否为无卡响应（8字节 = 16个十六进制字符）
  if (response.length() == 16 && response == "aabbcccc0002200123") {
    return ""; // 无卡
  }
  
  // 检查是否为有效读卡响应（17字节 = 34个十六进制字符）
  if (response.length() == 34) {
    // 提取第13、14、15、16字节（对应字符串位置24-31）
    String cardBytes = response.substring(24, 32);
    // 手动转换为大写
    for (int i = 0; i < cardBytes.length(); i++) {
      if (cardBytes[i] >= 'a' && cardBytes[i] <= 'f') {
        cardBytes[i] = cardBytes[i] - 32;
      }
    }
    return cardBytes;
  }
  
  return ""; // 其他情况返回空字符串
}

