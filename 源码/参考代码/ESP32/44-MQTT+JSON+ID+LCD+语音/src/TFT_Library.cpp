/**
 * @file TFT_Library.cpp
 * @brief TFT液晶屏显示库实现文件
 * @details 本文件实现了基于ST7789V2控制器的TFT液晶屏显示功能
 *          支持中英文显示、图形绘制、颜色控制等功能
 * @author 物联网教学实验平台
 * @version 1.0
 * @date 2024
 *
 * 主要功能：
 * - 基础显示控制（清屏、填充、画点画线等）
 * - 图形绘制（矩形、圆形、三角形等）
 * - 文本显示（支持中英文混显，GB2312编码）
 * - 字体控制（多种字体大小，颜色设置）
 * - 高级显示功能（进度条、图标、滚动文本等）
 * - SPI通信和字库读取
 */

#include "TFT_Library.h"
#include <stdio.h>

/**
 * @brief 扩展颜色表 - RGB565格式
 * @details 定义了16种常用颜色，每个颜色用2字节表示（高字节+低字节）
 *          颜色格式为RGB565，即红色5位，绿色6位，蓝色5位
 */
const unsigned char TAB_COLOR[][2] =
    {
        0X00,
        0X00, // 黑色
        0XF8,
        0X00, // 红色
        0X07,
        0XE0, // 绿色
        0X00,
        0X1F, // 蓝色
        0XFF,
        0XFF, // 白色
        0XFF,
        0XE0, // 黄色
        0X07,
        0XFF, // 青色
        0XF8,
        0X1F, // 洋红色
        0X84,
        0X10, // 灰色
        0XFD,
        0X20, // 橙色
        0X80,
        0X1F, // 紫色
        0XA5,
        0X45, // 棕色
        0XFE,
        0X19, // 粉色
        0X87,
        0XE0, // 青柠色
        0X00,
        0X0F, // 海军蓝
        0X04,
        0X0F, // 蓝绿色
};

/**
 * @brief 字库缓存数组
 * @details 用于存储从SPI Flash中读取的字库数据
 *          支持最大26*26汉字，即4*26=104字节
 *          每个汉字占用4字节数据（26*26像素点阵）
 */
unsigned char FONT_BUFFER[104];

/**
 * @brief 中文字符串示例
 * @details 存储"GB32312字库测试"的GB2312编码
 *          用于测试字库功能是否正常
 */
char DIS_CHINA[] =
    {
        0x47,
        0x42,
        0x32,
        0x33,
        0x31,
        0x32,
        0xd7,
        0xd6,
        0xbf,
        0xe2,
        0xb2,
        0xe2,
        0xca,
        0xd4,
        0x00,
};

/**
 * @brief 当前光标X坐标
 * @details 记录文本显示时的水平位置
 */
unsigned int current_x = 0;

/**
 * @brief 当前光标Y坐标
 * @details 记录文本显示时的垂直位置
 */
unsigned int current_y = 0;

/**
 * @brief 当前字体颜色
 * @details 记录当前文本显示的前景色
 * @see TAB_COLOR 颜色表
 */
unsigned char current_font_color = WHITE;

/**
 * @brief 当前背景颜色
 * @details 记录当前文本显示的背景色
 * @see TAB_COLOR 颜色表
 */
unsigned char current_back_color = BLACK;

/**
 * @brief 当前字体样式
 * @details 记录当前使用的字体大小和类型
 * @see type_of_font 字体类型枚举
 */
type_of_font current_font_style = SONG_STYLE16;

/**
 * @brief GPIO引脚初始化函数
 * @details 初始化ESP32与TFT液晶屏连接的GPIO引脚
 *          配置SPI通信引脚和背光控制引脚
 *
 * 引脚配置说明：
 * - GPIO18: SPI时钟线(SCK) - 输出模式
 * - GPIO23: SPI数据线(SDA) - 输出模式
 * - GPIO13: SPI数据输入线(FSO) - 输入模式
 * - GPIO26: SPI片选线(CS) - 输出模式
 * - GPIO25: SPI数据/命令选择线(DC) - 输出模式
 * - GPIO19: SPI复位线(RST) - 输出模式
 * - GPIO5:  SPI Flash片选线(CS2) - 输出模式
 * - GPIO14: 背光控制线(BL) - 输出模式
 */
void IO_init(void)
{
    pinMode(18, OUTPUT); // SPI时钟线(SCK) - 输出模式
    pinMode(23, OUTPUT); // SPI数据线(SDA) - 输出模式
    pinMode(13, INPUT);  // SPI数据输入线(FSO) - 输入模式
    pinMode(26, OUTPUT); // SPI片选线(CS) - 输出模式
    pinMode(25, OUTPUT); // SPI数据/命令选择线(DC) - 输出模式
    pinMode(19, OUTPUT); // SPI复位线(RST) - 输出模式
    pinMode(5, OUTPUT);  // SPI Flash片选线(CS2) - 输出模式
    pinMode(14, OUTPUT); // 背光控制线(BL) - 输出模式

    // 初始化SPI信号线为高电平（空闲状态）
    SPI_SCK_1; // 时钟线置高
    SPI_CS_1;  // 片选线置高（未选中状态）
    SPI_SDA_1; // 数据线置高
}

// 背光控制函数
/*void TFT_SetBacklight(unsigned char brightness)
{
    if(brightness == 0)
    {
        digitalWrite(BL_PIN, LOW);  // 关闭背光
    }
    else if(brightness >= 100)
    {
        digitalWrite(BL_PIN, HIGH); // 全亮
    }
    else
    {
        analogWrite(BL_PIN, map(brightness, 0, 100, 0, 255)); // PWM控制亮度
    }
}

// 背光开关函数
void TFT_BacklightOn(void)
{
    digitalWrite(BL_PIN, HIGH);
}

void TFT_BacklightOff(void)
{
    digitalWrite(BL_PIN, LOW);
}*/

/**
 * @brief 微秒级延时函数
 * @param _us_time 延时时间（微秒）
 * @details 通过软件循环实现微秒级精确延时
 *          注意：此函数精度有限，仅适用于短时间延时
 */
