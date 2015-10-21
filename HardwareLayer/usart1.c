/*----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/

#include "usart1.h"

/*---------------------------------------------------------------------------*/
void usart1_sendChar(const uint8_t ch)
{
  while(USART_GetFlagStatus(USART1, USART_FLAG_TC) != SET);
  USART_SendData(USART1, ch);
}

/*---------------------------------------------------------------------------*/
uint8_t usart1_readByte()
{
  return (uint8_t)(USART1->RDR & (uint8_t)0x0FF);
}

/*---------------------------------------------------------------------------*/
void usart1_init(const uint32_t baud)
{
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);    
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
  
  /* TX pin */
  pinMode(A,2,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);

  /* RX pin */
  pinMode(A,3,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);
  
  USART_InitTypeDef USART_InitStructure;  
  USART_InitStructure.USART_BaudRate = baud;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;  
  USART_Init(USART1, &USART_InitStructure);    
  
  USART_Cmd(USART1, ENABLE);  
}

/*---------------------------------------------------------------------------*/
void usart1_enableReceiveISR()
{
  USART_ITConfig(USART1, USART_IT_RXNE, SET);
  NVIC_EnableIRQ(USART1_IRQn);
}
