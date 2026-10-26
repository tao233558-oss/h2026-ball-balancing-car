/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sys.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
u8 Mode=0,BLUETOOTH_Mode=1,ELE_Line_Patrol_Mode=0,CCD_Line_Patrol_Mode=0,PS2_Mode=0,PS2_Flag=0,APP_RX,APP_Flag;      //小车的模式,蓝牙遥控、CCD巡线、ELE巡线
u8 Flag_Way=0,Flag_Show=0,Flag_Stop=1,Flag_Next;       //停止标志位和 显示标志位 默认停止 显示打开
int Encoder_Left,Encoder_Right;                        //左右编码器的脉冲计数
int Flag_Direction;  
float Velocity,Velocity_Set,Angle_Set,Turn;
int Motor_A,Motor_B,Servo,Target_A,Target_B;           //电机舵机控制相关           
int Voltage;                                           //电池电压采样相关的变量
float Show_Data_Mb;                                    //全局显示变量，用于显示需要查看的数据
u8 Delay_50,Delay_Flag; 							   //延时变量
float Velocity_KP=15,Velocity_KI=7;	       //速度控制PID参数
u16 ADV[128]={0};              
int PS2_LX,PS2_LY,PS2_RX,PS2_RY,PS2_KEY;
float Bluetooth_Velocity=30,PS2_Velocity=30,PS2_Turn_Velocity=0;                        //蓝牙遥控速度和APP接收的数据
u8 CCD_Zhongzhi,CCD_Yuzhi,PID_Send,Flash_Send;          //线性CCD FLASH相关
int Sensor_Left,Sensor_Middle,Sensor_Right,Sensor;      //电磁巡线相关
u16 PID_Parameter[10],Flash_Parameter[10];              //Flash相关数组 
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  JTAG_Set(JTAG_SWD_DISABLE);     //关闭JTAG接口
  delay_init(72);
  OLED_Init();
  while(select())	{	}	        //=====选择运行模式 
  delay_ms(300);
  if(CCD_Line_Patrol_Mode) CCD_Init();      //=====CCD初始化
  else if(ELE_Line_Patrol_Mode) ELE_Init(); //=====电磁传感器初始化
  else if(PS2_Mode){PS2_Init();PS2_SetInit(); }  
  Flash_Read();	                //=====读取PID参数
  // 清除定时器初始化过程中的更新中断标志，避免定时器一启动就中断	 
//  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); //开启引脚外部中断
   MX_TIM2_Init();;   		//=====如果是舵机小车初始化PWM100HZ驱动舵机和10ms中断
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      if(Flash_Send==1)         //写入PID参数到Flash,由app控制该指令
		{
            Flash_Write();	
			 Flash_Send=0;	
		}	
	  if(Flag_Show==0)         //使用MiniBalance APP和OLED显示屏
	  {
  			APP_Show();	
			oled_show();     //===显示屏打开
		    PS2_Read();
		}
	  else                     //使用MiniBalance上位机 上位机使用的时候需要严格的时序，故此时关闭app监控部分和OLED显示屏
	  {
	       DataScope();           //开启MiniBalance上位机
		}	
//	  Delay_Flag=1;	
//	  Delay_50=0;
//	  while(Delay_Flag);	     //上位机通过定时中断实现的50ms精准延时	
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
