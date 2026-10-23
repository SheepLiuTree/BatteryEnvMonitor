#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>
#include "delay.h"
#include "usart.h"
#include "ds18b20.h"
#include "dht11.h"
#include "beep.h"
#include "store.h"
#include "screen.h"
#include "stm32f10x_it.h"

/* 智能蓄电池环境监测系统 硬件连接:
 * 接线分配:
 *   PB0  -- 18路DS18B20温度传感器单总线(单总线并联,靠出厂唯一ROM序列号区分)
 *   PA9  -- 串口发送TX (USART1,串口屏TJC3224k024_011,115200)
 *   PA10 -- 串口接收RX (USART1)
 *   PB1  -- 电源蜂鸣器控制(高电平导通)
 *   PB2  -- DHT11湿度传感器数据口(单只,整数%RH)
 * 软件功能:
 *   1.温度巡检:轮询已登记的DS18B20(最多18路),阈值比较结果存内部Flash
 *   2.湿度检测:单只DHT11,每HUMI_READ_DIV次巡检周期读取一次,阈值报警
 *   3.传感器注册:编号注册(总线仅单只新的)与加热注册(多只同时在线)
 *   4.超温/超湿报警:蜂鸣器提示,屏上对应控件红色显示
 * 数据阈值单位: 温度0.1度,湿度1%RH */

#define ALARM_TH_STEP       5           //温度报警阈值步进(0.5度)
#define HUMI_TH_STEP        1           //湿度报警阈值步进(1%RH)
#define HEAT_DELTA          30          //加热注册判定温升阈值(3.0度)
#define BEEP_TICK_MS        10          //蜂鸣计时基准周期(ms)
#define HUMI_READ_DIV       3           //每3次巡检周期读取一次湿度

//系统模式
typedef enum
{
    MODE_MONITOR = 0,                   //监测巡检模式
    MODE_REG_ADD,                       //编号注册:单只新传感器自动识别
    MODE_REG_HEAT,                      //加热注册:多只新传感器逐一升温
} SysMode;

//蜂鸣延时计时(10ms)
u16 beep_timer = 0;                     //蜂鸣剩余时间(10ms单位)

//监测模式状态
u8 mon_state = 0;
u16 mon_wait = 0;
u8 mon_idx = 0;

//编号注册状态
u8 reg_state = 0;
u16 reg_wait = 0;
u8 search_tab[DS18B20_MAX_NUM][8];      //总线搜索结果表
u8 search_cnt = 0;
u8 new_list[DS18B20_MAX_NUM][8];        //未登记新ROM列表
u8 new_cnt = 0;

//加热注册状态
u8 heat_state = 0;
u16 heat_wait = 0;
u8 heat_idx = 0;
u8 cand_tab[DS18B20_MAX_NUM][8];        //未登记候选传感器表(动态)
u8 cand_cnt = 0;
short baseline[DS18B20_MAX_NUM];        //加热前基准温度
short heat_cur = 0;                     //当前轮前温度
u8 heat_hot[DS18B20_MAX_NUM];           //升温触发标志表

//公共数据
SysMode mode = MODE_MONITOR;            //当前工作模式
u8 cur_page = 0;                        //屏幕当前页号
short temp_tab[STORE_ROM_MAX];          //各路温度,单位0.1度
u8 alarm_flag[STORE_ROM_MAX];           //各路温度报警标志
u8 alarm_prev[STORE_ROM_MAX];           //上次显示颜色状态(用于边界刷新)
u8 home_alarm_prev = 0xFF;              //主页报警状态缓存
u8 t18_alarm_prev = 0xFF;               //监测页汇总状态缓存

//湿度数据
u8 humi_val = 0;                        //当前湿度(%RH)
u8 humi_ok = 0;                         //湿度读取有效标志
u8 humi_alarm = 0;                      //湿度报警标志
u8 humi_div = 0;                        //湿度读取分频计数
u8 humi_disp_prev = 0xFF;               //t19颜色状态缓存

void Reset_Tasks(void);
void Humi_Task(void);
void Home_Show(void);
void Monitor_Page_Init(void);
void Monitor_Page_Show(void);
void Set_Page_Show(void);
void Set_Msg(char *txt);
void Cmd_Process(void);
void Monitor_Tick(void);
void RegAdd_Tick(void);
void RegHeat_Tick(void);

