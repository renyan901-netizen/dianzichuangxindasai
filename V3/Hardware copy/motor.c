/*#include "motor.h"

// 左右轮周期已彻底统一
#define PERIOD 1600 

void Motor_Init(void) {
    DL_TimerA_startCounter(PWM_MOTOR_L_INST);
    DL_TimerA_startCounter(PWM_MOTOR_R_INST);
    // 上电瞬间强制输出硬刹车信号，锁死防抖
    Motor_SetSpeed(0, 0);
}

void Motor_SetSpeed(int8_t left_speed, int8_t right_speed) {
    // 1. 安全限幅
    if(left_speed > 100) left_speed = 100;
    if(left_speed < -100) left_speed = -100;
    if(right_speed > 100) right_speed = 100;
    if(right_speed < -100) right_speed = -100;

    // ==========================================
    // 【左电机】强力慢衰减模式
    // ==========================================
    if (left_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); // IN1=1
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, 0, DL_TIMER_CC_3_INDEX); // 刹车
    } else if (left_speed > 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); // 前进
        uint32_t compare = (left_speed * PERIOD) / 100;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, compare, DL_TIMER_CC_3_INDEX);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); // 后退
        uint32_t speed_abs = -left_speed;
        uint32_t compare = PERIOD - ((speed_abs * PERIOD) / 100);
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, compare, DL_TIMER_CC_3_INDEX);
    }

    // ==========================================
    // 【右电机】强力慢衰减模式 (与左轮逻辑完全对齐)
    // ==========================================
    if (right_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); // IN1=1
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, 0, DL_TIMER_CC_0_INDEX); // 刹车
    } 
    else if (right_speed > 0) 
    {
       DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); // 后退
        uint32_t speed_abs = -right_speed;
        uint32_t compare = PERIOD - ((speed_abs * PERIOD) / 100);
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, compare, DL_TIMER_CC_0_INDEX);
    }
     else 
     {
         DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); // 前进
        uint32_t compare = (right_speed * PERIOD) / 100;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, compare, DL_TIMER_CC_0_INDEX);
    }
}

*/


/*#include "motor.h"

#define PERIOD 1600 

// 🚨 核心黑科技：快衰减模式的起步补偿值 (打底注入动力)
// 目前设为 600。如果跑直线时，发现右轮跑得比左轮快，把 600 改小 (比如500)
// 如果跑直线时右轮依然偏慢，把 600 改大 (比如700)
#define FAST_BOOST 500


// 定义两个全局变量，用来累计脉冲
volatile int32_t left_pulse_count = 0;
volatile int32_t right_pulse_count = 0;

// MSPM0 的 PORTB 中断统一在这个函数里处理
void GROUP1_IRQHandler(void)
{
    // 获取并自动清除中断标志位
    uint32_t pending = DL_GPIO_getPendingInterrupt(GPIO_ENCODER_PORT);

    // =====================================
    // 处理左轮中断 (LA: PB12 触发)
    // =====================================
    if (pending == GPIO_ENCODER_LA_IIDX) {
        // 左轮：现象完美，保持不变！
        if (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_LB_PIN) == 0) {
            left_pulse_count++;
        } else {
            left_pulse_count--; 
        }
    }
    
    // =====================================
    // 处理右轮中断 (RA: PB2 触发)
    // =====================================
    else if (pending == GPIO_ENCODER_RA_IIDX) {
        // 右轮：因为镜像安装导致数值为负，互换 ++ 和 -- 的位置！
        if (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_RB_PIN) == 0) {
            right_pulse_count--; // <--- 这里改成 --
        } else {
            right_pulse_count++; // <--- 这里改成 ++
        }
    }
}
void Motor_Init(void) {
    // 🚨 必须先强制输出 0 占空比，把引脚死死拉低！
    Motor_SetSpeed(0, 0); 
    
    // 然后再启动 PWM 定时器，绝对杜绝默认电平泄露
    DL_TimerA_startCounter(PWM_MOTOR_L_INST);
    DL_TimerA_startCounter(PWM_MOTOR_R_INST);
}


void Motor_SetSpeed(int8_t left_speed, int8_t right_speed) {
    // 1. 绝对安全的限幅
    if(left_speed > 100) left_speed = 100;
    if(left_speed < -100) left_speed = -100;
    if(right_speed > 100) right_speed = 100;
    if(right_speed < -100) right_speed = -100;

    // ==========================================
    // 【左电机】物理前进=慢衰减(LOW电平驱动)；物理后退=快衰减(HIGH电平驱动)
    // ==========================================
    if (left_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, 0, DL_TIMER_CC_3_INDEX);
    } else if (left_speed > 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); // IN1=1 (慢衰减)
        // 慢衰减：正比映射 LOW 电平。CC值越大动力越强。
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, left_speed * 16, DL_TIMER_CC_3_INDEX);
    } else {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); // IN1=0 (快衰减)
        // 快衰减(倒车)：补偿计算真实 HIGH 电平时间
        uint32_t speed_abs = -left_speed;
        uint32_t high_time = FAST_BOOST + speed_abs * 10; 
        if(high_time > 1600) high_time = 1600;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, 1600 - high_time, DL_TIMER_CC_3_INDEX);
    }

    // ==========================================
    // 【右电机】物理前进=快衰减(HIGH电平驱动)；物理后退=慢衰减(LOW电平驱动)
    // ==========================================
    if (right_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, 0, DL_TIMER_CC_0_INDEX);
    } else if (right_speed > 0) {
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); // IN1=0 (快衰减)
        // 快衰减(前进)：补偿计算真实 HIGH 电平时间。
        // 🚨 修正：这次速度越大，high_time 越大，动力真的会越来越猛！
        uint32_t high_time = FAST_BOOST + right_speed * 10;
        if(high_time > 1600) high_time = 1600;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, 1600 - high_time, DL_TIMER_CC_0_INDEX);
    } else {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); // IN1=1 (慢衰减)
        // 慢衰减(倒车)：正比映射 LOW 电平。
        uint32_t speed_abs = -right_speed;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, speed_abs * 16, DL_TIMER_CC_0_INDEX);
    }
}
*/
#include "motor.h"

