
//1.0
// #ifndef __CTRL_H
// #define __CTRL_H

// #include <stdint.h>

// // 1. 底盘独立工作模式 (去掉了视觉，视觉变成独立开关)
// typedef enum {
//     CHASSIS_IDLE = 0,     // K4: 待机停驶
//     CHASSIS_TRACKING = 1, // K2: 循迹模式
//     CHASSIS_STRAIGHT = 2  // K1: 纯直行模式
// } ChassisMode_e;

// // 2. 接口函数
// void Ctrl_Init(void);
// void Ctrl_Process(void);
// void Ctrl_KeyScan(void);

// // 获取状态供 OLED 显示
// ChassisMode_e Ctrl_GetChassisMode(void);
// uint8_t Ctrl_GetVisionState(void); 

// #endif


//test

#ifndef __CTRL_H
#define __CTRL_H

#include <stdint.h>

// 1. 全局核心模式枚举 (已修正为顺延的 M0, M1, M2)
typedef enum {
    MODE_IDLE = 0,        // 上电默认：待机空闲 (电机锁死，云台复位)
    MODE_M0_TRACK = 1,    // M0: 单循迹模式 (纯底盘，视觉关)
    MODE_M1_GIMBAL = 2,   // M1: 单云台模式 (底盘死锁，视觉开)
    MODE_M2_ALL = 3       // M2: 循迹 + 云台双开
} CarMode_e;

// 2. 调参子菜单枚举 (仅在 M0 / M2 下可用)
typedef enum {
    SET_NONE = 0,         // 正常运行状态
    SET_SPEED = 1,        // 设置直线速度
    SET_LAPS = 2          // 设置目标圈数
} SettingMode_e;

// 3. 接口函数
void Ctrl_Init(void);
void Ctrl_Process(void);
void Ctrl_KeyScan(void);

// 4. 获取状态供 OLED 显示的接口
CarMode_e Ctrl_GetMode(void);
SettingMode_e Ctrl_GetSettingMode(void);
uint8_t Ctrl_GetConfigSpeed(void); 
uint8_t Ctrl_GetConfigLaps(void);  
uint8_t Ctrl_GetRemainLaps(void);  

#endif

