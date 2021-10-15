#include "DShot.h"

DShot esc1(DShot::Mode::DSHOT300);

uint16_t throttle = 48;
uint16_t target = 0;

void setup() {
  // put your setup code here, to run once:
 Serial.begin(115200); 
 esc1.attach(6);
 esc1.attach(3);
 esc1.setThrottle(throttle);
}

int count=0;

void loop() {


 if (Serial.available()>0){
    int serial_int = Serial.parseInt();
    if(serial_int!=0){
      if(serial_int >0 && serial_int <= 2047){
        throttle = serial_int;
      }
      if(serial_int==6000){

        if(count==0){
          // short - long
          beep beeps[2];
          beeps[0]= {5, 220};
          beeps[1]= {1, 260};
          esc1.sequenceBeep(beeps,2);
        }else if(count==1){
           //   short - short
          beep beeps[2];
          beeps[0]= {2, 220};
          beeps[1]= {1, 260};
          esc1.sequenceBeep(beeps,2);
        }else{
            //   short - 3
          beep beeps[4];
          beeps[0]= {3, 220};
          beeps[1]= {3, 220};
          beeps[2]= {3, 220};
          beeps[3]= {1, 260};
          esc1.sequenceBeep(beeps,4);
        }
        count++;
        count=count%3;
        
        return;
      }
      if(serial_int==5000){
        throttle = 0;
      }
      Serial.print("Command: ");
      Serial.println(throttle);
      esc1.setThrottle(throttle);
    }
  }
  
    delay(10);
}
