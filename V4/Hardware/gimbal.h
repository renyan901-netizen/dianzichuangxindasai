#ifndef __GIMBAL_H
#define __GIMBAL_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

// ==========================================
// 张大头步进云台控制接口 (带软限位保护)
// ==========================================

// 初始化云台 (将虚拟位置复位到正中心)
void Gimbal_Init(void);

// 设置 X 轴 (水平) 旋转速度
// speed_hz: 频率值。正数向一侧转，负数向另一侧转
void Gimbal_SetSpeed_X(float speed_hz);

// 设置 Y 轴 (俯仰) 旋转速度
// speed_hz: 频率值。正数向上抬，负数向下低
void Gimbal_SetSpeed_Y(float speed_hz);

// 获取当前虚拟角度 (用于OLED调试显示)
float Gimbal_GetAngle_X(void);
float Gimbal_GetAngle_Y(void);

#endif