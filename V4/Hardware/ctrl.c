#include "ctrl.h"
#include "ti_msp_dl_config.h"
#include "pid.h"
#include "track.h"
#include "motor.h"
#include "gimbal.h"
#include "k230.h"
#include "key.h" // 确保引入 KEY 定义

// ==========================================
// 🚗 底盘私有参数与变量 (原版稳定参数)
// ==========================================
#define SPEED_KP 1.0f  
#define SPEED_KI 0.2f  
#define SPEED_KD 0.0f  

#define TRACK_KP 2.8f  
#define TRACK_KD 20.0f 

// 🚗 底盘物理对称死区补偿校准参数 (二分法精确定位平衡点)
// 上一轮实测：差值40→轻微右偏，差值53→较大左偏，本轮取中间值≈44
#define DEADBAND_L_FWD   14.0f   // 左轮前进死区 (在 16 与 10 之间取偏高值)
#define DEADBAND_L_BWD  -54.0f   // 左轮后退死区 (不变)
#define DEADBAND_R_FWD   58.0f   // 右轮前进死区 (在 56 与 62 之间取偏低值)
#define DEADBAND_R_BWD  -18.0f   // 右轮后退死区 (不变)

// 🚀 非对称起步 Kick 值 (前进方向专用，二分法精调)
#define KICK_L_FWD   1.0f       // 左轮前进 kick (在 1.2 与 0.8 之间)
#define KICK_R_FWD   1.3f       // 右轮前进 kick (在 1.2 与 1.6 之间，偏保守)
#define KICK_BWD    -1.2f       // 后退方向 kick (不变)

#define CORNERS_PER_LAP 4  
#define WHITE_TIMEOUT 60 

PID_TypeDef pid_left;
PID_TypeDef pid_right;

extern volatile int32_t left_pulse_count;
extern volatile int32_t right_pulse_count;

volatile uint8_t car_running = 0;  
float STRAIGHT_SPEED = 3.0f;   
float CURVE_SPEED    = 1.5f;   

// 暴露给全局的 UI 变量
uint8_t  target_laps = 1;      
uint8_t  speed_mode  = 0;      
uint8_t  start_flag  = 0;      
uint16_t corner_count   = 0;   
volatile uint8_t gesture_control_active = 0; // 新增：上电默认锁定为0
volatile float g_act_speed_L = 0.0f;
volatile float g_act_speed_R = 0.0f;

// 🚨 综合模式变量
uint8_t  sys_mode     = 0;  // 0=常规, 1=综合
uint8_t  track_enable = 1;  // 1=循迹开, 0=循迹关(仅打靶)
 
// 底盘状态变量
static uint16_t debounce_timer = 0;   
static uint32_t t_first = 0;      
static uint32_t t_full = 0;       
static uint32_t homing_ticks = 0; 
static uint8_t  track_state = 0;  
static float current_base_speed = 0.0f; 
static float last_valid_pos = 4.5f; 
static float last_pos_error = 0.0f; 
static uint32_t white_area_cnt = 0;
static uint8_t curve_mode = 0; 
 
// ==========================================
// 🎯 云台私有参数与变量
// ==========================================
PID_TypeDef pid_test_x;
PID_TypeDef pid_test_y;
static uint16_t target_lost_timeout = 0;
 
// ==========================================
// 🛠️ 内部私有函数
// ==========================================
static float Get_Track_Position(void) {
    float sum = 0;
    int active_cnt = 0;
    for(int i = 0; i < 8; i++) {
        if(IR_Data[i]) { 
            sum += (i + 1.0f);
            active_cnt++;
        }
    }
    if(active_cnt == 0) return 0.0f; 
    return sum / active_cnt;
}
 
// ==========================================
// 🚀 核心控制接口实现
// ==========================================
 
