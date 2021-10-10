#include "Arduino.h"

#ifndef DShot_h
#define DShot_h

//#define DSHOT_PORT VPORTB.OUT
#define DSHOT_PORT VPORTF.OUT


typedef struct command {
   uint16_t packet;
   uint16_t delay_ms;
};

typedef struct beep {
   uint16_t tonality;
   uint16_t delay_ms;
};

class DShot{
  public:
    enum Mode {
      DSHOT300,
      DSHOT300INV
    };
    DShot(const enum Mode mode);
    void attach(uint8_t pin);
    void setThrottle(uint16_t throttle);
    void singleBeep();
    void sequenceBeep(beep beeps[], int beeps_count);
  private:
    uint16_t _packet = 0;
    uint16_t _throttle = 0;
};

#endif