void delay_us(unsigned int _us_time)
{
    unsigned char x = 0;
    for (; _us_time > 0; _us_time--)
    {
        x++; // 空循环实现延时
    }
}

/**
 * @brief SPI发送单字节数据函数
 * @param byte 要发送的字节数据
 * @details 通过软件模拟SPI协议发送一个字节的数据
 *          使用位操作逐位发送，从最高位(MSB)开始
 *
 * SPI时序说明：
 * 1. 拉低时钟线(SCK)
 * 2. 根据数据位设置数据线(SDA)
 * 3. 拉高时钟线，产生上升沿
 * 4. 拉低时钟线，准备下一位
 */
void SPI_SendByte(unsigned char byte)
{
    unsigned char counter;

    for (counter = 0; counter < 8; counter++) // 循环8次，发送8位数据
    {
        SPI_SCK_0;              // 拉低时钟线
        if ((byte & 0x80) == 0) // 检查最高位
        {
            SPI_SDA_0; // 数据位为0，拉低数据线
        }
        else
            SPI_SDA_1;    // 数据位为1，拉高数据线
        byte = byte << 1; // 左移一位，准备下一位
        SPI_SCK_1;        // 拉高时钟线，产生上升沿
        SPI_SCK_0;        // 拉低时钟线，准备下一位
    }
}

/**
 * @brief 发送TFT命令函数
 * @param o_command 要发送的命令字节
 * @details 向TFT控制器发送控制命令
 *          设置DC线为低电平表示发送命令
 */
void TFT_SEND_CMD(unsigned char o_command)
{
    SPI_DC_0;                // 设置DC为低电平，表示发送命令
    SPI_CS_0;                // 拉低片选线，选中TFT
    SPI_SendByte(o_command); // 发送命令字节
    SPI_CS_1;                // 拉高片选线，结束传输
}

/**
 * @brief 发送TFT数据函数
 * @param o_data 要发送的数据字节
 * @details 向TFT控制器发送显示数据
 *          设置DC线为高电平表示发送数据
 */
void TFT_SEND_DATA(unsigned char o_data)
{
    SPI_DC_1;             // 设置DC为高电平，表示发送数据
    SPI_CS_0;             // 拉低片选线，选中TFT
    SPI_SendByte(o_data); // 发送数据字节
    SPI_CS_1;             // 拉高片选线，结束传输
}

/**
 * @brief 设置TFT显示区域函数
 * @param x_start 起始X坐标
 * @param y_start 起始Y坐标
 * @param x_end 结束X坐标
 * @param y_end 结束Y坐标
 * @details 设置TFT显示窗口的矩形区域，后续的像素数据将写入此区域
 *          使用ST7789V2的列地址设置(0x2A)和行地址设置(0x2B)命令
 */
void TFT_SET_ADD(unsigned int x_start, unsigned int y_start, unsigned int x_end, unsigned int y_end)
{
    // 计算列地址（X坐标），加上偏移量
    unsigned int x = x_start + TFT_COLUMN_OFFSET, y = x_end + TFT_COLUMN_OFFSET;
    TFT_SEND_CMD(0x2a);    // 列地址设置命令
    TFT_SEND_DATA(x >> 8); // 起始列地址高字节
    TFT_SEND_DATA(x);      // 起始列地址低字节
    TFT_SEND_DATA(y >> 8); // 结束列地址高字节
    TFT_SEND_DATA(y);      // 结束列地址低字节

    // 计算行地址（Y坐标），加上偏移量
    x = y_start + TFT_LINE_OFFSET;
    y = y_end + TFT_LINE_OFFSET;
    TFT_SEND_CMD(0x2b);    // 行地址设置命令
    TFT_SEND_DATA(x >> 8); // 起始行地址高字节
    TFT_SEND_DATA(x);      // 起始行地址低字节
    TFT_SEND_DATA(y >> 8); // 结束行地址高字节
    TFT_SEND_DATA(y);      // 结束行地址低字节
    TFT_SEND_CMD(0x2C);    // 内存写入命令，准备接收像素数据
}

/**
 * @brief 清屏函数
 * @details 将整个TFT屏幕清空为黑色
 *          通过双重循环遍历所有像素点，发送黑色像素数据
 */
void TFT_clear(void)
{
    unsigned int ROW, column;
    TFT_SET_ADD(0, 0, TFT_COLUMN_NUMBER - 1, TFT_LINE_NUMBER - 1); // 设置整个屏幕区域
    for (ROW = 0; ROW < TFT_LINE_NUMBER; ROW++)                    // 行循环
    {
        for (column = 0; column < TFT_COLUMN_NUMBER; column++) // 列循环
        {
            TFT_SEND_DATA(0x00); // 发送像素数据高字节（黑色）
            TFT_SEND_DATA(0x00); // 发送像素数据低字节（黑色）
        }
    }
}

/**
 * @brief 全屏填充函数
 * @param color 填充颜色索引（对应TAB_COLOR数组中的颜色）
 * @details 将整个TFT屏幕填充为指定颜色
 *          通过双重循环遍历所有像素点，发送指定颜色的像素数据
 */
void TFT_full(unsigned int color)
{
    unsigned int ROW, column;
    TFT_SET_ADD(0, 0, TFT_COLUMN_NUMBER - 1, TFT_LINE_NUMBER - 1); // 设置整个屏幕区域
    for (ROW = 0; ROW < TFT_LINE_NUMBER; ROW++)                    // 行循环
    {
        for (column = 0; column < TFT_COLUMN_NUMBER; column++) // 列循环
        {
            TFT_SEND_DATA(TAB_COLOR[color][0]); // 发送颜色数据高字节
            TFT_SEND_DATA(TAB_COLOR[color][1]); // 发送颜色数据低字节
        }
    }
}

/**
 * @brief 画点函数
 * @param x X坐标
 * @param y Y坐标
 * @param color 颜色索引（对应TAB_COLOR数组中的颜色）
 * @details 在指定坐标绘制一个像素点
 *          包含边界检查，防止越界访问
 */
