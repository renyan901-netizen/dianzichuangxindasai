#include "ti_msp_dl_config.h"
#include "oled_hardware_i2c.h"
#include "key.h"
#include "track.h"
#include "motor.h"
#include "gimbal.h"
#include "k230.h"
#include "ctrl.h" 

void delay_seconds(uint32_t s) {
    for(uint32_t i = 0; i < s; i++) delay_cycles(32000000); 
}

int main(void) {
    // 1. 底层硬件与中断初始化
    SYSCFG_DL_init();
    NVIC_ClearPendingIRQ(GPIOB_INT_IRQn);
    NVIC_EnableIRQ(GPIOB_INT_IRQn);       
    NVIC_ClearPendingIRQ(TIMER_PID_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_PID_INST_INT_IRQN); 
    NVIC_EnableIRQ(UART_K230_INST_INT_IRQN); 

    // 2. 模块初始化
    Motor_Init(); 
    Gimbal_Init();
    Track_Init(); 
    OLED_Init();

    // 3. 核心控制大脑初始化
    Ctrl_Init(); 
    DL_TimerG_startCounter(TIMER_PID_INST);

    delay_seconds(1);
    OLED_Clear();

    uint16_t oled_refresh_cnt = 0;

    while (1) {
        // ===========================================
        // [模块 1] 扫描按键并送入控制大脑
        // ===========================================
        uint8_t key_val = Key_Scan(0);
        Ctrl_Key_Process(key_val);

        // ===========================================
        // [模块 2] UI 手势控制战术仪表盘刷新系统
        // ===========================================
        oled_refresh_cnt++;
        if (oled_refresh_cnt >= 10) {
            oled_refresh_cnt = 0;

            if (!gesture_control_active) {
                // 上电未激活状态 UI：提示用户按下 Key 1
                OLED_ShowString(0, 0, (uint8_t *)"* SYSTEM LOCKED *", 16);
                OLED_ShowString(0, 2, (uint8_t *)"PRESS KEY1 START", 16);
                OLED_ShowString(0, 4, (uint8_t *)"STATUS: LOCKED  ", 16);
                OLED_ShowString(0, 6, (uint8_t *)"UART PKTS:", 16);
                OLED_ShowNum(80, 6, raw_byte_count, 5, 16);
            } else {
                // 已按下按键 1 激活后的手势控制 UI：
                // 第一行：手势控制模式标志
                OLED_ShowString(0, 0, (uint8_t *)"* GESTURE CTRL *", 16);

                // 第二行：运动状态显示
                OLED_ShowString(0, 2, (uint8_t *)"STATE: ", 16);
                if (g_car_state == CAR_STATE_FORWARD) {
                    OLED_ShowString(48, 2, (uint8_t *)"FORWARD ", 16);
                } else if (g_car_state == CAR_STATE_BACKWARD) {
                    OLED_ShowString(48, 2, (uint8_t *)"BACKWARD", 16);
                } else {
                    OLED_ShowString(48, 2, (uint8_t *)"STOP    ", 16);
                }

                // 第三行：左轮目标与实际速度对照
                OLED_ShowString(0, 4, (uint8_t *)"L:T", 16);
                OLED_ShowFloat(24, 4, pid_left.target, 2, 1, 16);
                OLED_ShowString(56, 4, (uint8_t *)" A:", 16);
                OLED_ShowFloat(80, 4, g_act_speed_L, 2, 1, 16);

                // 第四行：右轮目标与实际速度对照
                OLED_ShowString(0, 6, (uint8_t *)"R:T", 16);
                OLED_ShowFloat(24, 6, pid_right.target, 2, 1, 16);
                OLED_ShowString(56, 6, (uint8_t *)" A:", 16);
                OLED_ShowFloat(80, 6, g_act_speed_R, 2, 1, 16);
            }
        }

        // ===========================================
        // [模块 3] 执行双线核心任务
        // ===========================================
        Ctrl_Chassis_Process();  // 底盘任务
        Ctrl_Gimbal_Process();   // 云台任务

        delay_cycles(160000); 
    }
}

// ========================================================
// 底层定时器中断 (转交控制权给大脑)
// ========================================================
void TIMER_PID_INST_IRQHandler(void) {
    switch (DL_TimerG_getPendingInterrupt(TIMER_PID_INST)) {
        case DL_TIMER_IIDX_ZERO:
            Ctrl_Motor_IRQ_Process(); 
            break;
        default: break;
    }
}