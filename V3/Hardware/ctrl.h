#ifndef __CTRL_H__
#define __CTRL_H__

#include <stdint.h>
#include "pid.h"

// ==========================================
// 📺 暴露给 main.c 用于 OLED 刷新的全局变量
// ==========================================
extern uint8_t  target_laps;
extern uint8_t  speed_mode;
extern uint8_t  start_flag;
extern uint16_t corner_count;

// 新增：系统综合模式状态变量
extern uint8_t  sys_mode;      // 0: NORM (常规模式), 1: COMP (综合模式)
extern uint8_t  track_enable;  // 1: 允许底盘循迹移动, 0: 底盘锁死(仅云台打靶)

// 暴露电机 PID 结构体以供 OLED 刷新目标设定速度
extern PID_TypeDef pid_left;
extern PID_TypeDef pid_right;

// ==========================================
// 🚀 核心控制接口函数
// ==========================================
void Ctrl_Init(void);                      // 初始化所有底盘和云台的 PID
void Ctrl_Key_Process(uint8_t key_val);    // 隔离按键逻辑
void Ctrl_Chassis_Process(void);           // 底盘循迹主逻辑
void Ctrl_Gimbal_Process(void);            // 云台视觉主逻辑
void Ctrl_Motor_IRQ_Process(void);         // 底层电机速度环逻辑 (放在定时器中断内)

#endif