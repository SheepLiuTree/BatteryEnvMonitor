/**
  ******************************************************************************
  * @file    stm32f10x_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  */

#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#include "stm32f10x.h"

/* 串口屏串口接收缓冲区(USART1, PA9/PA10) */
#define USART1_RX_LEN   64                              //接收缓冲区长度
extern u8 USART1_RX_BUF[USART1_RX_LEN];                 //接收缓冲区
extern volatile u8 USART1_RX_CNT;                       //接收字节计数
extern volatile u8 USART1_RX_FLAG;                      //接收完成标志

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void USART1_IRQHandler(void);

#endif /* __STM32F10x_IT_H */
