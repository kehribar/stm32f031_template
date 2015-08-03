/*----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/

#include "stm32f0xx.h"
#include "digital.h"

/* blocking 'sendChar' method for usart1 */
void usart1_sendChar(const uint8_t ch);

/* basic initialisation for usart1 */
void usart1_init(const uint32_t baud);