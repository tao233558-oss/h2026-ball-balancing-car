#include "headfile.h"
#include "_pid.h"
#include "key.h"

int left_pwm,right_pwm,left_angle_pwm,right_angle_pwm;
int base_left_pwm,base_right_pwm,angle_pwm,angle_pwm1;
int cur_state,last_state,pre_state;
unsigned int Temp[2] = { 0 };
int turn_pwm,cnt;
//int mode,set;

// KeyNum 保存本次按键值：KEY1=切换模式，KEY2=启动当前模式。
uint8_t KeyNum;
int display_left_encoder, display_right_encoder;
#define STEPPER_DIR_PORT GPIOA
#define STEPPER_DIR_PIN  DL_GPIO_PIN_14
#define STEPPER_DIR_IOMUX PA14
#define STEPPER_PWM_PORT GPIOA
#define STEPPER_PWM_PIN  DL_GPIO_PIN_18
#define STEPPER_PWM_IOMUX PA18

static void stepper_init(void)
{
    gpio_init(STEPPER_DIR_PORT, STEPPER_DIR_PIN, STEPPER_DIR_IOMUX, OUT);
    gpio_init(STEPPER_PWM_PORT, STEPPER_PWM_PIN, STEPPER_PWM_IOMUX, OUT);
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, 0);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0);
}

static void stepper_pulse(uint16_t steps, uint8_t dir)
{
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, dir);
    delay_ms(10);
    for(uint16_t i = 0; i < steps; i++)
    {
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 1);
        delay_us(5000);
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0);
        delay_us(5000);
    }
}

static void stepper_test_run(void)
{
    Set_left_pwm(0);
    Set_right_pwm(0);
    stepper_pulse(800, 0);
    delay_ms(1000);
    stepper_pulse(800, 1);
    delay_ms(1000);
}