/***********************************************************
*@名称	:Reset_Tasks
*@功能	:复位全部任务状态(切换模式或页面时调用)
*@日期	:2026-10-22
***********************************************************/
void Reset_Tasks(void)
{
    mon_state = 0;
    mon_wait = 0;
    reg_state = 0;
    reg_wait = 0;
    heat_state = 0;
    heat_wait = 0;
    beep_timer = 0;
    BEEP_OFF();
    memset(alarm_prev, 0, sizeof(alarm_prev));
    home_alarm_prev = 0xFF;
    t18_alarm_prev = 0xFF;
    humi_disp_prev = 0xFF;              //湿度颜色强制下次刷新
}

/***********************************************************
*@名称	:Humi_Task
*@功能	:低频读取一次DHT11湿度(耗时约25ms,拆20ms以上信号)
*@说明	:DHT11两次读取间隔应大于1秒,故每HUMI_READ_DIV次巡检周期读一次
*@日期	:2026-10-22
***********************************************************/
void Humi_Task(void)
{
    u8 ht;

    if (++humi_div < HUMI_READ_DIV) return;             //分频,降低读取频率
    humi_div = 0;
    ht = 0;
    humi_ok = (DHT11_Read_Data(&ht, &humi_val) == 0);   //读取失败保持无效标志
}

/***********************************************************
*@名称	:Home_Show
*@功能	:刷新主页显示(页号0)
*@说明	:t1报警状态 t2已登记数量
*@日期	:2026-10-22
***********************************************************/
void Home_Show(void)
{
    char buf[24];
    u8 i;
    u8 any = 0;
    u8 st;

    for (i = 0; i < g_store.rom_cnt; i++)
    {
        if (alarm_flag[i])
        {
            any = 1;
            break;
        }
    }
    if (humi_alarm) any = 1;                            //湿度报警同样汇总

    if (any) strcpy(buf, "监测报警中!");
    else if (g_store.rom_cnt == 0) strcpy(buf, "未登记传感器!");
    else strcpy(buf, "监测运行正常");
    Screen_Set_Text("t1", buf);

    st = any ? 1 : 0;
    if (st != home_alarm_prev)                          //状态变化才刷新颜色
    {
        home_alarm_prev = st;
        Screen_Set_Color("t1", any ? SCREEN_RED : SCREEN_GREEN);
    }

    sprintf(buf, "已登记:%d/18", g_store.rom_cnt);
    Screen_Set_Text("t2", buf);
}

/***********************************************************
*@名称	:Monitor_Page_Init
*@功能	:监测页控件填充占位(页号1)
*@日期	:2026-10-22
***********************************************************/
void Monitor_Page_Init(void)
{
    char obj[6];
    u8 i;

    for (i = 0; i < DS18B20_MAX_NUM; i++)
    {
        sprintf(obj, "t%d", i);
        Screen_Set_Text(obj, "--");
    }
    Screen_Set_Text("t18", "正在巡检...");
    Screen_Set_Text("t19", "湿度:--");
}

/***********************************************************
*@名称	:Monitor_Page_Show
*@功能	:刷新监测页显示(页号1,t0~t17温度,t18汇总,t19湿度)
*@日期	:2026-10-22
***********************************************************/
void Monitor_Page_Show(void)
{
    char obj[6], buf[24];
    u8 i;
    u8 first_alarm = 0xFF;
    u8 any = 0;

    //18路温度显示: 编号:温度
    for (i = 0; i < DS18B20_MAX_NUM; i++)
    {
        sprintf(obj, "t%d", i);
        if (i >= g_store.rom_cnt) strcpy(buf, "--");                    //未登记
        else if (temp_tab[i] == DS18B20_ERR_VAL) strcpy(buf, "ERR");    //读取失败
        else sprintf(buf, "%02d:%.1f", i + 1, temp_tab[i] / 10.0);
        Screen_Set_Text(obj, buf);

        if (i < g_store.rom_cnt && alarm_flag[i]) any = 1;
        if (i < g_store.rom_cnt && alarm_flag[i] != alarm_prev[i])
        {
            alarm_prev[i] = alarm_flag[i];                              //报警状态变化才刷新颜色
            Screen_Set_Color(obj, alarm_flag[i] ? SCREEN_RED : SCREEN_BLACK);
        }
    }

    //汇总栏: 优先显示温度报警,其次湿度报警
    for (i = 0; i < g_store.rom_cnt; i++)
    {
        if (alarm_flag[i])
        {
            first_alarm = i;
            break;
        }
    }
    if (first_alarm != 0xFF)
        sprintf(buf, "温度报警:%02d号 %.1f度", first_alarm + 1, temp_tab[first_alarm] / 10.0);
    else if (humi_alarm)
        sprintf(buf, "湿度报警:%d%%", humi_val);
    else
        strcpy(buf, "正在巡检...");
    Screen_Set_Text("t18", buf);

    if (humi_alarm) any = 1;                            //湿度报警也令汇总变红
    if (any != t18_alarm_prev)
    {
        t18_alarm_prev = any;
        Screen_Set_Color("t18", any ? SCREEN_RED : SCREEN_GREEN);
    }

    //湿度显示(t19): 单只DHT11
    if (!humi_ok) strcpy(buf, "湿度:ERR");
    else sprintf(buf, "湿度:%d%%", humi_val);
    Screen_Set_Text("t19", buf);

    if (humi_alarm != humi_disp_prev)
    {
        humi_disp_prev = humi_alarm;
        Screen_Set_Color("t19", humi_alarm ? SCREEN_RED : SCREEN_BLACK);
    }
}

