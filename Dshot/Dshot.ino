#include "DShot.h"

DShot esc1(DShot::Mode::DSHOT300INV);

uint16_t throttle = 48;
uint16_t target = 0;

void setup() {
  // put your setup code here, to run once:
 Serial.begin(115200); 
 esc1.attach(6);
 esc1.attach(3); //Multiple ports is not working
 //esc1.attach(20);  
 esc1.setThrottle(throttle);
}

void loop() {

  /*
  // put your main code here, to run repeatedly:
  if (Serial.available()>0){
    int serial_int = Serial.parseInt();
    if(serial_int>0){
      target = serial_int;
      Serial.println(target);
      if (target>2047) target = 2047;
    }
    if(serial_int>=5000){
      target = 0;
      Serial.println(0);
    }
    
  }
  if (target<=48){
    esc1.setThrottle(target);
  }else{
     if (throttle<48){
        throttle = 48;
      }
    if (target>throttle){
      throttle ++;
      esc1.setThrottle(throttle);
    }else if (target<throttle){
      throttle --;
      esc1.setThrottle(throttle);
    }
  }
*/

  

 if (Serial.available()>0){
    int serial_int = Serial.parseInt();
    if(serial_int!=0){
      if(serial_int >0 && serial_int <= 2047){
        throttle = serial_int;
      }
      if(serial_int>=5000){
        throttle = 0;
      }
      Serial.print("Command: ");
      Serial.println(throttle);
      esc1.setThrottle(throttle);
    }
  }
  
    delay(10);
}
