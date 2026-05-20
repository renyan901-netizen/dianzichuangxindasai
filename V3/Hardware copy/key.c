#include "key.h"

/**
 * @brief 按键扫描函数
 * @param mode: 0 不支持连续按，1 支持连续按
 * @return 返回按下的键值，0表示无按键按下
 */
uint8_t Key_Scan(uint8_t mode) {
    static uint8_t key_up = 1; // 按键松开标志位
    
    if (mode) {
        key_up = 1; // 支持连按时，每次都清除松开标志
    }
    
    // 判断是否有任意按键被按下 (低电平有效)
    // K1/K2 在 GPIOA，K3/K4 在 GPIOB
    if (key_up && (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 0 || 
                   DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 0 || 
                   DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 0 || 
                   DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 0)) 
    {
        delay_cycles(320000); // 延时约 10ms 进行软件消抖 (主频 32MHz)
        key_up = 0;           // 标记按键已被按下
        
        // 逐个判断是哪个按键按下了
        if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 0) {
            return KEY1_PRES;
        } 
        else if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 0) {
            return KEY2_PRES;
        } 
        else if (DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 0) {
            return KEY3_PRES;
        } 
        else if (DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 0) {
            return KEY4_PRES;
        }
    } 
    // 如果所有按键都松开了
    else if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 1 && 
             DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 1 && 
             DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 1 && 
             DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 1) 
    {
        key_up = 1; // 重置松开标志
    }
    
    return 0; // 无按键按下
}