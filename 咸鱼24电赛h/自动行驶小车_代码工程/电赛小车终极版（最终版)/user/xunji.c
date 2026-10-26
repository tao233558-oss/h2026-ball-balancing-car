#include "headfile.h"
#include "ml_gpio.h"
#include "xunji.h"
#include "ml_motor.h"
#include "pid.h"
#include "buzzer_light.h"

//int left_encoder,right_encoder,left_pwm,right_pwm,left_angle_pwm,right_angle_pwm;
//int base_left_pwm,base_right_pwm,angle_pwm;

int now_statue,last_statue,change_flag1;

/****************************************************************************************
 * 调参区：优先改这里
 *
 * 1. 方形黑线循迹参数（mode 1）
 *    传感器显示规则：白地=0，黑线=1。
 *    D1/D2 在车左侧，D7/D8 在车右侧，D4/D5 接近车头中间。
 *
 * 2. 24 年 H 题要求 1 参数（mode 2）
 *    mode 2 不走循迹黑线，而是用陀螺仪保持启动瞬间的车头方向直行。
 *    KEY2 按下那一刻，会把当前 yaw_angle_int 记录为目标航向。
 ****************************************************************************************/

// mode 1：方形黑线循迹的基础前进 PWM。越大车越快，也越容易冲出线；小白调车建议 1200~2200。
#define TRACK_SQUARE_BASE_PWM       1800

// mode 1：丢线后找线的 PWM。越大找线越快，但容易转过头；建议 1000~1800。
#define TRACK_SQUARE_SEARCH_PWM     1400

// mode 1：循迹比例系数。越大纠偏越猛；车不转弯就加大，左右抖动就减小。
#define TRACK_SQUARE_KP             75

// mode 1：循迹微分系数。用于抑制来回摆动；抖动明显可稍微加大，不灵敏可减小。
#define TRACK_SQUARE_KD             25

// mode 1：PWM 最大限幅，防止算法算出过大的电机输出。一般不用改。
#define TRACK_SQUARE_PWM_LIMIT      5000

// mode 1：检测到边缘双传感器时的转弯 PWM。直角转不过去就加大，原地打转就减小。
#define TRACK_SQUARE_TURN_PWM       2200

// mode 2：H 题 A->B 直行基础 PWM。飞出去就减小，比如 1000；太慢/带不动就增大。
#define H_REQ1_STRAIGHT_BASE        1200

// mode 2：A->B 直行持续时间。TIMG8 是 10ms 中断，700 约等于 7 秒。
// 没到 B 点就加大，冲过 B 点就减小。比如每次按 50 或 100 调。
#define H_REQ1_AB_TICKS             700

// 最近一次检测到黑线的位置偏差。丢线时用它判断应该向左找线还是向右找线。
static int track_last_error = 0;

// 把 PWM 限制在允许范围内，避免电机输出突然过大。
static int track_limit_pwm(int pwm)
{
	if(pwm > TRACK_SQUARE_PWM_LIMIT)
	{
		return TRACK_SQUARE_PWM_LIMIT;
	}
	if(pwm < -TRACK_SQUARE_PWM_LIMIT)
	{
		return -TRACK_SQUARE_PWM_LIMIT;
	}
	return pwm;
}


// 初始化 8 路循迹传感器的 GPIO 引脚。
// 当前接线：D1=PA0, D2/L3=PA25, D3=PA31, D4=PA28, D5=PA1, D6=PA7, D7=PA2, D8=PA16。
void xunji_init()
{
  gpio_init(GPIOA, DL_GPIO_PIN_0,  PA0,  IN_UP);   // D1
	gpio_init(GPIOA, DL_GPIO_PIN_25, PA25, IN_UP);   // D2 / L3
	gpio_init(GPIOA, DL_GPIO_PIN_31, PA31, IN_UP);   // D3
	gpio_init(GPIOA, DL_GPIO_PIN_28, PA28, IN_UP);   // D4
	gpio_init(GPIOA, DL_GPIO_PIN_1,  PA1,  IN_UP);   // D5
	gpio_init(GPIOA, DL_GPIO_PIN_7,  PA7,  IN_UP);   // D6
	gpio_init(GPIOA, DL_GPIO_PIN_2,  PA2,  IN_UP);   // D7
	gpio_init(GPIOA, DL_GPIO_PIN_16, PA16, IN_UP);   // D8
}

