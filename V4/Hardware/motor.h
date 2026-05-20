#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "ti_msp_dl_config.h"

void Motor_Init(void);
void Motor_SetSpeed(int8_t left_speed, int8_t right_speed);

#endif