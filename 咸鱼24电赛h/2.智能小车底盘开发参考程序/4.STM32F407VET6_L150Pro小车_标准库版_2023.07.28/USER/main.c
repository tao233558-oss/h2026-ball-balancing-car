#include "system.h"


int main(void)
{ 
  systemInit(); //Hardware initialization //硬件初始化
	
  /**下面是通过直接操作库函数的方式实现IO控制**/	
	while(1)
	{
		show_task();	//OLED显示屏和APP打印内容
    PS2_Read(); //读取PS2的数据	
		if(data_sent_flag) data_task();//串口 can发送数据 10HZ发送频率
	}
}



 



