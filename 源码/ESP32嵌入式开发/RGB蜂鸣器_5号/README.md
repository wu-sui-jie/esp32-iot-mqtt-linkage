# 5 号板 · ADC / RGB / 蜂鸣器综合模块（rgb、buzzer）

## 这个模块是干什么的

一块板子上挂了三个设备，本次实现了其中两个：

| device | 是什么 | 本次状态 |
|---|---|---|
| `rgb` | WS2812B 彩灯 × 4 颗 | 已实现 |
| `buzzer` | 蜂鸣器 | 已实现 |
| `adc` | 电位器 | 本次不实现 |

除了听平台命令，它还承担**两条板端联动**——自己根据其他板的状态决定灯和蜂鸣器怎么动，不经过平台：

- 光照过暗 → 亮暖白光当照明
- 检测到火焰 → 多彩循环 + 蜂鸣器间歇报警

因为两个场景抢同一组灯和同一个蜂鸣器，板内有一套优先级仲裁，见下面「场景仲裁」一节。

---

## 硬件与接线

| 项目 | 引脚 | 说明 |
|---|---|---|
| WS2812B 数据线 | GPIO27 | 4 颗灯串联，LED1~LED4 |
| 蜂鸣器 | GPIO14 | 经 S8050 三极管驱动，高电平导通 |
| 电位器 | GPIO35 | 输入专用脚，本次未实现 |

蜂鸣器是无源的（原理图标注 3 kHz，那是它的谐振频率），所以代码用 3 kHz 方波驱动。万一实际装的是有源蜂鸣器且声音发闷，把 `src/my_config.h` 里的 `BUZZER_USE_PWM` 改成 0 改用电平驱动。

---

## 用 MQTTX 调试

### 连接设置

| 参数 | 值 |
|---|---|
| 服务器 | `yunyismart.tech` |
| 端口 | `1883` |
| 用户名 / 密码 | 留空 |

### 订阅与发布

| 参数 | 值 |
|---|---|
| 订阅主题 | `202609_PP2/#` |
| 发布主题 | `202609_PP2/1/cmd` |
| QoS | 1 |
| Retain | 不勾 |
| Payload 格式 | Plaintext |

---

## 单独调试与验证

这块板可以**脱离其他七块板单独验证**。需要的东西：本板 + USB 线 + MQTTX。

### 第 1 步：上电自检

烧录后打开串口监视器（波特率 115200），正常应该依次出现：

