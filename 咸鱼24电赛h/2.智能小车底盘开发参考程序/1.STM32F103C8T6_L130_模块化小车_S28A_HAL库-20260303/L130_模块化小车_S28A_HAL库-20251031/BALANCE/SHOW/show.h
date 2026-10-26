#ifndef __SHOW_H
#define __SHOW_H
#include "sys.h"
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
void oled_show(void);
void APP_Show(void);
void DataScope(void);
void OLED_DrawPoint_Shu(u8 x,u8 y,u8 t);
void OLED_Show_CCD(void);
void oled_show_once(void);
#endif
