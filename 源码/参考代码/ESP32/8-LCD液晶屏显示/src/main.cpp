#include <Arduino.h>
#include "TFT_Library.h"

// 函数声明
void testBasicColors();
void testEnglishText();
void testChineseText();

void setup()
{
    Serial.begin(115200);
    Serial.println("TFT Library Test Starting...");
    // 初始化TFT显示屏
    IO_init();
    TFT_init();
    // 设置字体样式
    SET_FONT_STYLE(WHITE, BLACK, SONG_STYLE16);
    // 清屏
    TFT_clear();
}

void loop()
{
    testBasicColors();
    delay(3000);
    TFT_clear();

    testEnglishText();
    delay(3000);
    TFT_clear();

    testChineseText();
    delay(3000);
    TFT_clear();
}

// 测试1: 图形绘制与填充演示（放大版）
void testBasicColors()
{
    Serial.println("Test 1: Draw and Fill Shapes");

    // 显示标题
    TFT_ShowString(10, 10, "Draw & Fill Shapes");

    // 画更大的长方形（红色边框）
    TFT_DrawRect(20, 40, 120, 60, RED);

    // 画更大的圆形（绿色边框）
    TFT_DrawCircle(180, 70, 30, GREEN);

    // 填充更大的长方形（蓝色填充）
    TFT_FillRect(20, 120, 120, 60, BLUE);

    // 填充更大的圆形（黄色填充）
    TFT_FillCircle(180, 150, 30, YELLOW);

    // 恢复默认颜色
    TFT_SetTextColor(WHITE, BLACK);
}

// 测试2: 英文文本显示
void testEnglishText()
{
    Serial.println("Test 2: English Text");

    // 设置不同字体大小并用不同颜色显示
    TFT_SetFontSize(SONG_STYLE12);
    TFT_SetTextColor(RED, BLACK);
    TFT_ShowString(10, 10, "Font Size 12");

    TFT_SetFontSize(SONG_STYLE16);
    TFT_SetTextColor(GREEN, BLACK);
    TFT_ShowString(10, 30, "Font Size 16");

    TFT_SetFontSize(SONG_STYLE20);
    TFT_SetTextColor(BLUE, BLACK);
    TFT_ShowString(10, 55, "Font Size 20");

    // 显示数字和浮点数，使用不同颜色
    TFT_SetFontSize(SONG_STYLE16);
    TFT_SetTextColor(MAGENTA, BLACK);
    TFT_ShowNumber(10, 85, 12345, 5);

    TFT_SetTextColor(CYAN, BLACK);
    TFT_ShowFloat(10, 110, 3.14159, 2);

    // 显示时间，使用黄色
    TFT_SetTextColor(YELLOW, BLACK);
    TFT_ShowTime(10, 135, 14, 30, 25);

    // 恢复默认颜色
    TFT_SetTextColor(WHITE, BLACK);
}

// 测试3: 中文文本显示
void testChineseText()
{
    Serial.println("Test 3: Chinese Text");

    // 显示中文文本（需要GB2312编码），用不同颜色显示
    TFT_SetFontSize(SONG_STYLE16);

    TFT_SetTextColor(RED, BLACK);
    TFT_ShowString(10, 50, "物联网综合应用平台");

    TFT_SetFontSize(SONG_STYLE18);
    TFT_SetTextColor(GREEN, BLACK);
    TFT_ShowString(10, 90, "中文测试");

    TFT_SetFontSize(SONG_STYLE20);
    TFT_SetTextColor(BLUE, BLACK);
    TFT_ShowString(10, 130, "ESP32功能模块");

    // 恢复默认颜色
    TFT_SetTextColor(WHITE, BLACK);
}
