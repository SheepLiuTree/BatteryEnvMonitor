#ifndef __DHT11_H
#define __DHT11_H
#include "sys.h"

/* DHT11温湿度传感器驱动  数据口: PB2 */

#define DHT11_DQ_OUT(x)     PBout(2) = (x)                          //数据口输出电平
#define DHT11_DQ_IN         PBin(2)                                 //数据口输入电平

#define DHT11_IO_OUT()      {GPIOB->CRL &= 0XFFFFF0FF; GPIOB->CRL |= 3 << 8;}   //PB2推挽输出
#define DHT11_IO_IN()       {GPIOB->CRL &= 0XFFFFF0FF; GPIOB->CRL |= 8 << 8;}   //PB2上拉输入

u8 DHT11_Init(void);                    //初始化DHT11,返回0=检测到器件
u8 DHT11_Read_Data(u8 *temp, u8 *humi); //读取温湿度,返回0=成功
u8 DHT11_Read_Byte(void);               //读取一个字节
u8 DHT11_Read_Bit(void);                //读取一位
void DHT11_Rst(void);                   //复位DHT11(起始信号)
u8 DHT11_Check(void);                   //检测DHT11响应信号

#endif
