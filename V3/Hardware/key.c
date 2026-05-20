// #include "key.h"

// /**
//  * @brief 按键扫描函数
//  * @param mode: 0 不支持连续按，1 支持连续按
//  * @return 返回按下的键值，0表示无按键按下
//  */
// uint8_t Key_Scan(uint8_t mode) {
//     static uint8_t key_up = 1; // 按键松开标志位
    
//     if (mode) {
//         key_up = 1; // 支持连按时，每次都清除松开标志
//     }
    
//     // 判断是否有任意按键被按下 (低电平有效)
//     // K1/K2 在 GPIOA，K3/K4 在 GPIOB
//     if (key_up && (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 0 || 
//                    DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 0 || 
//                    DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 0 || 
//                    DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 0)) 
//     {
//         delay_cycles(320000); // 延时约 10ms 进行软件消抖 (主频 32MHz)
//         key_up = 0;           // 标记按键已被按下
        
//         // 逐个判断是哪个按键按下了
//         if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 0) {
//             return KEY1_PRES;
//         } 
//         else if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 0) {
//             return KEY2_PRES;
//         } 
//         else if (DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 0) {
//             return KEY3_PRES;
//         } 
//         else if (DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 0) {
//             return KEY4_PRES;
//         }
//     } 
//     // 如果所有按键都松开了
//     else if (DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) == 1 && 
//              DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) == 1 && 
//              DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) == 1 && 
//              DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) == 1) 
//     {
//         key_up = 1; // 重置松开标志
//     }
    
//     return 0; // 无按键按下
// }
#include "key.h"

/**
 * @brief 【电磁免疫版】按键扫描函数 (专治电机干扰死锁)
 * @param mode: 0 不支持连续按，1 支持连续按
 * @return 返回按下的键值，0表示无按键按下
 */
uint8_t Key_Scan(uint8_t mode) {
    // 🚨 核心大招：使用“强行冷却期”代替脆弱的“松开检测”
    static uint16_t cooldown_timer = 0; 

    // 1. 如果还在冷却期内，无视一切电机干扰和物理抖动，直接返回 0
    if (cooldown_timer > 0) {
        cooldown_timer--;
        return 0;
    }

    // 2. 初次检测是否有按键被拉低 (受干扰的几率极小，因为我们要检测实打实的 0)
    uint8_t k1 = DL_GPIO_readPins(GPIOA, GPIO_KEY_K1_PIN) ? 1 : 0;
    uint8_t k2 = DL_GPIO_readPins(GPIOA, GPIO_KEY_K2_PIN) ? 1 : 0;
    uint8_t k3 = DL_GPIO_readPins(GPIOB, GPIO_KEY_K3_PIN) ? 1 : 0;
    uint8_t k4 = DL_GPIO_readPins(GPIOB, GPIO_KEY_K4_PIN) ? 1 : 0;

    if (k1 == 0 || k2 == 0 || k3 == 0 || k4 == 0) {
        // 触发无敌帧：因为 main.c 的 while(1) 循环大概是 5ms 进来一次
        // 我们设置 60 * 5ms = 300ms 的绝对冷却期
        cooldown_timer = 60; 

        if (mode) {
            cooldown_timer = 10; // 如果支持连按，冷却期缩短到 50ms
        }

        // 返回对应的键值
        if (k1 == 0) return KEY1_PRES;
        if (k2 == 0) return KEY2_PRES;
        if (k3 == 0) return KEY3_PRES;
        if (k4 == 0) return KEY4_PRES;
    }
    
    return 0; // 无按键按下
}