#include "pid.h"

// 初始化 PID 参数
void PID_Init(PID_TypeDef *pid, float p, float i, float d) {
    pid->Kp = p;
    pid->Ki = i;
    pid->Kd = d;
    pid->target = 0;
    pid->error = 0;
    pid->last_error = 0;
    pid->integral = 0;
    pid->output = 0;
}


// 位置式 PID 计算
float PID_Calc(PID_TypeDef *pid, float current_error) {
    pid->error = pid->target - current_error;
    
    // 🚨 终极解药：解除 300 的封印，释放右电机的洪荒之力！
    pid->integral += pid->error;
    if(pid->integral > 800) pid->integral = 800;   // <--- 改成 2000
    if(pid->integral < -800) pid->integral = -800; // <--- 改成 -2000
    
    // PID 核心公式：P * e + I * ∫e + D * (e - e_last)
    pid->output = (pid->Kp * pid->error) + 
                  (pid->Ki * pid->integral) + 
                  (pid->Kd * (pid->error - pid->last_error));
                  
    // 更新上一次误差
    pid->last_error = pid->error;
    
    return pid->output;
}