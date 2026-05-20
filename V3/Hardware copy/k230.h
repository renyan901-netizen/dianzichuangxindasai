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

// 宣告给外部文件使用的全局变量
extern volatile int k230_target_x;
extern volatile int k230_target_y;
extern volatile int k230_target_area;
extern volatile uint8_t k230_update_flag;
extern volatile int raw_byte_count; // 物理层字节探针

#endif