/***********************************************************
*@名称	:Set_Page_Show
*@功能	:刷新设置页显示(页号2)
*@说明	:t0已登记 t1下一个待注册 t3温度阈值 t4湿度阈值
*@日期	:2026-10-22
***********************************************************/
void Set_Page_Show(void)
{
    char buf[24];

    sprintf(buf, "已登记:%d/18", g_store.rom_cnt);
    Screen_Set_Text("t0", buf);

    if (g_store.rom_cnt >= STORE_ROM_MAX)
        strcpy(buf, "下一个待注册:已满");
    else
        sprintf(buf, "下一个待注册:%02d", g_store.rom_cnt + 1);
    Screen_Set_Text("t1", buf);

    sprintf(buf, "温度阈值:%d.%d度", g_store.alarm_th / 10, g_store.alarm_th % 10);
    Screen_Set_Text("t3", buf);

    sprintf(buf, "湿度阈值:%d%%", g_store.humi_th);
    Screen_Set_Text("t4", buf);
}

/***********************************************************
*@名称	:Set_Msg
*@功能	:设置页提示信息刷新(t2控件)
*@日期	:2026-10-22
***********************************************************/
void Set_Msg(char *txt)
{
    Screen_Set_Text("t2", txt);
}

/***********************************************************
*@名称	:Monitor_Tick
*@功能	:监测模式状态机(10ms周期调用一次)
*@说明	:温度转换(12位约750ms)期间穿插读取湿度,等待窗口供主循环响应
*@日期	:2026-10-22
***********************************************************/
void Monitor_Tick(void)
{
    u8 i, any;
    u8 first_alarm;

    switch (mon_state)
    {
    case 0:                                             //启动新一轮巡检
        if (g_store.rom_cnt == 0)                       //无温度传感器时,只测湿度
        {
            Humi_Task();
            humi_alarm = (humi_ok && (u16)humi_val >= g_store.humi_th) ? 1 : 0;
            if (cur_page == PAGE_MAIN) Home_Show();
            if (cur_page == PAGE_MONITOR) Monitor_Page_Show();
            mon_wait = 100;
            mon_state = 5;
            break;
        }
        DS18B20_Start_All();
        mon_wait = 76;                                  //等待转换完成约750ms
        mon_state = 1;
        break;

    case 1:                                             //等待温度转换
        if (mon_wait == 76)                             //等待窗口穿插读取一次湿度
        {
            Humi_Task();
        }
        if (--mon_wait == 0)
        {
            mon_idx = 0;
            mon_state = 2;
        }
        break;

    case 2:                                             //逐只读取(瞬间等待)
        temp_tab[mon_idx] = DS18B20_Read_Temp_By_Rom(g_store.rom_tab[mon_idx]);
        alarm_flag[mon_idx] = (temp_tab[mon_idx] >= (short)g_store.alarm_th &&
                              temp_tab[mon_idx] != DS18B20_ERR_VAL) ? 1 : 0;
        mon_idx++;
        if (mon_idx >= g_store.rom_cnt)                 //全部读取完成
        {
            any = 0;
            first_alarm = 0xFF;
            for (i = 0; i < g_store.rom_cnt; i++)
            {
                if (alarm_flag[i])
                {
                    any = 1;
                    if (first_alarm == 0xFF) first_alarm = i;
                }
            }
            humi_alarm = (humi_ok && (u16)humi_val >= g_store.humi_th) ? 1 : 0;
            if (humi_alarm) any = 1;                    //湿度报警同样触发报警
            if (cur_page == PAGE_MONITOR) Monitor_Page_Show();
            if (cur_page == PAGE_MAIN) Home_Show();

            if (any)                                    //有报警,响250ms
            {
                BEEP_ON();
                beep_timer = 25;
            }
            mon_wait = 20;
            mon_state = 3;
        }
        break;

    case 3:                                             //巡检间歇
        if (--mon_wait == 0) mon_state = 0;
        break;

    case 5:                                             //无传感器纯湿度定时
        if (--mon_wait == 0) mon_state = 0;
        break;

    default:
        mon_state = 0;
        break;
    }
}

