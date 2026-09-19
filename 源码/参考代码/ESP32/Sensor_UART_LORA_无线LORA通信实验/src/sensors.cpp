#include "Arduino.h"
#include "sensors.h"

String readLORAData(void) {
  String loraData = "";
  
  // 检查串口2是否有可用数据
  if (Serial2.available()) {
    // 等待一小段时间确保数据接收完整
    delay(10);
    
    // 读取所有可用数据
    while (Serial2.available()) {
      char c = Serial2.read();
      loraData += c;
      // 每读取一个字符后短暂等待，确保数据完整性
      delay(1);
    }
    
    // 再次等待确保没有更多数据
    delay(5);
    
    // 如果还有数据，继续读取
    while (Serial2.available()) {
      char c = Serial2.read();
      loraData += c;
    }
    
    // 去除首尾空白字符
    loraData.trim();
    
    // 验证数据完整性（检查JSON格式是否完整）
    if (loraData.length() > 0) {
      // 检查是否以{开始，以}结束
      if (loraData.startsWith("{") && loraData.endsWith("}")) {
        Serial.print("数据接收完整: ");
        Serial.println(loraData);
        return loraData;
      } else {
        Serial.print("数据不完整，丢弃: ");
        Serial.println(loraData);
        return "";
      }
    }
  }
  
  return ""; // 没有数据时返回空字符串
}

