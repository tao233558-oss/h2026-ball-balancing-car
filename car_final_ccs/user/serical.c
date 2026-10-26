#include "headfile.h"
#include "k230_link.h"

uint8_t USART_RX_STA[22] = { 0 };    //接收状态标记	，字节数：1+3+16+2  
uint8_t Num = 0;              //接收数据的当前位置
volatile uint8_t k230_state = 0;       //0 NONE, 1 LEFT, 2 CENTER, 3 RIGHT, 4 NO_LINE
volatile uint8_t k230_rx_ready = 0;
volatile uint8_t k230_rx_len = 0;
char k230_rx_msg[16] = {0};
static uint8_t k230_rx_index = 0;


void Read_Data2(uint8_t addr, uint32_t *Data)	     
{	
	unsigned char y=0;
	unsigned int Receive_data = 0;       //数据缓存区

//	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
//	USART_SendData(USART3, 0x57);
	while( DL_UART_isBusy(UART0) == true );
	uart_sendbyte(UART0,0x57);
	
	while( DL_UART_isBusy(UART0) == true );
	uart_sendbyte(UART0,addr);

	
///////////////////////////偏移量数值///////////////////////////////	
	while(1)
  {		
		if(Num == 5)
		{
			Num = 0;
			if(USART_RX_STA[0x04] == 0x04)  //判断帧尾0x04,否者不赋值
			{
				Receive_data = USART_RX_STA[2];
				Receive_data <<= 8;
				Receive_data |= USART_RX_STA[3];
				
				*Data = USART_RX_STA[1];		//位置数据赋值	  
				*(Data+1) = Receive_data;			//偏移量赋值	  
				break;
			}        
			break;
		}
    else
    {
      delay_ms(1);y++;
      if(y==3) { Num = 0;break; }
    }		
	}
///////////////////////////偏移量数值///////////////////////////////	
}

void uart0_init(void)
{
	uart_init(UART0,115200,1);
}

uint8_t k230_get_vision_sample(aim_vision_sample_t *sample)
{
	return k230_link_get_snapshot(sample, 0);
}


void UART0_IRQHandler(void)
{
	if(DL_UART_getPendingInterrupt(UART0) == DL_UART_IIDX_RX)
	{
		uint8_t ch = uart_getbyte(UART0);
#if E2025_ENABLE_K230_UART0_BRIDGE
		/* Temporary compatibility path only; PA11 is shared with board CH340 RX. */
		(void) k230_link_push_byte(ch);
#else
		/* Default UART0 role: board CH340 debug and legacy ASCII commands. */
		if(ch == '\n' || ch == '\r')
		{
			if(k230_rx_index > 0)
			{
				k230_rx_msg[k230_rx_index] = '\0';
				k230_rx_len = k230_rx_index;
				k230_rx_ready = 1;

				if(k230_rx_msg[0] == 'L')
				{
					k230_state = 1;
				}
				else if(k230_rx_msg[0] == 'C')
				{
					k230_state = 2;
				}
				else if(k230_rx_msg[0] == 'R')
				{
					k230_state = 3;
				}
				else if(k230_rx_msg[0] == 'N')
				{
					k230_state = 4;
				}
				else
				{
					k230_state = 5;
				}
				k230_rx_index = 0;
			}
		}
		else if(ch >= 0x20U && ch <= 0x7EU)
		{
			if(k230_rx_index < (sizeof(k230_rx_msg) - 1))
			{
				k230_rx_msg[k230_rx_index++] = ch;
			}
			else
			{
				k230_rx_index = 0;
			}
		}
		else
		{
			/* Non-printable debug byte: restart the legacy ASCII buffer. */
			k230_rx_index = 0;
		}
#endif
	}
	NVIC_ClearPendingIRQ(UART0_INT_IRQn);
}



