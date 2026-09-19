#ifndef TFT_LIBRARY_H
#define TFT_LIBRARY_H

#include <Arduino.h>
#include "UTF8ToGB2312.h"

// 引脚定义
#define SPI_SCK_0  digitalWrite(18,LOW)   //字库sck与屏复用     2-- 20      
#define SPI_SCK_1  digitalWrite(18,HIGH)
#define SPI_SDA_0  digitalWrite(23,LOW)    //字库SDA与屏复用   3-- 19   
#define SPI_SDA_1  digitalWrite(23,HIGH) 
#define SPI_FSO    digitalRead(13)     //字库输出    4--
#define SPI_RST_0  digitalWrite(26,LOW)             //  5-- 5      
#define SPI_RST_1  digitalWrite(26,HIGH)
#define SPI_DC_0  digitalWrite(25,LOW)               //6-- 6
#define SPI_DC_1  digitalWrite(25,HIGH)
#define SPI_CS_0  digitalWrite(19,LOW)               //7--7     
#define SPI_CS_1  digitalWrite(19,HIGH)
#define SPI_CS2_0  digitalWrite(5,LOW)               //8--8           
#define SPI_CS2_1  digitalWrite(5,HIGH)

//定义显示区域大小，偏移
#define TFT_COLUMN_NUMBER 240
#define TFT_LINE_NUMBER 240
#define TFT_COLUMN_OFFSET 0
#define TFT_LINE_OFFSET 0
#define PIC_NUM 28800      //图片数据大小

//定义常用颜色 - 扩展颜色定义
#define   BLACK  0  //黑色
#define   RED  1  //红色
#define   GREEN 2  //绿色
#define   BLUE  3  //蓝色
#define   WHITE  4  //白色
#define   YELLOW 5  //黄色
#define   CYAN 6   //青色
#define   MAGENTA 7 //洋红色
#define   GRAY 8   //灰色
#define   ORANGE 9 //橙色
#define   PURPLE 10 //紫色
#define   BROWN 11  //棕色
#define   PINK 12   //粉色
#define   LIME 13   //青柠色
#define   NAVY 14   //海军蓝
#define   TEAL 15   //蓝绿色

//指令表
#define W25X_WriteEnable 0x06
#define W25X_WriteDisable 0x04
#define W25X_ReadStatusReg 0x05
#define W25X_WriteStatusReg 0x01
#define W25X_ReadData 0x03
#define W25X_FastReadData 0x0B
#define W25X_FastReadDual 0x3B
#define W25X_PageProgram 0x02
#define W25X_BlockErase 0xD8
#define W25X_SectorErase 0x20
#define W25X_ChipErase 0xC7
#define W25X_PowerDown 0xB9
#define W25X_ReleasePowerDown 0xAB
#define W25X_DeviceID 0xAB
#define W25X_ManufactDeviceID 0x90
#define W25X_JedecDeviceID 0x9F

//字库基地址
#define CHAR6_12_ADD     0X1000L 
#define CHAR7_14_ADD     0X1600LL
#define CHAR8_16_ADD     0X1D00L 
#define CHAR9_18_ADD     0X2500L
#define CHAR10_20_ADD     0X3700L
#define CHAR11_22_ADD     0X4B00L 
#define CHAR12_24_ADD     0X6100L
#define CHAR13_26_ADD     0X7900L

#define CHINA12_12_ADD     0X9300L 
#define CHINA14_14_ADD     0X39300L 
#define CHINA16_16_ADD     0X71300L 
#define CHINA18_18_ADD     0XB1300L 
#define CHINA20_20_ADD     0X11D300L 
#define CHINA22_22_ADD     0X195300L
#define CHINA24_24_ADD     0X219300L
#define CHINA26_26_ADD     0X2A9300L 
#define END_ADD             0X379300L

#define TRUE             1
#define FALSE           0

typedef enum        // 不同字体选择
{
     SONG_STYLE12,SONG_STYLE14,SONG_STYLE16,SONG_STYLE18,SONG_STYLE20,SONG_STYLE22,SONG_STYLE24,SONG_STYLE26
}type_of_font;

