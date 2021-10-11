#include "DShot.h"
#include <EEPROM.h>
#define SERIAL_SPEED      115200


// Declare the pins for the Button and the LED<br>int buttonPin = 12;
int LED = 13;
int revPin = 8;
int triggerPin = 7;
int relay = 2;
String version = "0.1";

struct Confing {
  int  pusher_pull_time; //time required for the pusher to move
  int  pusher_push_time; //time required for the pusher to move
  int  esc_max_power;
  int  min_rampup_time; //time required for the flywheel to get up to speed
};


Confing configuration = {
  110, //pusher_pull_time -- min: 65 (tested)
  90,  //pusher_push_time -- min: 55 (tested)
  1650, //esc_max_power
  140, //min_rampup_time
};

struct ConfigParam {
  String  name;
  int     default_value;
  int     max_value;
  int     min_value;
  int*    value;
};

ConfigParam config_params[4] = {
  ConfigParam{"pusher_pull_time",110,500,20,&configuration.pusher_pull_time},
  ConfigParam{"pusher_push_time",90,500,20,&configuration.pusher_push_time},
  ConfigParam{"esc_max_power",1650,1300,2000,&configuration.esc_max_power},
  ConfigParam{"min_rampup_time",140,0,500,&configuration.min_rampup_time},
};



 
long timer_rampup = 0;
int  min_command_time = 25;       
long timer_command = 0;
int  command_timeout = 3000;       
long timer_command_timeout = 0;


enum states {Idle, Rampup, Ready, Fire, Hold, Click1, Click2, Click2Ready, Command};
const char* statesNames[] = {"Idle", "Rampup", "Ready", "Fire", "Hold", "Click1", "Click2", "Click2Ready", "Command"};

enum fireRates {Single, Auto, Burst};

enum states state = Idle;
enum states previous_state = Idle;
enum fireRates fireRate = Burst;

int burstCount = 0;

int revPrevious = LOW;
int triggerPrevious = LOW;

String serial_command;

DShot ESC1(DShot::Mode::DSHOT300INV);



void setup() {

  Serial.begin(SERIAL_SPEED);

 
  pinMode(revPin, INPUT_PULLUP);
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(relay, OUTPUT);

  // Define pin #13 as output, for the LED
  pinMode(LED, OUTPUT); 

  digitalWrite(relay, LOW);

  EEPROM.get( 0, configuration );

  // Attach the ESC on pin 6 and 3
  ESC1.attach(3);
  ESC1.attach(6); 

}

void loop() {
  /*
    float vabt= getLiPoVoltage();
    if(vabt < 3.5){
    Serial.print("LiPo low:");
    Serial.println(vabt);
    }*/

  if (Serial.available()) {
    serial_command = Serial.readStringUntil('\n');
    char* token = strtok(serial_command.c_str(), "= ,\r\n");

    bool set_value = false;
    String variable_name;
    String variable_value;
    while (token != NULL) {

      if(strcmp(token, "version") == 0){
          Serial.println("Tamaro Blaster");
          Serial.print("version:");
          Serial.println(version);
      }
      
      if (set_value && variable_name != NULL && variable_value == NULL) {
        variable_value = token;
      }
      if (set_value && variable_name == NULL) {
        variable_name = token;
      }
      if (strcmp(token, "set") == 0) {
        set_value = true;
      }
      token = strtok(NULL, "= ,\r\n");
    }

    if (set_value && variable_name != NULL && variable_value != NULL) {
      setSerialParam(variable_name, variable_value.toInt());
      EEPROM.put(0, configuration);
    }



  }

  state = getState();
  if (previous_state != state) {
    Serial.println(statesNames[state]);
    previous_state = state;
  }


  // Read the value of the input. It can either be 1 or 0
  int revValue = !digitalRead(revPin);
  int triggerValue = !digitalRead(triggerPin);

  if (state != Idle && state != Click1 && state != Command && state != Hold) {
    digitalWrite(LED, HIGH);
    ESC1.setThrottle(configuration.esc_max_power);    // Send the signal to the ESC
  } else {
    digitalWrite(LED, LOW );
    ESC1.setThrottle(0);
  }

  if (state == Fire ) {
    digitalWrite(relay, HIGH);
    delay(configuration.pusher_pull_time);
    digitalWrite(relay, LOW);
    delay(configuration.pusher_push_time);
  }

  if (state == Command ) {
    ESC1.setThrottle(0);
    delay(800); 
     
    if (fireRate == Single) {
      fireRate = Burst;
      //   short - 3
      beep beeps[4];
      beeps[0]= {1, 260};
      beeps[1]= {3, 260};
      beeps[2]= {3, 260};
      beeps[3]= {3, 260};
      ESC1.sequenceBeep(beeps,4);
    } else if (fireRate == Burst) {
      fireRate = Auto;         
      // short - long
      beep beeps[2];
      beeps[0]= {1, 260};
      beeps[1]= {5, 260};
      ESC1.sequenceBeep(beeps,2);
    } else {
      fireRate = Single;
       //   short - short
      beep beeps[2];
      beeps[0]= {1, 260};
      beeps[1]= {3, 260};
      ESC1.sequenceBeep(beeps,2);
      
    }
  }

}