void TFT_DrawPixel(unsigned int x, unsigned int y, unsigned int color)
{
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER)
        return; // 边界检查

    TFT_SET_ADD(x, y, x, y);            // 设置单个像素的显示区域
    TFT_SEND_DATA(TAB_COLOR[color][0]); // 发送颜色数据高字节
    TFT_SEND_DATA(TAB_COLOR[color][1]); // 发送颜色数据低字节
}

/**
 * @brief 画线函数 - Bresenham算法
 * @param x1 起始点X坐标
 * @param y1 起始点Y坐标
 * @param x2 结束点X坐标
 * @param y2 结束点Y坐标
 * @param color 线条颜色索引
 * @details 使用Bresenham算法绘制直线，算法效率高且线条平滑
 *          支持任意方向的直线绘制
 */
void TFT_DrawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color)
{
    int dx = abs((int)x2 - (int)x1); // X方向距离
    int dy = abs((int)y2 - (int)y1); // Y方向距离
    int sx = (x1 < x2) ? 1 : -1;     // X方向步进方向
    int sy = (y1 < y2) ? 1 : -1;     // Y方向步进方向
    int err = dx - dy;               // 误差值

    int current_x = (int)x1; // 当前X坐标
    int current_y = (int)y1; // 当前Y坐标
    int end_x = (int)x2;     // 终点X坐标
    int end_y = (int)y2;     // 终点Y坐标

    while (true)
    {
        TFT_DrawPixel(current_x, current_y, color); // 绘制当前像素点

        if (current_x == end_x && current_y == end_y)
            break; // 到达终点，退出循环

        int e2 = 2 * err; // 误差值的2倍
        if (e2 > -dy)     // 如果误差大于负Y距离
        {
            err -= dy;       // 更新误差值
            current_x += sx; // X方向步进
        }
        if (e2 < dx) // 如果误差小于X距离
        {
            err += dx;       // 更新误差值
            current_y += sy; // Y方向步进
        }
    }
}

/**
 * @brief 画矩形函数（空心）
 * @param x 矩形左上角X坐标
 * @param y 矩形左上角Y坐标
 * @param width 矩形宽度
 * @param height 矩形高度
 * @param color 边框颜色索引
 * @details 绘制空心矩形，只画边框不填充内部
 *          通过绘制四条边线实现
 */
void TFT_DrawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
    TFT_DrawLine(x, y, x + width - 1, y, color);                           // 上边
    TFT_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color); // 下边
    TFT_DrawLine(x, y, x, y + height - 1, color);                          // 左边
    TFT_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);  // 右边
}

/**
 * @brief 填充矩形函数（实心）
 * @param x 矩形左上角X坐标
 * @param y 矩形左上角Y坐标
 * @param width 矩形宽度
 * @param height 矩形高度
 * @param color 填充颜色索引
 * @details 绘制实心矩形，填充整个矩形区域
 *          包含边界检查，防止越界访问
 */
void TFT_FillRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER)
        return; // 边界检查

    // 计算实际结束坐标，防止越界
    unsigned int x_end = (x + width > TFT_COLUMN_NUMBER) ? TFT_COLUMN_NUMBER : x + width;
    unsigned int y_end = (y + height > TFT_LINE_NUMBER) ? TFT_LINE_NUMBER : y + height;

    TFT_SET_ADD(x, y, x_end - 1, y_end - 1); // 设置矩形显示区域

    // 双重循环填充矩形
    for (unsigned int row = y; row < y_end; row++) // 行循环
    {
        for (unsigned int col = x; col < x_end; col++) // 列循环
        {
            TFT_SEND_DATA(TAB_COLOR[color][0]); // 发送颜色数据高字节
            TFT_SEND_DATA(TAB_COLOR[color][1]); // 发送颜色数据低字节
        }
    }
}

// 清除指定区域函数 - 使用当前背景色清除指定矩形区域
void TFT_ClearArea(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    if (x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER)
        return;

    unsigned int x_end = (x + width > TFT_COLUMN_NUMBER) ? TFT_COLUMN_NUMBER : x + width;
    unsigned int y_end = (y + height > TFT_LINE_NUMBER) ? TFT_LINE_NUMBER : y + height;

    TFT_SET_ADD(x, y, x_end - 1, y_end - 1);

    for (unsigned int row = y; row < y_end; row++)
    {
        for (unsigned int col = x; col < x_end; col++)
        {
            TFT_SEND_DATA(TAB_COLOR[current_back_color][0]);
            TFT_SEND_DATA(TAB_COLOR[current_back_color][1]);
        }
    }
}

/**
 * @brief 画圆函数 - Bresenham算法
 * @param x0 圆心X坐标
 * @param y0 圆心Y坐标
 * @param radius 圆半径
 * @param color 圆的颜色索引
 * @details 使用Bresenham算法绘制圆形，只画边框不填充
 *          通过绘制8个对称点实现完整的圆形
 *          包含边界检查，防止越界访问
 */
void TFT_DrawCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color)
{
    int x = radius; // 当前X坐标
    int y = 0;      // 当前Y坐标
    int err = 0;    // 误差值

    while (x >= y) // 当X坐标大于等于Y坐标时继续
    {
        // 绘制8个对称点，利用圆的对称性
        // 第一象限
        if (x0 + x < TFT_COLUMN_NUMBER && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 + x, y0 + y, color);
        if (x0 + y < TFT_COLUMN_NUMBER && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 + y, y0 + x, color);
        // 第二象限
        if (x0 >= y && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 - y, y0 + x, color);
        if (x0 >= x && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 - x, y0 + y, color);
        // 第三象限
        if (x0 >= x && y0 >= y)
            TFT_DrawPixel(x0 - x, y0 - y, color);
        if (x0 >= y && y0 >= x)
            TFT_DrawPixel(x0 - y, y0 - x, color);
        // 第四象限
        if (x0 + y < TFT_COLUMN_NUMBER && y0 >= x)
            TFT_DrawPixel(x0 + y, y0 - x, color);
        if (x0 + x < TFT_COLUMN_NUMBER && y0 >= y)
            TFT_DrawPixel(x0 + x, y0 - y, color);

        // Bresenham算法误差计算
        if (err <= 0)
        {
            y += 1;           // Y坐标增加
            err += 2 * y + 1; // 更新误差值
        }
        if (err > 0)
        {
            x -= 1;           // X坐标减少
            err -= 2 * x + 1; // 更新误差值
        }
    }
}

