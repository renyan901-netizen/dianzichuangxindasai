#include "oled_hardware_i2c.h"
#include "oledfont.h"

// 替换掉原有的 mspm0_delay_ms，免得你报错找不到
void delay_ms(uint32_t ms)
{
    delay_cycles(ms * 32000); // 假设主频 32MHz
}

static int mspm0_i2c_disable(void)
{
    DL_I2C_reset(I2C_OLED_INST);
    DL_GPIO_initDigitalOutput(GPIO_I2C_OLED_IOMUX_SCL);
    DL_GPIO_initDigitalInputFeatures(GPIO_I2C_OLED_IOMUX_SDA,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_clearPins(GPIO_I2C_OLED_SCL_PORT, GPIO_I2C_OLED_SCL_PIN);
    DL_GPIO_enableOutput(GPIO_I2C_OLED_SCL_PORT, GPIO_I2C_OLED_SCL_PIN);
    return 0;
}

static int mspm0_i2c_enable(void)
{
    DL_I2C_reset(I2C_OLED_INST);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_OLED_IOMUX_SDA,
        GPIO_I2C_OLED_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_I2C_OLED_IOMUX_SCL,
        GPIO_I2C_OLED_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_OLED_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_OLED_IOMUX_SCL);
    DL_I2C_enablePower(I2C_OLED_INST);
    SYSCFG_DL_I2C_OLED_init();
    return 0;
}

void oled_i2c_sda_unlock(void)
{
    uint8_t cycleCnt = 0;
    mspm0_i2c_disable();
    do
    {
        DL_GPIO_clearPins(GPIO_I2C_OLED_SCL_PORT, GPIO_I2C_OLED_SCL_PIN);
        delay_ms(1);
        DL_GPIO_setPins(GPIO_I2C_OLED_SCL_PORT, GPIO_I2C_OLED_SCL_PIN);
        delay_ms(1);

        if(DL_GPIO_readPins(GPIO_I2C_OLED_SDA_PORT, GPIO_I2C_OLED_SDA_PIN))
            break;
    }while(++cycleCnt < 100);
    mspm0_i2c_enable();
}

void OLED_ColorTurn(uint8_t i) { if(i==0) OLED_WR_Byte(0xA6,OLED_CMD); else OLED_WR_Byte(0xA7,OLED_CMD); }
void OLED_DisplayTurn(uint8_t i) { if(i==0) { OLED_WR_Byte(0xC8,OLED_CMD); OLED_WR_Byte(0xA1,OLED_CMD); } else { OLED_WR_Byte(0xC0,OLED_CMD); OLED_WR_Byte(0xA0,OLED_CMD); } }

void OLED_WR_Byte(uint8_t dat, uint8_t mode)
{
    unsigned char ptr[2];
    uint32_t idle_timeout = 20000; 
    uint32_t tx_timeout = 100000; // 免依赖版的发送超时计数

    if(mode) { ptr[0] = 0x40; ptr[1] = dat; }
    else     { ptr[0] = 0x00; ptr[1] = dat; }

    DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, ptr, 2);
    DL_I2C_clearInterruptStatus(I2C_OLED_INST, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);

    while (!(DL_I2C_getControllerStatus(I2C_OLED_INST) & DL_I2C_CONTROLLER_STATUS_IDLE))
    {
        if (--idle_timeout == 0) 
        {
            DL_I2C_reset(I2C_OLED_INST);
            SYSCFG_DL_I2C_OLED_init();
            return; 
        }
    }

    DL_I2C_startControllerTransfer(I2C_OLED_INST, 0x3C, DL_I2C_CONTROLLER_DIRECTION_TX, 2);

    while (!DL_I2C_getRawInterruptStatus(I2C_OLED_INST, DL_I2C_INTERRUPT_CONTROLLER_TX_DONE))
    {
        if(--tx_timeout == 0)
        {
            oled_i2c_sda_unlock();
            break;
        }
    }
}

void OLED_Set_Pos(uint8_t x, uint8_t y) { OLED_WR_Byte(0xb0+y,OLED_CMD); OLED_WR_Byte(((x&0xf0)>>4)|0x10,OLED_CMD); OLED_WR_Byte((x&0x0f),OLED_CMD); }
void OLED_Display_On(void) { OLED_WR_Byte(0X8D,OLED_CMD); OLED_WR_Byte(0X14,OLED_CMD); OLED_WR_Byte(0XAF,OLED_CMD); }
void OLED_Display_Off(void) { OLED_WR_Byte(0X8D,OLED_CMD); OLED_WR_Byte(0X10,OLED_CMD); OLED_WR_Byte(0XAE,OLED_CMD); }
	 
void OLED_Clear(void)  
{  
    uint8_t i,n;		    
    for(i=0;i<8;i++)  
    {  
        OLED_WR_Byte (0xb0+i,OLED_CMD);   
        OLED_WR_Byte (0x00,OLED_CMD);     
        OLED_WR_Byte (0x10,OLED_CMD);       
        for(n=0;n<128;n++)OLED_WR_Byte(0,OLED_DATA); 
    } 
}

