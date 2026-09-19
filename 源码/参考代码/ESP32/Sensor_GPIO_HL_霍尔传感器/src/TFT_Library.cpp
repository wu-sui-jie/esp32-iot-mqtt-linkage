#include "TFT_Library.h"
#include <stdio.h>

// 扩展颜色表 - RGB565格式
const unsigned char TAB_COLOR[][2] = 
{
    0X00,0X00,      //黑色
    0XF8,0X00,    //红色
    0X07,0XE0,    //绿色
    0X00,0X1F,    //蓝色
    0XFF,0XFF,    //白色
    0XFF,0XE0,    //黄色
    0X07,0XFF,    //青色
    0XF8,0X1F,    //洋红色
    0X84,0X10,    //灰色
    0XFD,0X20,    //橙色
    0X80,0X1F,    //紫色
    0XA5,0X45,    //棕色
    0XFE,0X19,    //粉色
    0X87,0XE0,    //青柠色
    0X00,0X0F,    //海军蓝
    0X04,0X0F,    //蓝绿色
};

// 全局变量定义
unsigned char FONT_BUFFER[104];         //字库缓存，支持最大26*26汉字，即4*26字节           

char DIS_CHINA[] = 
{
  0x47,0x42,0x32,0x33,0x31,0x32,0xd7,0xd6,0xbf,0xe2,0xb2,0xe2,0xca,0xd4,0x00,
};

// 全局变量
unsigned int current_x = 0;
unsigned int current_y = 0;
unsigned char current_font_color = WHITE;
unsigned char current_back_color = BLACK;
type_of_font current_font_style = SONG_STYLE16;

void  IO_init(void )
{
pinMode(18,OUTPUT);//设置数字脚为输出
pinMode(23,OUTPUT);//设置数字脚为输出
pinMode(13,INPUT);//设置数字脚为输入
pinMode(26,OUTPUT);//设置数字脚为输出
pinMode(25,OUTPUT);//设置数字脚为输出
pinMode(19,OUTPUT);//设置数字脚为输出
pinMode(5,OUTPUT);//设置数字脚为输出
pinMode(14,OUTPUT);//设置数字脚为输出 - 背光控制
SPI_SCK_1;
SPI_CS_1;
SPI_SDA_1;
}



void delay_us(unsigned int _us_time)
{       
  unsigned char x=0;
  for(;_us_time>0;_us_time--)
  {
    x++;
  }
}

void SPI_SendByte(unsigned char byte)
{
  unsigned char counter;
   
  for(counter=0;counter<8;counter++)
  { 
    SPI_SCK_0;
    if((byte&0x80)==0)
    {
      SPI_SDA_0;
    }
    else SPI_SDA_1;
    byte=byte<<1;
    SPI_SCK_1;  
    SPI_SCK_0;
  } 
}

void TFT_SEND_CMD(unsigned char o_command)
{
    SPI_DC_0;
    SPI_CS_0;
    SPI_SendByte(o_command);
    SPI_CS_1;
}

void TFT_SEND_DATA(unsigned char o_data)
{ 
    SPI_DC_1;
    SPI_CS_0;
    SPI_SendByte(o_data);
    SPI_CS_1;
}

void TFT_SET_ADD(unsigned int x_start,unsigned int y_start,unsigned int x_end,unsigned int y_end)
{
  unsigned int x = x_start + TFT_COLUMN_OFFSET,y=x_end+ TFT_COLUMN_OFFSET;
    TFT_SEND_CMD(0x2a);     //Column address set
    TFT_SEND_DATA(x>>8);    //start column
    TFT_SEND_DATA(x); 
    TFT_SEND_DATA(y>>8);    //end column
    TFT_SEND_DATA(y);
  x = y_start + TFT_LINE_OFFSET;
  y=y_end+ TFT_LINE_OFFSET;
    TFT_SEND_CMD(0x2b);     //Row address set
    TFT_SEND_DATA(x>>8);    //start row
    TFT_SEND_DATA(x); 
    TFT_SEND_DATA(y>>8);    //end row
    TFT_SEND_DATA(y);
    TFT_SEND_CMD(0x2C);     //Memory write
}