// 填充圆函数
void TFT_FillCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color)
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        // 绘制水平线填充圆，并检查边界
        if (x0 >= x && x0 + x < TFT_COLUMN_NUMBER && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        if (x0 >= y && x0 + y < TFT_COLUMN_NUMBER && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        if (x0 >= x && x0 + x < TFT_COLUMN_NUMBER && y0 >= y)
            TFT_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        if (x0 >= y && x0 + y < TFT_COLUMN_NUMBER && y0 >= x)
            TFT_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// 画三角形函数
void TFT_DrawTriangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2,
                      unsigned int x3, unsigned int y3, unsigned int color)
{
    TFT_DrawLine(x1, y1, x2, y2, color);
    TFT_DrawLine(x2, y2, x3, y3, color);
    TFT_DrawLine(x3, y3, x1, y1, color);
}

// 设置显示区域函数
void TFT_SetDisplayArea(unsigned int x_start, unsigned int y_start, unsigned int x_end, unsigned int y_end)
{
    TFT_SET_ADD(x_start, y_start, x_end, y_end);
}

// 获取屏幕尺寸
unsigned int TFT_GetWidth(void)
{
    return TFT_COLUMN_NUMBER;
}

unsigned int TFT_GetHeight(void)
{
    return TFT_LINE_NUMBER;
}

/**
 * @brief 读取SPI Flash数据函数
 * @param pBuffer 数据缓冲区指针
 * @param ReadAddr 读取地址（24位地址）
 * @param NumByteToRead 要读取的字节数
 * @details 从W25QXX系列SPI Flash中读取数据
 *          使用W25X_ReadData命令（0x03）进行读取
 *          支持24位地址寻址，最大16MB容量
 */
void W25QXX_Read(unsigned char *pBuffer, unsigned long ReadAddr, unsigned int NumByteToRead)
{
    unsigned int i;
    unsigned char counter, redata = 0;
    SPI_CS2_0;                                     // 拉低Flash片选线，选中器件
    SPI_SendByte(W25X_ReadData);                   // 发送读取命令(0x03)
    SPI_SendByte((unsigned char)(ReadAddr >> 16)); // 发送地址高字节
    SPI_SendByte((unsigned char)(ReadAddr >> 8));  // 发送地址中字节
    SPI_SendByte((unsigned char)(ReadAddr));       // 发送地址低字节
    for (i = 0; i < NumByteToRead; i++)            // 循环读取指定字节数
    {
        for (counter = 0; counter < 8; counter++) // 读取8位数据
        {
            SPI_SCK_0;    // 拉低时钟线
            SPI_SDA_1;    // 数据线置高（读取模式）
            redata <<= 1; // 数据左移一位
            if (SPI_FSO)  // 如果数据输入线为高
            {
                redata |= 0x01; // 设置最低位为1
            }
            SPI_SCK_1; // 拉高时钟线，产生上升沿
        }

        SPI_SCK_0;           // 拉低时钟线
        pBuffer[i] = redata; // 保存读取的数据
    }
    SPI_CS2_1; // 拉高Flash片选线，结束传输
}

/**
 * @brief 检查SPI Flash字库函数
 * @return 返回TRUE表示字库正常，FALSE表示字库异常
 * @details 通过读取Flash前22字节的标识字符串来验证字库是否正常
 *          标识字符串为"JYC-4MbByte-FONT-FLASH"
 */
unsigned char CHECK_FALSH(void)
{
    unsigned int x = 0;
    const unsigned char string[] = "JYC-4MbByte-FONT-FLASH"; // 字库标识字符串
    W25QXX_Read(FONT_BUFFER, 0X000000, 22);                  // 读取Flash前22字节
    for (x = 0; x < 22; x++)
    {
        if (FONT_BUFFER[x] != string[x]) // 逐字节比较
        {
            return (FALSE); // 发现不匹配，返回错误
        }
    }
    return (TRUE); // 所有字节匹配，字库正常
}

/**
 * @brief 设置字体样式函数
 * @param font_color 字体颜色索引
 * @param back_color 背景颜色索引
 * @param TYPE_CHAR 字体类型枚举
 * @details 配置字体显示的各种参数，包括颜色、大小、字库地址等
 *          支持多种字体大小：12、14、16、18、20、22、24、26像素
 */
void SET_FONT_STYLE(unsigned char font_color, unsigned char back_color, type_of_font TYPE_CHAR)
{
    DIS_CHAR_MODE.BACK_COLOR = back_color; // 设置背景颜色
    DIS_CHAR_MODE.FONT_COLOR = font_color; // 设置字体颜色
    switch (TYPE_CHAR)
    {
    case SONG_STYLE12:                                // 12像素字体
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR6_12_ADD;   // ASCII字库起始地址
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA12_12_ADD; // 中文字库起始地址
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 12;        // ASCII字符数据大小
        DIS_CHAR_MODE.CHAR_HIGH = 12;                 // ASCII字符高度
        DIS_CHAR_MODE.CHAR_WIDE = 6;                  // ASCII字符宽度
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 12;        // 中文字符数据大小
        DIS_CHAR_MODE.WORD_HIGH = 12;                 // 中文字符高度
        DIS_CHAR_MODE.WORD_WIDE = 12;                 // 中文字符宽度
        break;

    case SONG_STYLE14:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR7_14_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA14_14_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 14;
        DIS_CHAR_MODE.CHAR_HIGH = 14;
        DIS_CHAR_MODE.CHAR_WIDE = 7;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 14;
        DIS_CHAR_MODE.WORD_HIGH = 14;
        DIS_CHAR_MODE.WORD_WIDE = 14;
        break;
    case SONG_STYLE16:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR8_16_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA16_16_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 1 * 16;
        DIS_CHAR_MODE.CHAR_HIGH = 16;
        DIS_CHAR_MODE.CHAR_WIDE = 8;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 2 * 16;
        DIS_CHAR_MODE.WORD_HIGH = 16;
        DIS_CHAR_MODE.WORD_WIDE = 16;
        break;
    case SONG_STYLE18:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR9_18_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA18_18_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 18;
        DIS_CHAR_MODE.CHAR_HIGH = 18;
        DIS_CHAR_MODE.CHAR_WIDE = 9;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 18;
        DIS_CHAR_MODE.WORD_HIGH = 18;
        DIS_CHAR_MODE.WORD_WIDE = 18;
        break;
    case SONG_STYLE20:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR10_20_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA20_20_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 20;
        DIS_CHAR_MODE.CHAR_HIGH = 20;
        DIS_CHAR_MODE.CHAR_WIDE = 10;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 20;
        DIS_CHAR_MODE.WORD_HIGH = 20;
        DIS_CHAR_MODE.WORD_WIDE = 20;
        break;
    case SONG_STYLE22:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR11_22_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA22_22_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 22;
        DIS_CHAR_MODE.CHAR_HIGH = 22;
        DIS_CHAR_MODE.CHAR_WIDE = 11;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 22;
        DIS_CHAR_MODE.WORD_HIGH = 22;
        DIS_CHAR_MODE.WORD_WIDE = 22;
        break;
    case SONG_STYLE24:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR12_24_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA24_24_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 24;
        DIS_CHAR_MODE.CHAR_HIGH = 24;
        DIS_CHAR_MODE.CHAR_WIDE = 12;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 3 * 24;
        DIS_CHAR_MODE.WORD_HIGH = 24;
        DIS_CHAR_MODE.WORD_WIDE = 24;
        break;
    case SONG_STYLE26:
        DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR13_26_ADD;
        DIS_CHAR_MODE.BASE_WORD_ADD = CHINA26_26_ADD;
        DIS_CHAR_MODE.CHAR_DATA_SIZE = 2 * 26;
        DIS_CHAR_MODE.CHAR_HIGH = 26;
        DIS_CHAR_MODE.CHAR_WIDE = 13;
        DIS_CHAR_MODE.WORD_DATA_SIZE = 4 * 26;
        DIS_CHAR_MODE.WORD_HIGH = 26;
        DIS_CHAR_MODE.WORD_WIDE = 26;
        break;
    }
}

/**
 * @brief 中文字符串显示函数
 * @param x_start 显示起始X坐标
 * @param y_start 显示起始Y坐标
 * @param string 要显示的字符串（GB2312编码）
 * @details 支持中英文混合显示，使用GB2312编码
 *          自动处理字符换行和边界检查
 *          从SPI Flash中读取字库数据进行显示
 */
void DIS_CHINESE(unsigned int x_start, unsigned int y_start, const char *string)
{
    unsigned char times = 0, CACHE = 0;                    // 循环计数器和缓存变量
    unsigned long Address;                                 // 字库地址
    unsigned int x = 0, z = 0, m, n, f;                    // 循环变量和计算变量
    unsigned char WORD_CODE_MSB, WORD_CODE_LSB;            // 字符编码的高字节和低字节
    unsigned int ADD_X_START = x_start,                    // 当前显示X起始位置
        ADD_Y_START = y_start,                             // 当前显示Y起始位置
        ADD_X_END = x_start + DIS_CHAR_MODE.WORD_WIDE - 1, // 当前显示X结束位置
        ADD_Y_END = y_start + DIS_CHAR_MODE.WORD_HIGH;     // 当前显示Y结束位置

    while (*string != '\0')
    {
        WORD_CODE_MSB = *string++;
        WORD_CODE_LSB = *string++;

        if (((unsigned char)WORD_CODE_MSB >= 0xA1) && ((unsigned char)WORD_CODE_LSB >= 0xA1)) // GB2312编码范围,序列号1410  为：啊 后续为：阿埃挨暗
        {
            Address = (WORD_CODE_MSB - 0xA1) * 94;
            Address = (Address + (WORD_CODE_LSB - 0xA1));
            Address = Address * (DIS_CHAR_MODE.WORD_DATA_SIZE);
            Address = Address + DIS_CHAR_MODE.BASE_WORD_ADD;
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.WORD_DATA_SIZE));
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.WORD_WIDE - 1;

            if ((ADD_X_END > (TFT_LINE_NUMBER - 1))) // 超出x地址范围,转到下一行
            {
                ADD_Y_START = ADD_Y_END + 1;
                ADD_X_START = 0;
                ADD_X_END = DIS_CHAR_MODE.WORD_WIDE - 1;
                ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.WORD_HIGH;
                if (ADD_Y_END > TFT_COLUMN_NUMBER) // 超出Y范围
                {
                    ADD_Y_START = 0; // 移动到第一行
                    ADD_Y_END = DIS_CHAR_MODE.WORD_HIGH;
                }
            }
            TFT_SET_ADD(ADD_X_START, ADD_Y_START, ADD_X_END, ADD_Y_END);
            TFT_SEND_CMD(0x2C); // 写数据

            z = 0;
            for (x = 0; x < DIS_CHAR_MODE.WORD_HIGH; x++) // 按行显示
            {
                m = DIS_CHAR_MODE.WORD_WIDE / 8;
                f = DIS_CHAR_MODE.WORD_WIDE % 8;
                for (n = 0; n < m; n++) //  取完整字节
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < 8; times++)
                    {
                        if ((CACHE & 0x80) == 0) // 无内容,填充底色
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
                if (f != 0)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < f; times++) //  取不完整字节
                    {
                        if ((CACHE & 0x80) == 0) // 无内容,填充底色
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
            }
            ADD_X_START = ADD_X_END;
        }
        else // 英文范围
        {
            Address = (WORD_CODE_MSB)*DIS_CHAR_MODE.CHAR_DATA_SIZE + DIS_CHAR_MODE.BASE_CHAR_ADD;
            string--;
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.CHAR_WIDE - 1;
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.CHAR_DATA_SIZE));

            if ((ADD_X_END > (TFT_COLUMN_NUMBER - 1))) // 超出x地址范围,转到下一行
            {
                ADD_Y_START = ADD_Y_START + (ADD_X_END / (TFT_COLUMN_NUMBER - 1)) * DIS_CHAR_MODE.CHAR_HIGH;
                ADD_X_START = 0;
                ADD_X_END = DIS_CHAR_MODE.CHAR_WIDE - 1;
                ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.CHAR_HIGH;
                if (ADD_Y_END > (TFT_LINE_NUMBER - 1)) // 超出Y范围
                {
                    ADD_Y_START = 0; // 移动到第一行
                    ADD_Y_END = DIS_CHAR_MODE.CHAR_HIGH;
                }
            }

            TFT_SET_ADD(ADD_X_START, ADD_Y_START, ADD_X_END, ADD_Y_END);

            z = 0;
            for (x = 0; x < DIS_CHAR_MODE.CHAR_HIGH; x++) // 按行显示
            {
                m = DIS_CHAR_MODE.CHAR_WIDE / 8;
                f = DIS_CHAR_MODE.CHAR_WIDE % 8;
                for (n = 0; n < m; n++) //  取完整字节
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < 8; times++)
                    {
                        if ((CACHE & 0x80) == 0) // 无内容,填充底色
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
                if (f != 0)
                {
                    CACHE = FONT_BUFFER[z++];
                    for (times = 0; times < f; times++) //  取不完整字节
                    {
                        if ((CACHE & 0x80) == 0) // 无内容,填充底色
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]);
                        }
                        else
                        {
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                            TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]);
                        }
                        CACHE = CACHE << 1;
                    }
                }
            }
            ADD_X_START = ADD_X_END;
        }
    }
}