/***********************************************************
*@名称	:RegAdd_Tick
*@功能	:编号注册状态机(500ms轮询自动识别新传感器)
*@说明	:新传感器ROM与已登记位号表比对,恰好发现一只新ROM时自动登记
*@日期	:2026-10-22
***********************************************************/
void RegAdd_Tick(void)
{
    char buf[24];
    u8 s, r;
    u8 found;

    switch (reg_state)
    {
    case 0:                                             //500ms轮询等待
        if (++reg_wait < 50) break;
        reg_wait = 0;
        reg_state = 1;
        break;

    case 1:                                             //单总线搜索并筛出新传感器
        search_cnt = DS18B20_Search_All(search_tab, DS18B20_MAX_NUM);
        new_cnt = 0;
        for (s = 0; s < search_cnt; s++)
        {
            found = 0;
            for (r = 0; r < g_store.rom_cnt; r++)
            {
                if (!memcmp(search_tab[s], g_store.rom_tab[r], 8))
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                memcpy(new_list[new_cnt], search_tab[s], 8);
                new_cnt++;
            }
        }

        if (new_cnt == 1)                               //恰好一只新传感器,登记
        {
            if (g_store.rom_cnt >= STORE_ROM_MAX)
            {
                Set_Msg("已达注册18路");
                mode = MODE_MONITOR;
                Reset_Tasks();
                break;
            }
            memcpy(g_store.rom_tab[g_store.rom_cnt], new_list[0], 8);
            g_store.rom_cnt++;
            Store_Save();                               //立即写Flash
            BEEP_ON();                                  //蜂鸣提示注册成功
            beep_timer = 20;
            sprintf(buf, "新编%02d注册成功", g_store.rom_cnt);
            Set_Msg(buf);
            Set_Page_Show();
        }
        else if (new_cnt == 0)
        {
            Set_Msg("未检测到新传感器,请接线...");
        }
        else
        {
            Set_Msg("发现多只新传感器!");
        }
        reg_state = 0;
        break;

    default:
        reg_state = 0;
        break;
    }
}

