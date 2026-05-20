#ifndef __SERVO_H
#define __SERVO_H

#include "ti_msp_dl_config.h"

void Servo_Init(void);
void Servo_SetAngle(uint8_t channel, float angle);

#endif