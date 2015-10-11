/*----------------------------------------------------------------------------
/
/
/
/----------------------------------------------------------------------------*/
#include "stm32f0xx.h"
#include "stm32f0xx_rcc.h"
#include "stm32f0xx_spi.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_dma.h"
#include "stm32f0xx_misc.h"
#include "stm32f0xx_usart.h"
/*---------------------------------------------------------------------------*/
#include "lut.h"
#include "usart1.h"
#include "digital.h"
#include "xprintf.h"
#include "systick_delay.h"
/*---------------------------------------------------------------------------*/
int32_t I2S_Buffer_Tx[128];
/*---------------------------------------------------------------------------*/
static void hardware_init();
static int32_t convertDataForDma_24b(const int32_t data);
/*---------------------------------------------------------------------------*/
int main(void)
{ 
  uint8_t i;
  int32_t data;
  uint16_t phaseCnt;
  uint16_t phaseInc;  
  int32_t* const Buffer_A = &(I2S_Buffer_Tx[0]);
  int32_t* const Buffer_B = &(I2S_Buffer_Tx[64]);

  phaseCnt = 0;
  phaseInc = 10000;

  hardware_init();  

  xprintf("> Hello World\r\n");

  while(1)
  { 
    uint32_t tmpreg = DMA1->ISR;    

    /* Buffer_A will be read after this point. Start to fill Buffer_B now! */
    if(tmpreg & DMA1_IT_TC3)
    {
      digitalWrite(A,0,HIGH);

      DMA1->IFCR |= DMA1_IT_TC3;

      for(i=0; i<64; i+=2)
      {       
        /* Generate data */
        phaseCnt += phaseInc;
        data = sin_lut[phaseCnt >> 7];

        /* Left ch */
        Buffer_B[i] = convertDataForDma_24b(data);
        
        /* Right ch */
        Buffer_B[i+1] = convertDataForDma_24b(data);
      }

      digitalWrite(A,0,LOW);
    }
    /* Buffer_B will be read after this point. Start to fill Buffer_A now! */
    else if(tmpreg & DMA1_IT_HT3)
    {
      digitalWrite(A,0,HIGH);

      DMA1->IFCR |= DMA1_IT_HT3;      

      for(i=0; i<64; i+=2)
      {
        /* Generate data */
        phaseCnt += phaseInc;
        data = sin_lut[phaseCnt >> 7];
        
        /* Left ch */
        Buffer_A[i] = convertDataForDma_24b(data); 
        
        /* Right ch */
        Buffer_A[i+1] = convertDataForDma_24b(data);
      }
      
      digitalWrite(A,0,LOW);
    }    
  }
  
  return 0;
}
/*---------------------------------------------------------------------------*/
static int32_t convertDataForDma_24b(const int32_t data)
{      
  uint32_t result;
  uint32_t shiftedData;
  shiftedData = data << 8;
  result = (shiftedData & 0xFFFF0000) >> 16;
  result |= (shiftedData & 0x0000FFFF) << 16;
  return (int32_t)result;  
}
/*---------------------------------------------------------------------------*/
static void hardware_init()
{  
  I2S_InitTypeDef I2S_InitStructure;
  DMA_InitTypeDef DMA_InitStructure;

  /* init systick and delay system */
  systick_init(SYSTICK_1MS);  

  /* init uart and enable xprintf library */
  usart1_init(115200);
  xdev_out(usart1_sendChar);

  /* Debug pins ... */
  pinMode(A,9,OUTPUT);
  digitalWrite(A,9,LOW);

  pinMode(A,0,OUTPUT);
  digitalWrite(A,0,LOW);

  /* I2S pin: Word select */
  pinMode(A,4,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_0);

  /* I2S pin: Bit clock */
  pinMode(A,5,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_0);
  
  /* I2S pin: Master clock */
  pinMode(A,6,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_0);
  
  /* I2S pin: Data out */
  pinMode(A,7,ALTFUNC);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_0);

  /* Enable the DMA1 and SPI1 clocks */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

  /* I2S peripheral configuration */
  I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;  
  I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
  I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_96k; // Divide this to 2 since we are using 24 bit I2S
  I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_32b;
  I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;  
  I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
  I2S_Init(SPI1, &I2S_InitStructure);

  /* DMA peripheral configuration */  
  DMA_InitStructure.DMA_BufferSize = 128 * 2;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;  
  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
  DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)I2S_Buffer_Tx;
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; 
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(SPI1->DR);
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  DMA_Init(DMA1_Channel3, &DMA_InitStructure);

  /* Enable the DMA channel Tx */
  DMA_Cmd(DMA1_Channel3, ENABLE);  

  /* Enable the I2S TX DMA request */
  SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

  /* Enable the SPI1 Master peripheral */
  I2S_Cmd(SPI1, ENABLE);     

  /* Small delay for general stabilisation */
  _delay_ms(100);
}
/*---------------------------------------------------------------------------*/