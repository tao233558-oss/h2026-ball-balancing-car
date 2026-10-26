#include "control.h"		
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
#define T 0.156f
#define L 0.1445f
#define K 622.8f
u8 Flag_Target;
int Voltage_Temp,Voltage_Count,Voltage_All,sum;

/**************************************************************************
函数功能：小车运动数学模型
入口参数：速度和转角
返回  值：无
**************************************************************************/
void Kinematic_Analysis(float velocity,float turn)
{
	#if Akm_Car
		Target_A=velocity*(1+T*tan(turn)/2/L); 
		Target_B=velocity*(1-T*tan(turn)/2/L);      //后轮差速
		Servo=SERVO_INIT+turn*K;                    //舵机转向  
    #elif Diff_Car
	    Target_A=(velocity+turn); 
		Target_B=(velocity-turn);      //后轮差速
	#elif Small_Tank_Car
	    Target_A=(velocity+turn)*1.0; 
		Target_B=(velocity-turn);      //后轮差速
	#endif
	
}
/**************************************************************************
函数功能：所有的控制代码都在这里面
          5ms定时中断由定时器5触发
          严格保证采样和数据处理的时间同步				 
**************************************************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == (&htim2))
    {                   	
		  	 Encoder_Left=-Read_Encoder(3);                                       //===读取编码器的值							 //为了保证M法测速的时间基准，首先读取编码器数据
		     Encoder_Right=-Read_Encoder(4);                                      //===读取编码器的值
		    if(Delay_Flag==1)
			 {
				 if(++Delay_50==10)	 Delay_50=0,Delay_Flag=0;                      //给主函数提供50ms的精准延时
			 }
			if(CCD_Line_Patrol_Mode)
		   {	 
				 RD_TSL();                           		 //===读取线性CCD数据 
				 Find_CCD_Zhongzhi();			          		 //===提取中线 
		   }
			if(ELE_Line_Patrol_Mode)		
			 {
				Sensor_Left=Get_Adc(ADC_CHANNEL_2);                //采集左边电感的数据
				Sensor_Right=Get_Adc(ADC_CHANNEL_9);               //采集右边电感的数据
				Sensor_Middle=Get_Adc(ADC_CHANNEL_1);              //采集中间电感的数据
				sum=Sensor_Left*1+Sensor_Middle*100+Sensor_Right*199;    //归一化处理
				Sensor=sum/(Sensor_Left+Sensor_Middle+Sensor_Right);   //求偏差
			 }						 
			Get_RC();
			#if Akm_Car
			   Kinematic_Analysis(Velocity,Turn);  //舵机小车运动学分析   
   			  if(Turn_Off(Voltage)==0)                              					//===如果不存在异常
				{
					Motor_A=Incremental_PI_A(Encoder_Left,Target_A);                   //===速度闭环控制计算电机A最终PWM
					Motor_B=Incremental_PI_B(Encoder_Right,Target_B);                  //===速度闭环控制计算电机B最终PWM 
					Xianfu_Pwm();                                                      //===PWM限幅
					Set_Pwm(Motor_A,Motor_B);                                 	       //===赋值给PWM寄存器  
					SERVO=Servo;
				}		
				else  
			   {
				 Set_Pwm(0,0);
				 SERVO=SERVO_INIT;  
			   }	   												  
			#elif Diff_Car
			   Kinematic_Analysis(Velocity,Turn);   //差速小车和履带小车运动学分析   
               if(Turn_Off(Voltage)==0)                              				   //===如果不存在异常
				{
					Motor_A=Incremental_PI_A(Encoder_Left,Target_A);                   //===速度闭环控制计算电机A最终PWM
					Motor_B=Incremental_PI_B(Encoder_Right,Target_B);                  //===速度闭环控制计算电机B最终PWM 
					Xianfu_Pwm();                                                      //===PWM限幅
					Set_Pwm(Motor_A,Motor_B);                                 	       //===赋值给PWM寄存器  
				}		
				else  
			   {
				 Set_Pwm(0,0);
			   }	
			#elif Small_Tank_Car
				Kinematic_Analysis(Velocity,Turn);   //差速小车和履带小车运动学分析   
                if(Turn_Off(Voltage)==0)                              					//===如果不存在异常
				{
					Motor_A=Incremental_PI_A(Encoder_Left,Target_A);                   //===速度闭环控制计算电机A最终PWM
					Motor_B=Incremental_PI_B(Encoder_Right,Target_B);                  //===速度闭环控制计算电机B最终PWM 
					Xianfu_Pwm();                                                      //===PWM限幅
					Set_Pwm(Motor_A,Motor_B);                                 	       //===赋值给PWM寄存器  
				}		
				else  
			   {
				 Set_Pwm(0,0);
			   }	
			#endif				
			Voltage_Temp=Get_battery_volt();		                             //=====读取电池电压		
			Voltage_Count++;                                                     //=====平均值计数器
			Voltage_All+=Voltage_Temp;                                           //=====多次采样累积
			if(Voltage_Count==100) Voltage=Voltage_All/100,Voltage_All=0,Voltage_Count=0;//=====求平均值		                                   
			  
			if(Flag_Show==0)				Led_Flash(100);
			else if(Flag_Show==1)	        Led_Flash(0);  //led闪烁
			 Key();    //===扫描按键状态 单击双击可以改变小车运行状态
	}
} 
/**************************************************************************
函数功能：赋值给PWM寄存器
入口参数：左轮PWM、右轮PWM
返回  值：无
**************************************************************************/
void Set_Pwm(int motor_a,int motor_b)
{
	    if(ELE_Line_Patrol_Mode==1||CCD_Line_Patrol_Mode==1)//巡线模式下，只允许电机正转
		{
		    if(motor_a>0)motor_a=0;
		    if(motor_b>0)motor_b=0;
		
		 }
    	if(motor_a>0)	AIN1=0,			AIN2=1;
		else 	        AIN1=1,			AIN2=0;
		PWMA=myabs(motor_a);
		if(motor_b>0)	BIN1=0,			BIN2=1;
		else            BIN1=1,			BIN2=0;
		PWMB=myabs(motor_b);		
}

