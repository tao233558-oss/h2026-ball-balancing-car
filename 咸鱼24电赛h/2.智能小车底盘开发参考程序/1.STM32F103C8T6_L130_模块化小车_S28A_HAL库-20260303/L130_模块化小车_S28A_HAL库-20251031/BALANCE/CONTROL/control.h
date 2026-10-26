#ifndef __CONTROL_H
#define __CONTROL_H
#include "sys.h"
#include "tim.h"
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
//小车各模式定义
#define Pi 3.14159265
#define PWMA   TIM1->CCR1  
#define BIN2   PBout(12)
#define BIN1   PBout(13)
#define AIN2   PBout(15)
#define AIN1   PBout(14)
#define PWMB   TIM1->CCR4
#define SERVO   TIM2->CCR1  //舵机引脚

void Kinematic_Analysis(float velocity,float angle);
void Set_Pwm(int motor_a,int motor_b);
void Key(void);
void Xianfu_Pwm(void);
u8 Turn_Off( int voltage);
void Get_Angle(u8 way);
int myabs(int a);
int Incremental_PI_A (int Encoder,int Target);
int Incremental_PI_B (int Encoder,int Target);
void Get_RC(void);
void  Find_CCD_Zhongzhi(void);
int limit(int input,int min,int max);
#endif
