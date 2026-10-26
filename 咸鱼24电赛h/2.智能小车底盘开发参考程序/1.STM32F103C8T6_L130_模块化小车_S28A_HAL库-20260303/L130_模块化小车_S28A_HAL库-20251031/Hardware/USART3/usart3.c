#include "usart3.h"
u8 Usart3_Receive;
u8 Usart3_Receive_buf[1];//串口3接收中断数据存放的缓冲区
/***********************************************
公司：轮趣科技（东莞）有限公司
品牌：WHEELTEC
官网：wheeltec.net
淘宝店铺：shop114407458.taobao.com 
速卖通: https://minibalance.aliexpress.com/store/4455017
版本：V1.0
修改时间：2023-01-04

Brand: WHEELTEC
Website: wheeltec.net
Taobao shop: shop114407458.taobao.com 
Aliexpress: https://minibalance.aliexpress.com/store/4455017
Version: V1.0
Update：2023-01-04

All rights reserved
***********************************************/
/**************************************************************************
函数功能：串口3接收中断
入口参数：无
返回  值：无
**************************************************************************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle) //接收回调函数
{	
	if(UartHandle->Instance == USART3)//接收到数据
	{	  
		static u8 Flag_PID,i,j,Receive[50];
		static float Data;
  	    Usart3_Receive=Usart3_Receive_buf[0]; 
		APP_RX=Usart3_Receive;
		if(Usart3_Receive>=0x41&&Usart3_Receive<=0x48)  
		  Flag_Direction=Usart3_Receive-0x40;
		else if(Usart3_Receive<10)  
		  Flag_Direction=Usart3_Receive;	
		else if(Usart3_Receive==0X5A)  
		  Flag_Direction=0;	
		else if(Usart3_Receive==0X59)
			Bluetooth_Velocity -=10;
		else if(Usart3_Receive==0x58)
			Bluetooth_Velocity +=10; 
		Bluetooth_Velocity = limit(Bluetooth_Velocity,0,60);
			
				//以下是与APP调试界面通讯
		if(Usart3_Receive==0x7B) Flag_PID=1;   //APP参数指令起始位
		if(Usart3_Receive==0x7D) Flag_PID=2;   //APP参数指令停止位

		 if(Flag_PID==1)  //采集数据
		 {
			Receive[i]=Usart3_Receive;
			i++;
		 }
		 if(Flag_PID==2)  //分析数据
		 {
			 if(Receive[3]==0x50) 	 PID_Send=1;
			 else  if(Receive[3]==0x57) 	 Flash_Send=1;
			 else  if(Receive[1]!=0x23) 
			 {								
				for(j=i;j>=4;j--)
				{
				  Data+=(Receive[j-1]-48)*pow(10,i-j);
				}
				switch(Receive[1])
				 {
					 case 0x30:  break;
					 case 0x31:  Velocity_KP=Data;break;
					 case 0x32:  Velocity_KI=Data;break;
					 case 0x33:  break;
					 case 0x34:  break;
					 case 0x35:  break;
					 case 0x36:  break;
					 case 0x37:  break; //预留
					 case 0x38:  break; //预留
				 }
				}				 
			 Flag_PID=0;//相关标志位清零
			 i=0;
			 j=0;
			 Data=0;
			 memset(Receive, 0, sizeof(u8)*50);//数组清零
		 } 	 	
      HAL_UART_Receive_IT(&huart3,Usart3_Receive_buf,sizeof(Usart3_Receive_buf));//串口2回调函数执行完毕之后，需要再次开启接收中断等待下一次接收中断的发生		 
	}  											 
} 


