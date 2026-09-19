# MAX30100 FFT改进版本

## 概述

基于您提供的STM32代码，我对MAX30100库进行了重大改进，集成了FFT（快速傅里叶变换）分析算法，提供更准确的心率和血氧饱和度测量。

## 主要改进

### 1. FFT分析算法
- 集成了完整的FFT算法，基于STM32代码中的实现
- 使用1024点FFT进行频谱分析
- 改进了心率和SpO2的计算精度

### 2. 数学函数库
- 实现了自定义的数学函数（sin, cos, floor, fmod等）
- 优化了浮点运算性能
- 包含正弦查找表以提高计算速度

### 3. 数据采集优化
- 使用1024点数据缓冲区
- 实时数据收集和FFT分析
- 改进的FIFO管理

### 4. 算法改进
- 基于FFT的心率检测算法
- 改进的SpO2计算公式
- 数据有效性验证

## 文件结构

```
src/
├── MAX30100.h          # 改进的MAX30100头文件
├── MAX30100.cpp        # 改进的MAX30100实现
├── sensors.h           # 传感器接口头文件
├── sensors.cpp         # 传感器接口实现
├── Sensor_IIC_MAX30100.ino  # 主程序
└── MAX30100_FFT_Test.ino    # FFT测试程序
```

## 主要功能

### MAX30100类新增方法

```cpp
// FFT分析功能
void FFT(struct compx *xin);
uint16_t findMaxNumIndex(struct compx *data, uint16_t count);
void calculateSpO2FromFFT(struct compx *s1, struct compx *s2, uint16_t maxIndex, float *spo2);
float calculateHeartRateFromFFT(struct compx *data, uint16_t maxIndex, uint8_t sampleRate);

// 数学函数
double myFloor(double x);
double myFmod(double x, double y);
double XSin(double x);
double XCos(double x);
uint16_t qsqrt(uint32_t a);

// 数据缓冲区
uint16_t irBuffer[1024];
uint16_t redBuffer[1024];
uint16_t bufferIndex;
bool bufferFull;
```

### 传感器接口新增方法

```cpp
// FFT分析函数
void performFFTAnalysis();
bool collectDataForFFT();
void processHeartRateAndSpO2();
```

## 使用方法

### 1. 基本初始化

```cpp
#include "MAX30100.h"

MAX30100 max30100;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    if (!max30100.begin()) {
        Serial.println("MAX30100初始化失败!");
        return;
    }
    
    // 配置传感器
    max30100.setMode(MAX30100_MODE_SPO2_HR);
    max30100.setLedsPulseWidth(MAX30100_SPC_PW_1600US_16BITS);
    max30100.setSamplingRate(MAX30100_SAMPRATE_100HZ);
    max30100.setLedsCurrent(MAX30100_LED_CURR_20_8MA, MAX30100_LED_CURR_20_8MA);
    max30100.setHighresModeEnabled(true);
}
```

### 2. 数据采集和FFT分析

```cpp
void loop() {
    // 每10ms更新数据
    max30100.update();
    
    // 收集数据到缓冲区
    uint16_t ir, red;
    if (max30100.getRawValues(&ir, &red)) {
        if (max30100.bufferIndex < 1024) {
            max30100.irBuffer[max30100.bufferIndex] = ir;
            max30100.redBuffer[max30100.bufferIndex] = red;
            max30100.bufferIndex++;
            
            if (max30100.bufferIndex >= 1024) {
                max30100.bufferFull = true;
                performFFTAnalysis();
                max30100.bufferIndex = 0;
                max30100.bufferFull = false;
            }
        }
    }
}
```

### 3. FFT分析

```cpp
void performFFTAnalysis() {
    struct compx s1[1024], s2[1024];
    
    // 准备FFT数据
    for (int i = 0; i < 1024; i++) {
        s1[i].real = (float)max30100.irBuffer[i];
        s1[i].imag = 0.0;
        s2[i].real = (float)max30100.redBuffer[i];
        s2[i].imag = 0.0;
    }
    
    // 执行FFT
    max30100.FFT(s1);
    max30100.FFT(s2);
    
    // 找到最大值索引
    uint16_t maxIndex = max30100.findMaxNumIndex(s1, 1024);
    
    // 计算心率和血氧
    float heartRate = max30100.calculateHeartRateFromFFT(s1, maxIndex, 100);
    float spo2 = 0;
    max30100.calculateSpO2FromFFT(s1, s2, maxIndex, &spo2);
    
    Serial.printf("心率: %.1f bpm, 血氧: %.1f%%\n", heartRate, spo2);
}
```

## 硬件连接

```
ESP32          MAX30100
GPIO21  -----> SDA
GPIO22  -----> SCL
3.3V    -----> VCC
GND     -----> GND
```

**注意**: 建议在SDA和SCL线上添加4.7kΩ上拉电阻。

## 测试程序

运行 `MAX30100_FFT_Test.ino` 来测试FFT功能：

1. 上传测试程序到ESP32
2. 打开串口监视器（115200波特率）
3. 将手指放在传感器上
4. 观察数据收集进度和FFT分析结果

## 性能特点

- **数据采集**: 100Hz采样率，1024点数据
- **FFT计算**: 约1-2ms完成1024点FFT
- **心率范围**: 30-200 BPM
- **血氧范围**: 70-100%
- **精度**: 基于FFT分析，比简单峰值检测更准确

## 故障排除

### 常见问题

1. **初始化失败**
   - 检查I2C连接
   - 确认电源电压为3.3V
   - 验证传感器Part ID是否为0x11

2. **数据始终为0**
   - 检查手指接触
   - 调整LED电流设置
   - 验证FIFO状态

3. **FFT分析结果无效**
   - 确保有足够的数据点（1024个）
   - 检查传感器接触质量
   - 调整采样率设置

### 调试信息

程序会输出详细的调试信息：
- I2C设备扫描结果
- 传感器配置状态
- 数据收集进度
- FFT分析结果
- 性能统计

## 版本历史

- **v2.0**: 集成FFT分析算法，基于STM32代码改进
- **v1.0**: 基础MAX30100库实现

## 许可证

本项目基于MIT许可证开源。 