struct                   //显示字符参数传递结构体
{                
    unsigned char  CHAR_WIDE;           //英文字体宽度       
    unsigned char  CHAR_HIGH;           //英文字体高度
    unsigned char  WORD_WIDE;           //汉字宽度
    unsigned char  WORD_HIGH;           //汉字高度
    unsigned int CHAR_DATA_SIZE;     //英文一个字符总数据大小  字节
    unsigned int WORD_DATA_SIZE;     //汉字一个字符总数据大小  字节
    unsigned char  BACK_COLOR;              //字符颜色
    unsigned char  FONT_COLOR;              //字符颜色
    unsigned long   BASE_WORD_ADD;           //汉字字库基地址
    unsigned long   BASE_CHAR_ADD;           //英文字库基地址
} DIS_CHAR_MODE;

// 扩展颜色表 - RGB565格式
extern const unsigned char TAB_COLOR[][2];

// 全局变量声明
extern unsigned char FONT_BUFFER[104];
extern char DIS_CHINA[];
extern unsigned int current_x;
extern unsigned int current_y;
extern unsigned char current_font_color;
extern unsigned char current_back_color;
extern type_of_font current_font_style;

// 基础函数声明
void IO_init(void);
void delay_us(unsigned int _us_time);
void SPI_SendByte(unsigned char byte);
void TFT_SEND_CMD(unsigned char o_command);
void TFT_SEND_DATA(unsigned char o_data);
void TFT_SET_ADD(unsigned int x_start, unsigned int y_start, unsigned int x_end, unsigned int y_end);
void TFT_clear(void);
void TFT_full(unsigned int color);
void TFT_init(void);
unsigned char CHECK_FALSH(void);
void W25QXX_Read(unsigned char *pBuffer, unsigned long ReadAddr, unsigned int NumByteToRead);
void SET_FONT_STYLE(unsigned char font_color, unsigned char back_color, type_of_font TYPE_CHAR);
void DIS_CHINESE(unsigned int x_start, unsigned int y_start, const char *string);


// 绘图函数
void TFT_DrawPixel(unsigned int x, unsigned int y, unsigned int color);
void TFT_DrawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color);
void TFT_DrawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void TFT_FillRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color);
void TFT_ClearArea(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
void TFT_DrawCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color);
void TFT_FillCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color);
void TFT_DrawTriangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, 
                      unsigned int x3, unsigned int y3, unsigned int color);
void TFT_SetDisplayArea(unsigned int x_start, unsigned int y_start, unsigned int x_end, unsigned int y_end);

// 文本显示函数
void TFT_ShowNumber(unsigned int x, unsigned int y, unsigned long number, unsigned char length);
void TFT_ShowFloat(unsigned int x, unsigned int y, float number, unsigned char decimal_places);
void TFT_ShowASCII(unsigned int x, unsigned int y, const char* str);
void TFT_ShowString(unsigned int x, unsigned int y, const char* str);
void TFT_ShowStringGB2312(unsigned int x, unsigned int y, const char* gb2312_str);
void TFT_ShowTime(unsigned int x, unsigned int y, unsigned char hour, unsigned char minute, unsigned char second);
void TFT_ShowDate(unsigned int x, unsigned int y, unsigned int year, unsigned char month, unsigned char day);
void TFT_ShowStringCenter(unsigned int y, const char* str);

// 文本控制函数
void TFT_SetTextColor(unsigned char font_color, unsigned char back_color);
void TFT_SetFontSize(type_of_font font_style);
void TFT_SetCursor(unsigned int x, unsigned int y);
void TFT_GetCursor(unsigned int* x, unsigned int* y);
void TFT_Print(const char* str);
void TFT_PrintNumber(unsigned long number);
void TFT_PrintFloat(float number, unsigned char decimal_places);
void TFT_ClearScreen(void);
void TFT_ScrollText(unsigned int x, unsigned int y, const char* str, unsigned int scroll_speed);

// 界面元素函数
void TFT_ShowProgressBar(unsigned int x, unsigned int y, unsigned int width, unsigned int height, 
                        unsigned char percentage, unsigned int bar_color, unsigned int bg_color);
void TFT_ShowBattery(unsigned int x, unsigned int y, unsigned char percentage);
void TFT_ShowWiFi(unsigned int x, unsigned int y, bool connected);

// 工具函数
unsigned int TFT_GetWidth(void);
unsigned int TFT_GetHeight(void);
static void TFT_DrawArc(unsigned int cx, unsigned int cy, unsigned int r1, unsigned int r2, int start_angle, int end_angle, unsigned int color);

#endif // TFT_LIBRARY_H 