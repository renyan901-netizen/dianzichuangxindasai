// #ifndef __K230_H
// #define __K230_H

// #include <stdint.h>

// // 宣告给外部文件使用的全局变量
// extern volatile int k230_target_x;
// extern volatile int k230_target_y;
// extern volatile int k230_target_area;
// extern volatile uint8_t k230_update_flag;
// extern volatile int raw_byte_count;

// #endif



#ifndef __K230_H
#define __K230_H

#include <stdint.h>

// 手势运动状态枚举
typedef enum {
    CAR_STATE_STOP = 0,
    CAR_STATE_FORWARD,
    CAR_STATE_BACKWARD
} CarState_t;

// 宣告给外部文件使用的全局变量
extern volatile int k230_target_x;
extern volatile int k230_target_y;
extern volatile int k230_target_area;
extern volatile uint8_t k230_update_flag;
extern volatile int raw_byte_count; // 物理层字节探针

extern volatile CarState_t g_car_state; // 全局小车运动状态

#endif