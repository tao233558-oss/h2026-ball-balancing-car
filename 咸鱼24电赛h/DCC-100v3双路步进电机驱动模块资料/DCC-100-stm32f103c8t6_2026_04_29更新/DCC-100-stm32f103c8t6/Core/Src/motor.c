#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "motor.h"

// 电机编号：0为左右，1为上下
int motor_ud_1_flag = 0; // 上下电机状态标志位，0表示停止，1表示运行
int motor_lr_0_flag = 0; // 左右电机状态标志位，0表示停止，1表示运行

// 每一个priod为10us
int motor_priod_counts[2] = {-1, -1}; // 电机速度数组，motor_speed[0]为左右电机速度，motor_speed[1]为上下电机速度
int motor_counters[2] = {0, 0}; // 电机计数器数组，motor_counters[0]为左右电机计数器，motor_counters[1]为上下电机计数器

void Motor_Init()
{
    // 设置PB3、PB4、PB5、PB6、PB7为高电平
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);

    // 设置PA8、PB15、PB14、PB13、PB12为高电平
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

    HAL_TIM_Base_Start_IT(&htim2);
}

void Motor_Start(int motor_number)
{
    if (motor_number == 1) // 上下电机
    {
        motor_ud_1_flag = 1;
    }
    else if (motor_number == 0) // 左右电机
    {
        motor_lr_0_flag = 1;
    }
}

void Motor_Stop(int motor_number)
{
    if (motor_number == 1) // 上下电机
    {
        motor_ud_1_flag = 0;
    }
    else if (motor_number == 0) // 左右电机
    {
        motor_lr_0_flag = 0;
    }
}

void Motor_SetDirection(int motor_number, int direction)
{
    if (motor_number == 1) // 上下电机
    {
        if (direction == 1)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
        }
    }
    else if (motor_number == 0) // 左右电机
    {
        if (direction == 1)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
        }
    }
}

// 这里的speed单位为 角度/s
void Motor_SetSpeed(int motor_number, int speed)
{
    if (motor_number == 1) // 上下电机
    {
        if (speed <= 0)
        {
            motor_priod_counts[1] = -1; // 停止
        }
        else
        {
            motor_priod_counts[1] = 1e5 / speed * 0.05625;

        }
    }
    else if (motor_number == 0) // 左右电机
    {
        if (speed <= 0)
        {
            motor_priod_counts[0] = -1; // 停止
        }
        else
        {
            motor_priod_counts[0] = 1e5 / speed * 0.05625;
        }
    }
}


int motor_remain_counts[2] = {-2, -2}; // 电机剩余转动定时器周期，motor_remain_angle[0]为左右电机，motor_remain_angle[1]为上下电机

// 控制电机转一个角度
void Motor_Turn_Angle(int motor_number, float angle)
{
    if (motor_number == 1)
    {
        Motor_Start(motor_number);
        motor_remain_counts[1] = angle / 0.05625 * 2;
    }
    else if (motor_number == 0)
    {
        Motor_Start(motor_number);
        motor_remain_counts[0] = angle / 0.05625 * 2;
    }
}



void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == (&htim2))
    {  
        if (motor_lr_0_flag == 1) // 左右电机运行
        {
            if (motor_priod_counts[0] > 0)
            {
                motor_counters[0]++;
                if (motor_counters[0] >= motor_priod_counts[0])
                {
                    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8); // 切换PWM引脚状态
                    motor_counters[0] = 0;
                    motor_remain_counts[0]--;
                    if (motor_remain_counts[0] == -1)
                    {
                        Motor_Stop(0); // 停止电机
                        motor_remain_counts[0]--;
                    }
                }
            }
        }
        if (motor_ud_1_flag == 1) // 上下电机运行
        {
            if (motor_priod_counts[1] > 0)
            {
                motor_counters[1]++;
                if (motor_counters[1] >= motor_priod_counts[1])
                {
                    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8); // 切换PWM引脚状态
                    motor_counters[1] = 0;
                    motor_remain_counts[1]--;
                    if (motor_remain_counts[1] == -1)
                    {
                        Motor_Stop(1); // 停止电机
                        motor_remain_counts[1]--;
                    }
                }
            }
        }
    }
}

