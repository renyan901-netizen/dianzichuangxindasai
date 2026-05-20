#include "track.h"

static float last_error = 0; 
volatile uint8_t IR_Data[8] = {0}; 

void Track_Init(void) {} 
void Track_Scan(void) {}

void ReadEightIR(void) {
    for (int channel = 0; channel < 8; channel++) {
        uint8_t c = (channel >> 2) & 0x01;
        uint8_t b = (channel >> 1) & 0x01;
        uint8_t a = channel & 0x01;

        if(a) DL_GPIO_setPins(GPIO_AD_AD0_PORT, GPIO_AD_AD0_PIN);
        else  DL_GPIO_clearPins(GPIO_AD_AD0_PORT, GPIO_AD_AD0_PIN);

        if(b) DL_GPIO_setPins(GPIO_AD_AD1_PORT, GPIO_AD_AD1_PIN);
        else  DL_GPIO_clearPins(GPIO_AD_AD1_PORT, GPIO_AD_AD1_PIN);

        if(c) DL_GPIO_setPins(GPIO_AD_AD2_PORT, GPIO_AD_AD2_PIN);
        else  DL_GPIO_clearPins(GPIO_AD_AD2_PORT, GPIO_AD_AD2_PIN);

        // 第一次读取与防抖
        delay_cycles(32000); 
        uint8_t read1 = (DL_GPIO_readPins(GPIO_AD_OUT_PORT, GPIO_AD_OUT_PIN) & GPIO_AD_OUT_PIN) ? 1 : 0;
        
        delay_cycles(16000); 
        uint8_t read2 = (DL_GPIO_readPins(GPIO_AD_OUT_PORT, GPIO_AD_OUT_PIN) & GPIO_AD_OUT_PIN) ? 1 : 0;

        if (read1 == read2) {
            IR_Data[channel] = read1;
        }
    }
}

float Track_GetError(void) {
    ReadEightIR(); 

    int active_sensors = 0;
    int transition_count = 0;
    float current_error = 0;
    int weights[8] = {-40, -30, -20, -10, 10, 20, 30, 40};

    // 1. 统计激活数量，并计算 "0/1" 的连通跳变次数
    for(int i = 0; i < 8; i++) {
        if(IR_Data[i] == 1) { 
            active_sensors++;
            current_error += weights[i];
        }
        // 统计相邻探头状态不一致的次数 (如 0变1, 或 1变0)
        if(i < 7 && IR_Data[i] != IR_Data[i+1]) {
            transition_count++;
        }
    }

    // ========================================================
    // 🛡️ 防御塔 1：连通性与面积屏蔽 (专杀地砖缝与散落反光)
    // 正常的黑线必定连在一起，最多只有 2 次跳变。
    // 如果跳变次数 > 2，说明有散落噪点！直接无视，保持姿态。
    // 如果激活探头 > 3，说明碰到了十字交叉或宽大横缝！
    // ========================================================
    if (transition_count > 2 || active_sensors > 3) {
        return last_error; 
    }

    if (active_sensors > 0) {
        current_error = current_error / active_sensors;
        
        // ========================================================
        // 🛡️ 防御塔 2：空间防瞬移滤波 (专杀边缘单点干扰)
        // 正常行驶时，误差不可能瞬间改变超过 25。
        // ========================================================
        float delta = current_error - last_error;
        if (delta < 0) delta = -delta; // 取绝对值
        
        if (delta > 25.0f) {
            // 瞬间出现巨大的空间位移，大概率是边缘探头扫到了脏东西
            // 判定为无效跳变，直接丢弃
            return last_error; 
        }
        
        last_error = current_error; 
    } else {
        // 完全丢线时，保持最后的正确记忆方向，让车头偏转找回黑线
        current_error = last_error; 
    }
    
    return current_error;
}

// 占位
uint8_t Track_IsFullBlack(void) { return 0; }
uint8_t Track_IsLineDetected(void) { return 0; }