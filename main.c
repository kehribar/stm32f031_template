/*----------------------------------------------------------------------------
/
/
/----------------------------------------------------------------------------*/
#include "stm32f0xx.h"
#include "stm32f0xx_rcc.h"
/*---------------------------------------------------------------------------*/
#include "digital.h"
#include "systick_delay.h"
/*---------------------------------------------------------------------------*/
static void hardware_init();
/*---------------------------------------------------------------------------*/
int main(void)
{  
  hardware_init();

  while(1)
  {
    if(digitalRead(A,4) == 0)
    {
      _delay_ms(50);
      digitalWrite(A,5,HIGH);

      _delay_ms(50);
      digitalWrite(A,5,LOW);
    }
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
static void hardware_init()
{
  systick_init(SYSTICK_1MS);

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

  pinMode(A,5,OUTPUT);
  digitalWrite(A,5,LOW);

  pinMode(A,4,INPUT);
  setInternalPullup(A,4);

  _delay_ms(10);
}
/*---------------------------------------------------------------------------*/
