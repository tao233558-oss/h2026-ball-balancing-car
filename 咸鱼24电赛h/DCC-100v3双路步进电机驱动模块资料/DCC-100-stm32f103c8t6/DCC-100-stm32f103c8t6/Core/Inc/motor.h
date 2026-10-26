#ifndef MOTOR_H
#define MOTOR_H

// 接线:左右方向电机
    // PB3 -> PWM
    // PB4 -> DIR
    // PB5 -> DCY
    // PB6 -> SLP
    // PB7 -> RST
// 接线:上下方向电机
    // PA8 -> PWM
    // PB15 -> DIR
    // PB14 -> DCY
    // PB13 -> SLP
    // PB12 -> RST

void Motor_Init(void);
void Motor_SetSpeed(int motor_number, int speed);
void Motor_Start(int motor_number);
void Motor_Stop(int motor_number);
void Motor_SetDirection(int motor_number, int direction);
void Motor_Turn_Angle(int motor_number, float angle);


#endif