#define PERIOD 1600 

// 定义两个全局变量，用来累计脉冲
volatile int32_t left_pulse_count = 0;
volatile int32_t right_pulse_count = 0;

// MSPM0 的 PORTB 中断统一在这个函数里处理
void GROUP1_IRQHandler(void)
{
    uint32_t pending = DL_GPIO_getPendingInterrupt(GPIO_ENCODER_PORT);

    // =====================================
    // 处理左轮中断 (LA: PB12 触发)
    // =====================================
    if (pending == GPIO_ENCODER_LA_IIDX) {
        if (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_LB_PIN) == 0) {
            left_pulse_count++;
        } else {
            left_pulse_count--; 
        }
    }
    
    // =====================================
    // 处理右轮中断 (RA: PB2 触发)
    // =====================================
    else if (pending == GPIO_ENCODER_RA_IIDX) {
        // 右轮硬件反装，互换 ++ 和 -- 即可矫正逻辑
        if (DL_GPIO_readPins(GPIO_ENCODER_PORT, GPIO_ENCODER_RB_PIN) == 0) {
            right_pulse_count--; 
        } else {
            right_pulse_count++; 
        }
    }
}

void Motor_Init(void) {
    Motor_SetSpeed(0, 0); 
    DL_TimerA_startCounter(PWM_MOTOR_L_INST);
    DL_TimerA_startCounter(PWM_MOTOR_R_INST);
}

void Motor_SetSpeed(int8_t left_speed, int8_t right_speed) {
    // 1. 绝对安全的限幅
    if(left_speed > 100) left_speed = 100;
    if(left_speed < -100) left_speed = -100;
    if(right_speed > 100) right_speed = 100;
    if(right_speed < -100) right_speed = -100;

    // ==========================================
    // 【左电机】全域丝滑慢衰减
    // ==========================================
    if (left_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, 0, DL_TIMER_CC_3_INDEX);
    } else if (left_speed > 0) {
        // 前进：DIR=1，直接给正比例 PWM
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, left_speed * 16, DL_TIMER_CC_3_INDEX);
    } else {
        // 倒车：DIR=0，将 PWM 翻转 (1600 - 数值)，即可保持平滑的慢衰减！
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_LDIR_PIN); 
        uint32_t speed_abs = -left_speed;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_L_INST, 1600 - (speed_abs * 16), DL_TIMER_CC_3_INDEX);
    }

    // ==========================================
    // 【右电机】全域丝滑慢衰减 (纯软件矫正物理反装)
    // ==========================================
    if (right_speed == 0) {
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, 0, DL_TIMER_CC_0_INDEX);
    } else if (right_speed > 0) {
        // 右轮前进 (物理倒车)：DIR=0，用翻转 PWM 保持平滑
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); 
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, 1600 - (right_speed * 16), DL_TIMER_CC_0_INDEX);
    } else {
        // 右轮倒车 (物理前进)：DIR=1，直接给正比例 PWM
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_RDIR_PIN); 
        uint32_t speed_abs = -right_speed;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_R_INST, speed_abs * 16, DL_TIMER_CC_0_INDEX);
    }
}

