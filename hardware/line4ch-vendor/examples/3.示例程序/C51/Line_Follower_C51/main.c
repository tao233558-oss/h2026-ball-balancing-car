#include <STC15.h>	//ºÊ»›STC11xxœµ¡–


typedef unsigned short	u16, U16, uint16, UINT16, WORD;
void DelayMs(uint16 ms)
{
	uint16 i,j;
	for(i=0;i<800;i++)
		for(j=0;j<ms;j++);
}

void InitUart1(void)
{
	SCON = 0x50;		
	AUXR |= 0x40;		
	AUXR &= 0xFE;		
	TMOD &= 0x0F;		
	TL1 = 0xC7;		
	TH1 = 0xFE;		
	ET1 = 0;		
	TR1 = 1;		
	ES = 1;			
	TI = 1;
}

void Uart1SendData(unsigned char dat) 
{
  SBUF = dat;             			
	while (!TI);
	TI = 0;                
}

// Function to print the level of GPIO
void printGPIOLevel(void) 
{
    Uart1SendData(P24 ? '1' : '0');
    Uart1SendData(P54 ? '1' : '0');
    Uart1SendData(P55 ? '1' : '0');
    Uart1SendData(P32 ? '1' : '0');
	
    Uart1SendData('\n');
}

int main(void)
{
    InitUart1();
    while(1)
    {
        printGPIOLevel(); // print GPIO level
				DelayMs(500);  // Delay for 500 ms
    }
    return 0;
}
