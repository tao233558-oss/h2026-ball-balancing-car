#ifndef __USRAT3_H
#define __USRAT3_H 
#include "sys.h"	  	

extern u8 Usart3_Receive;
extern u8 Usart3_Receive_buf[1];//串口2接收中断数据存放的缓冲区
void uart3_init(u32 bound);

#endif

