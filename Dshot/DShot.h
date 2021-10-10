#include "Arduino.h"

#ifndef DShot_h
#define DShot_h

//#define DSHOT_PORT VPORTB.OUT
#define DSHOT_PORT VPORTF.OUT

class DShot{
  public:
    enum Mode {
      DSHOT300,
      DSHOT300BIDIR
    };
    DShot(const enum Mode mode);
    void attach(uint8_t pin);
    uint16_t setThrottle(uint16_t throttle);
  private:
    uint16_t _packet = 0;
    uint16_t _throttle = 0;
};

#endif