int main(void)
{
		system_init();  // 系统初始化 系统频率80MHZ 必须加
		delay_ms(500);  // 延时0.5s 等待电源稳定
		OLED_Init();
	  imu_init();     // imu初始化 其实就是初始化串口 这里是串口1 
    key_init();

		motor_init();
		encoder_init();
	  uart0_init();
	  xunji_init();
//	track_init();
    buzzer_light_init(); 
    stepper_init();
		tim_interrupt_ms_init(TIMG8, 10, 1);   // 定时器中断初始化 中断内放电机PID控制程序
     
	  pid_init(&motorA,DELTA_PID,10,10,5);  
    pid_init(&motorB,DELTA_PID,10,10,5);
	  pid_init(&angle,POSITION_PID,0.04117,0,0.102);   //0.0405  0.1
		 
    while (1) 
	 {
			 if(imu_flag)
			{
				imu_flag=0;
				imu_analysis();  //获取角度值存放在  yaw_angle_int
			}
			
		            // 读取按键。
          // KEY1：切换 mode。
          // KEY2：把 set 置 1，开始执行当前 mode。
          KeyNum = key_GetNum();
		            // KEY1：切换模式。
          // 当前常用：mode 1=方形循迹调试，mode 2=H 题要求 1 直行 A->B。
          if (KeyNum == 1)			
		 {
		   mode++;
       if(mode==8)
			 {mode=1;}
       if(set==1)
       {set=0; }
       h_task_reset();				 
		 }
		            // KEY2：启动当前模式。
          // h_task_reset() 会清零计时，并记录当前陀螺仪 yaw 作为直行目标角。
          if (KeyNum == 2)			
		 {
			 h_task_reset();
             set=1;
		 }
			
			                        // OLED 调试显示：
                  // 第 1 行：Y 是陀螺仪偏航角，C 是收到的陀螺仪角度帧计数。
                  // 第 3 行：T 后面是 D1~D8 循迹传感器，白地=0，黑线=1。
                  // 第 4 行：mode 是当前模式，E 是循迹偏差。
                  if(mode!=7)
      {
OLED_ShowString(1, 1, "Y:");
OLED_ShowSignedNum(1, 3, yaw_angle_int,3);
OLED_ShowString(1, 8, "C:");
OLED_ShowNum(1, 10, imu_frame_count % 1000,3);
			OLED_ShowString(2, 1, "L4L3L2L1R1R2R3R4");
			OLED_ShowString(3, 1, "T:");
			OLED_ShowNum(3, 3, D1,1);
			OLED_ShowNum(3, 4, D2,1);
			OLED_ShowNum(3, 5, D3,1);
			OLED_ShowNum(3, 6, D4,1);
			OLED_ShowNum(3, 7, D5,1);
			OLED_ShowNum(3, 8, D6,1);
			OLED_ShowNum(3, 9, D7,1);
			OLED_ShowNum(3, 10, D8,1);
			OLED_ShowString(4, 1, "mode:");
			OLED_ShowNum(4, 6, mode,3);
			OLED_ShowString(4, 10, "E:");
			OLED_ShowSignedNum(4, 12, track_get_error(),3);
      }
      if(mode==7)
      {
          OLED_ShowString(1, 1, "K230 V3        ");
          OLED_ShowString(2, 1, "RX:             ");
          if(k230_state == 1)
          {
              OLED_ShowString(2, 4, "LEFT");
          }
          else if(k230_state == 2)
          {
              OLED_ShowString(2, 4, "CENTER");
          }
          else if(k230_state == 3)
          {
              OLED_ShowString(2, 4, "RIGHT");
          }
          else if(k230_state == 4)
          {
              OLED_ShowString(2, 4, "NO_LINE");
          }
          else if(k230_state == 5)
          {
              OLED_ShowString(2, 4, "UNKNOWN");
          }
          else
          {
              OLED_ShowString(2, 4, "WAIT");
          }
          OLED_ShowString(3, 1, "UART0 PA11 RX  ");
          OLED_ShowString(4, 1, "mode:007 CAM   ");
          if(k230_state == 1)
          {
              stepper_pulse(20, 0);
          }
          else if(k230_state == 3)
          {
              stepper_pulse(20, 1);
          }
      }
      if(mode==6&&set==1)
      {
          OLED_ShowString(1, 1, "STEPPER V2");
          OLED_ShowString(2, 1, "PA14 DIR");
          OLED_ShowString(3, 1, "PA18 PWM");
          OLED_ShowString(4, 1, "RUN TEST");
          stepper_test_run();
          set=0;
      }
			
			
   }
}

// 10ms 定时中断：真正让小车运动的代码在这里周期执行。
// set==1 才会跑对应模式；set==0 时只是显示数据，不让车动。
void TIMG8_IRQHandler()
{
     static uint16_t motor_test_cnt = 0;
	   if(DL_TimerG_getPendingInterrupt(TIMG8) == DL_TIMER_IIDX_LOAD)
	  {
        left_encoder=-read_encoder1();  
		    right_encoder=-read_encoder2();
        display_left_encoder += left_encoder;
        display_right_encoder += right_encoder;

        if(mode==5&&set==1)
       {
          if(motor_test_cnt < 200)
         {
            Set_left_pwm(4000);
            Set_right_pwm(0);
            motor_test_cnt++;
         }
          else if(motor_test_cnt < 300)
         {
            Set_left_pwm(0);
            Set_right_pwm(0);
            motor_test_cnt++;
         }
          else if(motor_test_cnt < 500)
         {
            Set_left_pwm(0);
            Set_right_pwm(4000);
            motor_test_cnt++;
         }
          else
         {
            Set_left_pwm(0);
            Set_right_pwm(0);
            motor_test_cnt = 0;
            set = 0;
         }
       }
        else
       {
          motor_test_cnt = 0;
       }

		                // mode 1：方形黑线循迹调试。
            if(mode==1&&set==1)
		   {
          track1();
		   }
			 	            // mode 2：2024 H 题要求 1，A 点直行到 B 点停车。
            if(mode==2&&set==1)
		   {
          h_req1_run();
		   }
			 	if(mode==3&&set==1)
		   {
          track3();
		   }
			 	if(mode==4&&set==1)
		   {
          track4();
		   }	
	  } 
}









