// #include "k230.h"
// #include "ti_msp_dl_config.h"

// // 绑定 SysConfig 里的真名
// #define MY_UART_INST UART_K230_INST 

// // 变量的真正实体定义
// volatile int k230_target_x = 320; 
// volatile int k230_target_y = 240; 
// volatile int k230_target_area = 0;  
// volatile uint8_t k230_update_flag = 0;
// volatile int raw_byte_count = 0; 

// // 因为你绑定的底层硬件是 UART0，所以中断函数名是 UART0_IRQHandler
// void UART0_IRQHandler(void) {
//     static uint8_t rx_state = 0;
//     static uint8_t cx_h = 0, cx_l = 0;
//     static uint8_t cy_h = 0, cy_l = 0;
//     static uint8_t area_h = 0, area_l = 0;

//     uint32_t status = DL_UART_getPendingInterrupt(MY_UART_INST);

//     switch (status) {
//         case DL_UART_IIDX_RX:
//         case DL_UART_IIDX_RX_TIMEOUT_ERROR: { 
//             uint8_t ch = DL_UART_Main_receiveData(MY_UART_INST);
//             raw_byte_count++; // 物理层探针：只要有电平跳变就加 1

//             // 严格解析新协议: [0xAA, 0x06, X_H, X_L, Y_H, Y_L, A_H, A_L, 0x55]
//             switch (rx_state) {
//                 case 0: if (ch == 0xAA) rx_state = 1; else rx_state = 0; break;
//                 case 1: if (ch == 0x06) rx_state = 2; else rx_state = 0; break;
//                 case 2: cx_h = ch; rx_state = 3; break;
//                 case 3: cx_l = ch; rx_state = 4; break;
//                 case 4: cy_h = ch; rx_state = 5; break;
//                 case 5: cy_l = ch; rx_state = 6; break;
//                 case 6: area_h = ch; rx_state = 7; break;
//                 case 7: area_l = ch; rx_state = 8; break;
//                 case 8:
//                     if (ch == 0x55) { 
//                         // 校验帧尾成功，合并高低八位计算真实坐标和面积
//                         k230_target_x = (cx_h << 8) | cx_l;
//                         k230_target_y = (cy_h << 8) | cy_l;
//                         k230_target_area = (area_h << 8) | area_l;
                        
//                         k230_update_flag = 1; // 触发刷新标志
//                     }
//                     rx_state = 0; // 重置状态机
//                     break;
//                 default: 
//                     rx_state = 0; 
//                     break;
//             }
//             break;
//         }

//         // 硬件级错误保护：发生溢出或错位时，重置状态机，防止死锁
//         case DL_UART_IIDX_OVERRUN_ERROR:
//         case DL_UART_IIDX_BREAK_ERROR:
//         case DL_UART_IIDX_PARITY_ERROR:
//         case DL_UART_IIDX_FRAMING_ERROR:
//             rx_state = 0; 
//             break;

//         default:
//             break;
//     }

//     // 强制清除所有挂起的中断标志位（极其重要）
//     DL_UART_clearInterruptStatus(MY_UART_INST, 0xFFFFFFFF);
// }



#include "k230.h"
#include "ti_msp_dl_config.h"
#include <string.h>

// 绑定 SysConfig 里的真名
#define MY_UART_INST UART_K230_INST 

// 变量的真正实体定义 (分配内存)
volatile int k230_target_x = 320; 
volatile int k230_target_y = 240; 
volatile int k230_laser_x = 320; 
volatile int k230_laser_y = 240; 

volatile int k230_target_area = 0;  
volatile uint8_t k230_update_flag = 0;
volatile int raw_byte_count = 0; 

// 全局小车运动状态，上电保持不动 (CAR_STATE_STOP)
volatile CarState_t g_car_state = CAR_STATE_STOP;

// 串口接收滑动窗口缓存，用于匹配 ASCII 指令字符串
#define CMD_BUF_LEN 16
static char cmd_buf[CMD_BUF_LEN] = {0};

// UART0 接收中断函数
void UART0_IRQHandler(void) {
    uint32_t status = DL_UART_getPendingInterrupt(MY_UART_INST);

    switch (status) {
        case DL_UART_IIDX_RX:
        case DL_UART_IIDX_RX_TIMEOUT_ERROR: { 
            while (!DL_UART_isRXFIFOEmpty(MY_UART_INST)) {
                uint8_t ch = DL_UART_Main_receiveData(MY_UART_INST);
                raw_byte_count++; // 物理层字节计数

                // 滑动窗口：向左移位，腾出最右侧位置写入新字符
                for (int i = 0; i < CMD_BUF_LEN - 2; i++) {
                    cmd_buf[i] = cmd_buf[i + 1];
                }
                cmd_buf[CMD_BUF_LEN - 2] = (char)ch;
                cmd_buf[CMD_BUF_LEN - 1] = '\0'; // 确保字符串合法

                // 识别手势指令字符串
                if (strstr(cmd_buf, "forward") != NULL) {
                    g_car_state = CAR_STATE_FORWARD;
                    k230_update_flag = 1; // 触发 OLED/系统状态更新
                    memset(cmd_buf, 0, sizeof(cmd_buf)); // 清空以防二次触发
                } 
                else if (strstr(cmd_buf, "backward") != NULL) {
                    g_car_state = CAR_STATE_BACKWARD;
                    k230_update_flag = 1;
                    memset(cmd_buf, 0, sizeof(cmd_buf));
                } 
                else if (strstr(cmd_buf, "stop") != NULL) {
                    g_car_state = CAR_STATE_STOP;
                    k230_update_flag = 1;
                    memset(cmd_buf, 0, sizeof(cmd_buf));
                }
            }
            break;
        }

        // 硬件级错误保护：重置接收缓存，防止死锁
        case DL_UART_IIDX_OVERRUN_ERROR:
        case DL_UART_IIDX_BREAK_ERROR:
        case DL_UART_IIDX_PARITY_ERROR:
        case DL_UART_IIDX_FRAMING_ERROR:
            memset(cmd_buf, 0, sizeof(cmd_buf));
            break;

        default:
            break;
    }

    // 强制清除所有挂起的中断标志位
    DL_UART_clearInterruptStatus(MY_UART_INST, 0xFFFFFFFF);
}