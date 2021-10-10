#include "Arduino.h"
#include "DShot.h"


// Each item contains the bit of the port
// Pins that are not attached will always be 1
// This is to offload the Off pattern calculation during bit send
static uint8_t dShotBits[16];

// Denote which pin is attached to dShot
static uint8_t dShotPins = 0;

static command commands[10];
static uint8_t command_index=0;

#define DSHOT_BEEP_DELAY_US (260)

#define NOP  "NOP\n"
#define NOP2 NOP NOP
#define NOP4 NOP2 NOP2
#define NOP8 NOP4 NOP4
#define NOP16 NOP8 NOP8

int delay_command = 0;
int delay_counter = 0;
int repete=0;
int repete_counter=0;

// Mode: 600/300/150
static enum DShot::Mode dShotMode = DShot::Mode::DSHOT300INV;
 

    /*
      DSHOT600 implementation
      For 20MHz CPU,
      0: 12 cycle ON, 21 cycle OFF   -> 600ns ON, 1050ns OFF     (target: 618ns  / 1052ns) 
      1: 25 cycle ON, 8 cycle OFF    -> 1250ns ON, 400ns OFF     (target: 1252ns / 418ns) 
      Total 27 cycle each bit        -> 1650 ns                  (target: 1670ns)       
    */

     /*
      DSHOT300 implementation
      For 20MHz CPU,
      0: 25 cycle ON, 42 cycle OFF   -> 1.25us ON, 2.1us  OFF     (target: 1.25us / 2.08us) 
      1: 50 cycle ON, 17 cycle OFF   -> 2.5us  ON, 0.85ns OFF     (target: 2.5ns / 0.83ns) 
      Total 67 cycle each bit        -> 3.33us                    (target: 3.35us )       
    */ 

    /*
      DSHOT600 implementation
      For 16MHz CPU,
      0: 10 cycle ON, 17 cycle OFF   -> 625ns ON, 1'062.5ns OFF  (target: 618ns  / 10452ns) 
      1: 20 cycle ON, 7 cycle OFF    -> 1250ns ON, 437.5ns OFF   (target: 1250ns / 418ns) 
      Total 27 cycle each bit        -> 1687.5 ns                (target: 1670ns)       
    */

static inline void setPortBits(uint16_t packet){
    uint16_t mask = 0x8000; //1000000000000000
   for (byte i=0; i<16; i++){
      bool isHigh = (packet & mask);
      
      switch (dShotMode) {
          case DShot::Mode::DSHOT300INV:
          isHigh = !isHigh;
          break;
      }
      
      if (isHigh)
        dShotBits[i] |= dShotPins;  //set to 1
      else
        dShotBits[i] &= ~(dShotPins); //set to 0
        
      mask >>= 1;
  }
}
    

