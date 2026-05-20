// #include "ctrl.h"
// #include "ti_msp_dl_config.h"
// #include "track.h"
// #include "motor.h"
// #include "servo.h"
// #include "pid.h"
// #include "k230.h"

// // ==========================================
// // 局部全局变量 
// // ==========================================
// static ChassisMode_e chassis_mode = CHASSIS_IDLE; // 底盘默认待机
// static uint8_t vision_enable = 0;                 // 视觉默认关闭

// // --- 视觉云台变量 ---
// PID_TypeDef pid_gimbal_x;
// PID_TypeDef pid_gimbal_y;
// static float angle_x = 90.0f;
// static float angle_y = 80.0f;

// // --- 底盘电机变量 ---
// extern volatile int32_t left_pulse_count;
// extern volatile int32_t right_pulse_count;
// PID_TypeDef pid_left;
// PID_TypeDef pid_right;

// volatile uint8_t car_running = 0;  
// float current_base_speed = 0.0f; 
// static uint8_t curve_mode = 0; 

// #define SPEED_KP 0.5f 
// #define SPEED_KI 0.1f
// #define SPEED_KD 0.0f 
// #define TRACK_KP 2.5f 
// #define TRACK_KD 12.0f 
// #define STRAIGHT_SPEED 4.0f   
// #define CURVE_SPEED    4.0f   

// static float Get_Track_Position(void) {
//     float sum = 0;
//     int active_cnt = 0;
//     for(int i = 0; i < 8; i++) {
//         if(IR_Data[i]) { sum += (i + 1.0f); active_cnt++; }
//     }
//     if(active_cnt == 0) return 0.0f; 
//     return sum / active_cnt;
// }

// void Ctrl_Init(void) {
//     PID_Init(&pid_left, SPEED_KP, SPEED_KI, SPEED_KD);
//     PID_Init(&pid_right, SPEED_KP, SPEED_KI, SPEED_KD);
//     PID_Init(&pid_gimbal_x, 0.032f, 0.0f, 0.05f); 
//     PID_Init(&pid_gimbal_y, 0.032f, 0.0f, 0.05f);
    
//     Servo_SetAngle(0, angle_x);
//     Servo_SetAngle(1, angle_y);
    
//     car_running = 0;
//     current_base_speed = 0.0f;
//     left_pulse_count = 0;
//     right_pulse_count = 0;
    
//     chassis_mode = CHASSIS_IDLE; 
//     vision_enable = 0;
// }

// // ==========================================
// // 接口函数 2：按键扫描 (解耦分离)
// // ==========================================
// void Ctrl_KeyScan(void) {
//     uint32_t porta_val = DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN | GPIO_KEY_K2_PIN);
//     uint32_t portb_val = DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN | GPIO_KEY_K4_PIN);

//     // 1. 底盘模式 (互斥)
//     if ((porta_val & GPIO_KEY_K1_PIN) == 0) chassis_mode = CHASSIS_STRAIGHT; 
//     if ((porta_val & GPIO_KEY_K2_PIN) == 0) chassis_mode = CHASSIS_TRACKING; 
//     if ((portb_val & GPIO_KEY_K4_PIN) == 0) chassis_mode = CHASSIS_IDLE; 

//     // 2. 视觉模式独立开关 (K3 状态翻转)
//     static uint8_t k3_pressed = 0;
//     if ((portb_val & GPIO_KEY_K3_PIN) == 0) {
//         if (k3_pressed == 0) { // 边缘检测，按一次只触发一次
//             vision_enable = !vision_enable; 
//             k3_pressed = 1;
//         }
//     } else {
//         k3_pressed = 0; // 松开后复位标志位
//     }
// }

// ChassisMode_e Ctrl_GetChassisMode(void) { return chassis_mode; }
// uint8_t Ctrl_GetVisionState(void) { return vision_enable; }

