#include "lcd.h"
#include "TFT_Library.h"
#include "mqtt.h"

// 用宏来控制是否编译，从而减小ROM占用
#if HAS_LCD

static String lcd_str = "";

// LCD初始化
void lcd_init()
{
    // 初始化TFT显示屏
    IO_init();
    TFT_init();
    // 清屏
    TFT_clear();
}

// LCD在固定位置显示ID编号
void lcd_showID()
{
    char strID[50];
    sprintf(strID, "G:%d  ID:%d  %s", GROUP_ID, ID, mqtt_get_mac());
    SET_FONT_STYLE(GRAY, BLACK, SONG_STYLE16);
    TFT_ShowString(10, 220, strID);
}

// LCD显示字符串
void lcd_showStr(String str)
{
    lcd_str = str;
}

// LCD的循环
void lcd_loop()
{
    // 设置字体样式
    SET_FONT_STYLE(WHITE, BLACK, SONG_STYLE24);
    // 显示字符串
    TFT_ShowString(10, 10, lcd_str.c_str());

    // 每次都显示ID，方便观察
    lcd_showID();

    // 延时，没必要太快
    delay(100);
}

#endif