void TFT_clear(void)
{
    unsigned int ROW,column;
    TFT_SET_ADD(0,0,TFT_COLUMN_NUMBER-1,TFT_LINE_NUMBER-1);
    for(ROW=0;ROW<TFT_LINE_NUMBER;ROW++)             //ROW loop
    { 
        for(column=0;column<TFT_COLUMN_NUMBER;column++)  //column loop
        {
            TFT_SEND_DATA(0x00);
            TFT_SEND_DATA(0x00);
        }
    }
}

void TFT_full(unsigned int color)
{
    unsigned int ROW,column;
    TFT_SET_ADD(0,0,TFT_COLUMN_NUMBER-1,TFT_LINE_NUMBER-1);
    for(ROW=0;ROW<TFT_LINE_NUMBER;ROW++)             //ROW loop
    { 
        for(column=0;column<TFT_COLUMN_NUMBER ;column++) //column loop
        {
            TFT_SEND_DATA(TAB_COLOR[color][0]);
            TFT_SEND_DATA(TAB_COLOR[color][1]);
        }
    }
}

// 画点函数
void TFT_DrawPixel(unsigned int x, unsigned int y, unsigned int color)
{
    if(x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER) return;
    
    TFT_SET_ADD(x, y, x, y);
    TFT_SEND_DATA(TAB_COLOR[color][0]);
    TFT_SEND_DATA(TAB_COLOR[color][1]);
}

// 画线函数 - Bresenham算法
void TFT_DrawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color)
{
    int dx = abs((int)x2 - (int)x1);
    int dy = abs((int)y2 - (int)y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int current_x = (int)x1;
    int current_y = (int)y1;
    int end_x = (int)x2;
    int end_y = (int)y2;
    
    while(true)
    {
        TFT_DrawPixel(current_x, current_y, color);
        
        if(current_x == end_x && current_y == end_y) break;
        
        int e2 = 2 * err;
        if(e2 > -dy)
        {
            err -= dy;
            current_x += sx;
        }
        if(e2 < dx)
        {
            err += dx;
            current_y += sy;
        }
    }
}

// 画矩形函数
void TFT_DrawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
    TFT_DrawLine(x, y, x + width - 1, y, color);                    // 上边
    TFT_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color); // 下边
    TFT_DrawLine(x, y, x, y + height - 1, color);                    // 左边
    TFT_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color); // 右边
}

// 填充矩形函数
void TFT_FillRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int color)
{
    if(x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER) return;
    
    unsigned int x_end = (x + width > TFT_COLUMN_NUMBER) ? TFT_COLUMN_NUMBER : x + width;
    unsigned int y_end = (y + height > TFT_LINE_NUMBER) ? TFT_LINE_NUMBER : y + height;
    
    TFT_SET_ADD(x, y, x_end - 1, y_end - 1);
    
    for(unsigned int row = y; row < y_end; row++)
    {
        for(unsigned int col = x; col < x_end; col++)
        {
            TFT_SEND_DATA(TAB_COLOR[color][0]);
            TFT_SEND_DATA(TAB_COLOR[color][1]);
        }
    }
}

// 清除指定区域函数 - 使用当前背景色清除指定矩形区域
void TFT_ClearArea(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    if(x >= TFT_COLUMN_NUMBER || y >= TFT_LINE_NUMBER) return;
    
    unsigned int x_end = (x + width > TFT_COLUMN_NUMBER) ? TFT_COLUMN_NUMBER : x + width;
    unsigned int y_end = (y + height > TFT_LINE_NUMBER) ? TFT_LINE_NUMBER : y + height;
    
    TFT_SET_ADD(x, y, x_end - 1, y_end - 1);
    
    for(unsigned int row = y; row < y_end; row++)
    {
        for(unsigned int col = x; col < x_end; col++)
        {
            TFT_SEND_DATA(TAB_COLOR[current_back_color][0]);
            TFT_SEND_DATA(TAB_COLOR[current_back_color][1]);
        }
    }
}