static inline void sendData(){

  if(delay_counter>0){
      delay_counter--;
      
      return;
   }
   struct command c = commands[command_index];
   if(command_index>0){
    Serial.print(command_index);
    Serial.print("--");
    Serial.println(c.delay_ms);
    command_index--;
   }
    
    
   setPortBits(c.packet);
  
  switch (dShotMode) {
  /*
      DSHOT300 implementation
      For 20MHz CPU,
      0: 25 cycle ON, 42 cycle OFF   -> 1.25us ON, 2.1us  OFF     (target: 1.25us / 2.08us) 
      1: 50 cycle ON, 17 cycle OFF   -> 2.5us  ON, 0.85ns OFF     (target: 2.5ns / 0.83ns) 
      Total 67 cycle each bit        -> 3.33us                    (target: 3.35us )       
  */ 
      
  case DShot::Mode::DSHOT300INV:
    asm(
    // For i = 0 to 15:
    "LDI  r23,  0\n"
    // Set LOW for every attached pins
    // DSHOT_PORT &= ~dShotPins;
    "IN r25,  %0\n"
    "_for_loop_1:\n"
    "AND r25,  %2\n"  // 1 cycle
    "OUT  %0, r25\n"  // 1 cycle
    //--- start frame 
    // Wait 21 cyles - what need to be computed to change the signal
    // 21 = 25 - 4

    NOP16
    NOP4
    NOP

    // Set HIGH for high bits only
    //DSHOT_PORT |= dShotBits[i];
    "LD r24,  Z+\n"              // 2 cycle (1 to load, 1 for post increment) 
    "OR  r25,  r24\n"          // 1 cycle
    "OUT  %0, r25\n"             // 1 cycle
 
    // Wait 23 cycles - what need to be computed to change the signal
    // 23 = 25 - 4

    NOP16
    NOP4
    NOP2
    NOP
   
    // set HIGH dShotPins everything
    // DSHOT_PORT |= dShotPins;
    "OR  r25,  %1\n"            // 1 cycle
    "OUT  %0, r25\n"            // 1 cycle

    // Wait 10 cycles - what need to be computed to change the signal
    // 10 = 17 - 7
    NOP8
    NOP2
    
    // Add to i (tmp_reg)
    "INC  r23\n"               // 1 cycle
    "CPI  r23,  16\n"          // 1 cycle
    "BRLO _for_loop_1\n"       // 2 cycle

    :
    : "I" (_SFR_IO_ADDR(DSHOT_PORT)), "r" (dShotPins), "r" (~dShotPins), "z" (dShotBits)
    : "r25", "r24", "r23"
    );
    break;
    case DShot::Mode::DSHOT300:
    asm(
    // For i = 0 to 15:
    "LDI  r23,  0\n"
    // Set LOW for every attached pins
    // DSHOT_PORT &= ~dShotPins;
    "IN r25,  %0\n"
    "_for_loop_2:\n"
    "OR r25,  %1\n"  // 1 cycle
    "OUT  %0, r25\n"  // 1 cycle
    //--- start frame 
    // Wait 21 cyles - what need to be computed to change the signal
    // 21 = 25 - 4

    NOP16
    NOP4
    NOP

    // Set HIGH for high bits only
    //DSHOT_PORT |= dShotBits[i];
    "LD r24,  Z+\n"              // 2 cycle (1 to load, 1 for post increment) 
    "AND  r25,  r24\n"          // 1 cycle
    "OUT  %0, r25\n"             // 1 cycle
 
    // Wait 23 cycles - what need to be computed to change the signal
    // 23 = 25 - 4

    NOP16
    NOP4
    NOP2
    NOP
   
    // set HIGH dShotPins everything
    // DSHOT_PORT |= dShotPins;
    "AND  r25,  %2\n"            // 1 cycle
    "OUT  %0, r25\n"            // 1 cycle

    // Wait 10 cycles - what need to be computed to change the signal
    // 10 = 17 - 7
    NOP8
    NOP2
    
    // Add to i (tmp_reg)
    "INC  r23\n"               // 1 cycle
    "CPI  r23,  16\n"          // 1 cycle
    "BRLO _for_loop_2\n"       // 2 cycle

    :
    : "I" (_SFR_IO_ADDR(DSHOT_PORT)), "r" (dShotPins), "r" (~dShotPins), "z" (dShotBits)
    : "r25", "r24", "r23"
    );
    break;
  }
 
  if(c.delay_ms>0){
    delay_counter = c.delay_ms;
  }
}


static boolean timerActive = false;

