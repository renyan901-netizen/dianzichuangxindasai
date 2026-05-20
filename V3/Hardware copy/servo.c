#include "servo.h"

void Servo_Init(void) {
    // 你的配置中，舵机使用的是 TIMG7
    DL_TimerG_startCounter(PWM_SERVO_INST);
    
    // 初始化云台到正中间 (90度)
    Servo_SetAngle(0, 90.0f); // X轴 (PA23)
    Servo_SetAngle(1, 90.0f); // Y轴 (PA27)
}

// 设置舵机角度 (参数: channel 0为X轴，1为Y轴; angle 0~180)
void Servo_SetAngle(uint8_t channel, float angle) {
    // 限幅保护防烧毁
    if(angle < 0.0f) angle = 0.0f;
    if(angle > 180.0f) angle = 180.0f;
    
    // PWM 占空比计算：20000个tick=20ms。500~2500 对应 0.5ms~2.5ms (0~180度)
    uint32_t compare_value = 500 + (uint32_t)((angle / 180.0f) * 2000.0f);
    uint32_t timer_val = 20000 - compare_value;
    
    // 使用 TimerG 的 API 下发占空比
    if (channel == 0) {
        DL_TimerG_setCaptureCompareValue(PWM_SERVO_INST, timer_val, DL_TIMER_CC_0_INDEX);
    } else if (channel == 1) {
        DL_TimerG_setCaptureCompareValue(PWM_SERVO_INST, timer_val, DL_TIMER_CC_1_INDEX);
    }
}