// 画圆函数 - Bresenham算法
void TFT_DrawCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color)
{
    int x = radius;
    int y = 0;
    int err = 0;
    
    while(x >= y)
    {
        // 绘制8个对称点，并检查边界
        if(x0 + x < TFT_COLUMN_NUMBER && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 + x, y0 + y, color);
        if(x0 + y < TFT_COLUMN_NUMBER && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 + y, y0 + x, color);
        if(x0 >= y && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 - y, y0 + x, color);
        if(x0 >= x && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawPixel(x0 - x, y0 + y, color);
        if(x0 >= x && y0 >= y)
            TFT_DrawPixel(x0 - x, y0 - y, color);
        if(x0 >= y && y0 >= x)
            TFT_DrawPixel(x0 - y, y0 - x, color);
        if(x0 + y < TFT_COLUMN_NUMBER && y0 >= x)
            TFT_DrawPixel(x0 + y, y0 - x, color);
        if(x0 + x < TFT_COLUMN_NUMBER && y0 >= y)
            TFT_DrawPixel(x0 + x, y0 - y, color);
        
        if(err <= 0)
        {
            y += 1;
            err += 2*y + 1;
        }
        if(err > 0)
        {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

// 填充圆函数
void TFT_FillCircle(unsigned int x0, unsigned int y0, unsigned int radius, unsigned int color)
{
    int x = radius;
    int y = 0;
    int err = 0;
    
    while(x >= y)
    {
        // 绘制水平线填充圆，并检查边界
        if(x0 >= x && x0 + x < TFT_COLUMN_NUMBER && y0 + y < TFT_LINE_NUMBER)
            TFT_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        if(x0 >= y && x0 + y < TFT_COLUMN_NUMBER && y0 + x < TFT_LINE_NUMBER)
            TFT_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        if(x0 >= x && x0 + x < TFT_COLUMN_NUMBER && y0 >= y)
            TFT_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        if(x0 >= y && x0 + y < TFT_COLUMN_NUMBER && y0 >= x)
            TFT_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        
        if(err <= 0)
        {
            y += 1;
            err += 2*y + 1;
        }
        if(err > 0)
        {
            x -= 1;
            err -= 2*x + 1;
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

//读取SPI FLASH
void W25QXX_Read(unsigned char *pBuffer, unsigned long ReadAddr, unsigned int NumByteToRead)
{
  unsigned int i;
    unsigned char counter,redata=0;
  SPI_CS2_0;             //使能器件
  SPI_SendByte(W25X_ReadData);//发送读取命令       
  SPI_SendByte((unsigned char)(ReadAddr >> 16));    //发送24bit地址
    SPI_SendByte((unsigned char)(ReadAddr >> 8));    //发送24bit地址
    SPI_SendByte((unsigned char)(ReadAddr));    //发送24bit地址
  for (i = 0; i < NumByteToRead; i++)         //循环读数
  {           
      for(counter=0;counter<8;counter++)
      { 
            SPI_SCK_0;    
            SPI_SDA_1;
            redata <<=1;
            if(SPI_FSO)
            {
               redata |=0x01; 
            } 
            SPI_SCK_1;      
      }
      
        SPI_SCK_0;
        pBuffer[i] = redata;
  }
  SPI_CS2_1;
}

unsigned char CHECK_FALSH(void)
{
    unsigned int x=0;
    const unsigned char string[] = "JYC-4MbByte-FONT-FLASH";
    W25QXX_Read(FONT_BUFFER, 0X000000, 22);
    for(x=0;x<22;x++)
    {
       if(FONT_BUFFER[x] != string[x]) 
       {
           return(FALSE);
       } 
    }
    return(TRUE);
}

void SET_FONT_STYLE (unsigned char font_color,unsigned char back_color,type_of_font TYPE_CHAR)   
{
    DIS_CHAR_MODE.BACK_COLOR = back_color;
    DIS_CHAR_MODE.FONT_COLOR = font_color;
    switch (TYPE_CHAR)
    {
        case SONG_STYLE12: 
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR6_12_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA12_12_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 1*12;
                DIS_CHAR_MODE.CHAR_HIGH = 12;
                DIS_CHAR_MODE.CHAR_WIDE = 6;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 2*12;
                DIS_CHAR_MODE.WORD_HIGH = 12;
                DIS_CHAR_MODE.WORD_WIDE = 12;
        break;
        
        case SONG_STYLE14:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR7_14_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA14_14_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 1*14;
                DIS_CHAR_MODE.CHAR_HIGH = 14;
                DIS_CHAR_MODE.CHAR_WIDE = 7;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 2*14;
                DIS_CHAR_MODE.WORD_HIGH = 14;
                DIS_CHAR_MODE.WORD_WIDE = 14;
            break;
        case SONG_STYLE16:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR8_16_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA16_16_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 1*16;
                DIS_CHAR_MODE.CHAR_HIGH = 16;
                DIS_CHAR_MODE.CHAR_WIDE = 8;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 2*16;
                DIS_CHAR_MODE.WORD_HIGH = 16;
                DIS_CHAR_MODE.WORD_WIDE = 16;
            break;
        case SONG_STYLE18:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR9_18_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA18_18_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 2*18;
                DIS_CHAR_MODE.CHAR_HIGH = 18;
                DIS_CHAR_MODE.CHAR_WIDE = 9;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 3*18;
                DIS_CHAR_MODE.WORD_HIGH = 18;
                DIS_CHAR_MODE.WORD_WIDE = 18;
            break;
        case SONG_STYLE20:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR10_20_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA20_20_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 2*20;
                DIS_CHAR_MODE.CHAR_HIGH = 20;
                DIS_CHAR_MODE.CHAR_WIDE = 10;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 3*20;
                DIS_CHAR_MODE.WORD_HIGH = 20;
                DIS_CHAR_MODE.WORD_WIDE = 20;
            break;
        case SONG_STYLE22:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR11_22_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA22_22_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 2*22;
                DIS_CHAR_MODE.CHAR_HIGH = 22;
                DIS_CHAR_MODE.CHAR_WIDE = 11;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 3*22;
                DIS_CHAR_MODE.WORD_HIGH = 22;
                DIS_CHAR_MODE.WORD_WIDE = 22;
            break;
        case SONG_STYLE24:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR12_24_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA24_24_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 2*24;
                DIS_CHAR_MODE.CHAR_HIGH = 24;
                DIS_CHAR_MODE.CHAR_WIDE = 12;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 3*24;
                DIS_CHAR_MODE.WORD_HIGH = 24;
                DIS_CHAR_MODE.WORD_WIDE = 24;
            break;
        case SONG_STYLE26:
                DIS_CHAR_MODE.BASE_CHAR_ADD = CHAR13_26_ADD;
                DIS_CHAR_MODE.BASE_WORD_ADD = CHINA26_26_ADD;
                DIS_CHAR_MODE.CHAR_DATA_SIZE = 2*26;
                DIS_CHAR_MODE.CHAR_HIGH = 26;
                DIS_CHAR_MODE.CHAR_WIDE = 13;
                DIS_CHAR_MODE.WORD_DATA_SIZE = 4*26;
                DIS_CHAR_MODE.WORD_HIGH = 26;
                DIS_CHAR_MODE.WORD_WIDE = 26;
            break;
    }
}

void DIS_CHINESE(unsigned int x_start,unsigned int y_start,const char *string)      //显示字符串，支持中英混显/GB2312编码
{
    unsigned char times=0,CACHE=0;
    unsigned long Address;
    unsigned int x=0,z=0,m,n,f;
    unsigned char  WORD_CODE_MSB,WORD_CODE_LSB;
    unsigned int ADD_X_START = x_start, \
                 ADD_Y_START = y_start,  \
                 ADD_X_END = x_start + DIS_CHAR_MODE.WORD_WIDE-1,  \
                 ADD_Y_END = y_start + DIS_CHAR_MODE.WORD_HIGH;
    
    while(*string!='\0')
    { 
        WORD_CODE_MSB = *string++;
        WORD_CODE_LSB = *string++;
        
        if(((unsigned char)WORD_CODE_MSB>=0xA1) &&  ((unsigned char)WORD_CODE_LSB >=0xA1))            //GB2312编码范围,序列号1410  为：啊 后续为：阿埃挨暗   
        {
            Address = (WORD_CODE_MSB - 0xA1) * 94 ;
            Address = (Address + (WORD_CODE_LSB - 0xA1));
            Address =Address *(DIS_CHAR_MODE.WORD_DATA_SIZE);
            Address =Address + DIS_CHAR_MODE.BASE_WORD_ADD ;
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.WORD_DATA_SIZE));            
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.WORD_WIDE-1;
                  
            if((ADD_X_END>(TFT_LINE_NUMBER-1)) )          //超出x地址范围,转到下一行
            {                                           
                 ADD_Y_START = ADD_Y_END+1;
                 ADD_X_START = 0;
                 ADD_X_END = DIS_CHAR_MODE.WORD_WIDE-1;            
                 ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.WORD_HIGH;
                 if (ADD_Y_END > TFT_COLUMN_NUMBER )       //超出Y范围
                 {
                    ADD_Y_START = 0;                          //移动到第一行
                    ADD_Y_END =  DIS_CHAR_MODE.WORD_HIGH; 
                 }
            }
            TFT_SET_ADD(ADD_X_START,ADD_Y_START,ADD_X_END,ADD_Y_END);
            TFT_SEND_CMD(0x2C);     //写数据   
              
            z = 0;
            for (x=0;x<DIS_CHAR_MODE.WORD_HIGH ;x++)      //按行显示
            {   
                    m = DIS_CHAR_MODE.WORD_WIDE / 8;
                    f = DIS_CHAR_MODE.WORD_WIDE % 8;
                    for(n=0;n<m;n++)            //  取完整字节
                    {
                        CACHE = FONT_BUFFER[z++];
                        for(times=0;times<8;times++)
                        {
                            if ((CACHE&0x80)==0)                //无内容,填充底色
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]); 
                                                           
                            }
                            else
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]); 
                                
                            }
                            CACHE = CACHE<<1;
                        }
                    }
                    if (f!=0)
                    {
                        CACHE = FONT_BUFFER[z++];
                        for(times=0;times<f;times++)        //  取不完整字节
                        {
                            if ((CACHE&0x80)==0)                //无内容,填充底色
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]); 
                                                              
                            }
                            else
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]); 
                                    
                            }
                            CACHE = CACHE<<1;
                        }
                    }
            }                
            ADD_X_START = ADD_X_END;  
        }
        else    //英文范围
        {           
            Address = (WORD_CODE_MSB ) * DIS_CHAR_MODE.CHAR_DATA_SIZE + DIS_CHAR_MODE.BASE_CHAR_ADD; 
            string--;
            ADD_X_END = ADD_X_START + DIS_CHAR_MODE.CHAR_WIDE-1;             
            W25QXX_Read(FONT_BUFFER, Address, (DIS_CHAR_MODE.CHAR_DATA_SIZE)); 

            if((ADD_X_END>(TFT_COLUMN_NUMBER-1)) )          //超出x地址范围,转到下一行
            {                                           
                ADD_Y_START = ADD_Y_START+(ADD_X_END/(TFT_COLUMN_NUMBER-1)) * DIS_CHAR_MODE.CHAR_HIGH;
                ADD_X_START = 0;
                ADD_X_END = DIS_CHAR_MODE.CHAR_WIDE-1;            
                ADD_Y_END = ADD_Y_START + DIS_CHAR_MODE.CHAR_HIGH;
                if (ADD_Y_END > (TFT_LINE_NUMBER-1) )       //超出Y范围
                {
                    ADD_Y_START = 0;                          //移动到第一行
                    ADD_Y_END =  DIS_CHAR_MODE.CHAR_HIGH; 
                }
            }
           
            TFT_SET_ADD(ADD_X_START,ADD_Y_START,ADD_X_END,ADD_Y_END);
      
            z = 0;
            for (x=0;x<DIS_CHAR_MODE.CHAR_HIGH ;x++)      //按行显示
            {   
                    m = DIS_CHAR_MODE.CHAR_WIDE / 8;
                    f = DIS_CHAR_MODE.CHAR_WIDE % 8;
                    for(n=0;n<m;n++)            //  取完整字节
                    {
                        CACHE = FONT_BUFFER[z++];
                        for(times=0;times<8;times++)
                        {
                            if ((CACHE&0x80)==0)                //无内容,填充底色
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]); 
                                                           
                            }
                            else
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]); 
                                
                            }
                            CACHE = CACHE<<1;
                        }
                    }
                    if (f!=0)
                    {
                        CACHE = FONT_BUFFER[z++];
                        for(times=0;times<f;times++)        //  取不完整字节
                        {
                            if ((CACHE&0x80)==0)                //无内容,填充底色
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.BACK_COLOR][1]); 
                                                              
                            }
                            else
                            {
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][0]);
                                TFT_SEND_DATA(TAB_COLOR[DIS_CHAR_MODE.FONT_COLOR][1]); 
                                    
                            }
                            CACHE = CACHE<<1;
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
void TFT_ShowASCII(unsigned int x, unsigned int y, const char* str)
{
    // 直接显示ASCII字符，不进行GB2312转换
    DIS_CHINESE(x, y, str);
}

