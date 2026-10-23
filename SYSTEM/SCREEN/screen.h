#ifndef __SCREEN_H
#define __SCREEN_H
#include "stm32f10x.h"

/* 串口屏指令封装(陶晶驰TJC3224k024_011,USART1 PA9/PA10,115200)
 * TJC系列指令格式: 指令内容 + 0xFF 0xFF 0xFF */

//常用RGB565颜色
#define SCREEN_RED      63488
#define SCREEN_GREEN    2016
#define SCREEN_BLUE     31
#define SCREEN_BLACK    0
#define SCREEN_WHITE    65535
#define SCREEN_YELLOW   65504

//页面编号
#define PAGE_MAIN       0       //主页
#define PAGE_MONITOR    1       //监测页面
#define PAGE_SET        2       //设置页面

void Screen_Send(char *s);                          //发送指令(自动补帧尾FF FF FF)
void Screen_Set_Text(char *obj, char *txt);         //文本控件赋值
void Screen_Set_Color(char *obj, u16 color);        //设置控件字体颜色并刷新
void Screen_Page(u8 page);                          //页面切换

#endif