static void initISR(){
  unsigned int per_value;
  cli(); // stop interrupts

  //per_value = 0x320;                         // Value required for 200Hz 0xC34 (800) with prescalar at 64 at 20MHz
  //per_value = 0x270;                         // Value required for 500Hz 0x270 (624) with prescalar at 64 at 20MHz
  //per_value = 0x1F3;                         // Value required for 500Hz 0x1F3 (499) with prescalar at 64 at 16Mhz
  //per_value = 0xF9;                        // Value required for 1kHz 0xF9 (249) with prescalar at 64
  per_value = 0x137;                        // Value required for 1kHz 0xF9 (311) with prescalar at 64 at 20Mhz
  //per_value = 0x31;                        // Value required for 5kHz 0x31 (49) with prescalar at 64
  //per_value = 0xC;                        // Value required for 20kHz 0xC (12) with prescalar at 64
  //per_value = 0x9B;                             // Value required for 2kHz 0x9B (155) with prescalar at 64  at 20MHz

  
  TCA0.SINGLE.PER = per_value;                // Set period register
  TCA0.SINGLE.CMP1 = per_value;               // Set compare channel match value
  TCA0.SINGLE.INTCTRL |= bit(5);              // Enable channel 1 compare match interrupt.
                                              // Use bit(4) for CMP0, bit(5) CMP1, bit(6) CMP2

  timerActive = true;
  for (byte i=0; i<16; i++){
    dShotBits[i] = 0;
  }
  dShotPins = 0;

  sei(); // allow interrupts
}

static boolean isTimerActive(){
  return timerActive;
}

ISR(TCA0_CMP1_vect){
   noInterrupts(); // stop interrupts
   sendData();
   TCA0.SINGLE.INTFLAGS |= bit(5);
   interrupts(); // allow interrupts
}

/*
  Prepare data packet, attach 0 to telemetry bit, and calculate CRC
  throttle: 11-bit data
*/
static inline uint16_t createPacket(uint16_t throttle, bool telemetry){

    throttle = (throttle << 1) | (telemetry ? 1 : 0);


    // compute checksum
    unsigned csum = 0;
    unsigned csum_data = throttle;
    for (int i = 0; i < 3; i++) {
        csum ^=  csum_data;   // xor data by nibbles
        csum_data >>= 4;
    }

     
  switch (dShotMode) {
    case DShot::Mode::DSHOT300INV:
        csum = ~csum;
    break;
  }

    csum &= 0xf;
    throttle = (throttle << 4) | csum;

    return throttle;
  
}

/****************** end of static functions *******************/

DShot::DShot(const enum Mode mode){
    dShotMode = mode;
}

void DShot::attach(uint8_t pin){
  this->_packet = 0;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH); //to disable PWM
  if (!isTimerActive()){
    initISR();
  }
  dShotPins |= digitalPinToBitMask(pin);
}



/*
  Set the throttle value and prepare the data packet and store
  throttle: 11-bit data
*/
void DShot::setThrottle(uint16_t throttle){
   
   bool telemetry = false;
   uint16_t delay_ms = 0;
   if(throttle > 0 && throttle <48){  
       telemetry = true;  //Some dShot commands need telemetry set to 1
   }

  struct command c = {createPacket(throttle, telemetry), delay_ms};

  command_index=0;
  commands[command_index]=c;
}

void DShot::singleBeep(){


   bool telemetry = true;
   uint16_t delay_ms = DSHOT_BEEP_DELAY_US;

  struct command c = {createPacket(1, telemetry), delay_ms};

  command_index=1;
  commands[command_index]=c;
}

void DShot::sequenceBeep(beep beeps[], int beeps_count){
   noInterrupts(); // stop interrupts
   bool telemetry = true;
   command_index=0;
   for (int i = 0; i < beeps_count ; i++) {
       command_index++;
       beep b =  beeps[i];
       Serial.print(b.tonality);
       Serial.print("--");
       Serial.println( b.delay_ms);
       commands[command_index]={createPacket(b.tonality, telemetry), b.delay_ms};
       
   }
   Serial.println("...");
  interrupts(); // allow interrupts
}



//beacon

//dshotCommandWrite(ALL_MOTORS, getMotorCount(), beeperConfig()->dshotBeaconTone, DSHOT_CMD_TYPE_INLINE);



//Source: https://forum.arduino.cc/t/arduino-nano-every-setting-up-timer-interrupt-isr/639882/10

