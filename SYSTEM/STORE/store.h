#ifndef __STORE_H
#define __STORE_H
#include "stm32f10x.h"

/* 参数掉电保存模块: 用STM32内部Flash最后一页(1KB)保存温/湿度报警上下限和传感器位号表
 * STM32F103C8(medium density)页大小1KB,Flash总大小64KB */

#define STORE_FLASH_ADDR    0x0800FC00      //Flash最后一页起始地址
#define STORE_MAGIC         0x59444342      //参数有效标志'BCDY'

#define STORE_TH_DEFAULT    500             //默认温度报警上限50.0度
#define STORE_TH_MIN        100             //温度上限可调下限10.0度
#define STORE_TH_MAX        900             //温度上限可调上限90.0度

#define STORE_TL_DEFAULT    0               //默认温度报警下限0.0度
#define STORE_TL_MIN        0               //温度下限可调下限0.0度
#define STORE_TL_MAX        400             //温度下限可调上限40.0度

#define STORE_HUMI_DEFAULT  70              //默认湿度报警上限70%RH
#define STORE_HUMI_MIN      10              //湿度上限可调下限10%
#define STORE_HUMI_MAX      90              //湿度上限可调上限90%

#define STORE_HL_DEFAULT    10              //默认湿度报警下限10%RH
#define STORE_HL_MIN        10              //湿度下限可调下限10%
#define STORE_HL_MAX        40              //湿度下限可调上限40%

#define STORE_ROM_MAX       18              //最大传感器数量

typedef struct
{
    u32 magic;                              //参数有效标志
    u16 alarm_th;                           //温度报警上限,单位0.1度
    u16 alarm_lo;                           //温度报警下限,单位0.1度
    u16 humi_th;                            //湿度报警上限,单位1%RH
    u16 humi_lo;                            //湿度报警下限,单位1%RH
    u8  rom_cnt;                            //已登记传感器数量
    u8  rsv;                                //保留
    u8  rom_tab[STORE_ROM_MAX][8];          //位号表:位号N对应rom_tab[N-1]的64位ROM码
    u16 crc16;                              //CRC16校验(保存时覆盖前部所有字段)
} StoreData;

extern StoreData g_store;

u8 Store_Load(void);                        //上电读取参数,返回1=参数有效
void Store_Save(void);                      //保存参数到Flash

#endif