/***********************************************************
*@名称	:RegHeat_Tick
*@功能	:加热注册状态机:多只新传感器通过逐只加热分辨登记
*@说明	:先采集全部未登记传感器基准温度,人工加热单只,再次采集3秒
*         后恰好只有一只升温时自动登记,直至全部注册完成
*@日期	:2026-10-22
***********************************************************/
void RegHeat_Tick(void)
{
    char buf[24];
    u8 s, r, i;
    u8 found;
    u8 hot_cnt, hot_idx;

    switch (heat_state)
    {
    case 0:                                             //搜索总线,筛出未登记候选传感器
        search_cnt = DS18B20_Search_All(search_tab, DS18B20_MAX_NUM);
        cand_cnt = 0;
        for (s = 0; s < search_cnt; s++)
        {
            found = 0;
            for (r = 0; r < g_store.rom_cnt; r++)
            {
                if (!memcmp(search_tab[s], g_store.rom_tab[r], 8))
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                memcpy(cand_tab[cand_cnt], search_tab[s], 8);
                cand_cnt++;
            }
        }

        if (cand_cnt == 0)                              //没有候选传感器
        {
            Set_Msg("总线上无可注册传感器");
            heat_wait = 100;
            heat_state = 9;
            break;
        }
        DS18B20_Start_All();
        heat_wait = 76;
        heat_state = 1;
        break;

    case 1:                                             //等待本轮转换完成
        if (--heat_wait == 0)
        {
            heat_idx = 0;
            heat_state = 2;
        }
        break;

    case 2:                                             //逐只读取基准温度
        baseline[heat_idx] = DS18B20_Read_Temp_By_Rom(cand_tab[heat_idx]);
        heat_idx++;
        if (heat_idx >= cand_cnt)
        {
            Set_Msg("基准温度已采集,请加热");
            heat_wait = 40;
            heat_state = 3;
        }
        break;

    case 3:                                             //等待加热后转换
        if (--heat_wait) break;
        DS18B20_Start_All();
        heat_wait = 76;
        heat_state = 4;
        break;

    case 4:                                             //等待转换完成
        if (--heat_wait == 0)
        {
            heat_idx = 0;
            heat_state = 5;
        }
        break;

    case 5:                                             //逐只比较温升
        heat_cur = DS18B20_Read_Temp_By_Rom(cand_tab[heat_idx]);
        heat_hot[heat_idx] = (heat_cur != DS18B20_ERR_VAL &&
                              baseline[heat_idx] != DS18B20_ERR_VAL &&
                              (heat_cur - baseline[heat_idx]) >= HEAT_DELTA) ? 1 : 0;
        heat_idx++;
        if (heat_idx >= cand_cnt)
        {
            hot_cnt = 0;
            hot_idx = 0xFF;
            for (i = 0; i < cand_cnt; i++)
            {
                if (heat_hot[i])
                {
                    hot_cnt++;
                    if (hot_idx == 0xFF) hot_idx = i;
                }
            }

            if (hot_cnt == 1)                           //恰好一只被加热,登记
            {
                memcpy(g_store.rom_tab[g_store.rom_cnt], cand_tab[hot_idx], 8);
                g_store.rom_cnt++;
                Store_Save();
                BEEP_ON();
                beep_timer = 30;
                sprintf(buf, "新编%02d注册成功", g_store.rom_cnt);
                Set_Msg(buf);
                Set_Page_Show();

                for (i = hot_idx; i < cand_cnt - 1; i++)//从候选表移除已登记的传感器
                {
                    memcpy(cand_tab[i], cand_tab[i + 1], 8);
                    baseline[i] = baseline[i + 1];
                }
                cand_cnt--;

                if (cand_cnt == 0)                      //全部注册完成
                {
                    Set_Msg("全部注册完成");
                    mode = MODE_MONITOR;
                    Reset_Tasks();
                }
                else
                {
                    heat_wait = 40;                     //剩余候选重采基准
                    heat_state = 3;
                }
            }
            else if (hot_cnt == 0)
            {
                heat_wait = 20;                         //没有升温,继续等待加热
                heat_state = 3;
            }
            else
            {
                Set_Msg("多只同时升温,请逐只操作!");
                heat_wait = 100;                        //提示后暂停一轮
                heat_state = 3;
            }
        }
        break;

    case 9:                                             //无候选传感器,延时重试
        if (--heat_wait == 0) heat_state = 0;
        break;

    default:
        heat_state = 0;
        break;
    }
}

