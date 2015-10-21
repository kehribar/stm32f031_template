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
#include "synth.h"
#include "math_func.h"
#include "usart1.h"
#include "digital.h"
#include "ringBuffer.h"
#include "xprintf.h"
#include "systick_delay.h"
/*---------------------------------------------------------------------------*/
#define MAX_VOICE 14
struct t_key voice[MAX_VOICE];
/*---------------------------------------------------------------------------*/
#define BURST_SIZE 16
int32_t I2S_Buffer_Tx[BURST_SIZE * 2 * 2]; // 2 x 2channels worth of data
/*---------------------------------------------------------------------------*/
uint8_t midibuf[3];
RingBuffer_t midi_ringBuf;
uint8_t midi_ringBufData[128];
/*---------------------------------------------------------------------------*/
static void hardware_init();
static inline void check_notes();
static inline void synth_loop(int32_t* const buf);
static void midi_noteOffMessageHandler(const uint8_t note);
static inline int32_t convertDataForDma_24b(const int32_t data);
static void midi_controlMessageHandler(const uint8_t id, const uint8_t data);
static void midi_noteOnMessageHandler(const uint8_t note,const uint8_t velocity);
static inline int32_t fm_iterate(t_key* key, const int16_t depth_mod, const uint16_t depth_amp);
static int16_t envelope_iterate(t_envelope* env, const t_envSetting* setting, const uint16_t maxVal);
/*---------------------------------------------------------------------------*/
t_lfo outputLfo;
t_envSetting ampEnvSetting;
t_envSetting modEnvSetting;
/*---------------------------------------------------------------------------*/
int main(void)
{ 
  uint8_t i;
  int32_t* const Buffer_A = &(I2S_Buffer_Tx[0]);
  int32_t* const Buffer_B = &(I2S_Buffer_Tx[BURST_SIZE * 2]);

  hardware_init();      

  xprintf("> Hello World\r\n");

  ampEnvSetting.attackRate = 10000;
  modEnvSetting.attackRate = 2048;
  
  ampEnvSetting.sustainRate = 500;
  modEnvSetting.sustainRate = 500;

  outputLfo.depth = 0;
  outputLfo.freq = 7;

  while(1)
  { 
    uint32_t tmpreg = DMA1->ISR;    

    /* Buffer_A will be read after this point. Start to fill Buffer_B now! */
    if(tmpreg & DMA1_IT_TC3)
    {
      digitalWrite(A,0,HIGH);
      DMA1->IFCR |= DMA1_IT_TC3;      

      synth_loop(Buffer_B);      

      digitalWrite(A,0,LOW);
    }
    /* Buffer_B will be read after this point. Start to fill Buffer_A now! */
    else if(tmpreg & DMA1_IT_HT3)
    {
      digitalWrite(A,0,HIGH);
      DMA1->IFCR |= DMA1_IT_HT3;      

      synth_loop(Buffer_A);      

      digitalWrite(A,0,LOW);
    }    
  }
  
  return 0;
}
/*---------------------------------------------------------------------------*/
static int16_t envelope_iterate(t_envelope* env, const t_envSetting* setting, const uint16_t maxVal)
{  
  int16_t output;

  if(env->state == 0) /* idle */
  {        
    env->envelopeCounter = 0;   
  }    
  else if(env->state == 1) /* attack */
  {    
    env->envelopeCounter += setting->attackRate;
    
    /* Check overflow! */
    if(env->envelopeCounter < setting->attackRate)
    {
      env->state = 2;                   
      env->sustainCounter = 0;
      env->envelopeCounter = 0xFFFF;
    }    
  }
  else if(env->state == 2) /* sustain */
  {    
    env->sustainCounter += setting->sustainRate;
    
    /* Detect overflow ... */
    if(env->sustainCounter < setting->sustainRate)
    {          
      env->state = 3;      
      env->envelopeCounter = 0xFFFF;
    }    
  }
  else if(env->state == 3) /* decay */
  {    
    /* Detect underflow ... */
    if(env->envelopeCounter < env->fallRate)
    {          
      env->state = 0;      
      env->envelopeCounter = 0;
    }
    else
    {
      env->envelopeCounter -= env->fallRate;     
    }
  }
  else
  {    
    env->state = 0;      
    env->envelopeCounter = 0;
  }

  output = S16S16MulShift16(env->envelopeCounter, maxVal);
 
  return output;  
}
/*---------------------------------------------------------------------------*/
static inline int32_t fm_iterate(t_key* key, const int16_t depth_mod, const uint16_t depth_amp)
{ 
  int16_t signal_mod;
  uint16_t signal_phase;
  
  key->phaseCounterMod += key->freqMod;
  key->phaseCounterTone += key->freqTone;  

  signal_mod = sin_lut[key->phaseCounterMod >> 6];
  signal_mod = S16S16MulShift8(signal_mod, depth_mod);

  signal_phase = key->phaseCounterTone;    
  signal_phase += signal_mod;  
  
  return S16S16MulShift16(sin_lut[signal_phase >> 6], depth_amp);    
}
/*---------------------------------------------------------------------------*/
static inline void check_notes()
{
  uint8_t i;
  uint8_t data;

  /* Analyse MIDI input stream */
  while(1)    
  {       
    __disable_irq();
      if(RingBuffer_GetCount(&midi_ringBuf) == 0) 
      {
        __enable_irq(); 
        break; 
      }    
      data = RingBuffer_Remove(&midi_ringBuf);
    __enable_irq();

    /* Sliding buffer ... */
    midibuf[0] = midibuf[1];
    midibuf[1] = midibuf[2];
    midibuf[2] = data;

    if((midibuf[1] < 128) && (midibuf[2] < 128))
    {      
      /* Note on */
      if((midibuf[0] & 0xF0) == 0x90)
      {
        /* If velocity is zero, treat it like 'note off' */
        if(midibuf[2] == 0x00)
        {
          midi_noteOffMessageHandler(midibuf[1]);
        }
        else
        {
          midi_noteOnMessageHandler(midibuf[1],midibuf[2]);
        }
      }
      /* Note off */
      else if((midibuf[0] & 0xF0) == 0x80)
      {
        midi_noteOffMessageHandler(midibuf[1]);
      }
      /* Control message */
      else if((midibuf[0] & 0xF0) == 0xB0)
      {
        midi_controlMessageHandler(midibuf[1],midibuf[2]);            
      }
    }
  } 

  /* scan voices */
  for(i=0;i<MAX_VOICE;++i)
  {         
    /* key down */
    if((voice[i].noteState == NOTE_TRIGGER) && (voice[i].noteState_d != NOTE_TRIGGER))
    {
      voice[i].phaseCounterMod = 0;
      voice[i].phaseCounterTone = 0;

      voice[i].maxModulation = voice[i].keyVelocity >> 9;

      voice[i].ampEnvelope.state = 1;
      voice[i].ampEnvelope.fallRate = 10;
      voice[i].ampEnvelope.envelopeCounter= 0;

      voice[i].modEnvelope.state = 1;
      voice[i].modEnvelope.fallRate = 20;
      voice[i].modEnvelope.envelopeCounter = 0;
    }
    /* key up */
    else if((voice[i].noteState != NOTE_TRIGGER) && (voice[i].noteState_d == NOTE_TRIGGER))
    {
      voice[i].ampEnvelope.state = 3;      
      voice[i].ampEnvelope.fallRate = 50;

      voice[i].modEnvelope.state = 3;      
      voice[i].modEnvelope.fallRate = 100;
    }

    voice[i].noteState_d = voice[i].noteState;
  }
}
/*---------------------------------------------------------------------------*/
static inline void synth_loop(int32_t* const buf)
{
  uint8_t i;
  uint8_t k;    
  int32_t* buf_p;
  int32_t* out_p;
  int32_t outputBuffer[BURST_SIZE];

  buf_p = buf;
  out_p = outputBuffer;

  check_notes();

  /* Deal with the initial voice first */
  {          
    int16_t mod_amp = envelope_iterate(&(voice[0].modEnvelope),&modEnvSetting,voice[0].maxModulation);  
    int16_t sig_amp = envelope_iterate(&(voice[0].ampEnvelope),&ampEnvSetting,voice[0].keyVelocity);  

    if(sig_amp == 0)
    {
      voice[0].lastnote = 0;
      voice[0].noteState = NOTE_SILENT;
    } 
    
    for(i=0;i<BURST_SIZE;i++)
    {              
      outputBuffer[i] = fm_iterate(&(voice[0]),mod_amp,sig_amp);
    }
  }
  
  /* Scan through other voices */
  for(k=1;k<MAX_VOICE;k++) 
  {          
    int16_t mod_amp = envelope_iterate(&(voice[k].modEnvelope),&modEnvSetting,voice[k].maxModulation);  
    int16_t sig_amp = envelope_iterate(&(voice[k].ampEnvelope),&ampEnvSetting,voice[k].keyVelocity);    

    if(sig_amp == 0)
    {
      voice[k].lastnote = 0;
      voice[k].noteState = NOTE_SILENT;
    }    

    for(i=0;i<BURST_SIZE;i++)
    {      
      outputBuffer[i] += fm_iterate(&(voice[k]),mod_amp,sig_amp);
    }
  }   

  /* Put the values inside the DMA buffer for later */
  for(i=0;i<BURST_SIZE;i++)
  {
    // TODO: put LFO here
    // TODO: put lowpass here

    *buf_p++ = convertDataForDma_24b(*out_p * 32); // left
    *buf_p++ = convertDataForDma_24b(*out_p++ * 32); // right
  } 
}
/*---------------------------------------------------------------------------*/
static inline int32_t convertDataForDma_24b(const int32_t data)
{      
  uint32_t result;
  uint32_t shiftedData;
  shiftedData = data << 8;
  result = (shiftedData & 0xFFFF0000) >> 16;
  result |= (shiftedData & 0x0000FFFF) << 16;
  return (int32_t)result;  
}
/*---------------------------------------------------------------------------*/
void USART1_IRQHandler()
{
  uint8_t data = usart1_readByte();
  if(!RingBuffer_IsFull(&midi_ringBuf))
  {
    RingBuffer_Insert(&midi_ringBuf,data);
  }
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
  usart1_enableReceiveISR();

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
  DMA_InitStructure.DMA_BufferSize = BURST_SIZE * 2 * 2 * 2;
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

  RingBuffer_InitBuffer(&midi_ringBuf, midi_ringBufData, sizeof(midi_ringBufData));
}
/*---------------------------------------------------------------------------*/
static void midi_controlMessageHandler(const uint8_t id, const uint8_t data)
{
  // ... 
}
/*---------------------------------------------------------------------------*/
static void midi_noteOnMessageHandler(const uint8_t note,const uint8_t velocity)
{
  /* Lokup table has limited range */
  if((note > 20) && (note < 109))
  {
    uint8_t k;

    /* Search for an silent empty slot */
    for (k = 0; k < MAX_VOICE; ++k)
    {
      if((voice[k].lastnote == 0) && (voice[k].noteState == NOTE_SILENT))
      {               
        voice[k].freqTone = noteToFreq[note - 21];
        voice[k].freqMod = ((voice[k].freqTone) * 1) + 2;
        voice[k].lastnote = note;
        voice[k].keyVelocity = velocity * 512;
        voice[k].noteState = NOTE_TRIGGER; 
        return;         
      }
    }
  }  
}
/*---------------------------------------------------------------------------*/
static void midi_noteOffMessageHandler(const uint8_t note)
{
  uint8_t k;

  /* Search for previously triggered key */
  for (k = 0; k < MAX_VOICE; ++k)
  {
    if(voice[k].lastnote == note)
    {
      voice[k].lastnote = 0;
      voice[k].noteState = NOTE_DECAY;             
      break;
    }
  }          
}
/*---------------------------------------------------------------------------*/