/*-----------------------------------------------------------------------------
/
/
/----------------------------------------------------------------------------*/
#include <stdint.h>
#include "systick_delay.h"

#define uart_set_pin() digitalWrite(A,9,HIGH)
#define uart_clr_pin() digitalWrite(A,9,LOW)
#define uart_bit_dly() _delay_nop(77)

/* Sends 115200 baud rate UART messages with 48Mhz system clock */
static void dbg_sendChar(uint8_t tx)
{
  uint8_t i;

  /* Start condition */
  uart_clr_pin();
  uart_bit_dly();

  for(i=0;i<8;i++)
  {
    if(tx & 0x01)
    {
      uart_set_pin();
    }
    else
    {
      uart_clr_pin();
    }        
    uart_bit_dly();
    tx = tx >> 1;
  }

  /* Stop condition */
  uart_set_pin();
  uart_bit_dly();

  /* Extra delay ... */
  uart_bit_dly();
  uart_bit_dly();
}