// 读取单个循迹通道。
// 参数 channel：1~8，对应 D1~D8。
// 返回值：0 表示白地，1 表示黑线。
// 你的循迹模块原始电平是白地=1、黑线=0，所以这里已经做了逻辑转换。
unsigned char digtal(unsigned char channel)//1-8	  获取X通道数字值
{
	uint8_t value = 0;
	switch(channel) 
	{
		case 1:  
			if (gpio_get(GPIOA, DL_GPIO_PIN_0)) value = 1;
			else value = 0;  
			break;  
		case 2: 
			if(gpio_get(GPIOA, DL_GPIO_PIN_25)) value = 1;
			else value = 0;  
			break;  
		case 3: 
			if(gpio_get(GPIOA, DL_GPIO_PIN_31)) value = 1;
			else value = 0;  
			break;   
		case 4:  
			if(gpio_get(GPIOA, DL_GPIO_PIN_28)) value = 1;
			else value = 0;  
			break;   
		case 5:
			if(gpio_get(GPIOA, DL_GPIO_PIN_1)) value = 1;
			else value = 0;  
			break;
		case 6:  
			if(gpio_get(GPIOA, DL_GPIO_PIN_7)) value = 1;
			else value = 0;  
			break;  
		case 7: 
			if(gpio_get(GPIOA, DL_GPIO_PIN_2)) value = 1;
			else value = 0;  
			break;  
 		case 8: 
 			if(gpio_get(GPIOA, DL_GPIO_PIN_16)) value = 1;
 			else value = 0;  
 			break;   
	}
	return value; 
}

// 把 8 路循迹状态打包成一个 8 位数，方便判断边缘传感器。
// bit0 对应 D1，bit1 对应 D2，...，bit7 对应 D8。
uint8_t track_read_pattern(void)
{
	uint8_t pattern = 0;
	uint8_t i;

	for(i = 1; i <= 8; i++)
	{
		if(digtal(i))
		{
			pattern |= (uint8_t)(1 << (i - 1));
		}
	}
	return pattern;
}

// 计算循迹偏差 error。
// 左侧传感器给负数，右侧传感器给正数，中间接近 0。
// OLED 第 4 行 E: 后面的值就是这个 error。
// E 为负：黑线偏左；E 为正：黑线偏右；E 接近 0：黑线在中间。
int track_get_error(void)
{
	static const int weight[8] = {-35, -25, -15, -5, 5, 15, 25, 35};
	int sum = 0;
	int active = 0;
	uint8_t i;

	for(i = 1; i <= 8; i++)
	{
		if(digtal(i))
		{
			sum += weight[i - 1];
			active++;
		}
	}

	if(active == 0)
	{
		return track_last_error;
	}

	track_last_error = sum / active;
	return track_last_error;
}