// Arduino Nano Every Timer/Counter A example
// =========================================================
// There is one type A timer/counter (TCA0) and it has three
// compare channels (CMP0, CMP1, CMP2).
//
// The clock prescaler (bits 3:1 of CTRLA register) is set to
// 64 by default and changing it breaks delay() and
// millis() (and maybe other things, not checked):
// increasing/decreasing it increases/decreases the length
// of "a second" proportionally. It also changes the PWM
// frequency of pins 5, 9, and 10, although they will still
// do PWM (increasing/decreasing the prescaler value
// decreases/increases the frequency proportionally).
//
// The period value (PER register) is set to 255 by default.
// Changing it affects the duty cycle of PWM pins (i.e. the
// range 0-255 in analogWrite no longer corresponds to a
// 0-100% duty cycle).
//
// PWM pin 5 uses compare channel 2 (CMP2), pin 9 channel
// 0 (CMP0), and pin 10 channel 1 (CMP1).
//
// If you don't mind losing PWM capability (or at least
// changing the frequency and messing up the duty cycle)
// on pins 5, 9, or 10, you can set up these channels for
// your own use.
//
// If you don't mind messing up delay() and millis(), you
// can change the prescaler value to suit your needs. In
// principle, since the prescaler affects millis() and
// delay() in an inversely proportional manner, this could
// be compensated for in your sketch, but this is getting
// messy.
//
// If the prescaler is not changed, the lowest interrupt
// frequency obtainable (by setting PER and CMPn to 65535)
// is about 3.8 Hz.
//
// The interrupt frequency (Hz) given a prescaler value
// and PER value is:
// f = 16000000 / (PER + 1) / prescaler
//
// The PER value for a required frequency f is:
// PER = (16000000 / prescaler / f) - 1
//
// The prescaler can take a setting of 1, 2, 4, 8,
// 16, 64, 256, or 1024. See the ATmega 4809 datasheet
// for the corresponding bit patterns in CTRLA.
//
// http://ww1.microchip.com/downloads/en/DeviceDoc/megaAVR-0-series-Family-Data-Sheet-40002015C.pdf
//
// The program below demonstrates TCA0 interrupts on
// compare channel 1, with and without changing the
// prescaler (change the value of CHANGE_PRESCALER
// from 0 to 1).
//
// PER is the TOP value for the counter. When it
// reaches this value it is reset back to 0. The
// CMP1 value is set to the same value, so that an
// interrupt is generated at the same time that the
// timer is reset back to 0. If you use more than
// one compare channels at the same time, all CMPn
// registers must have a value smaller or equal to PER or
// else they will not generate an interrupt. The
// smallest obtainable frequency is governed by the
// value of PER.
//
// With CHANGE_PRESCALER left at 0, the prescale
// factor is left at its default value of 64, and the
// value of PER is set to the maximum (65535). The
// interrupts fire approx 3.8 times per second while
// the main program prints text to the terminal
// exactly once per second using millis().
//
// When CHANGE_PRESCALER is changed to 1, the prescaler
// factor is changed to 256. The value of PER is calculated
// so that the interrupts will fire exactly once per
// second. The main program still uses millis() to
// print text to the terminal once every 1000 milliseconds
// but now a "second" takes four seconds. This could be
// compensated for by waiting for only 250 milliseconds.
//
// Change CMP1 to CMP0 or CMP2 as required. Change indicated
// bit values too.

