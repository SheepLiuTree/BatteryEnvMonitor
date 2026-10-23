#include "dht11.h"
#include "delay.h"

/***********************************************************
*@函数	:DHT11_Init
*@功能	:DHT11初始化(PB2数据口)
*@返回	:0=检测到器件,1=未检测到
*@日期	:2026-10-22
***********************************************************/
u8 DHT11_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   //使能GPIOB时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;               //PB2
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;        //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    DHT11_DQ_OUT(1);                                        //总线释放
    DHT11_Rst();
    return DHT11_Check();                                   //检测器件是否存在
}

/***********************************************************
*@函数	:DHT11_Rst
*@功能	:复位DHT11(起始信号)
*@日期	:2026-10-22
***********************************************************/
void DHT11_Rst(void)
{
    DHT11_IO_OUT();
    DHT11_DQ_OUT(0);        //拉低至少18ms
    delay_ms(20);
    DHT11_DQ_OUT(1);        //拉高20~40us
    delay_us(30);
}

/***********************************************************
*@函数	:DHT11_Check
*@功能	:检测DHT11响应信号
*@返回	:0=器件存在,1=器件不存在
*@日期	:2026-10-22
***********************************************************/
u8 DHT11_Check(void)
{
    u8 retry = 0;

    DHT11_IO_IN();
    while (DHT11_DQ_IN && retry < 100)                      //等待器件拉低(约83us响应)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)                     //等待器件拉高(约87us)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;

    return 0;
}

/***********************************************************
*@函数	:DHT11_Read_Bit
*@功能	:读取一位数据
*@返回	:读到的位值
*@日期	:2026-10-22
***********************************************************/
u8 DHT11_Read_Bit(void)
{
    u8 retry = 0;

    while (DHT11_DQ_IN && retry < 100)                      //等待本位开始(变为低电平)
    {
        retry++;
        delay_us(1);
    }
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)                     //等待数据位变为高电平
    {
        retry++;
        delay_us(1);
    }
    delay_us(40);                                           //高电平40us后采样
    if (DHT11_DQ_IN) return 1;                              //高电平约70us,数据为1
    else return 0;                                          //高电平26~28us,数据为0
}

/***********************************************************
*@函数	:DHT11_Read_Byte
*@功能	:读取一个字节(高位在前)
*@返回	:读到的字节
*@日期	:2026-10-22
***********************************************************/
u8 DHT11_Read_Byte(void)
{
    u8 i, dat = 0;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= DHT11_Read_Bit();
    }

    return dat;
}

/***********************************************************
*@函数	:DHT11_Read_Data
*@功能	:读取温湿度数据
*@参数	:temp--温度整数部分(℃)  humi--湿度整数部分(%RH)
*@返回	:0=读取成功,1=读取失败
*@日期	:2026-10-22
***********************************************************/
u8 DHT11_Read_Data(u8 *temp, u8 *humi)
{
    u8 buf[5];
    u8 i;

    DHT11_Rst();                                            //发起始信号
    if (DHT11_Check() == 0)                                 //器件有响应
    {
        for (i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();                     //读出40位数据
        }
        if ((u8)(buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])  //校验和正确
        {
            *humi = buf[0];                                 //湿度整数
            *temp = buf[2];                                 //温度整数
        }
    }
    else return 1;

    return 0;
}
