# 1 号板 · 舵机模块（servo）

## 这个模块是干什么的

控制一个舵机转到指定角度。它有两件事同时在做：

1. **听平台命令**——平台下发目标角度，舵机平滑转过去，转到位之后回执确认
2. **听其他板的状态**——3 号板红外对射检测到遮挡时，自己把舵机转过去，不经过平台

舵机走的是**平滑轨迹**：内部把大角度差拆成"每 10 毫秒走 1 度"，所以 0° 转到 180° 要看它慢慢走大约 1.8 秒，这是正常现象，不是卡住了。

板子上还有一颗 LED 会在转动期间点亮，转到位就灭，用来在没接电脑的时候一眼看出有没有在动作。

---

## 硬件与接线

| 项目 | 引脚 | 说明 |
|---|---|---|
| 舵机信号线 | GPIO14 | 橙线（信号），必须接这个脚 |
| 转动指示 LED | GPIO15 | 板载，低电平点亮，转动期间亮 |
| 舵机电源 | 5V 独立供电 | 见下面的注意事项 |

**舵机供电必须注意**：舵机堵转时电流能超过 1 A，绝不能从 ESP32 的 3.3V 引脚取电，也不能只靠 USB 供电。要用实验箱的 5V 排针单独给舵机，而且**舵机电源的地必须和 ESP32 的地接在一起**（共地），否则信号电平没有参考，舵机会乱转或者完全不动。

---

## 用 MQTTX 调试

### 连接设置

| 参数 | 值 |
|---|---|
| 服务器 | `yunyismart.tech` |
| 端口 | `1883` |
| 用户名 / 密码 | 留空 |
| Client ID | 自动生成即可 |

### 订阅

订阅主题填 `202609_PP2/#`，一次把本组三条主题全订阅上，调试时能看到所有收发。

### 发布

| 参数 | 值 |
|---|---|
| 主题 | `202609_PP2/1/cmd` |
| QoS | 1 |
| Retain | **不勾** |
| Payload 格式 | Plaintext（先别用 JSON，避开本地校验拦截） |

---

## 单独调试与验证

这块板可以**脱离其他七块板单独验证**。需要的东西：本板 + USB 线 + MQTTX。

### 第 1 步：上电自检

烧录后打开串口监视器（波特率 115200），正常应该依次出现：

```
[proto] WiFi 已连接，IP = 192.168.x.x
[proto] MQTT 已连接
[proto] 已订阅 202609_PP2/1/cmd
[servo] 初始化完成，已回到中位 90°
```

**卡住时按这个顺序排查**：

| 串口停在这里 | 说明 |
|---|---|
| `连接 WiFi` 后面一直点点点 | 热点名字或密码不对，改本模块 `my_config.h` 里的 `WIFI_SSID` / `WIFI_PASSWORD` |
| `MQTT 连接中...` 反复刷 | 服务器地址不对，或者网络没放行 1883 端口 |
| 没有最后那一行模块日志 | 外设接线问题，见下面「常见问题」 |

### 第 2 步：验证清单

| # | 检查项 | 怎么做 | 通过标准 |
|---|---|---|---|
| 1 | 板子活着 | 看串口 | 打印出上面那段启动日志 |
| 2 | 网络通 | 看串口 | 出现 `[proto] MQTT 已连接` 和订阅成功 |
| 3 | 能收命令 | MQTTX 发一条 `set_angle` 命令 | 舵机平滑转过去，转到位置后才回 ack，param 里的 angle 和发的一致 |
| 4 | 能回执 | 发完命令后看订阅区 | 收到 ack，且 seq 与刚才那条命令一致 |
| 5 | 联动（可选） | 往 report 发一条 src=3 的 `blocked` 事件 | 舵机自动转到 0°，并发出一条 `auto_move` |

这一轮走完，说明这块板本身没有问题了。具体的报文内容和预期现象在下面各节里。
重点确认：舵机是不是收到命令就动、转到位才回执。

## 报文格式与逐条测试

### 下行命令（你发给板子）

**转到 90 度**——舵机会平滑转过去，约 0.5 秒到位，然后回执

```json
{"ver":"1.0","type":"cmd","seq":1,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle","param":{"angle":90}}}
```

**转到 180 度**——全程约 1.8 秒，别以为卡住了

```json
{"ver":"1.0","type":"cmd","seq":2,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle","param":{"angle":180}}}
```

**转到 0 度**

```json
{"ver":"1.0","type":"cmd","seq":3,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle","param":{"angle":0}}}
```

### 参数非法的情况（舵机保持原位不动，回执 result 为 fail）

**角度越界 200**

```json
{"ver":"1.0","type":"cmd","seq":4,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle","param":{"angle":200}}}
```

**不是整数 90.5**

```json
{"ver":"1.0","type":"cmd","seq":5,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle","param":{"angle":90.5}}}
```