enum states getState() {

  int revValue = !digitalRead(revPin);
  int triggerValue = !digitalRead(triggerPin);

  //Detect Changes
  bool commandValid = false;
  if (revPrevious != revValue || triggerPrevious != triggerValue) {
    timer_command = millis();
  } else if ((millis() - timer_command) > min_command_time) {
    commandValid = true;
  }
  revPrevious = revValue;
  triggerPrevious = triggerValue;

  switch (state) {

    case Idle  :
      if (revValue == HIGH) {
        timer_rampup = millis();
        return Rampup;
      }
      return Idle;
      break;

    case Rampup :
      if (revValue == LOW && commandValid) {
        timer_command_timeout = millis();
        return Click1;
      }
      if ((millis() - timer_rampup) > configuration.min_rampup_time) {
        return Ready;
      } else {
        return Rampup;
      }
      break;

    case Ready :
      if (revValue == LOW && commandValid) {
        timer_command_timeout = millis();
        return Click1;
      }
      if (triggerValue == HIGH) {
        burstCount = 1;
        return Fire;
      }
      return Ready;
      break;

    case Fire:
      if (fireRate == Auto && triggerValue == HIGH) {
        return Fire;
      }
      if (fireRate == Burst && triggerValue == HIGH && burstCount < 3) {
        burstCount++;
        return Fire;
      }
      if (triggerValue == LOW) {
        return Idle;
      }
      return Hold;

    case Hold:
      if (revValue == LOW) {
        return Idle;
      }
      return Hold;

    case Click1:
      if (revValue == HIGH ) {
        timer_rampup = millis();
      }
      if (revValue == HIGH  && commandValid) {
        return Click2;
      }
      if ((millis() - timer_command_timeout) > command_timeout) {
        return Idle;
      }
      return Click1;

    case Click2:
      if (revValue == LOW  && commandValid) {
        return Command;
      }
      if ((millis() - timer_rampup) > configuration.min_rampup_time) {
        return Click2Ready;
      }
      return Click2;

    case Click2Ready:
      if (triggerValue == HIGH) {
        burstCount = 1;
        return Fire;
      }
      if (revValue == LOW  && commandValid) {
        return Command;
      }
      return Click2Ready;

    default : break;

  }


  return Idle;
}

void setSerialParam(String param, int value){

  for(int i=0;i<sizeof config_params;i++){
    if(param == config_params[i].name){

      if(value < config_params[i].min_value){
        Serial.print("Minimum value for ");
        Serial.print(param);
        Serial.print(" is ");
        Serial.println(config_params[i].min_value);
        return;
      }
      if(value > config_params[i].max_value){
        Serial.print("Maximum value for ");
        Serial.print(param);
        Serial.print(" is ");
        Serial.println(config_params[i].max_value);
        return;
      }

      *config_params[i].value=value;
      EEPROM.put(0, configuration);

      Serial.print(param);
      Serial.print(" set to ");
      Serial.println(value);
      
      return;
    }
  }
}




/*
  float getLiPoVoltage(){

   // read the input on analog pin 0:
  int sensorValue = analogRead(ADC_BATTERY);
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 4.3V):
  float voltage = sensorValue * (4.3 / 1023.0);

  return voltage;
  }*/