void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey)
{      	
    uint8_t c=0,sizex=sizey/2;
    uint16_t i=0,size1;
    if(sizey==8)size1=6;
    else size1=(sizey/8+((sizey%8)?1:0))*(sizey/2);
    c=chr-' ';
    OLED_Set_Pos(x,y);
    for(i=0;i<size1;i++)
    {
        if(i%sizex==0&&sizey!=8) OLED_Set_Pos(x,y++);
        if(sizey==8) OLED_WR_Byte(asc2_0806[c][i],OLED_DATA);
        else if(sizey==16) OLED_WR_Byte(asc2_1608[c][i],OLED_DATA);
        else return;
    }
}

uint32_t oled_pow(uint8_t m,uint8_t n) { uint32_t result=1; while(n--)result*=m; return result; }

void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t sizey)
{         	
    uint8_t t,temp,m=0; uint8_t enshow=0;
    if(sizey==8)m=2;
    for(t=0;t<len;t++)
    {
        temp=(num/oled_pow(10,len-t-1))%10;
        if(enshow==0&&t<(len-1)) { if(temp==0) { OLED_ShowChar(x+(sizey/2+m)*t,y,' ',sizey); continue; }else enshow=1; }
        OLED_ShowChar(x+(sizey/2+m)*t,y,temp+'0',sizey);
    }
}

void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *chr,uint8_t sizey)
{
    uint8_t j=0;
    while (chr[j]!='\0')
    {		
        OLED_ShowChar(x,y,chr[j++],sizey);
        if(sizey==8)x+=6;
        else x+=sizey/2;
    }
}

// ============== 这里是我们附加的浮点数显示功能 ================
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t dec_len, uint8_t sizey) {
    uint8_t step = (sizey == 16 ? 8 : 6);
    
    if (num < 0) { OLED_ShowChar(x, y, '-', sizey); num = -num; x += step; }
    else { OLED_ShowChar(x, y, ' ', sizey); x += step; }
    
    uint32_t pow10 = 1;
    for (uint8_t i = 0; i < int_len - 1; i++) pow10 *= 10;
    
    uint32_t integer = (uint32_t)num;
    for (uint8_t i = 0; i < int_len; i++) {
        OLED_ShowChar(x, y, (integer / pow10) % 10 + '0', sizey);
        pow10 /= 10; x += step;
    }
    OLED_ShowChar(x, y, '.', sizey); x += step;
    
    uint32_t decimal = (uint32_t)((num - (uint32_t)num) * (dec_len == 1 ? 10 : 100));
    if (dec_len == 2) {
        OLED_ShowChar(x, y, decimal / 10 + '0', sizey);
        OLED_ShowChar(x + step, y, decimal % 10 + '0', sizey);
    } else {
        OLED_ShowChar(x, y, decimal + '0', sizey);
    }
}
// ==============================================================

void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t no,uint8_t sizey)
{
    uint16_t i,size1=(sizey/8+((sizey%8)?1:0))*sizey;
    for(i=0;i<size1;i++)
    {
        if(i%sizey==0) OLED_Set_Pos(x,y++);
        if(sizey==16) OLED_WR_Byte(Hzk[no][i],OLED_DATA);
        else return;
    }				
}

void OLED_DrawBMP(uint8_t x,uint8_t y,uint8_t sizex, uint8_t sizey,uint8_t BMP[])
{ 	
    uint16_t j=0; uint8_t i,m;
    sizey=sizey/8+((sizey%8)?1:0);
    for(i=0;i<sizey;i++) { OLED_Set_Pos(x,i+y); for(m=0;m<sizex;m++) { OLED_WR_Byte(BMP[j++],OLED_DATA); } }
}

void OLED_Init(void)
{
    if(DL_I2C_getSDAStatus(I2C_OLED_INST) == DL_I2C_CONTROLLER_SDA_LOW)
        oled_i2c_sda_unlock();

    delay_ms(200);

    OLED_WR_Byte(0xAE,OLED_CMD); OLED_WR_Byte(0x00,OLED_CMD); OLED_WR_Byte(0x10,OLED_CMD);
    OLED_WR_Byte(0x40,OLED_CMD); OLED_WR_Byte(0x81,OLED_CMD); OLED_WR_Byte(0xCF,OLED_CMD); 
    OLED_WR_Byte(0xA1,OLED_CMD); OLED_WR_Byte(0xC8,OLED_CMD); OLED_WR_Byte(0xA6,OLED_CMD);
    OLED_WR_Byte(0xA8,OLED_CMD); OLED_WR_Byte(0x3f,OLED_CMD); OLED_WR_Byte(0xD3,OLED_CMD);
    OLED_WR_Byte(0x00,OLED_CMD); OLED_WR_Byte(0xd5,OLED_CMD); OLED_WR_Byte(0x80,OLED_CMD);
    OLED_WR_Byte(0xD9,OLED_CMD); OLED_WR_Byte(0xF1,OLED_CMD); OLED_WR_Byte(0xDA,OLED_CMD);
    OLED_WR_Byte(0x12,OLED_CMD); OLED_WR_Byte(0xDB,OLED_CMD); OLED_WR_Byte(0x40,OLED_CMD);
    OLED_WR_Byte(0x20,OLED_CMD); OLED_WR_Byte(0x02,OLED_CMD); OLED_WR_Byte(0x8D,OLED_CMD);
    OLED_WR_Byte(0x14,OLED_CMD); OLED_WR_Byte(0xA4,OLED_CMD); OLED_WR_Byte(0xA6,OLED_CMD); 
    OLED_Clear();
    OLED_WR_Byte(0xAF,OLED_CMD); 
}