**根本没带 param**

```json
{"ver":"1.0","type":"cmd","seq":6,"src":"web","dst":1,"body":{"device":"servo","action":"set_angle"}}
```

> 这三种情况都是**宁可不动作，不能误动作**。参数校验和动作执行在同一步完成，校验不过就不进入执行分支——把 135° 误当成 0° 甩过去会打到限位、烧齿轮。

### 广播与其他板号

**dst=0 表示广播**，本板照样执行

```json
{"ver":"1.0","type":"cmd","seq":7,"src":"web","dst":0,"body":{"device":"servo","action":"set_angle","param":{"angle":45}}}
```

**dst=7 是发给别的板的**，本板完全没反应、也不回执

```json
{"ver":"1.0","type":"cmd","seq":8,"src":"web","dst":7,"body":{"device":"servo","action":"set_angle","param":{"angle":90}}}
```

### 上行报文（板子发出来的，订阅区能看到）

**执行成功**（注意 seq 和命令一致，而且是**转到位之后**才发出来的）

```json
{"ver":"1.0","type":"ack","seq":1,"src":"1","dst":0,"body":{"device":"servo","action":"set_angle","result":"ok","param":{"angle":90}}}
```

**执行失败**（不带 param）

```json
{"ver":"1.0","type":"ack","seq":4,"src":"1","dst":0,"body":{"device":"servo","action":"set_angle","result":"fail"}}
```

**联动转动**（收到 3 号板的遮挡事件后自己转的，不是平台命令）

```json
{"ver":"1.0","type":"evt","seq":31,"src":"1","dst":0,"body":{"device":"servo","event":"auto_move","level":1}}
{"ver":"1.0","type":"evt","seq":32,"src":"1","dst":0,"body":{"device":"servo","event":"auto_home","level":0}}
```

**上线**（发到 `202609_PP2/1/online`，Retain）

```json
{"ver":"1.0","type":"sys","seq":1,"src":"1","dst":0,"body":{"device":"sys","event":"online","uptime":12}}
```

---

## 联动行为

本板订阅了 report 主题来听 3 号板的消息：

| 收到的消息 | 本板的动作 |
|---|---|
| 3 号板 `evt blocked`（光束被遮挡） | 舵机转到 **0°**，并发一条 `auto_move` |
| 3 号板 `evt clear`（光束恢复） | 舵机回到 **90°**，并发一条 `auto_home` |

几个细节：

- 3 号板在状态不变时每 30 秒会发一次心跳 evt，那是防平台漏报用的，**不会**让舵机重复转动——本板只在状态真的跳变时才动作
- 舵机还在转的时候收到新的事件，会等它停稳再应用，不会中途打断
- 目标角度和当前角度一样时，既不再转一次也不再上报

### 想现场调这个联动

改 `src/my_config.h`：

```c
#define LINK_SERVO_BLOCKED_DEG 0    // 遮挡时转到
#define LINK_SERVO_CLEAR_DEG 90     // 恢复时回到
```

想看联动效果但手头没有 3 号板，可以用 MQTTX 模拟——往 `202609_PP2/1/report` 发（注意 src 必须写 **3**，否则会被当成别的板发的）：

```json
{"ver":"1.0","type":"evt","seq":58,"src":"3","dst":0,"body":{"device":"ir_beam","event":"blocked","level":2}}
```

---

## 其他可调参数

都在 `src/my_config.h` 里：

```c
#define SERVO_PIN 14            // 舵机信号引脚
#define PIN_LED1 15             // 转动指示 LED
#define SERVO_STEP_DEG 1        // 平滑步长（度）
#define SERVO_STEP_MS 10        // 每步间隔（毫秒），两个一起决定转动快慢
#define SERVO_HOME_DEG 90       // 上电回到的中位角
```

---

## 常见问题

| 现象 | 先查什么 |
|---|---|
| 舵机完全不动 | 5V 独立供电、共地；信号线是不是 GPIO14 |
| 舵机乱转或抖动 | 供电不足，或者没共地 |
| 转到一半就停 | 是不是被新命令打断了（后来的目标会覆盖先前的） |
| ESP32 反复重启 | 舵机从板子取电了，电流不够 |
| 串口卡在 `Connecting` | 按住 BOOT → 点一下 EN → 松开 EN → 松开 BOOT，再点 Upload |

---

## 文件说明

| 文件 | 内容 |
|---|---|
| `src/main.cpp` | 程序入口，命令分发 |
| `src/servo.cpp/.h` | 角度校验、平滑转动状态机、LED 指示 |
| `src/linkage.cpp/.h` | 对射联动的规则 |
| `src/mqtt_proto.cpp/.h` | 协议公共层（与其余七块板相同） |
| `src/my_config.h` | 全部可调参数 |