/**************************************************************************
函数功能：限制PWM赋值 
入口参数：无
返回  值：无
**************************************************************************/
void Xianfu_Pwm(void)
{	
	int Amplitude=6900;    //===PWM满幅是7200 限制在6900
    if(Motor_A<-Amplitude) Motor_A=-Amplitude;	
	if(Motor_A>Amplitude)  Motor_A=Amplitude;	
	if(Motor_B<-Amplitude) Motor_B=-Amplitude;	
	if(Motor_B>Amplitude)  Motor_B=Amplitude;
    #if Akm_Car	
	  if(Servo<(SERVO_INIT-500))     Servo=SERVO_INIT-500;	  //舵机限幅
	  if(Servo>(SERVO_INIT+500))     Servo=SERVO_INIT+500;		  //舵机限幅
	#endif
}
/**************************************************************************
函数功能：按键修改小车运行状态 
入口参数：无
返回  值：无
**************************************************************************/
void Key(void)
{	
	u8 tmp,tmp2;
	tmp=click(); 
	if(tmp==1)Flag_Stop=!Flag_Stop;//单击控制小车的启停
	//if(tmp==2)Flag_Show=!Flag_Show;//双击控制小车的显示状态
	tmp2=Long_Press();          
  if(tmp2==1)Flag_Show=!Flag_Show;//控制小车的显示状态
}

/**************************************************************************
函数功能：异常关闭电机
入口参数：电压
返回  值：1：异常  0：正常
**************************************************************************/
u8 Turn_Off( int voltage)
{
	  u8 temp;
	if(voltage<10.0||Flag_Stop==1)//电池电压低于10.5V关闭电机
	{	                                                
      temp=1;    
      }
	else
      temp=0;
    return temp;			
}
/**************************************************************************
函数功能：绝对值函数
入口参数：int
返回  值：unsigned int
**************************************************************************/
int myabs(int a)
{ 		   
	  int temp;
		if(a<0)  temp=-a;  
	  else temp=a;
	  return temp;
}

