/*----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/

#include "systick_delay.h"

/*---------------------------------------------------------------------------*/
static volatile uint32_t DelayCounter;

/*---------------------------------------------------------------------------*/
int systick_init(const uint32_t arg)
{
  return SysTick_Config(arg);
}

/*---------------------------------------------------------------------------*/
void _delay_nop(const uint32_t nopAmout)
{
  uint32_t i = nopAmout;

  while(i--)
  {
    __ASM("nop");
  }
}

/*---------------------------------------------------------------------------*/
void _delay_ms(const uint32_t ms)
{
  DelayCounter = ms;

  while(DelayCounter != 0);
}

/*---------------------------------------------------------------------------*/
void SysTick_Handler(void)
{  
  if(DelayCounter != 0x00)
  {
    DelayCounter--;
  }
}