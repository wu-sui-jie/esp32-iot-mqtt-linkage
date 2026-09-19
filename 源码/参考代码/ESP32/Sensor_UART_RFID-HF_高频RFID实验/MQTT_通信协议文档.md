# MQTT通信协议文档

## 概述
本文档描述了ESP32 RFID读卡系统的MQTT通信协议，包括数据格式、主题结构、消息类型等。

## 基本信息
- **MQTT Broker**: 用户配置的MQTT服务器
- **端口**: 1883
- **QoS**: 0
- **客户端ID**: "home" + WiFi MAC地址

## 主题结构

### 发布主题 (Publish Topic)
```
/IOT/{设备ID}/post
```

**示例**:
```
/IOT/JYWL0001/post
```

### 订阅主题 (Subscribe Topic)
```
/IOT/{设备ID}/set
```

**示例**:
```
/IOT/JYWL0001/set
```

## 数据格式

### 1. RFID读卡数据上报

#### 消息格式
```json
{
  "commType": "Wifi",
  "sensorType": "RFID_HF",
  "data": {
    "card_id": "12345678"
  }
}
```

#### 字段说明
- **commType**: 通信类型，固定为 "Wifi"
- **sensorType**: 传感器类型，固定为 "RFID_HF"（高频RFID）
- **data**: 数据对象
  - **card_id**: RFID卡号，4字节十六进制字符串（大写）

#### 示例数据
```json
{
  "commType": "Wifi",
  "sensorType": "RFID_HF",
  "data": {
    "card_id": "A1B2C3D4"
  }
}
```

#### 发送条件
- 检测到新的RFID卡时（卡号变化）
- 每10秒的第5秒定时上报（心跳）

### 2. 系统状态数据

#### 消息格式
```json
{
  "commType": "Wifi",
  "sensorType": "STATUS",
  "data": {
    "wifi_status": "connected",
    "mqtt_status": "connected",
    "device_id": "JYWL0001",
    "timestamp": 1640995200
  }
}
```

## 通信流程

### 1. 连接建立
1. ESP32连接到WiFi网络
2. 连接到MQTT Broker
3. 订阅控制主题 `/IOT/{设备ID}/set`
4. 开始发送数据到发布主题 `/IOT/{设备ID}/post`

### 2. 数据上报
1. 每秒读取RFID模块
2. 检测到卡号变化时立即上报
3. 定时心跳上报（每10秒的第5秒）

### 3. 断线重连
- 检测到MQTT连接断开时自动重连
- 重连成功后重新订阅主题

## 错误处理

### 1. 网络错误
- WiFi连接断开时自动重连
- MQTT连接断开时自动重连
- 重连失败时等待2秒后重试

### 2. 数据错误
- RFID模块无响应时返回空字符串
- 响应格式错误时忽略本次数据
- JSON格式错误时记录日志

## 配置参数

### 设备配置
- **设备ID**: 用户配置的设备唯一标识
- **WiFi SSID**: 无线网络名称
- **WiFi密码**: 无线网络密码
- **MQTT服务器**: MQTT Broker地址
- **传感器类型**: "01"（RFID模块）

### 通信参数
- **MQTT端口**: 1883
- **MQTT用户名**: 空
- **MQTT密码**: 空
- **QoS级别**: 0
- **心跳间隔**: 10秒

## 数据示例

### 正常读卡
```json
{
  "commType": "Wifi",
  "sensorType": "RFID_HF",
  "data": {
    "card_id": "ABCD1234"
  }
}
```

### 无卡状态
```json
{
  "commType": "Wifi",
  "sensorType": "RFID_HF",
  "data": {
    "card_id": ""
  }
}
```

## 调试信息

### 串口输出
- RFID模块初始化状态
- 读卡命令发送日志
- 响应数据解析结果
- MQTT连接状态
- 数据发送日志

### 错误日志
- WiFi连接失败
- MQTT连接失败
- RFID通信错误
- JSON格式错误

## 注意事项

1. **数据格式**: 所有JSON数据必须使用UTF-8编码
2. **卡号格式**: RFID卡号为4字节十六进制字符串，自动转换为大写
3. **发送频率**: 避免过于频繁的数据发送，建议间隔不少于1秒
4. **网络稳定性**: 确保WiFi网络稳定，避免频繁断线重连
5. **MQTT Broker**: 确保MQTT服务器支持QoS 0级别的消息

## 版本信息
- **协议版本**: v1.0
- **最后更新**: 2024年
- **兼容性**: ESP32 Arduino框架 