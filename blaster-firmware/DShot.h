#include "Arduino.h"
#include <CircularBuffer.h>
#ifndef DShot_h
#define DShot_h


#if defined(ARDUINO_ARCH_MEGAAVR)
//#define DSHOT_PORT VPORTB.OUT
#define DSHOT_PORT VPORTF.OUT
#endif
#if defined(__AVR_ATmega328P__)
// For UNO, PortD 0-7: i.e. D0-D7
#define DSHOT_PORT PORTD
#endif




typedef struct command {
  uint16_t packet;
  uint16_t delay_ms;
};

typedef struct beep {
  uint16_t tonality;
  uint16_t delay_ms;
};

class DShot {
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
    CircularBuffer<command, 10> commands;
    command permanent_command;
    uint8_t dShotPins = 0;
    enum DShot::Mode dShotMode = DShot::Mode::DSHOT300INV;
    int delay_counter = 0;
};

#endif
