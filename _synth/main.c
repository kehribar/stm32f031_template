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
#include "soft_uart.h"
#include "systick_delay.h"
/*---------------------------------------------------------------------------*/
#define MAX_VOICE 14
#define BURST_SIZE 16
#define LOWPASS_ORDER 3
/*---------------------------------------------------------------------------*/
t_lfo outputLfo;
uint8_t midibuf[3];
int16_t g_cutoff = 0;
int16_t g_resonance = 0;
RingBuffer_t midi_ringBuf;
t_envSetting ampEnvSetting;
t_envSetting modEnvSetting;
int16_t g_modulationIndex = 1;
struct t_key voice[MAX_VOICE];
uint8_t midi_ringBufData[1024];
int32_t g_history[BURST_SIZE][LOWPASS_ORDER+1];
int32_t I2S_Buffer_Tx[BURST_SIZE * 2 * 2]; // 2 x 2channels worth of data
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
int main(void)
{ 
  int32_t* const Buffer_A = &(I2S_Buffer_Tx[0]);
  int32_t* const Buffer_B = &(I2S_Buffer_Tx[BURST_SIZE * 2]);

  hardware_init();
 
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

  if(env->state == 0) /* Idle */
  {        
    env->envelopeCounter = 0;   
  }    
  else if(env->state == 1) /* Attack */
  {    
    env->envelopeCounter += setting->attackRate;
    
    /* Check overflow! */
    if(env->envelopeCounter < setting->attackRate)
    {
      env->state = 2;                         
      env->envelopeCounter = 0xFFFF;
    }    
  }
  else if(env->state == 2) /* Decay */
  {    
    /* Detect sustain point */
    if(env->envelopeCounter < (setting->sustainLevel + setting->decayRate))
    {          
      env->state = 3;      
      env->envelopeCounter = setting->sustainLevel;
    }
    else
    {
      env->envelopeCounter -= setting->decayRate;     
    }
  }
  else if(env->state == 3) /* Sustain */
  {    
    /* Wait for note-off event ... */  
  }
  else if(env->state == 4) /* Release */
  {    
    /* Detect underflow ... */
    if(env->envelopeCounter < setting->releaseRate)
    {          
      env->state = 0;      
      env->envelopeCounter = 0;
    }
    else
    {
      env->envelopeCounter -= setting->releaseRate;     
    }
  }
  else /* What? */
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
      else
      {     
        // ...        
      }
    }    
    else
    {
      // ...
    }
  }  

  /* Scan voices */
  for(i=0;i<MAX_VOICE;++i)
  {         
    /* Key down */
    if((voice[i].noteState == NOTE_TRIGGER) && (voice[i].noteState_d != NOTE_TRIGGER))
    {
      voice[i].phaseCounterMod = 0;
      voice[i].phaseCounterTone = 0;      

      /* Attack state */
      voice[i].ampEnvelope.state = 1;      
      voice[i].modEnvelope.state = 1;      
      voice[i].ampEnvelope.envelopeCounter= 0;
      voice[i].modEnvelope.envelopeCounter = 0;

      voice[i].maxModulation = voice[i].keyVelocity >> 8;
    }
    /* Key up */
    else if((voice[i].noteState != NOTE_TRIGGER) && (voice[i].noteState_d == NOTE_TRIGGER))
    {
      /* Release state */
      voice[i].ampEnvelope.state = 4;      
      voice[i].modEnvelope.state = 4;      
    }

    voice[i].noteState_d = voice[i].noteState;
  }
}
/*---------------------------------------------------------------------------*/
static inline void synth_loop(int32_t* const buf)
{
  uint8_t i;
  uint8_t k;
  int32_t* buf_p = buf;    
  int32_t outputBuffer[BURST_SIZE];  

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
    int32_t left;
    int32_t input;
    int32_t right;
    int32_t inp_mod;
    int32_t lfoValue;          

    /* Run lowpass */
    for(k=0;k<LOWPASS_ORDER;k++) 
    {
      g_history[i][k] = S16S16MulShift8(g_history[i][k],g_cutoff) + S16S16MulShift8(g_history[i][k+1],255-g_cutoff);
    }
    g_history[i][LOWPASS_ORDER] = outputBuffer[i] - S16S16MulShift8(g_history[i][0],g_resonance);
    outputBuffer[i] = g_history[i][0];    

    input = outputBuffer[i];

    /* Psuedo stereo panning effect using an LFO with phase counters with slight offsets */
    outputLfo.phaseCounter_left += outputLfo.freq;
    outputLfo.phaseCounter_right = outputLfo.phaseCounter_left + outputLfo.stereoPanning_offset;

    /* Right channel output */
    lfoValue = (sin_lut[outputLfo.phaseCounter_right >> 6] + 32768) >> 8;
    inp_mod = S16S16MulShift8(input,lfoValue);      
    right = S16S16MulShift4(inp_mod,outputLfo.depth) + S16S16MulShift4(input,255-outputLfo.depth);

    /* Left channel output */
    lfoValue = (sin_lut[outputLfo.phaseCounter_left >> 6] + 32768) >> 8;
    inp_mod = S16S16MulShift8(input,lfoValue);
    left = S16S16MulShift4(inp_mod,outputLfo.depth) + S16S16MulShift4(input,255-outputLfo.depth);

    /* Put them on the DMA train */
    *buf_p++ = convertDataForDma_24b(left);
    *buf_p++ = convertDataForDma_24b(right);
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
  USART1->ICR = 0xFFFFFFFF;
  uint8_t data = usart1_readByte();
  if(!RingBuffer_IsFull(&midi_ringBuf))
  {
    RingBuffer_Insert(&midi_ringBuf,data);    
  }  
}
/*---------------------------------------------------------------------------*/
static void midi_controlMessageHandler(const uint8_t id, const uint8_t data)
{
  switch(id)
  {    
    case 61:
    {
      g_cutoff = data * 2;
      break;
    }
    case 62:
    {
      g_resonance = data * 2;
      break;
    }
    case 63:
    {
      ampEnvSetting.attackRate = data * 16;
      break;
    }
    case 102:
    {
      modEnvSetting.attackRate = data * 16;
      break;
    }
    case 103:
    {
      g_modulationIndex = data * 16;
      break;
    }
    default:
    {
      break;
    }
  }
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
        voice[k].freqMod = S16S16MulShift8(voice[k].freqTone, g_modulationIndex);
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
static void hardware_init()
{  
  uint8_t i;
  uint8_t k;

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
  digitalWrite(A,9,HIGH);

  pinMode(A,0,OUTPUT);
  digitalWrite(A,0,HIGH);

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

  for(i=0;i<BURST_SIZE;i++)
  {    
    for(k=0;k<LOWPASS_ORDER+1;k++) 
    {
      g_history[i][k] = 0;
    }
  }
  
  g_modulationIndex = 256;

  ampEnvSetting.attackRate = 2000;
  modEnvSetting.attackRate = 5000;

  ampEnvSetting.decayRate = 20;
  modEnvSetting.decayRate = 20;

  ampEnvSetting.sustainLevel = 4500;
  modEnvSetting.sustainLevel = 4500;

  ampEnvSetting.releaseRate = 50;
  modEnvSetting.releaseRate = 50;
    
  outputLfo.phaseCounter_left = 0;
  outputLfo.phaseCounter_right = 0;  
  outputLfo.stereoPanning_offset = 0x8000;
  
  outputLfo.freq = 3;
  outputLfo.depth = 150;

  xfprintf(dbg_sendChar,"> Hello World\r\n");
  RingBuffer_InitBuffer(&midi_ringBuf, midi_ringBufData, sizeof(midi_ringBufData));
}
/*---------------------------------------------------------------------------*/
