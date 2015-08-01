/*
* ----------------------------------------------------------------------------
* “THE COFFEEWARE LICENSE” (Revision 1):
* <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
* can do whatever you want with this stuff. If we meet some day, and you think
* this stuff is worth it, you can buy me a coffee in return.
* -----------------------------------------------------------------------------
*/

/* STM32F0XX spesific digital I/O macros */

#ifndef DIGITAL
#define DIGITAL

#include "stm32f0xx.h"

/* Pin mode */
#define INPUT 0
#define OUTPUT 1 
#define ALTFUNC 2
#define ANALOG 3

#define LOWSPEED 0
#define MIDSPEED 1
#define HIGHSPEED 3

#define AF0 0
#define AF1 1
#define AF2 2
#define AF3 3
#define AF4 4
#define AF5 5
#define AF6 6
#define AF7 7

#define HIGH 1
#define LOW 0

#define OUTPUT 1
#define INPUT 0

#define digitalRead(port,pin) (GPIO ##port ->IDR & (1<<pin))

#define digitalWrite(port,pin,state) state ? (GPIO ##port ->BSRR = (1<<pin)) : (GPIO ##port ->BRR = (1<<pin))

#define pinMode(port,pin,state) (GPIO ##port ->MODER &= ~(0x03 << (pin << 1))); (GPIO ##port ->MODER |= (state << (pin << 1))) 

#define pinSpeed(port,pin,state) (GPIO ##port ->MODER &= ~(0x03 << (pin << 1))); (GPIO ##port ->MODER |= (state << (pin << 1))) 

#define setInternalPullup(port,pin) (GPIO ##port ->PUPDR &= ~(0x03 << (pin << 1))); (GPIO ##port ->PUPDR |= (0x01 << (pin << 1))) 

#define setInternalPullDown(port,pin) (GPIO ##port ->PUPDR &= ~(0x03 << (pin << 1))); (GPIO ##port ->PUPDR |= (0x02 << (pin << 1))) 

#define setAlternativeFunction(port,pin,state) (pin < 8) ? ((GPIO ##port ->AFRL) &= ~(0x07 << (pin << 2))); (GPIO ##port ->AFRL) |= (state << (pin << 2)))) : \
                                                           ((GPIO ##port ->AFRH) &= ~(0x07 << ((pin-8) << 2))); (GPIO ##port ->AFRH) |= (state << ((pin-8) << 2))))

#endif