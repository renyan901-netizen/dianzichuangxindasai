#ifndef HARDWARE_KEY_H_
#define HARDWARE_KEY_H_

#include "ti_msp_dl_config.h"

// 宏定义按键状态
#define KEY_PRESSED  1
#define KEY_RELEASED 0

// 宏定义按键键值
#define KEY1_PRES    1
#define KEY2_PRES    2
#define KEY3_PRES    3
#define KEY4_PRES    4

// 函数声明
uint8_t Key_Scan(uint8_t mode);

#endif /* HARDWARE_KEY_H_ */