// // ==========================================
// // 接口函数 4：核心控制周期 
// // ==========================================
// void Ctrl_Process(void) {
//     // --------------------------------------------------
//     // 模块 A：视觉云台控制 (由独立的 vision_enable 控制)
//     // --------------------------------------------------
//     if (vision_enable) {
//         if (k230_update_flag == 1) { 
//             k230_update_flag = 0; 
//             static float smooth_x = 320.0f; 
//             static float smooth_y = 260.0f;
//             float alpha = 0.3f; 
            
//             smooth_x = (alpha * k230_target_x) + ((1.0f - alpha) * smooth_x);
//             smooth_y = (alpha * k230_target_y) + ((1.0f - alpha) * smooth_y);
            
//             float offset_x = smooth_x - 375.0f;
//             float offset_y = smooth_y - 250.0f;

//             if (offset_x > -10.0f && offset_x < 10.0f) offset_x = 0;
//             if (offset_y > -10.0f && offset_y < 10.0f) offset_y = 0;

//             angle_x += PID_Calc(&pid_gimbal_x, offset_x); 
//             angle_y -= PID_Calc(&pid_gimbal_y, offset_y); 

//             if(angle_x < 20.0f) angle_x = 20.0f; if(angle_x > 160.0f) angle_x = 160.0f;
//             if(angle_y < 20.0f) angle_y = 20.0f; if(angle_y > 160.0f) angle_y = 160.0f;

//             Servo_SetAngle(0, angle_x);
//             Servo_SetAngle(1, angle_y);
//         }
//     } else {
//         // 🚨 视觉关闭时：云台强制复位！
//         k230_update_flag = 0; 
//         angle_x = 90.0f;
//         angle_y = 80.0f;
//         Servo_SetAngle(0, angle_x);
//         Servo_SetAngle(1, angle_y);
        
//         // 🚨 终极防卡死解药：彻底清空 PID 历史包袱！
//         pid_gimbal_x.integral = 0;
//         pid_gimbal_y.integral = 0;
//         pid_gimbal_x.last_error = 0;
//         pid_gimbal_y.last_error = 0;
//     }

//     // --------------------------------------------------
//     // 模块 B：底盘电机与循迹控制
//     // --------------------------------------------------
//     if (chassis_mode == CHASSIS_IDLE) {
//         car_running = 0;
//         current_base_speed = 0.0f;
//         return; 
//     }

//     ReadEightIR();
//     int black_cnt = 0;
//     for(int i=0; i<8; i++) if(IR_Data[i]) black_cnt++;
    
//     float current_pos = Get_Track_Position();

//     if (black_cnt >= 6 && chassis_mode != CHASSIS_STRAIGHT) {
//         car_running = 0;
//         current_base_speed = 0.0f; 
//     } else {
//         if(car_running == 0) {
//             car_running = 1;
//             current_base_speed = 2.0f; 
//         }
        
//         if (chassis_mode == CHASSIS_STRAIGHT) {
//             if (current_base_speed < STRAIGHT_SPEED) current_base_speed += 0.04f;
//             if (current_base_speed > STRAIGHT_SPEED) current_base_speed = STRAIGHT_SPEED;
//             pid_left.target  = current_base_speed;
//             pid_right.target = current_base_speed;
//         } else {
//             // 全白虚线处理：强行直行
//             if(current_pos == 0.0f) {
//                 current_pos = 4.5f; 
//                 curve_mode = 0;     
//             } 
            
//             float error = current_pos - 4.5f; 
//             float abs_error = (error > 0) ? error : -error;

//             if (IR_Data[0] || IR_Data[1] || IR_Data[6] || IR_Data[7] || abs_error > 1.2f) curve_mode = 1; 
//             else if (!IR_Data[0] && !IR_Data[1] && !IR_Data[6] && !IR_Data[7] && abs_error < 0.5f) curve_mode = 0;

//             float target_speed = curve_mode ? CURVE_SPEED : STRAIGHT_SPEED;

//             if (current_base_speed < target_speed) {
//                 if (current_base_speed < 2.0f) current_base_speed = 2.0f;
//                 current_base_speed += 0.04f;  
//                 if (current_base_speed > target_speed) current_base_speed = target_speed;
//             } else if (current_base_speed > target_speed) {
//                 current_base_speed -= 0.05f;  
//                 if (current_base_speed < target_speed) current_base_speed = target_speed;
//             }

