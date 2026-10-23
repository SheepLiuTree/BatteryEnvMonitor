#include "store.h"
#include "stm32f10x_flash.h"
#include <string.h>
#include <stddef.h>

StoreData g_store;

/***********************************************************
*@????	:Store_CRC16
*@????	:????CRC16(Modbus§µ??,???????????0xA001)
*@????	:2026-10-22
***********************************************************/
static u16 Store_CRC16(u8 *buf, u16 len)
{
    u16 crc = 0xFFFF;
    u16 i;
    u8 j;

    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 8; j; j--)
        {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }

    return crc;
}

/***********************************************************
*@????	:Store_Load
*@????	:??Flash?????????????§µ????§¹??
*@????	:1=??????§¹,0=??????§¹(???????)
*@????	:2026-10-22
***********************************************************/
u8 Store_Load(void)
{
    memcpy(&g_store, (void *)STORE_FLASH_ADDR, sizeof(StoreData));

    if (g_store.magic != STORE_MAGIC ||
            g_store.crc16 != Store_CRC16((u8 *)&g_store, offsetof(StoreData, crc16)))
    {
        //????§µ?????,???????(?????????????CRC????????)
        g_store.magic = STORE_MAGIC;
        g_store.alarm_th = STORE_TH_DEFAULT;
        g_store.alarm_lo = STORE_TL_DEFAULT;
        g_store.humi_th = STORE_HUMI_DEFAULT;
        g_store.humi_lo = STORE_HL_DEFAULT;
        g_store.rom_cnt = 0;
        g_store.rsv = 0;
        memset(g_store.rom_tab, 0xFF, sizeof(g_store.rom_tab));
        return 0;
    }

    //???????????????
    if (g_store.rom_cnt > STORE_ROM_MAX) g_store.rom_cnt = 0;
    if (g_store.alarm_th < STORE_TH_MIN || g_store.alarm_th > STORE_TH_MAX)
    {
        g_store.alarm_th = STORE_TH_DEFAULT;
    }
    if (g_store.alarm_lo > STORE_TL_MAX)                //u16?????????§³??0,????????
    {
        g_store.alarm_lo = STORE_TL_DEFAULT;
    }
    if (g_store.humi_th < STORE_HUMI_MIN || g_store.humi_th > STORE_HUMI_MAX)
    {
        g_store.humi_th = STORE_HUMI_DEFAULT;
    }
    if (g_store.humi_lo < STORE_HL_MIN || g_store.humi_lo > STORE_HL_MAX)
    {
        g_store.humi_lo = STORE_HL_DEFAULT;
    }

    return 1;
}

/***********************************************************
*@????	:Store_Save
*@????	:?????????§Õ??Flash(??????????§Õ??)
*@???	:?¨²?????????CPU?20~40ms,???????§Ø???????§Ö???
*@????	:2026-10-22
***********************************************************/
void Store_Save(void)
{
    u32 *psrc = (u32 *)&g_store;
    u32 i;
    u32 words = sizeof(StoreData) / 4;

    g_store.crc16 = Store_CRC16((u8 *)&g_store, offsetof(StoreData, crc16));

    FLASH_Unlock();
    FLASH_WaitForLastOperation(0x000FFFFF);                 //?????¦Â??????
    FLASH_ErasePage(STORE_FLASH_ADDR);
    for (i = 0; i < words; i++)
    {
        FLASH_ProgramWord(STORE_FLASH_ADDR + i * 4, psrc[i]);
    }
    FLASH_Lock();
}