// 显示字符串函数（简化版）
void TFT_ShowString(unsigned int x, unsigned int y, const char* str)
{
    // 使用GB.get()方法转换UTF-8到GB2312
    String gb_str = GB.get(str);
    DIS_CHINESE(x, y, gb_str.c_str());
}

// 显示字符串函数（直接GB2312编码）
void TFT_ShowStringGB2312(unsigned int x, unsigned int y, const char* gb2312_str)
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
void TFT_SetTextColor(unsigned char font_color, unsigned char back_color)
{
    current_font_color = font_color;
    current_back_color = back_color;
    SET_FONT_STYLE(font_color, back_color, current_font_style);
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
void TFT_GetCursor(unsigned int* x, unsigned int* y)
{
    *x = current_x;
    *y = current_y;
}

// 打印函数（带自动换行）
void TFT_Print(const char* str)
{
    unsigned int x = current_x;
    unsigned int y = current_y;
    
    // 使用GB.get()方法转换UTF-8到GB2312
    DIS_CHINESE(x, y, GB.get(str).c_str());
    
    // 更新光标位置（简化处理）
    current_x = 0;
    current_y += DIS_CHAR_MODE.WORD_HIGH + 2; // 加2像素间距
    
    if(current_y >= TFT_LINE_NUMBER)
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
void TFT_ScrollText(unsigned int x, unsigned int y, const char* str, unsigned int scroll_speed)
{
    // 先转换为GB2312
    String gb_str = GB.get(str);
    const char* gb = gb_str.c_str();
    unsigned int max_bytes = (TFT_COLUMN_NUMBER - x) / DIS_CHAR_MODE.WORD_WIDE * 2; // 最多字节数（全汉字时）
    unsigned int gb_len = gb_str.length();

    // 计算每个滚动窗口的起始位置
    for (unsigned int start = 0; start < gb_len;) {
        char temp_str[64];
        unsigned int temp_idx = 0;
        unsigned int byte_count = 0;
        unsigned int i = start;
        // 按字符单位截取，不拆汉字
        while (i < gb_len && byte_count < max_bytes) {
            if ((unsigned char)gb[i] >= 0xA1) {
                // 汉字，2字节
                if (byte_count + 2 > max_bytes || i + 1 >= gb_len) break;
                temp_str[temp_idx++] = gb[i++];
                temp_str[temp_idx++] = gb[i++];
                byte_count += 2;
            } else {
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
        if ((unsigned char)gb[start] >= 0xA1) start += 2;
        else start += 1;
    }
}

// 居中显示文本
void TFT_ShowStringCenter(unsigned int y, const char* str)
{
    // 先转换为GB2312
    String gb_str = GB.get(str);
    unsigned int str_len = gb_str.length();
    unsigned int total_width = str_len * DIS_CHAR_MODE.WORD_WIDE;
    unsigned int x = (TFT_COLUMN_NUMBER - total_width) / 2;
    
    if(x < 0) x = 0;
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
    if(bar_width > 0)
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
    if(percentage > 50) color = GREEN;
    else if(percentage > 20) color = YELLOW;
    else color = RED;
    
    // 绘制电池外框
    TFT_DrawRect(x, y, battery_width, battery_height, WHITE);
    TFT_DrawRect(x + battery_width, y + 2, 2, battery_height - 4, WHITE);
    
    // 绘制电池电量
    unsigned int level_width = (battery_width - 2) * percentage / 100;
    if(level_width > 0)
    {
        TFT_FillRect(x + 1, y + 1, level_width, battery_height - 2, color);
    }            
}

// 更大尺寸的WiFi图标（宽18高12，带边界保护）
void TFT_ShowWiFi(unsigned int x, unsigned int y, bool connected)
{
    unsigned int w = 18, h = 12;
    // 边界保护，防止越界
    if (x + w > TFT_COLUMN_NUMBER) x = (TFT_COLUMN_NUMBER > w) ? (TFT_COLUMN_NUMBER - w) : 0;
    if (y + h > TFT_LINE_NUMBER) y = (TFT_LINE_NUMBER > h) ? (TFT_LINE_NUMBER - h) : 0;
    unsigned int cx = x + w / 2;
    unsigned int cy = y + 3;
    unsigned int color = connected ? WHITE : GRAY;
    // 清除区域（确保不越界）
    TFT_FillRect(x, y, w, h, current_back_color);
    // 画3条WiFi弧线（朝下）
    for (unsigned char i = 1; i <= 3; i++) {
        unsigned int r1 = i * 3, r2 = i * 3 + 1;
        // 只画在屏幕内的部分
        if (cx + r2 < TFT_COLUMN_NUMBER && cx >= r2 && cy + r2 < TFT_LINE_NUMBER && cy >= r2) {
            TFT_DrawArc(cx, cy, r1, r2, 20, 160, color);
        }
    }
    // 画顶部大圆点（确保不越界）
    if (cx + 2 < TFT_COLUMN_NUMBER && cx >= 2 && cy + 2 < TFT_LINE_NUMBER && cy >= 2) {
        TFT_FillCircle(cx, cy, 2, color);
    }
    // 未连接时画红叉（确保不越界）
    if (!connected) {
        unsigned int x1 = x + 3, y1 = y + 3, x2 = x + w - 4, y2 = y + h - 4;
        if (x2 < TFT_COLUMN_NUMBER && y2 < TFT_LINE_NUMBER) {
            TFT_DrawLine(x1, y1, x2, y2, RED);
            TFT_DrawLine(x2, y1, x1, y2, RED);
        }
    }
}

// 辅助函数：画弧线（近似实现，使用点）
void TFT_DrawArc(unsigned int cx, unsigned int cy, unsigned int r1, unsigned int r2, int start_angle, int end_angle, unsigned int color)
{
    for (int angle = start_angle; angle <= end_angle; angle += 4) {
        float rad = angle * 3.14159 / 180.0;
        unsigned int x1 = cx + r1 * cos(rad);
        unsigned int y1 = cy - r1 * sin(rad);
        unsigned int x2 = cx + r2 * cos(rad);
        unsigned int y2 = cy - r2 * sin(rad);
        TFT_DrawLine(x1, y1, x2, y2, color);
    }
}


void TFT_init(void)        ////ST7789V2
{
  SPI_SCK_0;
  SPI_RST_0;
  delay(1000);
  SPI_RST_1;
  delay(1000);
    TFT_SEND_CMD(0x11);       //Sleep Out
  delay(120);               //DELAY120ms 
  //--------------------------------ST7789S Frame rate setting----------------------------------// 
  TFT_SEND_CMD(0x2a);     //Column address set
  TFT_SEND_DATA(0x00);    //start column
  TFT_SEND_DATA(0x00); 
  TFT_SEND_DATA(0x00);    //end column
  TFT_SEND_DATA(0xef);

  TFT_SEND_CMD(0x2b);     //Row address set
  TFT_SEND_DATA(0x00);    //start row
  TFT_SEND_DATA(0x28); 
  TFT_SEND_DATA(0x01);    //end row
  TFT_SEND_DATA(0x17);

  TFT_SEND_CMD(0xb2);     //Porch control
  TFT_SEND_DATA(0x0c); 
  TFT_SEND_DATA(0x0c); 
  TFT_SEND_DATA(0x00); 
  TFT_SEND_DATA(0x33); 
  TFT_SEND_DATA(0x33); 

  TFT_SEND_CMD(0x20);     //Display Inversion Off

  TFT_SEND_CMD(0xb7);     //Gate control
  TFT_SEND_DATA(0x56);      //35
//---------------------------------ST7789S Power setting--------------------------------------// 
  TFT_SEND_CMD(0xbb); //VCOMS Setting
  TFT_SEND_DATA(0x18);  //1f

  TFT_SEND_CMD(0xc0);     //LCM Control
  TFT_SEND_DATA(0x2c); 

  TFT_SEND_CMD(0xc2);     //VDV and VRH Command Enable
  TFT_SEND_DATA(0x01); 

  TFT_SEND_CMD(0xc3); //VRH Set
  TFT_SEND_DATA(0x1f); //12

  TFT_SEND_CMD(0xc4);       //VDV Setting
  TFT_SEND_DATA(0x20); 

  TFT_SEND_CMD(0xc6);       //FR Control 2
  TFT_SEND_DATA(0x0f); 

  TFT_SEND_CMD(0xd0);  //Power Control 1
  TFT_SEND_DATA(0xa6);   //a4
  TFT_SEND_DATA(0xa1); 
//--------------------------------ST7789S gamma setting---------------------------------------// 

  TFT_SEND_CMD(0xe0); 
  TFT_SEND_DATA(0xd0); 
  TFT_SEND_DATA(0x0d); 
  TFT_SEND_DATA(0x14); 
  TFT_SEND_DATA(0x0b); 
  TFT_SEND_DATA(0x0b); 
  TFT_SEND_DATA(0x07); 
  TFT_SEND_DATA(0x3a);  
  TFT_SEND_DATA(0x44); 
  TFT_SEND_DATA(0x50); 
  TFT_SEND_DATA(0x08); 
  TFT_SEND_DATA(0x13); 
  TFT_SEND_DATA(0x13); 
  TFT_SEND_DATA(0x2d); 
  TFT_SEND_DATA(0x32); 

  TFT_SEND_CMD(0xe1);         //Negative Voltage Gamma Contro
  TFT_SEND_DATA(0xd0); 
  TFT_SEND_DATA(0x0d); 
  TFT_SEND_DATA(0x14); 
  TFT_SEND_DATA(0x0b); 
  TFT_SEND_DATA(0x0b); 
  TFT_SEND_DATA(0x07); 
  TFT_SEND_DATA(0x3a); 
  TFT_SEND_DATA(0x44); 
  TFT_SEND_DATA(0x50); 
  TFT_SEND_DATA(0x08); 
  TFT_SEND_DATA(0x13); 
  TFT_SEND_DATA(0x13); 
  TFT_SEND_DATA(0x2d); 
  TFT_SEND_DATA(0x32);
  
  TFT_SEND_CMD(0x36);       //Memory data access control
  TFT_SEND_DATA(0x00); 
  
  TFT_SEND_CMD(0x3A);       //Interface pixel format
  TFT_SEND_DATA(0x55);      //65K 

  TFT_SEND_CMD(0xe7);       //SPI2 enable    启用2数据通道模式
  TFT_SEND_DATA(0x00); 

  TFT_SEND_CMD(0x21);     //Display inversion on
  TFT_SEND_CMD(0x29);       //Display on
} 