//             static float last_pos_error = 0.0f;
//             float error_d = error - last_pos_error;
//             last_pos_error = error;
//             float speed_adjust = (error * TRACK_KP) + (error_d * TRACK_KD);
            
//             float max_adjust = curve_mode ? 2.5f : 4.5f; 
//             if(speed_adjust > max_adjust) speed_adjust = max_adjust;
//             if(speed_adjust < -max_adjust) speed_adjust = -max_adjust;

//             pid_left.target  = current_base_speed + speed_adjust;
//             pid_right.target = current_base_speed - speed_adjust;
//         }
//     }
// }

// void TIMER_PID_INST_IRQHandler(void) {
//     switch (DL_TimerG_getPendingInterrupt(TIMER_PID_INST)) {
//         case DL_TIMER_IIDX_ZERO:
//         {   
//             float raw_speed_L = (float)left_pulse_count;
//             float raw_speed_R = (float)right_pulse_count;
//             left_pulse_count = 0;
//             right_pulse_count = 0;

//             static uint16_t startup_cnt = 0;
//             static float filtered_speed_L = 0;
//             static float filtered_speed_R = 0;

//             if (car_running == 0) {
//                 Motor_SetSpeed(0, 0);
//                 pid_left.integral = 0;
//                 pid_right.integral = 0;
//                 startup_cnt = 0; 
//                 filtered_speed_L = 0;
//                 filtered_speed_R = 0;
//             } else {
//                 if(startup_cnt < 100) startup_cnt++;

//                 filtered_speed_L = 0.3f * raw_speed_L + 0.7f * filtered_speed_L;
//                 filtered_speed_R = 0.3f * raw_speed_R + 0.7f * filtered_speed_R;

//                 float pid_adj_L = PID_Calc(&pid_left, filtered_speed_L);
//                 float pid_adj_R = PID_Calc(&pid_right, filtered_speed_R);

//                 float base_pwm_L = (pid_left.target > 0) ? (22.0f + pid_left.target * 2.0f) : (pid_left.target * 6.0f);
//                 float base_pwm_R = (pid_right.target > 0) ? (38.0f + pid_right.target * 2.0f) : (pid_right.target * 6.0f);
                
//                 float pwm_out_L = base_pwm_L + pid_adj_L;
//                 float pwm_out_R = base_pwm_R + pid_adj_R;

//                 if(pwm_out_L < 8.0f && pid_left.target > 0) pwm_out_L = 8.0f;
//                 if(pwm_out_R < 8.0f && pid_right.target > 0) pwm_out_R = 8.0f;

//                 Motor_SetSpeed((int8_t)pwm_out_L, (int8_t)pwm_out_R);
//             }
//             break;
//         }
//         default: break;
//     }
// }

#include "ctrl.h"
#include "ti_msp_dl_config.h"
#include "track.h"
#include "motor.h"
#include "servo.h"
#include "pid.h"
#include "k230.h"

// ==========================================
// 状态机全局变量 
// ==========================================
static CarMode_e current_mode = MODE_IDLE; 
static SettingMode_e setting_mode = SET_NONE;

static uint8_t cfg_speed_int = 40; 
static uint8_t cfg_target_laps = 1;

static uint8_t remaining_laps = 0;   
static uint8_t corner_count = 0;     
static uint8_t is_in_corner = 0;     
static uint16_t straight_timer = 100;
static uint8_t stop_pending = 0;     

volatile uint8_t car_running = 0;  
float current_base_speed = 0.0f; 
static uint8_t curve_mode = 0; 

PID_TypeDef pid_gimbal_x;
PID_TypeDef pid_gimbal_y;
static float angle_x = 90.0f;
static float angle_y = 80.0f;

extern volatile int32_t left_pulse_count;
extern volatile int32_t right_pulse_count;
PID_TypeDef pid_left;
PID_TypeDef pid_right;

#define SPEED_KP 0.5f 
#define SPEED_KI 0.1f
#define SPEED_KD 0.0f 
#define TRACK_KP 2.5f 
#define TRACK_KD 12.0f 

