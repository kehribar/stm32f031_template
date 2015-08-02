/*----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/

#include "stm32f0xx.h"

#define SYSTICK_1MS SystemCoreClock/1000

/* Systick initalisation method */
int systick_init(const uint32_t arg);

/* Systick based 1ms resolution timer */
void _delay_ms(const uint32_t ms);

/* Uncalibrated delay method for small delays */
void _delay_nop(const uint32_t nopAmout);