// 显示单个数字函数
void TFT_ShowNumber(unsigned int x, unsigned int y, unsigned long number, unsigned char length)
{
    char str[16];
    sprintf(str, "%0*lu", length, number);
    // 数字直接显示，不需要GB2312转换
    DIS_CHINESE(x, y, str);
}

// 显示浮点数函数
void TFT_ShowFloat(unsigned int x, unsigned int y, float number, unsigned char decimal_places)
{
    char str[16];
    dtostrf(number, 0, decimal_places, str);
    // 数字直接显示，不需要GB2312转换
    DIS_CHINESE(x, y, str);
}

// 显示纯英文数字字符串（不进行GB2312转换）
void TFT_ShowASCII(unsigned int x, unsigned int y, const char *str)
{
    // 直接显示ASCII字符，不进行GB2312转换
    DIS_CHINESE(x, y, str);
}

/**
 * @brief 显示字符串函数（简化版）
 * @param x    字符串显示的起始X坐标
 * @param y    字符串显示的起始Y坐标
 * @param str  要显示的UTF-8编码字符串
 */
void TFT_ShowString(unsigned int x, unsigned int y, const char *str)
{
    // 使用GB.get()方法转换UTF-8到GB2312
    String gb_str = GB.get(str);
    DIS_CHINESE(x, y, gb_str.c_str());
}