static float Get_Track_Position(void) {
    float sum = 0;
    int active_cnt = 0;
    for(int i = 0; i < 8; i++) {
        if(IR_Data[i]) { sum += (i + 1.0f); active_cnt++; }
    }
    if(active_cnt == 0) return 0.0f; 
    return sum / active_cnt;
}

void Ctrl_Init(void) {
    PID_Init(&pid_left, SPEED_KP, SPEED_KI, SPEED_KD);
    PID_Init(&pid_right, SPEED_KP, SPEED_KI, SPEED_KD);
    PID_Init(&pid_gimbal_x, 0.032f, 0.0f, 0.05f); 
    PID_Init(&pid_gimbal_y, 0.032f, 0.0f, 0.05f);
    
    Servo_SetAngle(0, angle_x);
    Servo_SetAngle(1, angle_y);
    
    car_running = 0;
    current_base_speed = 0.0f;
    left_pulse_count = 0;
    right_pulse_count = 0;
    
    current_mode = MODE_IDLE; 
    setting_mode = SET_NONE;
}

void Ctrl_KeyScan(void) {
    uint32_t porta_val = DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN | GPIO_KEY_K2_PIN);
    uint32_t portb_val = DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN | GPIO_KEY_K4_PIN);

    uint8_t k1_active = ((porta_val & GPIO_KEY_K1_PIN) == 0);
    uint8_t k2_active = ((porta_val & GPIO_KEY_K2_PIN) == 0);
    uint8_t k3_active = ((portb_val & GPIO_KEY_K3_PIN) == 0);
    uint8_t k4_active = ((portb_val & GPIO_KEY_K4_PIN) == 0);

    static uint8_t k1_s=0, k2_s=0, k3_s=0, k4_s=0;
    static uint16_t k1_d=0, k2_d=0, k3_d=0, k4_d=0;

    // K1: 主模式切换 (IDLE -> M0 -> M1 -> M2 -> IDLE)
    if (k1_active) { k1_d++; if (k1_d>=3 && !k1_s) { k1_s=1; 
        if (current_mode == MODE_IDLE) current_mode = MODE_M0_TRACK;
        else if (current_mode == MODE_M0_TRACK) current_mode = MODE_M1_GIMBAL;
        else if (current_mode == MODE_M1_GIMBAL) current_mode = MODE_M2_ALL; // 修正为 M2
        else current_mode = MODE_IDLE;
        
        setting_mode = SET_NONE; 
        remaining_laps = cfg_target_laps; 
        corner_count = 0; straight_timer = 100; stop_pending = 0;
    }} else { k1_d=0; k1_s=0; }

    // K2: 子菜单切换
    if (k2_active) { k2_d++; if (k2_d>=3 && !k2_s) { k2_s=1; 
        if (current_mode == MODE_M0_TRACK || current_mode == MODE_M2_ALL) {
            if (setting_mode == SET_NONE) setting_mode = SET_SPEED;
            else if (setting_mode == SET_SPEED) setting_mode = SET_LAPS;
            else { 
                setting_mode = SET_NONE; 
                remaining_laps = cfg_target_laps; 
            }
        }
    }} else { k2_d=0; k2_s=0; }

    // K3 & K4 调参逻辑
    if (k3_active) { k3_d++; if (k3_d>=3 && !k3_s) { k3_s=1; 
        if (setting_mode == SET_SPEED) { if(cfg_speed_int < 100) cfg_speed_int += 5; } 
        if (setting_mode == SET_LAPS)  { if(cfg_target_laps < 99) cfg_target_laps++; } 
    }} else { k3_d=0; k3_s=0; }

    if (k4_active) { k4_d++; if (k4_d>=3 && !k4_s) { k4_s=1; 
        if (setting_mode == SET_SPEED) { if(cfg_speed_int > 15) cfg_speed_int -= 5; }  
        if (setting_mode == SET_LAPS)  { if(cfg_target_laps > 0) cfg_target_laps--; }  
    }} else { k4_d=0; k4_s=0; }
}