// mode 1 使用：方形黑线循迹。
// 1. 完全看不到黑线 pattern==0：按上一次偏差方向慢慢找线。
// 2. 最左 D1/D2 同时压线：左轮停、右轮走来左转。
// 3. 最右 D7/D8 同时压线：右轮停、左轮走来右转。
// 4. 其它普通情况：用 error 计算左右轮差速，边走边修正。
void track_square_run(void)
{
	static int last_error = 0;
	static int last_turn_dir = 1;
	uint8_t pattern = track_read_pattern();
	int error;
	int turn;
	int left_pwm;
	int right_pwm;

	if(pattern == 0)
	{
		if(last_turn_dir < 0)
		{
			Set_left_pwm(0);
			Set_right_pwm(TRACK_SQUARE_SEARCH_PWM);
		}
		else
		{
			Set_left_pwm(TRACK_SQUARE_SEARCH_PWM);
			Set_right_pwm(0);
		}
		return;
	}

	error = track_get_error();
	if(error < -8)
	{
		last_turn_dir = -1;
	}
	else if(error > 8)
	{
		last_turn_dir = 1;
	}

	if((pattern & 0x03) == 0x03 && (pattern & 0x3C) == 0)
	{
		Set_left_pwm(0);
		Set_right_pwm(TRACK_SQUARE_TURN_PWM);
		last_error = error;
		return;
	}

	if((pattern & 0xC0) == 0xC0 && (pattern & 0x3C) == 0)
	{
		Set_left_pwm(TRACK_SQUARE_TURN_PWM);
		Set_right_pwm(0);
		last_error = error;
		return;
	}

	turn = TRACK_SQUARE_KP * error + TRACK_SQUARE_KD * (error - last_error);
	last_error = error;

	left_pwm = track_limit_pwm(TRACK_SQUARE_BASE_PWM + turn);
	right_pwm = track_limit_pwm(TRACK_SQUARE_BASE_PWM - turn);

	if(left_pwm < 500)
	{
		left_pwm = 500;
	}
	if(right_pwm < 500)
	{
		right_pwm = 500;
	}

	Set_left_pwm(left_pwm);
	Set_right_pwm(right_pwm);
}

static uint16_t h_task_ticks = 0;
static uint8_t h_task_done = 0;
static int h_yaw_target = 0;

// H 题任务复位。
// KEY1 切换模式、KEY2 开始任务时都会调用。
// 这里会记录当前 yaw_angle_int 作为直行目标角度，所以启动前要把车头摆正。
void h_task_reset(void)
{
	h_task_ticks = 0;
	h_task_done = 0;
	h_yaw_target = yaw_angle_int;
}


// 整数限幅函数：把 value 限制在 min_value 和 max_value 之间。
static int h_limit_int(int value, int min_value, int max_value)
{
	if(value > max_value)
	{
		return max_value;
	}
	if(value < min_value)
	{
		return min_value;
	}
	return value;
}

// 计算航向角误差，并处理 -180 到 +180 的跨界问题。
// 例如目标 179 度、当前 -179 度，真实误差应该是 2 度，而不是 358 度。
static int h_yaw_error(int target, int now)
{
	int error = target - now;
	while(error > 180)
	{
		error -= 360;
	}
	while(error < -180)
	{
		error += 360;
	}
	return error;
}