void Ctrl_Init(void) {
    PID_Init(&pid_left, SPEED_KP, SPEED_KI, SPEED_KD);
    PID_Init(&pid_right, SPEED_KP, SPEED_KI, SPEED_KD);
    
    PID_Init(&pid_test_x, 9.0f, 0.0f, 1.5f);
    PID_Init(&pid_test_y, 9.0f, 0.0f, 1.5f);

    gesture_control_active = 0; // 上电默认锁定
    g_car_state = CAR_STATE_STOP; // 保证上电电机停止
}
 
void Ctrl_Key_Process(uint8_t key_val) {
    if(key_val == 0) return;
 
    // 按键1用于解锁/激活手势接收
    if(key_val == KEY1_PRES) {
        gesture_control_active = 1;
        g_car_state = CAR_STATE_STOP; // 激活后初始为停止状态，等待手势指令
    }
    
    // 激活后，其余按键可作为辅助调试
    if(gesture_control_active) {
        if(key_val == KEY2_PRES) {
            g_car_state = CAR_STATE_FORWARD;
        }
        else if(key_val == KEY3_PRES) {
            g_car_state = CAR_STATE_BACKWARD;
        }
        else if(key_val == KEY4_PRES) {
            g_car_state = CAR_STATE_STOP;
        }
    }
}
 
void Ctrl_Chassis_Process(void) {
    // 若未通过按键1激活手势控制，底盘强制保持停止
    if (!gesture_control_active) {
        car_running = 0;
        pid_left.target = 0.0f;
        pid_right.target = 0.0f;
        g_car_state = CAR_STATE_STOP;
        return;
    }

    // 根据全局 g_car_state 手势状态机设置期望的目标速度
    float dest_L = 0.0f;
    float dest_R = 0.0f;

    switch (g_car_state) {
        case CAR_STATE_FORWARD:
            car_running = 1;
            dest_L = 2.0f;
            dest_R = 2.0f;
            break;
            
        case CAR_STATE_BACKWARD:
            car_running = 1;
            dest_L = -2.0f;
            dest_R = -2.0f;
            break;
            
        case CAR_STATE_STOP:
        default:
            // 优雅的软制动：先不立即把 car_running 设为 0，让目标值平缓滑行减速到 0.0f 之后再断电
            dest_L = 0.0f;
            dest_R = 0.0f;
            break;
    }

    // 🚀 渐进式速度斜坡滤波器 (Soft Start / Soft Stop + Startup Kick)
    // 每次底盘轮询步进 0.04f (约 250ms 从静止加速至 2.0 目标)
    float ramp_step = 0.04f;

    // 左轮渐进滤波与非对称起步打底
    if (pid_left.target < dest_L) {
        // 前进起步瞬间：用较小的 KICK_L_FWD 抑制左轮先行
        if (pid_left.target == 0.0f) {
            pid_left.target = KICK_L_FWD;
        } else {
            pid_left.target += ramp_step;
        }
        if (pid_left.target > dest_L) pid_left.target = dest_L;
    } else if (pid_left.target > dest_L) {
        // 后退起步瞬间
        if (pid_left.target == 0.0f && dest_L < 0.0f) {
            pid_left.target = KICK_BWD;
        } else {
            pid_left.target -= ramp_step;
        }
        if (pid_left.target < dest_L) pid_left.target = dest_L;
    }

    // 右轮渐进滤波与非对称起步打底
    if (pid_right.target < dest_R) {
        // 前进起步瞬间：用较大的 KICK_R_FWD 强制右轮同步突破高摩擦死区
        if (pid_right.target == 0.0f) {
            pid_right.target = KICK_R_FWD;
        } else {
            pid_right.target += ramp_step;
        }
        if (pid_right.target > dest_R) pid_right.target = dest_R;
    } else if (pid_right.target > dest_R) {
        // 后退起步瞬间
        if (pid_right.target == 0.0f && dest_R < 0.0f) {
            pid_right.target = KICK_BWD;
        } else {
            pid_right.target -= ramp_step;
        }
        if (pid_right.target < dest_R) pid_right.target = dest_R;
    }

    // 软制动完成：当目标设定全部降为零时，真正断开速度控制环，切入硬刹车防溜车
    if (g_car_state == CAR_STATE_STOP && pid_left.target == 0.0f && pid_right.target == 0.0f) {
        car_running = 0;
    }
}