CarMode_e Ctrl_GetMode(void) { return current_mode; }
SettingMode_e Ctrl_GetSettingMode(void) { return setting_mode; }
uint8_t Ctrl_GetConfigSpeed(void) { return cfg_speed_int; }
uint8_t Ctrl_GetConfigLaps(void) { return cfg_target_laps; }
uint8_t Ctrl_GetRemainLaps(void) { return remaining_laps; }

void Ctrl_Process(void) {
    // --------------------------------------------------
    // 模块 A：视觉云台控制 (M1 或 M2 下开启)
    // --------------------------------------------------
    if (current_mode == MODE_M1_GIMBAL || current_mode == MODE_M2_ALL) {
        if (k230_update_flag == 1) { 
            k230_update_flag = 0; 
            static float smooth_x = 320.0f; static float smooth_y = 260.0f;
            float alpha = 0.3f; 
            smooth_x = (alpha * k230_target_x) + ((1.0f - alpha) * smooth_x);
            smooth_y = (alpha * k230_target_y) + ((1.0f - alpha) * smooth_y);
            float offset_x = smooth_x - 375.0f;
            float offset_y = smooth_y - 250.0f;
            if (offset_x > -10.0f && offset_x < 10.0f) offset_x = 0;
            if (offset_y > -10.0f && offset_y < 10.0f) offset_y = 0;
            angle_x += PID_Calc(&pid_gimbal_x, offset_x); 
            angle_y -= PID_Calc(&pid_gimbal_y, offset_y); 
            if(angle_x < 20.0f) angle_x = 20.0f; if(angle_x > 160.0f) angle_x = 160.0f;
            if(angle_y < 20.0f) angle_y = 20.0f; if(angle_y > 160.0f) angle_y = 160.0f;
            Servo_SetAngle(0, angle_x);
            Servo_SetAngle(1, angle_y);
        }
    } else {
        k230_update_flag = 0; 
        angle_x = 90.0f; angle_y = 80.0f;
        Servo_SetAngle(0, angle_x); Servo_SetAngle(1, angle_y);
        pid_gimbal_x.integral = 0; pid_gimbal_y.integral = 0;
        pid_gimbal_x.last_error = 0; pid_gimbal_y.last_error = 0;
    }

    // --------------------------------------------------
    // 模块 B：底盘电机与循迹控制 
    // --------------------------------------------------
    if (current_mode == MODE_IDLE || current_mode == MODE_M1_GIMBAL) {
        car_running = 0;
        current_base_speed = 0.0f;
        return; 
    }

    // 🚨 调参子菜单处理：悬空调速电机运转，调圈数时停车
    if (setting_mode == SET_SPEED) {
        // 让轮子根据设定速度空转，供直观感受！
        car_running = 1;
        float test_spd = cfg_speed_int / 10.0f;
        pid_left.target = test_spd;
        pid_right.target = test_spd;
        return; // 跳过下方循迹逻辑
    } else if (setting_mode == SET_LAPS) {
        car_running = 0;
        return; // 跳过下方循迹逻辑
    }

    // --- 正常循迹逻辑开始执行 ---
    ReadEightIR();
    
    // 🚨 悬空保护判定 (8个全黑)
    int black_cnt = 0;
    for(int i=0; i<8; i++) if(IR_Data[i]) black_cnt++;
    
    if (black_cnt == 8) {
        car_running = 0;          // 把小车拿起时，强制断绝动力
        current_base_speed = 0.0f;
        is_in_corner = 0;         // 清空过弯标志防止误判
        return; 
    }

    float current_pos = Get_Track_Position();
    float error = current_pos - 4.5f; 
    float abs_error = (error > 0) ? error : -error;

    if (IR_Data[0] || IR_Data[1] || IR_Data[6] || IR_Data[7] || abs_error > 1.2f) curve_mode = 1; 
    else if (!IR_Data[0] && !IR_Data[1] && !IR_Data[6] && !IR_Data[7] && abs_error < 0.5f) curve_mode = 0;

    if (curve_mode == 1) { 
        if (is_in_corner == 0 && straight_timer > 30) { 
            is_in_corner = 1;      
            corner_count++;        
            straight_timer = 0;    
            if (corner_count >= 4) { 
                corner_count = 0;
                if (cfg_target_laps > 0) { 
                    if (remaining_laps > 0) remaining_laps--; 
                    if (remaining_laps == 0) stop_pending = 1; 
                }
            }
        }
    } else { 
        is_in_corner = 0; 
        if (straight_timer < 1000) straight_timer++; 
        if (stop_pending && straight_timer > 20) {
            current_mode = MODE_IDLE; 
            stop_pending = 0;         
            return; 
        }
    }

    float config_straight_speed = cfg_speed_int / 10.0f; 
    float config_curve_speed = config_straight_speed * 0.8f; 

    if(car_running == 0) {
        car_running = 1;
        current_base_speed = 2.0f; 
    }
    
    if(current_pos == 0.0f) { current_pos = 4.5f; curve_mode = 0; } 
    
    float target_speed = curve_mode ? config_curve_speed : config_straight_speed;

    if (current_base_speed < target_speed) {
        if (current_base_speed < 2.0f) current_base_speed = 2.0f;
        current_base_speed += 0.04f;  
        if (current_base_speed > target_speed) current_base_speed = target_speed;
    } else if (current_base_speed > target_speed) {
        current_base_speed -= 0.05f;  
        if (current_base_speed < target_speed) current_base_speed = target_speed;
    }

    static float last_pos_error = 0.0f;
    float error_d = error - last_pos_error;
    last_pos_error = error;
    float speed_adjust = (error * TRACK_KP) + (error_d * TRACK_KD);
    
    float max_adjust = curve_mode ? 2.5f : 4.5f; 
    if(speed_adjust > max_adjust) speed_adjust = max_adjust;
    if(speed_adjust < -max_adjust) speed_adjust = -max_adjust;

    pid_left.target  = current_base_speed + speed_adjust;
    pid_right.target = current_base_speed - speed_adjust;
}

