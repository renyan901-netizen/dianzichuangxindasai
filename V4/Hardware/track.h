#ifndef __TRACK_H__
#define __TRACK_H__

#include "ti_msp_dl_config.h"

// ==========================================
// 🚨 新增：将读取函数和数据数组暴露给 main.c 使用
// ==========================================
extern volatile uint8_t IR_Data[8];
void ReadEightIR(void);

// ==========================================
// 原有的函数声明
// ==========================================
void Track_Init(void);
float Track_GetError(void);
uint8_t Track_IsFullBlack(void);
uint8_t Track_IsLineDetected(void); 

#endif