/**************************************************************************
函数功能：限制函数
入口参数：int
返回  值：unsigned int
**************************************************************************/
int limit(int input,int min,int max)
{ 		   
	  if(input<min)  input = 0;  
	  if(input>max)  input =max;
	  return input;
}

/**************************************************************************
函数功能：增量PI控制器
入口参数：编码器测量值，目标速度
返回  值：电机PWM
根据增量式离散PID公式 
pwm+=Kp[e（k）-e(k-1)]+Ki*e(k)+Kd[e(k)-2e(k-1)+e(k-2)]
e(k)代表本次偏差 
e(k-1)代表上一次的偏差  以此类推 
pwm代表增量输出
在我们的速度控制闭环系统里面，只使用PI控制
pwm+=Kp[e（k）-e(k-1)]+Ki*e(k)
**************************************************************************/
int Incremental_PI_A (int Encoder,int Target)
{ 	
	 static int Bias,Pwm,Last_bias;
	 Bias=Encoder-Target;                //计算偏差
	 Pwm+=Velocity_KP*(Bias-Last_bias)+Velocity_KI*Bias;   //增量式PI控制器
	 Last_bias=Bias;	                   //保存上一次偏差 
	 return Pwm;                         //增量输出
}
int Incremental_PI_B (int Encoder,int Target)
{ 	
	 static int Bias,Pwm,Last_bias;
	 Bias=Encoder-Target;                //计算偏差
	 Pwm+=Velocity_KP*(Bias-Last_bias)+Velocity_KI*Bias;   //增量式PI控制器
	 Last_bias=Bias;	                   //保存上一次偏差 
	 return Pwm;                         //增量输出
}
/**************************************************************************
函数功能：遥控
入口参数：无
返回  值：无
**************************************************************************/
void Get_RC(void)
{
	static float Bias,Last_Bias;
	if(BLUETOOTH_Mode==1&&APP_Flag==1)//蓝牙控制
	{
		#if Akm_Car
		{
	    	if(Flag_Direction==0) Velocity=0,Turn=0;   //停止
	    	else if(Flag_Direction==0x41) Velocity=Bluetooth_Velocity,Turn=0;  //前进
	    	else if(Flag_Direction==0x42) Velocity=Bluetooth_Velocity,Turn=Pi/5;  //右前
	    	else if(Flag_Direction==0x43) Velocity=0,Turn=0;   //舵机向右
	    	else if(Flag_Direction==0x44) Velocity=-Bluetooth_Velocity,Turn=Pi/5;  // 右后
	    	else if(Flag_Direction==0x45) Velocity=-Bluetooth_Velocity,Turn=0;    //后退
	    	else if(Flag_Direction==0x46) Velocity=-Bluetooth_Velocity,Turn=-Pi/5;  //左后
	    	else if(Flag_Direction==0x47) Velocity=0,Turn=0;                       //舵机向左
	    	else if(Flag_Direction==0x48) Velocity=Bluetooth_Velocity,Turn=-Pi/5;  //左前
		}
		#elif Diff_Car
		{
			if(Flag_Direction==0) Velocity=0,Turn=0;   //停止
	    	else if(Flag_Direction==0x41) Velocity=Bluetooth_Velocity,Turn=0;  //前进
	    	else if(Flag_Direction==0x42) Velocity=Bluetooth_Velocity,Turn=20;  //右前
	    	else if(Flag_Direction==0x43) Velocity=0,Turn=20;   //舵机向右
	    	else if(Flag_Direction==0x44) Velocity=-Bluetooth_Velocity,Turn=-20;  // 右后
	    	else if(Flag_Direction==0x45) Velocity=-Bluetooth_Velocity,Turn=0;    //后退
	    	else if(Flag_Direction==0x46) Velocity=-Bluetooth_Velocity,Turn=20;  //左后
	    	else if(Flag_Direction==0x47) Velocity=0,Turn=-20;                       //舵机向左
	    	else if(Flag_Direction==0x48) Velocity=Bluetooth_Velocity,Turn=-20;  //左前
		}
		#elif Small_Tank_Car
		{
		  if(Flag_Direction==0) Velocity=0,Turn=0;   //停止
		else if(Flag_Direction==0x41) Velocity=Bluetooth_Velocity,Turn=0;  //前进
		else if(Flag_Direction==0x42) Velocity=Bluetooth_Velocity,Turn=20;  //右前
		else if(Flag_Direction==0x43) Velocity=0,Turn=20;   //舵机向右
		else if(Flag_Direction==0x44) Velocity=-Bluetooth_Velocity,Turn=-20;  // 右后
		else if(Flag_Direction==0x45) Velocity=-Bluetooth_Velocity,Turn=0;    //后退
		else if(Flag_Direction==0x46) Velocity=-Bluetooth_Velocity,Turn=20;  //左后
		else if(Flag_Direction==0x47) Velocity=0,Turn=-20;                       //舵机向左
		else if(Flag_Direction==0x48) Velocity=Bluetooth_Velocity,Turn=-20;  //左前
		}
		#endif
		
	}
   else if(CCD_Line_Patrol_Mode)//CCD巡线
   {
	    Velocity=30;	   //CCD巡线模式的速度
	    Bias=CCD_Zhongzhi-64;   //提取偏差
	    #if Akm_Car
	        Turn=Bias*0.010+(Bias-Last_Bias)*0.085; //PD控制
	    #elif Diff_Car
	        Turn=Bias*0.1+(Bias-Last_Bias)*1; //PD控制
	    #elif Small_Tank_Car	  
	        Turn=Bias*0.4+(Bias-Last_Bias)*2; //PD控制	 
	    #endif
	     Last_Bias=Bias;   //保存上一次的偏差
	
   }
	else if(ELE_Line_Patrol_Mode)//电磁巡线
  {  
	
	  Velocity=30;	  //电磁巡线模式下的速度
	  Bias=100-Sensor;  //提取偏差
	  #if Akm_Car
	    Turn=-myabs(Bias)*Bias*0.0002-Bias*0.005-(Bias-Last_Bias)*0.005; //	 
	  #elif Diff_Car
		Turn=-myabs(Bias)*Bias*0.003-Bias*0.005-(Bias-Last_Bias)*1.0; //
	  #elif Small_Tank_Car
		Turn=-myabs(Bias)*Bias*0.005-Bias*0.01-(Bias-Last_Bias)*2;
	  #endif
		  Last_Bias=Bias;   //上一次的偏差
	  }
  else if(PS2_Mode==1&&PS2_Flag==1)
  {
	 int LY,RX;									//手柄ADC的值
	int Threshold=20; 							//阈值，忽略摇杆小幅度动作
	static u8 Key1_Count = 0,Key2_Count = 0;	//用于控制读取摇杆的速度
	#if Akm_Car
	{
		PS2_Turn_Velocity=Pi/5;
	}
	#elif Diff_Car
	{
		PS2_Turn_Velocity=20;
	}
	#elif Small_Tank_Car
	{
		PS2_Turn_Velocity=20;
	}
	#endif
	//转化为128到-128的数值
	LY=-(PS2_LY-128);//左边Y轴控制前进后退
	RX=-(PS2_RX-128);//右边X轴控制转向

	if(LY>-Threshold&&LY<Threshold)	LY=0;
	if(RX>-Threshold&&RX<Threshold)	RX=0;		//忽略摇杆小幅度动作
	
	Velocity = (PS2_Velocity/128)*LY;				//速度控制，力度表示速度大小

	if(Velocity>=0)
		Turn = -(PS2_Turn_Velocity/128)*RX;	//转向控制，力度表示转向速度
	else
		Turn = (PS2_Turn_Velocity/128)*RX;
	if (PS2_KEY == PSB_L1) 					 	//按下左1键加速（按键在顶上）
	{	
		if((++Key1_Count) == 20)				//调节按键反应速度
		{
			PS2_KEY = 0;
			Key1_Count = 0;
			if((PS2_Velocity += 5)>40)				
				PS2_Velocity = 40;
            #if Akm_Car
				if((PS2_Turn_Velocity += Pi/10)>Pi/4)	
					PS2_Turn_Velocity = Pi/4;
			#elif Diff_Car
			     if((PS2_Turn_Velocity += 2)>30)	
					PS2_Turn_Velocity = 30;
			#elif Small_Tank_Car
			     if((PS2_Turn_Velocity += 2)>30)	
					PS2_Turn_Velocity = 30;
			#endif
			}
	}
	else if(PS2_KEY == PSB_R1) 					//按下右1键减速
	{
		if((++Key2_Count) == 20)
		{
			PS2_KEY = 0;
			Key2_Count = 0;
			if((PS2_Velocity -= 5)<15)			//前后最小速度
				PS2_Velocity = 15;
			 #if Akm_Car
			if((PS2_Turn_Velocity -= Pi/10)<Pi/10)//转向最小速度200
			   PS2_Turn_Velocity = Pi/10;
			#elif Diff_Car
			     if((PS2_Turn_Velocity -= 2)<10)	
					PS2_Turn_Velocity = 10;
			#elif Small_Tank_Car
			     if((PS2_Turn_Velocity -= 2)>10)	
					PS2_Turn_Velocity = 10;
			#endif
		}
	}
	else
		Key2_Count = 0,Key2_Count = 0;			//读取到其他按键重新计数
  }
}
/**************************************************************************
函数功能：线性CCD取中值
入口参数：无
返回  值：无
**************************************************************************/
void  Find_CCD_Zhongzhi(void)
{ 
	 static u16 i,j,Left,Right;
	 static u16 value1_max,value1_min;
	
	   value1_max=ADV[0];  //动态阈值算法，读取最大和最小值
     for(i=5;i<123;i++)   //两边各去掉5个点
     {
        if(value1_max<=ADV[i])
        value1_max=ADV[i];
     }
	   value1_min=ADV[0];  //最小值
     for(i=5;i<123;i++) 
     {
        if(value1_min>=ADV[i])
        value1_min=ADV[i];
     }
   CCD_Yuzhi=(value1_max+value1_min)/2;	  //计算出本次中线提取的阈值
	 for(i = 5;i<118; i++)   //寻找左边跳变沿
	{
		if(ADV[i]>CCD_Yuzhi&&ADV[i+1]>CCD_Yuzhi&&ADV[i+2]>CCD_Yuzhi&&ADV[i+3]<CCD_Yuzhi&&ADV[i+4]<CCD_Yuzhi&&ADV[i+5]<CCD_Yuzhi)
		{	
			Left=i;
			break;	
		}
	}
	 for(j = 118;j>5; j--)//寻找右边跳变沿
  {
		if(ADV[j]<CCD_Yuzhi&&ADV[j+1]<CCD_Yuzhi&&ADV[j+2]<CCD_Yuzhi&&ADV[j+3]>CCD_Yuzhi&&ADV[j+4]>CCD_Yuzhi&&ADV[j+5]>CCD_Yuzhi)
		{	
		  Right=j;
		  break;	
		}
  }
	CCD_Zhongzhi=(Right+Left)/2;//计算中线位置
//	if(myabs(CCD_Zhongzhi-Last_CCD_Zhongzhi)>90)   //计算中线的偏差，如果太大
//	CCD_Zhongzhi=Last_CCD_Zhongzhi;    //则取上一次的值
//	Last_CCD_Zhongzhi=CCD_Zhongzhi;  //保存上一次的偏差
}
