#include "beep.h"

/***********************************************************
*@函数	:BEEP_Init
*@功能	:蜂鸣器初始化(PB1驱动口,推挽输出,默认关闭)
*@日期	:2026-10-22
***********************************************************/
void BEEP_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   //使能GPIOB时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;               //PB1
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    BEEP_OFF();                                             //默认关闭蜂鸣器
}
