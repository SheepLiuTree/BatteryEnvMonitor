#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "sys.h" 
//////////////////////////////////////////////////////////////////////////////////	


	
//如果想串口中断接收，请不要注释以下宏定义
void UART1_init(u32 bound);
void UART3_init(u32 bound);
void SendDate(USART_TypeDef* USARTx,unsigned char *Date,unsigned char N);
#endif