// 显示字符串函数（直接GB2312编码）
void TFT_ShowStringGB2312(unsigned int x, unsigned int y, const char *gb2312_str)
{
    // 直接显示GB2312编码的字符串
    DIS_CHINESE(x, y, gb2312_str);
}

// 显示时间
void TFT_ShowTime(unsigned int x, unsigned int y, unsigned char hour, unsigned char minute, unsigned char second)
{
    char time_str[16];
    sprintf(time_str, "%02d:%02d:%02d", hour, minute, second);
    // 时间格式直接显示，不需要GB2312转换
    DIS_CHINESE(x, y, time_str);
}

// 显示日期
void TFT_ShowDate(unsigned int x, unsigned int y, unsigned int year, unsigned char month, unsigned char day)
{
    char date_str[16];
    sprintf(date_str, "%04d-%02d-%02d", year, month, day);
    // 日期格式直接显示，不需要GB2312转换
    DIS_CHINESE(x, y, date_str);
}

// 设置文本颜色
/**
 * @brief 设置文本前景色和背景色
 * @param font_color  字体颜色索引（对应TAB_COLOR数组中的颜色）
 * @param back_color  背景颜色索引（对应TAB_COLOR数组中的颜色）
 * @details 同时更新当前字体颜色和背景色，并调用SET_FONT_STYLE同步字体样式
 */
void TFT_SetTextColor(unsigned char font_color, unsigned char back_color)
{
    current_font_color = font_color;                            // 设置当前字体颜色
    current_back_color = back_color;                            // 设置当前背景颜色
    SET_FONT_STYLE(font_color, back_color, current_font_style); // 应用字体样式
}

// 设置字体大小
void TFT_SetFontSize(type_of_font font_style)
{
    current_font_style = font_style;
    SET_FONT_STYLE(current_font_color, current_back_color, font_style);
}

// 设置光标位置
void TFT_SetCursor(unsigned int x, unsigned int y)
{
    current_x = x;
    current_y = y;
}

// 获取光标位置
void TFT_GetCursor(unsigned int *x, unsigned int *y)
{
    *x = current_x;
    *y = current_y;
}

// 打印函数（带自动换行）
void TFT_Print(const char *str)
{
    unsigned int x = current_x;
    unsigned int y = current_y;

    // 使用GB.get()方法转换UTF-8到GB2312
    DIS_CHINESE(x, y, GB.get(str).c_str());

    // 更新光标位置（简化处理）
    current_x = 0;
    current_y += DIS_CHAR_MODE.WORD_HIGH + 2; // 加2像素间距

    if (current_y >= TFT_LINE_NUMBER)
    {
        current_y = 0; // 回到顶部
    }
}

// 打印数字
void TFT_PrintNumber(unsigned long number)
{
    char str[16];
    sprintf(str, "%lu", number);
    TFT_Print(str);
}

// 打印浮点数
void TFT_PrintFloat(float number, unsigned char decimal_places)
{
    char str[16];
    dtostrf(number, 0, decimal_places, str);
    TFT_Print(str);
}

// 清屏并重置光标
void TFT_ClearScreen(void)
{
    TFT_clear();
    current_x = 0;
    current_y = 0;
}

