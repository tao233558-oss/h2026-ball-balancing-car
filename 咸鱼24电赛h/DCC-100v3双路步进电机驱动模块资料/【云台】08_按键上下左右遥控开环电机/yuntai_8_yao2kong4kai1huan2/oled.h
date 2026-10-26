#ifndef __OLED_H
#define __OLED_H 

#include "stdlib.h"	
#include "ti_msp_dl_config.h"

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

// SCL   PB18
// SDA   PA9
// RES   PA8
// DC    PB2
// CS    PA2
// VCC  3.3V
// GND  GND

//

//-----------------OLED端口定义---------------- 

// #define OLED_SCL_Clr() DL_GPIO_clearPins(OLED_SPI_OLED_SPI_SCL_PORT, OLED_SPI_OLED_SPI_SCL_PIN);//SCL
// #define OLED_SCL_Set() DL_GPIO_setPins(OLED_SPI_OLED_SPI_SCL_PORT, OLED_SPI_OLED_SPI_SCL_PIN)

// #define OLED_SDA_Clr() DL_GPIO_clearPins(OLED_SPI_OLED_SPI_SDA_PORT, OLED_SPI_OLED_SPI_SDA_PIN);//SDA
// #define OLED_SDA_Set() DL_GPIO_setPins(OLED_SPI_OLED_SPI_SDA_PORT, OLED_SPI_OLED_SPI_SDA_PIN)

#define OLED_RES_Clr() DL_GPIO_clearPins(OLED_SPI_OLED_SPI_RES_PORT, OLED_SPI_OLED_SPI_RES_PIN);//RES
#define OLED_RES_Set() DL_GPIO_setPins(OLED_SPI_OLED_SPI_RES_PORT, OLED_SPI_OLED_SPI_RES_PIN)

#define OLED_DC_Clr()  DL_GPIO_clearPins(OLED_SPI_OLED_SPI_DC_PORT, OLED_SPI_OLED_SPI_DC_PIN);//DC
#define OLED_DC_Set()  DL_GPIO_setPins(OLED_SPI_OLED_SPI_DC_PORT, OLED_SPI_OLED_SPI_DC_PIN)
 		     
#define OLED_CS_Clr()  DL_GPIO_clearPins(OLED_SPI_OLED_SPI_CS_PORT, OLED_SPI_OLED_SPI_CS_PIN);//CS
#define OLED_CS_Set()  DL_GPIO_setPins(OLED_SPI_OLED_SPI_CS_PORT, OLED_SPI_OLED_SPI_CS_PIN)


#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据

void OLED_ClearPoint(u8 x,u8 y);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);
void OLED_DrawLine(u8 x1,u8 y1,u8 x2,u8 y2,u8 mode);
void OLED_DrawCircle(u8 x,u8 y,u8 r);
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size1,u8 mode);
void OLED_ShowChar6x8(u8 x,u8 y,u8 chr,u8 mode);
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 size1,u8 mode);
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size1,u8 mode);
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1,u8 mode);
void OLED_ScrollDisplay(u8 num,u8 space,u8 mode);
void OLED_ShowPicture(u8 x,u8 y,u8 sizex,u8 sizey,u8 BMP[],u8 mode);
void OLED_Init(void);

#endif

