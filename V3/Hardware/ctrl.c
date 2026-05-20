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
}

void Ctrl_Key_Process(uint8_t key_val) {
    if(key_val == 0) return;

    // 💡 极其贴心的按键调试功能：
    // K1: 强制前进 (模拟 forward)
    // K2: 强制后退 (模拟 backward)
    // K3: 强制停止 (模拟 stop)
    if(key_val == KEY1_PRES) {
        g_car_state = CAR_STATE_FORWARD;
    }
    else if(key_val == KEY2_PRES) {
        g_car_state = CAR_STATE_BACKWARD;
    }
    else if(key_val == KEY3_PRES) {
        g_car_state = CAR_STATE_STOP;
    }
}

void Ctrl_Chassis_Process(void) {
    // 根据全局 g_car_state 手势状态机设置底盘目标速度
    switch (g_car_state) {
        case CAR_STATE_FORWARD:
            car_running = 1;
            pid_left.target = 2.0f;
            pid_right.target = 2.0f;
            break;
            
        case CAR_STATE_BACKWARD:
            car_running = 1;
            pid_left.target = -2.0f;
            pid_right.target = -2.0f;
            break;
            
        case CAR_STATE_STOP:
        default:
            car_running = 0;
            pid_left.target = 0.0f;
            pid_right.target = 0.0f;
            break;
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
    } else {
        filtered_speed_L = 0.3f * raw_speed_L + 0.7f * filtered_speed_L;
        filtered_speed_R = 0.3f * raw_speed_R + 0.7f * filtered_speed_R;

        float pid_adj_L = PID_Calc(&pid_left, filtered_speed_L);
        float pid_adj_R = PID_Calc(&pid_right, filtered_speed_R);

        float base_pwm_L = 0;
        float base_pwm_R = 0;

        // 对称静摩擦与死区电压补偿设计
        // 左轮正向补偿 +24，反向补偿 -24；右轮正向补偿 +48，反向补偿 -48
        if (pid_left.target > 0) {
            base_pwm_L = 24.0f + pid_left.target * 2.0f; 
        } else if (pid_left.target < 0) {
            base_pwm_L = -24.0f + pid_left.target * 2.0f; 
        } else {
            base_pwm_L = 0;
        }

        if (pid_right.target > 0) {
            base_pwm_R = 48.0f + pid_right.target * 2.0f; 
        } else if (pid_right.target < 0) {
            base_pwm_R = -48.0f + pid_right.target * 2.0f; 
        } else {
            base_pwm_R = 0;
        }

        float pwm_out_L = base_pwm_L + pid_adj_L;
        float pwm_out_R = base_pwm_R + pid_adj_R;

        // 双向运行安全打底输出限幅，确保立即突破摩擦死区
        if (pwm_out_L < 8.0f && pid_left.target > 0) pwm_out_L = 8.0f;
        if (pwm_out_L > -8.0f && pid_left.target < 0) pwm_out_L = -8.0f;

        if (pwm_out_R < 8.0f && pid_right.target > 0) pwm_out_R = 8.0f;
        if (pwm_out_R > -8.0f && pid_right.target < 0) pwm_out_R = -8.0f;

        Motor_SetSpeed((int8_t)pwm_out_L, (int8_t)pwm_out_R);
    }
}