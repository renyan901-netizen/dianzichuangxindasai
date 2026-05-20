#ifndef HARDWARE_PID_H_
#define HARDWARE_PID_H_

#include <stdint.h>

// PID 结构体定义
typedef struct {
    float Kp;           // 比例系数
    float Ki;           // 积分系数
    float Kd;           // 微分系数
    
    float target;       // 目标值 (循迹小车目标误差永远是 0)
    float error;        // 当前误差
    float last_error;   // 上一次的误差
    float integral;     // 误差积分 (防积分饱和需限幅)
    
    float output;       // PID 计算后的输出值 (转向修正量)
} PID_TypeDef;

// 函数声明
void PID_Init(PID_TypeDef *pid, float p, float i, float d);
float PID_Calc(PID_TypeDef *pid, float current_error);

#endif