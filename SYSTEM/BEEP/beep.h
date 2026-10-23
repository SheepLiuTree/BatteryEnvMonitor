#ifndef __BEEP_H
#define __BEEP_H
#include "sys.h"

/* 有源蜂鸣器驱动  驱动口: PB1 */

#define BEEP_ON()           PBout(1) = 1                            //蜂鸣器响
#define BEEP_OFF()          PBout(1) = 0                            //蜂鸣器停

void BEEP_Init(void);           //蜂鸣器初始化

#endif