void Ctrl_Gimbal_Process(void) {
    // 视觉追踪云台彻底关闭，锁定不偏转
    Gimbal_SetSpeed_X(0);
    Gimbal_SetSpeed_Y(0);
    pid_test_x.integral = 0; 
    pid_test_y.integral = 0;
}

void Ctrl_Motor_IRQ_Process(void) {
    float raw_speed_L = (float)left_pulse_count;
    float raw_speed_R = (float)right_pulse_count;
    left_pulse_count = 0;
    right_pulse_count = 0;

    static float filtered_speed_L = 0;
    static float filtered_speed_R = 0;

    if (car_running == 0) {
        Motor_SetSpeed(0, 0);
        pid_left.integral = 0;
        pid_right.integral = 0;
        filtered_speed_L = 0;
        filtered_speed_R = 0;
        g_act_speed_L = 0.0f;
        g_act_speed_R = 0.0f;
    } else {
        filtered_speed_L = 0.3f * raw_speed_L + 0.7f * filtered_speed_L;
        filtered_speed_R = 0.3f * raw_speed_R + 0.7f * filtered_speed_R;

        // 写入实时遥测全局变量以供 OLED UI 刷新对照
        g_act_speed_L = filtered_speed_L;
        g_act_speed_R = filtered_speed_R;

        float pid_adj_L = PID_Calc(&pid_left, filtered_speed_L);
        float pid_adj_R = PID_Calc(&pid_right, filtered_speed_R);

        float base_pwm_L = 0;
        float base_pwm_R = 0;

        // 🚨 核心物理对称死区补偿设计 (由于左右轮电机镜像反装且工作模式不同，使用精细微调的校准死区参数)
        if (pid_left.target > 0) {
            base_pwm_L = DEADBAND_L_FWD + pid_left.target * 2.0f; 
        } else if (pid_left.target < 0) {
            base_pwm_L = DEADBAND_L_BWD + pid_left.target * 2.0f; 
        } else {
            base_pwm_L = 0;
        }

        if (pid_right.target > 0) {
            base_pwm_R = DEADBAND_R_FWD + pid_right.target * 2.0f; 
        } else if (pid_right.target < 0) {
            base_pwm_R = DEADBAND_R_BWD + pid_right.target * 2.0f; 
        } else {
            base_pwm_R = 0;
        }

        float pwm_out_L = base_pwm_L + pid_adj_L;
        float pwm_out_R = base_pwm_R + pid_adj_R;

        // 🚨 终极安全保护限幅：防止 PID 积分在电机堵转/死区期间无限堆积，
        // 导致浮点数溢出，在强转为 int8_t 时发生严重的数据翻转/回绕，锁死电机。
        if (pwm_out_L > 100.0f) pwm_out_L = 100.0f;
        if (pwm_out_L < -100.0f) pwm_out_L = -100.0f;
        if (pwm_out_R > 100.0f) pwm_out_R = 100.0f;
        if (pwm_out_R < -100.0f) pwm_out_R = -100.0f;

        // 双向运行安全打底输出限幅，确保立即突破摩擦死区
        if (pwm_out_L < 8.0f && pid_left.target > 0) pwm_out_L = 8.0f;
        if (pwm_out_L > -8.0f && pid_left.target < 0) pwm_out_L = -8.0f;

        if (pwm_out_R < 8.0f && pid_right.target > 0) pwm_out_R = 8.0f;
        if (pwm_out_R > -8.0f && pid_right.target < 0) pwm_out_R = -8.0f;

        Motor_SetSpeed((int8_t)pwm_out_L, (int8_t)pwm_out_R);
    }
}