/***********************************************************
*@名称	:Cmd_Process
*@功能	:解析串口屏按键指令(帧尾FF FF FF)
*@说明	:支持指令:
*         PG0/PG1/PG2--页面切换指令
*         REG1--编号注册模式    REG2--加热注册模式
*         THUP/THDN--温度阈值加减   HUP/HDN--湿度阈值加减
*         DEL--删除最后一只传感器   CLR--清空所有登记
*@日期	:2026-10-22
***********************************************************/
void Cmd_Process(void)
{
    char cmd[12];
    char buf[24];
    u8 i, n = 0;

    for (i = 0; i < USART1_RX_CNT && n < 11; i++)       //提取帧内ASCII指令
    {
        if (USART1_RX_BUF[i] == 0xFF) break;
        cmd[n++] = USART1_RX_BUF[i];
    }
    cmd[n] = 0;

    USART1_RX_CNT = 0;                                  //复位接收状态
    USART1_RX_FLAG = 0;

    if (!strcmp(cmd, "PG0"))                            //返回主页键
    {
        mode = MODE_MONITOR;
        cur_page = 0;
        Reset_Tasks();
        Home_Show();
    }
    else if (!strcmp(cmd, "PG1"))                       //进入监测页
    {
        mode = MODE_MONITOR;
        cur_page = 1;
        Reset_Tasks();
        Monitor_Page_Init();
    }
    else if (!strcmp(cmd, "PG2"))                       //进入参数设置页
    {
        mode = MODE_MONITOR;
        cur_page = 2;
        Reset_Tasks();
        Set_Page_Show();
        Set_Msg("等待指令...");
    }
    else if (!strcmp(cmd, "REG1"))                      //编号注册
    {
        if (cur_page == PAGE_SET)
        {
            mode = MODE_REG_ADD;
            Reset_Tasks();
            Set_Msg("编号注册:接好单只新传感器");
        }
    }
    else if (!strcmp(cmd, "REG2"))                      //加热注册
    {
        if (cur_page == PAGE_SET)
        {
            mode = MODE_REG_HEAT;
            Reset_Tasks();
            Set_Msg("加热注册:请加热新传感器");
        }
    }
    else if (!strcmp(cmd, "THUP"))                      //温度阈值+0.5度
    {
        if (g_store.alarm_th < STORE_TH_MAX) g_store.alarm_th += ALARM_TH_STEP;
        Store_Save();
        Set_Page_Show();
    }
    else if (!strcmp(cmd, "THDN"))                      //温度阈值-0.5度
    {
        if (g_store.alarm_th > STORE_TH_MIN) g_store.alarm_th -= ALARM_TH_STEP;
        Store_Save();
        Set_Page_Show();
    }
    else if (!strcmp(cmd, "HUP"))                       //湿度阈值+1%
    {
        if (g_store.humi_th < STORE_HUMI_MAX) g_store.humi_th += HUMI_TH_STEP;
        Store_Save();
        Set_Page_Show();
    }
    else if (!strcmp(cmd, "HDN"))                       //湿度阈值-1%
    {
        if (g_store.humi_th > STORE_HUMI_MIN) g_store.humi_th -= HUMI_TH_STEP;
        Store_Save();
        Set_Page_Show();
    }
    else if (!strcmp(cmd, "DEL"))                       //删除已登记的传感器
    {
        if (g_store.rom_cnt > 0)
        {
            g_store.rom_cnt--;
            memset(alarm_flag, 0, sizeof(alarm_flag));
            Store_Save();
            sprintf(buf, "已删除编号%02d", g_store.rom_cnt + 1);
            Set_Msg(buf);
            Set_Page_Show();
        }
    }
    else if (!strcmp(cmd, "CLR"))                       //清空所有登记
    {
        g_store.rom_cnt = 0;
        memset(alarm_flag, 0, sizeof(alarm_flag));
        Store_Save();
        Set_Msg("已清除全部传感器");
        Set_Page_Show();
    }
}

/***********************************************************
*@名称	:main
*@功能	:主循环入口
*@日期	:2026-10-22
***********************************************************/
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);     //NVIC中断分组2
    delay_init();                                       //延时函数初始化(SysTick)
    UART1_init(115200);                                 //串口初始化,连接串口屏
    BEEP_Init();                                        //蜂鸣器引脚初始化
    Store_Load();                                       //从Flash读取阈值和位号表
    DS18B20_Init();                                     //DS18B20单总线初始化
    DHT11_Init();                                       //DHT11湿度传感器初始化

    memset(temp_tab, 0, sizeof(temp_tab));
    memset(alarm_flag, 0, sizeof(alarm_flag));
    memset(alarm_prev, 0, sizeof(alarm_prev));

    BEEP_ON();                                          //开机提示音
    delay_ms(100);
    BEEP_OFF();

    Screen_Page(PAGE_MAIN);                             //显示主页
    delay_ms(200);                                      //等待页面切换完成
    Home_Show();

    while (1)
    {
        //解析串口屏指令(收到完整一帧指令)
        if (USART1_RX_FLAG || USART1_RX_CNT >= USART1_RX_LEN - 1)
        {
            Cmd_Process();
        }

        //蜂鸣时间到自动关闭
        if (beep_timer)
        {
            beep_timer--;
            if (beep_timer == 0) BEEP_OFF();
        }

        //根据当前模式跑状态机
        if (mode == MODE_MONITOR) Monitor_Tick();
        else if (mode == MODE_REG_ADD) RegAdd_Tick();
        else RegHeat_Tick();

        delay_ms(BEEP_TICK_MS);                         //10ms固定节拍
    }
}
