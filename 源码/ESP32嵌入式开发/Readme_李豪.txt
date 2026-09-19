# 小组作业源码说明

本目录为「物联网项目实践」第 1 组大作业的 ESP32 端固件源码，遵循《MQTT通信协议规范_第1组》V2.0。
MQTT 服务器：yunyismart.tech:1883，主题统一为 202609_PP2/1/cmd、/report、/online。
当前包含两个模块的 PlatformIO 工程。

---

## 一、光照度传感器模块（8 号板）

工程目录：光照\esp32_light_ir
源码文件：esp32_light_ir.ino

### 功能

- 自动识别光照传感器：优先检测 I2C 的 BH1750 数字光照传感器，未检测到时回退为光敏电阻（ADC，GPIO34）；
- 每 2 秒采集一次光照度（单位 lx，保留 1 位小数），以 dat 报文周期上报；
- 连接 MQTT 后立即发布 online 上线报文（Retain），异常掉线时由服务器代发 offline 遗嘱报文；
- 订阅 cmd 主题接收下行命令；本板为纯传感器板，无对应执行器，收到命令仅作提示并忽略。

### 硬件接线

- BH1750：SDA=GPIO21，SCL=GPIO22，VCC=3.3V，GND=GND
- 光敏电阻：信号 -> GPIO34（未接 BH1750 时生效）

### MQTT 主题与报文

| 方向 | 主题 | QoS/Retain | 说明 |
|---|---|---|---|
| 订阅 | 202609_PP2/1/cmd | QoS 1 | 下行命令，本板只订阅 cmd |
| 发布 | 202609_PP2/1/report | QoS 0 | 每 2 秒一条 dat 光照数据 |
| 发布 | 202609_PP2/1/online | QoS 1，Retain | sys 上线报文 + 遗嘱离线报文 |

报文示例（信封 + 载荷）：

```json
{"ver":"1.0","type":"dat","seq":1,"src":"8","dst":0,
 "body":{"device":"light_sensor","samples":[{"key":"illuminance","value":125.5}]}}
```

### 依赖库

PubSubClient、ArduinoJson

---

## 二、红外对射传感器模块（3 号板）

工程目录：红外\esp32_ir_beam
源码文件：esp32_ir_beam.ino

### 功能

- 检测红外对射光束是否被遮挡（DO 数字输入，HIGH=遮挡）；
- 状态变化时立即上报 evt 事件：遮挡为 blocked（等级 2 警告），恢复为 clear（等级 0）；
- 状态持续不变时每 30 秒发送一次 evt 心跳报文，便于平台判断传感器是否掉线；
- 软件去抖 50ms（状态连续稳定才确认），同一状态 500ms 内不重复上报；
- 选做扩展：每次确认遮挡后累计次数 +1，以 dat 报文上报 count（协议表 20）；
- 连接 MQTT 后立即发布 online 上线报文（Retain），异常掉线时由服务器代发 offline 遗嘱报文；
- 订阅 cmd 主题接收下行命令；本板为纯传感器板，无对应执行器，收到命令仅作提示并忽略。

### 硬件接线

- 红外对射 DO -> GPIO14
- VCC -> 3.3V 或 5V
- GND -> GND

### MQTT 主题与报文

| 方向 | 主题 | QoS/Retain | 说明 |
|---|---|---|---|
| 订阅 | 202609_PP2/1/cmd | QoS 1 | 下行命令，本板只订阅 cmd |
| 发布 | 202609_PP2/1/report | QoS 0 | evt 事件（立即）与 dat 数据（计数） |
| 发布 | 202609_PP2/1/online | QoS 1，Retain | sys 上线报文 + 遗嘱离线报文 |

报文示例（信封 + 载荷）：

```json
{"ver":"1.0","type":"evt","seq":1,"src":"3","dst":0,
 "body":{"device":"ir_beam","event":"blocked","level":2}}
{"ver":"1.0","type":"dat","seq":2,"src":"3","dst":0,
 "body":{"device":"ir_beam","samples":[{"key":"count","value":1}]}}
```

### 依赖库

PubSubClient、ArduinoJson

---

## 编译与烧录

- 环境：PlatformIO（VSCode 插件），开发板 esp32dev，框架 Arduino；
- 每个工程目录独立打开编译；上传前请确认 platformio.ini 中的 upload_port 与目标板实际串口号一致；
- 串口波特率 115200，用于观察连接状态与上报日志；
- WiFi 名称与密码在各 .ino 的「可修改配置」区修改（默认 SSID "333"）。
