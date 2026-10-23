/**
  ******************************************************************************
  * @file    stm32f10x_it.c
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and peripherals
  *          interrupt service routine.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "usart.h"

//串口屏数据接收相关变量
u8 USART1_RX_BUF[USART1_RX_LEN];        //串口屏接收缓冲区
volatile u8 USART1_RX_CNT = 0;          //接收字节计数
volatile u8 USART1_RX_FLAG = 0;         //接收完成标志(收到一条完整指令)

static u8 ff_cnt = 0;                   //连续0XFF计数

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/******************************************************************************/

/***********************************************************
*@函数	:USART1_IRQHandler
*@功能	:串口屏串口接收中断(PA9/PA10)
*@说明	:串口屏指令以三个0XFF结尾,收到完整指令置位USART1_RX_FLAG,
*         主循环处理完缓冲区后应清零USART1_RX_FLAG和USART1_RX_CNT
*@日期	:2026-10-22
***********************************************************/
void USART1_IRQHandler(void)
{
    u8 res;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        res = USART_ReceiveData(USART1);
        if (USART1_RX_CNT < USART1_RX_LEN)
        {
            USART1_RX_BUF[USART1_RX_CNT++] = res;
            if (res == 0XFF)                //串口屏每条指令以三个0XFF结束
            {
                ff_cnt++;
                if (ff_cnt >= 3)
                {
                    USART1_RX_FLAG = 1;     //一条指令接收完成
                    ff_cnt = 0;
                }
            }
            else
            {
                ff_cnt = 0;
            }
        }
    }
}
