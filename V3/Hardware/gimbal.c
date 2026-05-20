#include "gimbal.h"

// 虚拟位置追踪器 (上电默认居中)
static float virtual_angle_x = 180.0f; 
static float virtual_angle_y = 90.0f;  

#define CTRL_DT 0.02f 
#define DEG_PER_PULSE 0.1125f 

void Gimbal_Init(void) {
    virtual_angle_x = 180.0f;
    virtual_angle_y = 90.0f;
    
    // 🚨 救命修复：上电时直接把定时器关死，绝不允许泄漏出 1 个多余的脉冲！
    DL_Timer_stopCounter(PWM_STP_X_INST);
    DL_Timer_stopCounter(PWM_STP_Y_INST);
    
    DL_Timer_setCaptureCompareValue(PWM_STP_X_INST, 0, DL_TIMER_CC_0_INDEX);
    DL_Timer_setCaptureCompareValue(PWM_STP_Y_INST, 0, DL_TIMER_CC_0_INDEX);
}

void Gimbal_SetSpeed_X(float speed_hz) {
    // 软限位防绞线保护 (10度 ~ 350度)
    if (speed_hz > 0 && virtual_angle_x >= 350.0f) speed_hz = 0.0f; 
    if (speed_hz < 0 && virtual_angle_x <= 10.0f)  speed_hz = 0.0f; 

    float delta_angle = speed_hz * CTRL_DT * DEG_PER_PULSE;
    virtual_angle_x += delta_angle;

    // 🚨 刹车机制升级：直接停掉硬件定时器！
    if (speed_hz > -10.0f && speed_hz < 10.0f) { 
        DL_Timer_stopCounter(PWM_STP_X_INST); 
        return; 
    }

    if (speed_hz > 0) {
        DL_GPIO_setPins(GPIO_DIR_GPIO_DIR_X_PORT, GPIO_DIR_GPIO_DIR_X_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_DIR_GPIO_DIR_X_PORT, GPIO_DIR_GPIO_DIR_X_PIN);
        speed_hz = -speed_hz; 
    }

    if (speed_hz < 50.0f) speed_hz = 50.0f;
    if (speed_hz > 10000.0f) speed_hz = 10000.0f;
    
    uint32_t load_val = 1000000 / (uint32_t)speed_hz;
    
    DL_Timer_setLoadValue(PWM_STP_X_INST, load_val);
    DL_Timer_setCaptureCompareValue(PWM_STP_X_INST, load_val / 2, DL_TIMER_CC_0_INDEX); 
    
    // 🚨 只有当有速度时，才重新启动定时器！
    DL_Timer_startCounter(PWM_STP_X_INST);
}

void Gimbal_SetSpeed_Y(float speed_hz) {
    // Y轴 180 度防撞保护 (10度 ~ 170度)
    if (speed_hz > 0 && virtual_angle_y >= 170.0f) speed_hz = 0.0f; 
    if (speed_hz < 0 && virtual_angle_y <= 10.0f)  speed_hz = 0.0f; 

    float delta_angle = speed_hz * CTRL_DT * DEG_PER_PULSE;
    virtual_angle_y += delta_angle;

    if (speed_hz > -10.0f && speed_hz < 10.0f) {
        DL_Timer_stopCounter(PWM_STP_Y_INST); // 暴力刹车
        return; 
    }

    // ==========================================
    // 🎯 核心修复：把这里的 X 全部换成了 Y！
    // ==========================================
    if (speed_hz > 0) {
        DL_GPIO_setPins(GPIO_DIR_GPIO_DIR_Y_PORT, GPIO_DIR_GPIO_DIR_Y_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_DIR_GPIO_DIR_Y_PORT, GPIO_DIR_GPIO_DIR_Y_PIN);
        speed_hz = -speed_hz; 
    }

    if (speed_hz < 15.0f) speed_hz = 15.0f;//50
    if (speed_hz > 10000.0f) speed_hz = 10000.0f;
    
    uint32_t load_val = 1000000 / (uint32_t)speed_hz;
    DL_Timer_setLoadValue(PWM_STP_Y_INST, load_val);
    DL_Timer_setCaptureCompareValue(PWM_STP_Y_INST, load_val / 2, DL_TIMER_CC_0_INDEX);
    
    DL_Timer_startCounter(PWM_STP_Y_INST);
}

float Gimbal_GetAngle_X(void) { return virtual_angle_x; }
float Gimbal_GetAngle_Y(void) { return virtual_angle_y; }