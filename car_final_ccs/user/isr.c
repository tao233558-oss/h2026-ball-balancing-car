#include "headfile.h"
#include "k230_link.h"
#include "h2026_b_uart_diag.h"
#include "h2026_b_stepper_diag.h"

// 定时器中断

void TIMG0_IRQHandler()
{	
	if(DL_TimerG_getPendingInterrupt(TIMG0) == DL_TIMER_IIDX_LOAD)
	{
#if APP_PROFILE_H2026_B_STEPPER_DIAG
		h2026_b_stepper_diag_on_1ms_tick();
#endif
						// 此处编写中断函数
		

	}
}

void TIMG6_IRQHandler()
{
	if(DL_TimerG_getPendingInterrupt(TIMG6) == DL_TIMER_IIDX_LOAD)
	{
						// 此处编写中断函数

	}
}

void TIMG7_IRQHandler()
{
	if(DL_TimerG_getPendingInterrupt(TIMG7) == DL_TIMER_IIDX_LOAD)
	{
						// 此处编写中断函数
     gpio_set(GPIOB, DL_GPIO_PIN_12, 1);    // off
	   gpio_set(GPIOB, DL_GPIO_PIN_13, 0);    // off
		 DL_TimerG_disablePower(TIMG7);
	}
}


void TIMG12_IRQHandler()
{
	if(DL_TimerG_getPendingInterrupt(TIMG12) == DL_TIMER_IIDX_LOAD)
	{
						// 此处编写中断函数

	}
}


// 串口中断
//void UART0_IRQHandler(void)
//{
//	if(DL_UART_getPendingInterrupt(UART0) == DL_UART_IIDX_RX)
//	{
//						// 此处编写中断函数

//	}
// 
//}


void UART1_IRQHandler(void)
{
	if(DL_UART_getPendingInterrupt(UART1) == DL_UART_IIDX_RX)
	{
						// 此处编写中断函数
    imu_uart_callback();
	}
	NVIC_ClearPendingIRQ(UART1_INT_IRQn);
}


void UART2_IRQHandler(void)
{
	if(DL_UART_getPendingInterrupt(UART2) == DL_UART_IIDX_RX)
	{
		uint8_t byte = DL_UART_Main_receiveData(UART2);
#if E2025_ENABLE_K230_UART2
		(void) k230_link_push_byte(byte);
#else
		(void) byte;
#endif
	}
	NVIC_ClearPendingIRQ(UART2_INT_IRQn);
}


void UART3_IRQHandler(void)
{
	if(DL_UART_getPendingInterrupt(UART3) == DL_UART_IIDX_RX)
	{
		uint8_t byte = DL_UART_Main_receiveData(UART3);
#if APP_PROFILE_H2026_B_UART_DIAG
		h2026_b_uart_diag_on_uart3_byte(byte);
#else
		(void) byte;
#endif
	}
	NVIC_ClearPendingIRQ(UART3_INT_IRQn);
}


static void uart3_unused_handler_template(void)
{
	if(DL_UART_getPendingInterrupt(UART3) == DL_UART_IIDX_RX)
	{
						// 此处编写中断函数

	}
}



		
// 如果还需要其他外部中断引脚 可以按照下面的模板自行添加
//		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_0) // PA0外部中断
//		{
//        // 此处编写中断执行函数
		
//			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_0; // 清除中断标志位
//		}






