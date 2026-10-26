#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#include <stdint.h>

typedef struct {
	void (*init)(void);
    void (*on)(void);
    void (*off)(void);
	void (*toggle)(void);
	void (*AddTask)(uint8_t cnt,uint16_t time);//添加蜂鸣器任务,入口参数为蜂鸣次数、蜂鸣时间间隔(ms)
}BuzzerInterface_t,*pBuzzerInterface_t;

extern BuzzerInterface_t UserBuzzer;

#endif

