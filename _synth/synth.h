/*-----------------------------------------------------------------------------
/
/
/------------------------------------------------------------------------------
/ <ihsan@kehribar.me>
/----------------------------------------------------------------------------*/
#include <stdint.h>

#define NOTE_SILENT 0 
#define NOTE_TRIGGER 1
#define NOTE_DECAY 2

typedef struct t_envelope {
  uint8_t state;
  uint16_t fallRate;
  uint16_t sustainCounter;
  uint16_t envelopeCounter;
} t_envelope;

typedef struct t_envSetting {
  uint16_t attackRate;
  uint16_t decayRate;
  uint16_t sustainRate;
  uint16_t releaseRate;
} t_envSetting;

typedef struct t_lfo {
  uint16_t phaseCounter;
  uint16_t freq;
  int32_t outSignal;
  uint16_t depth;
} t_lfo;

typedef struct t_key {
  uint8_t noteState;
  uint8_t noteState_d;
  uint8_t lastnote;
  uint16_t keyVelocity;
  uint16_t maxModulation;  
  uint16_t freqTone;
  uint16_t freqMod;  
  uint16_t phaseCounterTone;  
  uint16_t phaseCounterMod;     
  t_envelope modEnvelope;
  t_envelope ampEnvelope;
} t_key;