// mode 2 使用：24 年 H 题要求 1，A 点直行到 B 点停车。
// 目前采用“固定 PWM + 陀螺仪小修正”的保守方案，避免速度 PID 暴冲。
// 调参优先看顶部的 H_REQ1_STRAIGHT_BASE 和 H_REQ1_AB_TICKS。
void h_req1_run(void)
{
	int yaw_error;
	int correction;
	int left_pwm;
	int right_pwm;

	if(h_task_done)
	{
		Set_left_pwm(0);
		Set_right_pwm(0);
		return;
	}

	if(h_task_ticks < H_REQ1_AB_TICKS)
	{
		// 当前航向和目标航向的差值。
		yaw_error = h_yaw_error(h_yaw_target, yaw_angle_int);
		// 航向修正量：18 是航向修正比例。走偏修不回来可增大，左右晃可减小。
		correction = h_limit_int(yaw_error * 18, -500, 500);
		// 左右轮 PWM 都被限制在 700~1800，防止突然飞出去。
		left_pwm = h_limit_int(H_REQ1_STRAIGHT_BASE - correction, 700, 1800);
		right_pwm = h_limit_int(H_REQ1_STRAIGHT_BASE + correction, 700, 1800);
		Set_left_pwm(left_pwm);
		Set_right_pwm(right_pwm);
		h_task_ticks++;
	}
	else
	{
		Set_left_pwm(0);
		Set_right_pwm(0);
		beep();
		h_task_done = 1;
	}
}
// mode 1 入口：当前用于方形胶带线循迹调试。
void track1(void)
{
	track_square_run();
}
void track2(void)
{
	  if(D1==1 && D2==1 && D3==1 && D4==0 && D5==0 && D6==1 && D7==1 && D8==1)     //11100111
	{
//		sensor_bias = 0;
		Set_right_pwm(3000); Set_left_pwm(3000); 
	}
	
	 else if(D1==1 && D2==1 && D3==0 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11001111
	{
//		sensor_bias = -15;
		Set_right_pwm(4000); Set_left_pwm(1800); 
	}
   else if(D1==1 && D2==0 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //10011111
	{
//		sensor_bias = -25;
		Set_right_pwm(5000); Set_left_pwm(1000); 
	}
	 else if(D1==0 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //00111111
	{
//		sensor_bias = -35;
		Set_right_pwm(5200); Set_left_pwm(700); 
	}
	 else if(D1==0 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //01111111
	{
//		sensor_bias = -45;
		Set_right_pwm(5600); Set_left_pwm(400); 
	}
	
	 // danglu
	 	else if(D1==1 && D2==1 && D3==1 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11101111
	{
//		sensor_bias = -5;
		Set_right_pwm(3500); Set_left_pwm(2500); 
	}
   else if(D1==1 && D2==1 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //11011111
	{
//		sensor_bias = -20;
		Set_right_pwm(4200); Set_left_pwm(2000); 
	}
	 else if(D1==1 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //10111111
	{
//		sensor_bias = -30;
		Set_right_pwm(4800); Set_left_pwm(1000); 
	}
	
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==1 && D7==1 && D8==1)  //11110111
	{
//		sensor_bias = 10;
		Set_right_pwm(2500); Set_left_pwm(3500);  
	}
   else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==1 && D8==1)   //11111011
	{
//		sensor_bias = 20;
		Set_right_pwm(2000); Set_left_pwm(4000);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==0 && D8==1)   //11111101
	{
//		sensor_bias = 30;
		Set_right_pwm(1200); Set_left_pwm(4800); 
	}

	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==0 && D7==1 && D8==1)  //11110011
	{
//		sensor_bias = 15;
		Set_right_pwm(1800); Set_left_pwm(4300);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==0 && D8==1)  //11111001
	{
//		sensor_bias = 25;
		Set_right_pwm(1400); Set_left_pwm(4500);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==0 && D5==0 && D6==1 && D7==0 && D8==0)  //11111100
	{
//		sensor_bias = 35;
		Set_right_pwm(1200);  Set_left_pwm(4800);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==0)  //11111110
	{
//		sensor_bias = 45;
		Set_right_pwm(800); Set_left_pwm(5200);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //11111111
   {
//	   sensor_bias = 0;
		  last_statue=now_statue;
		  now_statue=0;
   }
	   if(D1==0|D2==0|D3==0|D4==0|D5==0|D6==0|D7==0|D8==0)
	 {
		    last_statue=now_statue;
		    now_statue=1;
	 }
	        
	 		    if(now_statue!=last_statue) 
	       {
			     change_flag1++;
           beep();	
		     }
				 
				   if(change_flag1==0)
			    {
				    turn_pid(10,0);
		      }	 
					
           if(change_flag1==2)
			    {
						check(177);
				    turn_pid(10,177); 
		      }	 
			     if(change_flag1==4)
			    {
			    	Set_left_pwm(0);
   		      Set_right_pwm(0);
				    change_flag1++;
			    }
}
void track3(void)
{
	  if(D1==1 && D2==1 && D3==1 && D4==0 && D5==0 && D6==1 && D7==1 && D8==1)     //11100111
	{
//		sensor_bias = 0;
		Set_right_pwm(3000); Set_left_pwm(3000); 
	}
	
	 else if(D1==1 && D2==1 && D3==0 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11001111
	{
//		sensor_bias = -15;
//    Set_right_pwm(3500); Set_left_pwm(2200); 
		Set_right_pwm(4200); Set_left_pwm(1400); 
	}
   else if(D1==1 && D2==0 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //10011111
	{
//		sensor_bias = -25;
		Set_right_pwm(4700); Set_left_pwm(1000); 
	}
	 else if(D1==0 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //00111111
	{
//		sensor_bias = -35; 
			if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(700);  Set_left_pwm(5400);  
		}
		 else
		 { Set_right_pwm(5400);  Set_left_pwm(700); } 
		             //5000               800
	}
	
	 else if(D1==0 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //01111111
	{
			if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(200);  Set_left_pwm(6200);  
		}
		 else
		 { Set_right_pwm(6200);  Set_left_pwm(200); } 
		              //5500              500
	 }
	 // danglu
	 	else if(D1==1 && D2==1 && D3==1 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11101111
	{
//		sensor_bias = -5;
		Set_right_pwm(3500); Set_left_pwm(2500); 
	}
   else if(D1==1 && D2==1 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //11011111
	{
//		sensor_bias = -20;
		Set_right_pwm(4500); Set_left_pwm(1000); 
	}
	 else if(D1==1 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //10111111
	{
     	 if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(200);  Set_left_pwm(6200);  
		}
		 else
		 { Set_right_pwm(6200);  Set_left_pwm(200); } 
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==1 && D7==1 && D8==1)  //11110111
	{    
//		sensor_bias = 10;
		 Set_right_pwm(1500); Set_left_pwm(4500);  
	}
   else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==1 && D8==1)   //11111011
	{
//		sensor_bias = 20;
		Set_right_pwm(800); Set_left_pwm(5200);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==0 && D8==1)   //11111101
	{
//		sensor_bias = 30;
			if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(6200);  Set_left_pwm(200);  
		}
		 else
		 { Set_right_pwm(200);  Set_left_pwm(6200);} 
	}

	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==0 && D7==1 && D8==1)  //11110011
	{
//		sensor_bias = 15;
		Set_right_pwm(1200); Set_left_pwm(4800);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==0 && D8==1)  //11111001
	{
//		sensor_bias = 25;
		Set_right_pwm(1000); Set_left_pwm(5000);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==0 && D8==0)  //11111100
	{
		if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(5800);  Set_left_pwm(600);  
		}
//		sensor_bias = 35;
		 else
		 { Set_right_pwm(600);  Set_left_pwm(5800);} 
	}
	
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==0)  //11111110
	{
//		sensor_bias = 45;
			if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(6200);  Set_left_pwm(200);  
		}
		 else
		 { Set_right_pwm(200);  Set_left_pwm(6200);} 
		  
	}
	
	  else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //11111111
   {
		  last_statue=now_statue;
		  now_statue=0;
   }
	 
	   if(D1==0|D2==0|D3==0|D4==0|D5==0|D6==0|D7==0|D8==0)
	 {
		    last_statue=now_statue;
		    now_statue=1;
	 }
	        
	 		    if(now_statue!=last_statue) 
	       {
			     change_flag1++;
           beep();	
		     }
/****************************************************************************************
				                          第一圈
*****************************************************************************************/
			 if(change_flag1==0) {turn_pid(10,-42);}	 
			 if(change_flag1==2){check(-137); turn_pid(10,-137);}	 
					
					  if(change_flag1==4)
			    {
			    	Set_left_pwm(0);
   		      Set_right_pwm(0);
				    change_flag1++;
			    }
}

void track4(void)
{
	  if(D1==1 && D2==1 && D3==1 && D4==0 && D5==0 && D6==1 && D7==1 && D8==1)     //11100111
	{
//		sensor_bias = 0;
		Set_right_pwm(3000); Set_left_pwm(3000); 
	}
	
	 else if(D1==1 && D2==1 && D3==0 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11001111
	{
//		sensor_bias = -15;
//    Set_right_pwm(3500); Set_left_pwm(2200); 
		Set_right_pwm(4200); Set_left_pwm(1400); 
	}
   else if(D1==1 && D2==0 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //10011111
	{
//		sensor_bias = -25;
		Set_right_pwm(4700); Set_left_pwm(1000); 
	}
	 else if(D1==0 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //00111111
	{
//		sensor_bias = -35; 
			if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(700);  Set_left_pwm(5400);  
		}
		 else
		 { Set_right_pwm(5400);  Set_left_pwm(700); } 
		             //5000               800
	}
	
	 else if(D1==0 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //01111111
	{
			if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(200);  Set_left_pwm(6200);  
		}
		 else
		 { Set_right_pwm(6200);  Set_left_pwm(200); } 
		              //5500              500
	 }
	 // danglu
	 	else if(D1==1 && D2==1 && D3==1 && D4==0 && D5==1 && D6==1 && D7==1 && D8==1) //11101111
	{
//		sensor_bias = -5;
		Set_right_pwm(3500); Set_left_pwm(2500); 
	}
   else if(D1==1 && D2==1 && D3==0 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //11011111
	{
//		sensor_bias = -20;
		Set_right_pwm(4500); Set_left_pwm(1000); 
	}
	 else if(D1==1 && D2==0 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)   //10111111
	{
     	 if(yaw_angle_int>-120&&yaw_angle_int<-90)
		{
			Set_right_pwm(200);  Set_left_pwm(6200);  
		}
		 else
		 { Set_right_pwm(6200);  Set_left_pwm(200); } 
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==1 && D7==1 && D8==1)  //11110111
	{    
//		sensor_bias = 10;
		 Set_right_pwm(1500); Set_left_pwm(4500);  
	}
   else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==1 && D8==1)   //11111011
	{
//		sensor_bias = 20;
		Set_right_pwm(800); Set_left_pwm(5200);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==0 && D8==1)   //11111101
	{
//		sensor_bias = 30;
			if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(6200);  Set_left_pwm(200);  
		}
		 else
		 { Set_right_pwm(200);  Set_left_pwm(6200);} 
	}

	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==0 && D6==0 && D7==1 && D8==1)  //11110011
	{
//		sensor_bias = 15;
		Set_right_pwm(1200); Set_left_pwm(4800);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==0 && D7==0 && D8==1)  //11111001
	{
//		sensor_bias = 25;
		Set_right_pwm(1000); Set_left_pwm(5000);  
	}
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==0 && D8==0)  //11111100
	{
		if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(4500);  Set_left_pwm(2000);  
		}
//		sensor_bias = 35;
		 else
		 { Set_right_pwm(600);  Set_left_pwm(5800);} 
	}
	
	 else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==0)  //11111110
	{
//		sensor_bias = 45;
			if(yaw_angle_int>-60&&yaw_angle_int<-20)
		{
			Set_right_pwm(4500);  Set_left_pwm(2000);  
		}
		 else
		 { Set_right_pwm(200);  Set_left_pwm(6200);} 
		  
	}
	
	  else if(D1==1 && D2==1 && D3==1 && D4==1 && D5==1 && D6==1 && D7==1 && D8==1)  //11111111
   {
		  last_statue=now_statue;
		  now_statue=0;
   }
	 
	   if(D1==0|D2==0|D3==0|D4==0|D5==0|D6==0|D7==0|D8==0)
	 {
		    last_statue=now_statue;
		    now_statue=1;
	 }
	        
	 		    if(now_statue!=last_statue) 
	       {
			     change_flag1++;
           beep();	
		     }
/****************************************************************************************
				                          第一圈
*****************************************************************************************/
			 if(change_flag1==0) {turn_pid(10,-42);}	 
			 if(change_flag1==2) {check(-138); turn_pid(10,-138);}	 
			 
/****************************************************************************************
				                          第二圈
*****************************************************************************************/			 
			     if(change_flag1==4) {turn_pid(10,-46);}
			                                  //45 
			 	   if(change_flag1==6) //****** 白多
					{
						check(-141);     
				    turn_pid(10,-141); //137 138
				  }
					
/****************************************************************************************
				                          第三圈
*****************************************************************************************/	
		 if(change_flag1==8)  { turn_pid(10,-47);  }  //-45
		 if(change_flag1==10) { check(-142); turn_pid(10,-142);}
					
					
/****************************************************************************************
				                          第四圈
*****************************************************************************************/						
         
           if(change_flag1==12)
			    {
          turn_pid(10,-50);  //-45
			    }
					 if(change_flag1==14)
					{
						check(-142); turn_pid(10,-142); 
					}
					
					  if(change_flag1==16)
			    {
			    	Set_left_pwm(0);
   		      Set_right_pwm(0);
				    change_flag1++;
			    }
}