// 修正版滚动显示函数，避免拆开GB2312汉字
void TFT_ScrollText(unsigned int x, unsigned int y, const char *str, unsigned int scroll_speed)
{
    // 先转换为GB2312
    String gb_str = GB.get(str);
    const char *gb = gb_str.c_str();
    unsigned int max_bytes = (TFT_COLUMN_NUMBER - x) / DIS_CHAR_MODE.WORD_WIDE * 2; // 最多字节数（全汉字时）
    unsigned int gb_len = gb_str.length();

    // 计算每个滚动窗口的起始位置
    for (unsigned int start = 0; start < gb_len;)
    {
        char temp_str[64];
        unsigned int temp_idx = 0;
        unsigned int byte_count = 0;
        unsigned int i = start;
        // 按字符单位截取，不拆汉字
        while (i < gb_len && byte_count < max_bytes)
        {
            if ((unsigned char)gb[i] >= 0xA1)
            {
                // 汉字，2字节
                if (byte_count + 2 > max_bytes || i + 1 >= gb_len)
                    break;
                temp_str[temp_idx++] = gb[i++];
                temp_str[temp_idx++] = gb[i++];
                byte_count += 2;
            }
            else
            {
                // ASCII，1字节
                temp_str[temp_idx++] = gb[i++];
                byte_count += 1;
            }
        }
        temp_str[temp_idx] = '\0';
        TFT_FillRect(x, y, TFT_COLUMN_NUMBER - x, DIS_CHAR_MODE.WORD_HIGH, current_back_color);
        DIS_CHINESE(x, y, temp_str);
        delay(scroll_speed);
        // 下一个窗口：跳过第一个字符（1字节或2字节）
        if ((unsigned char)gb[start] >= 0xA1)
            start += 2;
        else
            start += 1;
    }
}

// 居中显示文本
void TFT_ShowStringCenter(unsigned int y, const char *str)
{
    // 先转换为GB2312
    String gb_str = GB.get(str);
    unsigned int str_len = gb_str.length();
    unsigned int total_width = str_len * DIS_CHAR_MODE.WORD_WIDE;
    unsigned int x = (TFT_COLUMN_NUMBER - total_width) / 2;

    if (x < 0)
        x = 0;
    // 使用转换后的GB2312字符串
    DIS_CHINESE(x, y, gb_str.c_str());
}

// 显示进度条
void TFT_ShowProgressBar(unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                         unsigned char percentage, unsigned int bar_color, unsigned int bg_color)
{
    // 绘制背景
    TFT_FillRect(x, y, width, height, bg_color);

    // 绘制边框
    TFT_DrawRect(x, y, width, height, WHITE);

    // 绘制进度条
    unsigned int bar_width = (width - 2) * percentage / 100;
    if (bar_width > 0)
    {
        TFT_FillRect(x + 1, y + 1, bar_width, height - 2, bar_color);
    }
}

// 显示电池图标
void TFT_ShowBattery(unsigned int x, unsigned int y, unsigned char percentage)
{
    unsigned int battery_width = 20;
    unsigned int battery_height = 10;
    unsigned int color;

    // 确定电池颜色
    if (percentage > 50)
        color = GREEN;
    else if (percentage > 20)
        color = YELLOW;
    else
        color = RED;

    // 绘制电池外框
    TFT_DrawRect(x, y, battery_width, battery_height, WHITE);
    TFT_DrawRect(x + battery_width, y + 2, 2, battery_height - 4, WHITE);

    // 绘制电池电量
    unsigned int level_width = (battery_width - 2) * percentage / 100;
    if (level_width > 0)
    {
        TFT_FillRect(x + 1, y + 1, level_width, battery_height - 2, color);
    }
}

// 更大尺寸的WiFi图标（宽18高12，带边界保护）
void TFT_ShowWiFi(unsigned int x, unsigned int y, bool connected)
{
    unsigned int w = 18, h = 12;
    // 边界保护，防止越界
    if (x + w > TFT_COLUMN_NUMBER)
        x = (TFT_COLUMN_NUMBER > w) ? (TFT_COLUMN_NUMBER - w) : 0;
    if (y + h > TFT_LINE_NUMBER)
        y = (TFT_LINE_NUMBER > h) ? (TFT_LINE_NUMBER - h) : 0;
    unsigned int cx = x + w / 2;
    unsigned int cy = y + 3;
    unsigned int color = connected ? WHITE : GRAY;
    // 清除区域（确保不越界）
    TFT_FillRect(x, y, w, h, current_back_color);
    // 画3条WiFi弧线（朝下）
    for (unsigned char i = 1; i <= 3; i++)
    {
        unsigned int r1 = i * 3, r2 = i * 3 + 1;
        // 只画在屏幕内的部分
        if (cx + r2 < TFT_COLUMN_NUMBER && cx >= r2 && cy + r2 < TFT_LINE_NUMBER && cy >= r2)
        {
            TFT_DrawArc(cx, cy, r1, r2, 20, 160, color);
        }
    }
    // 画顶部大圆点（确保不越界）
    if (cx + 2 < TFT_COLUMN_NUMBER && cx >= 2 && cy + 2 < TFT_LINE_NUMBER && cy >= 2)
    {
        TFT_FillCircle(cx, cy, 2, color);
    }
    // 未连接时画红叉（确保不越界）
    if (!connected)
    {
        unsigned int x1 = x + 3, y1 = y + 3, x2 = x + w - 4, y2 = y + h - 4;
        if (x2 < TFT_COLUMN_NUMBER && y2 < TFT_LINE_NUMBER)
        {
            TFT_DrawLine(x1, y1, x2, y2, RED);
            TFT_DrawLine(x2, y1, x1, y2, RED);
        }
    }
}

// 辅助函数：画弧线（近似实现，使用点）
void TFT_DrawArc(unsigned int cx, unsigned int cy, unsigned int r1, unsigned int r2, int start_angle, int end_angle, unsigned int color)
{
    for (int angle = start_angle; angle <= end_angle; angle += 4)
    {
        float rad = angle * 3.14159 / 180.0;
        unsigned int x1 = cx + r1 * cos(rad);
        unsigned int y1 = cy - r1 * sin(rad);
        unsigned int x2 = cx + r2 * cos(rad);
        unsigned int y2 = cy - r2 * sin(rad);
        TFT_DrawLine(x1, y1, x2, y2, color);
    }
}