/*
#define CHANGE_PRESCALER    0
volatile bool i = false;

void setup() {
    Serial.begin(115200);
    unsigned int per_value;
    cli();
#   if CHANGE_PRESCALER == 0                        // Use default 64 prescale factor
        per_value = 0xFFFF;                         // Use maximum possible PER/CMP value (65535)
        TCA0.SINGLE.PER = per_value;                // Set period register
        TCA0.SINGLE.CMP1 = per_value;               // Set compare channel match value
        TCA0.SINGLE.INTCTRL |= bit(5);              // Enable channel 1 compare match interrupt.
                                                    // Use bit(4) for CMP0, bit(5) CMP1, bit(6) CMP2
#   elif CHANGE_PRESCALER == 1                      // Change prescale factor
        TCA0.SINGLE.CTRLA = B00001101;              // Prescaler set to 256. Use the following for
                                                    // other prescalers:
                                                    // B00000001    1
                                                    // B00000011    2
                                                    // B00000101    4
                                                    // B00000111    8
                                                    // B00001001    16
                                                    // B00001011    64
                                                    // B00001101    256
                                                    // B00001111    1024
        per_value = 0xF423;                         // Value required for 1 Hz (62499)
        TCA0.SINGLE.PER = per_value;                // Set period register
        TCA0.SINGLE.CMP1 = per_value;               // Set compare channel match value
        TCA0.SINGLE.INTCTRL |= bit(5);              // Enable channel 1 compare match interrupt.
                                                    // Use bit(4) for CMP0, bit(5) CMP1, bit(6) CMP2
#   endif
    sei();
}

void loop() {
    unsigned long t = millis();
    int alive_count = 0;
    while(true) {
        if (i) {
            i = false;
            Serial.println("Interrupt!");
        }
        if (millis() - t >= 1000) {
            Serial.print("Still alive ");
            Serial.println(alive_count++);
            t = millis();
        }
    }
}

ISR(TCA0_CMP1_vect) {
    cli();
    i = true;
    TCA0.SINGLE.INTFLAGS |= bit(5);                 // Clears the interrupt flag. Rather confusingly,
                                                    // this is done by setting a bit in the register
                                                    // to 1.
                                                    // Use bit(4) for CMP0, bit(5) CMP1, bit(6) CMP2
    sei();
}*/



/*
 * static inline void sendData(){

  unsigned long times[16*2];

  noInterrupts();
  
  times[0]=micros(); 
  for (byte i=0; i<16; i++){
     boolean bit= bits[i];
  
    //digitalWrite(5,HIGH); //too slow
    //VPORTD.OUT |= dShotPins;  
    //VPORTD.OUT = B10101000;
    
    VPORTB.OUT = B00000100;  //D5=B2 D9=B0 D10=B1


    asm(
    // For i = 0 to 15:
    "LDI  r23,  0\n"
    // Set High for every attached pins
    // DSHOT_PORT |= dShotPins;
    "IN r25,  %0\n"
    "_for_loop_0:\n"
    "OR r25,  %1\n"
    // Wait 7 cycles (7 - 6 = 1)
    "NOP\n"

    "OUT  %0, r25\n"
    // Wait 10 cycles (10 - 4 = 6)
    NOP4
    NOP2
    // Set Low for low bits only
    //DSHOT_PORT &= dShotBits[i];
    "LD r24,  Z+\n"
    "AND  r25,  r24\n"
    "OUT  %0, r25\n"
    // Wait 10 cycles (10 - 2 = 8)
    NOP8
    // Turn off everything
    // DSHOT_PORT &= ~dShotPins;
    "AND  r25,  %2\n"
    "OUT  %0, r25\n"
    // Add to i (tmp_reg)
    "INC  r23\n"
    "CPI  r23,  16\n"
    "BRLO _for_loop_0\n"
    // 7 cycles to next bit (4 to add to i and branch, 2 to turn on), no wait
    :
    : "I" (_SFR_IO_ADDR(DSHOT_PORT)), "r" (dShotPins), "r" (~dShotPins), "z" (dShotBits)
    : "r25", "r24", "r23"
    );
    

  
    
    if (bit){
      //25 cycles
      asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
    }
    else{
      //12 cycles
      asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); 
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");  
    }
    //digitalWrite(5, LOW); //too slow
    //VPORTD.OUT &= ~ dShotPins;
    //VPORTD.OUT = B00000000;  
    VPORTB.OUT = B00000000;  //D5=B2 D9=B0 D10=B1
    
    if (bit){
      //8 cycles
      asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
    }else{
      //21 cycles
      asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
      //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
    }
  }
   times[1]=micros();
  //for (byte i=0; i<16*2; i++){
   Serial.println(times[1]-times[0]);
   
  //}
  Serial.println("---------------------");
  interrupts();
}/*
 */