// ========================================================
// 中断底层维持不变
// ========================================================
void TIMER_PID_INST_IRQHandler(void) {
    switch (DL_TimerG_getPendingInterrupt(TIMER_PID_INST)) {
        case DL_TIMER_IIDX_ZERO:
        {   
            float raw_speed_L = (float)left_pulse_count;
            float raw_speed_R = (float)right_pulse_count;
            left_pulse_count = 0; right_pulse_count = 0;

            static uint16_t startup_cnt = 0;
            static float filtered_speed_L = 0;
            static float filtered_speed_R = 0;

            if (car_running == 0) {
                Motor_SetSpeed(0, 0);
                pid_left.integral = 0; pid_right.integral = 0;
                startup_cnt = 0; filtered_speed_L = 0; filtered_speed_R = 0;
            } else {
                if(startup_cnt < 100) startup_cnt++;
                filtered_speed_L = 0.3f * raw_speed_L + 0.7f * filtered_speed_L;
                filtered_speed_R = 0.3f * raw_speed_R + 0.7f * filtered_speed_R;

                float pid_adj_L = PID_Calc(&pid_left, filtered_speed_L);
                float pid_adj_R = PID_Calc(&pid_right, filtered_speed_R);

                float base_pwm_L = (pid_left.target > 0) ? (22.0f + pid_left.target * 2.0f) : (pid_left.target * 6.0f);
                float base_pwm_R = (pid_right.target > 0) ? (38.0f + pid_right.target * 2.0f) : (pid_right.target * 6.0f);
                
                float pwm_out_L = base_pwm_L + pid_adj_L;
                float pwm_out_R = base_pwm_R + pid_adj_R;

                if(pwm_out_L < 8.0f && pid_left.target > 0) pwm_out_L = 8.0f;
                if(pwm_out_R < 8.0f && pid_right.target > 0) pwm_out_R = 8.0f;

                Motor_SetSpeed((int8_t)pwm_out_L, (int8_t)pwm_out_R);
            }
            break;
        }
        default: break;
    }
}