```
[proto] WiFi 已连接，IP = 192.168.x.x
[proto] MQTT 已连接
[proto] 已订阅 202609_PP2/1/cmd
[rgb] 显示颜色 r=0 g=0 b=0（熄灭）
[buzzer] 初始化完成（GPIO14，方波驱动），当前静音
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
| 3 | 能收命令 | MQTTX 发一条 `set_color` 或 `set_power`（buzzer） | 灯变颜色 / 蜂鸣器响，并且回了 ack |
| 4 | 能回执 | 发完命令后看订阅区 | 收到 ack，且 seq 与刚才那条命令一致 |
| 5 | 联动（可选） | 往 report 发一条 src=8 的光照过暗 dat，或 src=6 的火焰 detected | 灯亮暖白（或多彩循环 + 蜂鸣器），并发出 scene_* 事件 |

这一轮走完，说明这块板本身没有问题了。具体的报文内容和预期现象在下面各节里。
重点确认：灯和蜂鸣器是不是各自响应、参数非法时不动。

## 报文格式与逐条测试

### RGB 彩灯

**显示红色**（三通道各 0~255）

```json
{"ver":"1.0","type":"cmd","seq":1,"src":"web","dst":5,"body":{"device":"rgb","action":"set_color","param":{"r":255,"g":0,"b":0}}}
```

**蓝色**

```json
{"ver":"1.0","type":"cmd","seq":2,"src":"web","dst":5,"body":{"device":"rgb","action":"set_color","param":{"r":0,"g":0,"b":255}}}
```

**熄灭**——三通道全 0 就是熄灭（协议 §5.5 没有单独定义开关命令）

```json
{"ver":"1.0","type":"cmd","seq":3,"src":"web","dst":5,"body":{"device":"rgb","action":"set_color","param":{"r":0,"g":0,"b":0}}}
```

**参数非法的情况**（灯保持原状，回执 result 为 fail）

```json
{"ver":"1.0","type":"cmd","seq":4,"src":"web","dst":5,"body":{"device":"rgb","action":"set_color","param":{"r":300,"g":0,"b":0}}}
{"ver":"1.0","type":"cmd","seq":5,"src":"web","dst":5,"body":{"device":"rgb","action":"set_color","param":{"r":255,"g":0}}}
```

第一条越界，第二条缺了 `b` 通道。两种都会被拒绝——**不能"缺了就按 0 处理"**，那样一个残缺报文会把灯误熄灭。

**回执**（param 回发实际颜色）

```json
{"ver":"1.0","type":"ack","seq":1,"src":"5","dst":0,"body":{"device":"rgb","action":"set_color","result":"ok","param":{"r":255,"g":0,"b":0}}}
```

### 蜂鸣器

**持续鸣叫**（协议表 12 的必做项）

```json
{"ver":"1.0","type":"cmd","seq":6,"src":"web","dst":5,"body":{"device":"buzzer","action":"set_power","param":{"on":true}}}
```

**停止**

```json
{"ver":"1.0","type":"cmd","seq":7,"src":"web","dst":5,"body":{"device":"buzzer","action":"set_power","param":{"on":false}}}
```

**按时长鸣叫**（协议表 20 的选做项，单位毫秒，范围 50~5000）

```json
{"ver":"1.0","type":"cmd","seq":8,"src":"web","dst":5,"body":{"device":"buzzer","action":"beep","param":{"duration":500}}}
```

响 0.5 秒后自动停。超过 5000 或小于 50 会被拒绝并回 fail。

**回执**

```json
{"ver":"1.0","type":"ack","seq":6,"src":"5","dst":0,"body":{"device":"buzzer","action":"set_power","result":"ok","param":{"on":true}}}
```

### 上行事件（两条联动发出的）

```json
{"ver":"1.0","type":"evt","seq":30,"src":"5","dst":0,"body":{"device":"rgb","event":"scene_light","level":1}}
{"ver":"1.0","type":"evt","seq":31,"src":"5","dst":0,"body":{"device":"rgb","event":"scene_fire","level":3}}
{"ver":"1.0","type":"evt","seq":32,"src":"5","dst":0,"body":{"device":"rgb","event":"scene_off","level":0}}
```

---

## 两条联动

本板订阅了 report 主题，监听 8 号板的光照和 6 号板的火焰。

### 联动一：光照过暗 → 开照明

| 收到 | 动作 |
|---|---|
| 8 号板 illuminance ≤ **50 lx** | RGB 亮暖白光（r=255 g=200 b=120） |
| 8 号板 illuminance ≥ **120 lx** | RGB 熄灭 |

两个阈值分开是**迟滞**，只用一个门槛的话光照在门槛附近抖动时灯会闪个不停。

### 联动二：火焰 → 声光报警

| 收到 | 动作 |
|---|---|
| 6 号板 `evt detected` | RGB 在红→橙→黄之间循环（每 0.3 秒一色），蜂鸣器响 0.3 秒停 0.3 秒循环 |
| 6 号板 `evt clear` | 停声光，并按当前光照状态决定是转入照明还是熄灭 |

**超时兜底**：连续 **15 秒**一条火焰报文都没收到（6 号板烧着的时候每 5 秒会重发一次 detected），就认定火已经灭了或者 6 号板掉线了，自动把声光停掉。这样即使解除报文在路上丢了，也不会一直响到断电。

### 场景仲裁（火焰优先）

两个场景抢同一组硬件，优先级是：

```
火焰报警（最高）  >  平台手动命令  >  光照照明（最低）
```

实现上用的是**状态重算**而不是"来一条事件改一次灯"：板子把火焰、光照、平台手动三个输入都记成状态，任一变化就整体重算一次场景。这样事件以任何顺序到达，最终状态都一致，也不会留下"火焰灭了灯还红着"这类残影。

**平台命令会临时接管**：发一条 `set_color` 之后，联动暂停对灯光的控制（否则刚设好颜色就被联动改回去），直到下一次光照或火焰状态发生变化时才恢复。

### 没有传感器时怎么测联动

用 MQTTX 往 `202609_PP2/1/report` 发（**src 必须是被模拟板的号**）：

模拟光照过暗（src=8）：

```json
{"ver":"1.0","type":"dat","seq":76,"src":"8","dst":0,"body":{"device":"light_sensor","samples":[{"key":"illuminance","value":20.0}]}}
```

模拟检测到火焰（src=6）：

```json
{"ver":"1.0","type":"evt","seq":31,"src":"6","dst":0,"body":{"device":"flame","event":"detected","level":3}}
```

模拟火焰解除：

```json
{"ver":"1.0","type":"evt","seq":32,"src":"6","dst":0,"body":{"device":"flame","event":"clear","level":0}}
```

---

## 可调参数

都在 `src/my_config.h`：

```c
#define RGB_PIN 27              // WS2812B 数据线
#define BUZZER_PIN 14           // 蜂鸣器
#define BUZZER_USE_PWM 1        // 1=方波驱动（无源），0=电平驱动（有源）
#define BUZZER_FREQ 3000        // 方波频率，取原理图标注的谐振频率

#define FIRE_STEP_MS 300UL      // 火焰多彩循环每色停留
#define FIRE_BEEP_ON_MS 300UL   // 间歇报警响多久
#define FIRE_BEEP_OFF_MS 300UL  // 间歇报警停多久
#define FLAME_EVT_TIMEOUT_MS 15000UL  // 火焰超时兜底

#define LIGHT_DARK_LUX 50.0f    // 低于它就开灯
#define LIGHT_BRIGHT_LUX 120.0f // 高于它才关灯
#define LIGHT_SCENE_R 255       // 照明颜色
#define LIGHT_SCENE_G 200
#define LIGHT_SCENE_B 120
```

---

## 常见问题

| 现象 | 先查什么 |
|---|---|
| 灯不亮 | 数据线是不是 GPIO27；灯带供电（4 颗灯电流不小，别只靠板子供） |
| 蜂鸣器不响 | 引脚是不是 GPIO14；有源/无源与 `BUZZER_USE_PWM` 是否匹配 |
| 灯的颜色变了但不是你设的 | 被光照或火焰联动覆盖了，看串口会打印场景切换；发一条 `set_color` 可以临时接管 |
| 火焰灭了灯还在闪 | 看 6 号板串口：还在打印"火焰持续中"说明它仍判为有火；不再打印但灯还闪说明解除报文丢了，15 秒内会自动停 |

---

## 文件说明

| 文件 | 内容 |
|---|---|
| `src/main.cpp` | 程序入口，命令分发（rgb 与 buzzer 两个设备） |
| `src/rgb.cpp/.h` | 纯色显示、火焰多彩循环动画 |
| `src/buzzer.cpp/.h` | 持续鸣叫、定时鸣叫、间歇报警的状态机 |
| `src/linkage.cpp/.h` | 两条联动的规则与场景仲裁 |
| `src/mqtt_proto.cpp/.h` | 协议公共层（与其余七块板相同） |
| `src/my_config.h` | 全部可调参数 |
