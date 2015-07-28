
#include "stm32f0xx.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"

int main(void)
{
	volatile int i = 0;
	GPIO_InitTypeDef InitGpio;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

	// Configures the GPIOC pin8 and pin9, since leds are connected
	// to PC8 and PC9 of GPIOC
	InitGpio.GPIO_Pin = GPIO_Pin_5;
	InitGpio.GPIO_Mode = GPIO_Mode_OUT;
	InitGpio.GPIO_Speed = GPIO_Speed_Level_1;
	InitGpio.GPIO_OType = GPIO_OType_PP;
	InitGpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	// Initialises the GPIOC
	GPIO_Init(GPIOA, &InitGpio);

  while(1)
  {
		for(i=0;i<50000;i++);
		GPIO_ResetBits(GPIOA,GPIO_Pin_5);

		for(i=0;i<50000;i++);
		GPIO_SetBits(GPIOA,GPIO_Pin_5);

		for(i=0;i<50000;i++);
		GPIO_ResetBits(GPIOA,GPIO_Pin_5);

		for(i=0;i<500000;i++);
		GPIO_SetBits(GPIOA,GPIO_Pin_5);
  } 

  return 0;
}