/**
 * @brief TFT液晶屏初始化函数
 * @details 初始化ST7789V2控制器，配置各种显示参数
 *          包括复位、睡眠唤醒、帧率设置、电源设置、伽马校正等
 *          按照ST7789V2数据手册的推荐配置进行初始化
 */
void TFT_init(void)
{
    // 硬件复位序列
    SPI_SCK_0;   // 时钟线拉低
    SPI_RST_0;   // 复位线拉低，开始复位
    delay(1000); // 复位延时1秒
    SPI_RST_1;   // 复位线拉高，结束复位
    delay(1000); // 复位后稳定延时1秒

    TFT_SEND_CMD(0x11); // Sleep Out命令 - 退出睡眠模式
    delay(120);         // 等待120ms让控制器稳定
    //--------------------------------ST7789S Frame rate setting----------------------------------//
    // 帧率设置 - 配置显示区域和时序参数
    TFT_SEND_CMD(0x2a);  // 列地址设置命令
    TFT_SEND_DATA(0x00); // 起始列地址高字节
    TFT_SEND_DATA(0x00); // 起始列地址低字节
    TFT_SEND_DATA(0x00); // 结束列地址高字节
    TFT_SEND_DATA(0xef); // 结束列地址低字节 (240列)

    TFT_SEND_CMD(0x2b);  // 行地址设置命令
    TFT_SEND_DATA(0x00); // 起始行地址高字节
    TFT_SEND_DATA(0x28); // 起始行地址低字节 (40行偏移)
    TFT_SEND_DATA(0x01); // 结束行地址高字节
    TFT_SEND_DATA(0x17); // 结束行地址低字节 (320行)

    TFT_SEND_CMD(0xb2);  // 门控时序控制
    TFT_SEND_DATA(0x0c); // 前门控时间
    TFT_SEND_DATA(0x0c); // 后门控时间
    TFT_SEND_DATA(0x00); // 保留位
    TFT_SEND_DATA(0x33); // 前门控时间扩展
    TFT_SEND_DATA(0x33); // 后门控时间扩展

    TFT_SEND_CMD(0x20); // 显示反转关闭

    TFT_SEND_CMD(0xb7);  // 门控控制
    TFT_SEND_DATA(0x56); // 门控参数设置

    //---------------------------------ST7789S Power setting--------------------------------------//
    // 电源设置 - 配置各种电压和电流参数
    TFT_SEND_CMD(0xbb);  // VCOM电压设置
    TFT_SEND_DATA(0x18); // VCOM电压值

    TFT_SEND_CMD(0xc0);  // LCM控制
    TFT_SEND_DATA(0x2c); // LCM控制参数

    TFT_SEND_CMD(0xc2);  // VDV和VRH命令使能
    TFT_SEND_DATA(0x01); // 使能VDV和VRH设置

    TFT_SEND_CMD(0xc3);  // VRH设置
    TFT_SEND_DATA(0x1f); // VRH电压值

    TFT_SEND_CMD(0xc4);  // VDV设置
    TFT_SEND_DATA(0x20); // VDV电压值

    TFT_SEND_CMD(0xc6);  // 帧率控制2
    TFT_SEND_DATA(0x0f); // 帧率控制参数

    TFT_SEND_CMD(0xd0);  // 电源控制1
    TFT_SEND_DATA(0xa6); // 电源控制参数1
    TFT_SEND_DATA(0xa1); // 电源控制参数2
                         //--------------------------------ST7789S gamma setting---------------------------------------//
    // 伽马校正设置 - 优化显示色彩和对比度
    TFT_SEND_CMD(0xe0);  // 正电压伽马控制
    TFT_SEND_DATA(0xd0); // 伽马值1
    TFT_SEND_DATA(0x0d); // 伽马值2
    TFT_SEND_DATA(0x14); // 伽马值3
    TFT_SEND_DATA(0x0b); // 伽马值4
    TFT_SEND_DATA(0x0b); // 伽马值5
    TFT_SEND_DATA(0x07); // 伽马值6
    TFT_SEND_DATA(0x3a); // 伽马值7
    TFT_SEND_DATA(0x44); // 伽马值8
    TFT_SEND_DATA(0x50); // 伽马值9
    TFT_SEND_DATA(0x08); // 伽马值10
    TFT_SEND_DATA(0x13); // 伽马值11
    TFT_SEND_DATA(0x13); // 伽马值12
    TFT_SEND_DATA(0x2d); // 伽马值13
    TFT_SEND_DATA(0x32); // 伽马值14

    TFT_SEND_CMD(0xe1);  // 负电压伽马控制
    TFT_SEND_DATA(0xd0); // 伽马值1
    TFT_SEND_DATA(0x0d); // 伽马值2
    TFT_SEND_DATA(0x14); // 伽马值3
    TFT_SEND_DATA(0x0b); // 伽马值4
    TFT_SEND_DATA(0x0b); // 伽马值5
    TFT_SEND_DATA(0x07); // 伽马值6
    TFT_SEND_DATA(0x3a); // 伽马值7
    TFT_SEND_DATA(0x44); // 伽马值8
    TFT_SEND_DATA(0x50); // 伽马值9
    TFT_SEND_DATA(0x08); // 伽马值10
    TFT_SEND_DATA(0x13); // 伽马值11
    TFT_SEND_DATA(0x13); // 伽马值12
    TFT_SEND_DATA(0x2d); // 伽马值13
    TFT_SEND_DATA(0x32); // 伽马值14

    // 显示控制设置
    TFT_SEND_CMD(0x36);  // 内存数据访问控制
    TFT_SEND_DATA(0x00); // 设置扫描方向和颜色顺序

    TFT_SEND_CMD(0x3A);  // 接口像素格式
    TFT_SEND_DATA(0x55); // 16位RGB565格式

    TFT_SEND_CMD(0xe7);  // SPI2使能 - 启用2数据通道模式
    TFT_SEND_DATA(0x00); // SPI2参数

    TFT_SEND_CMD(0x21); // 显示反转开启
    TFT_SEND_CMD(0x29); // 显示开启 - 完成初始化
}