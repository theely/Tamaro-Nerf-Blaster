#include "DShot.h"
#include <EEPROM.h>
#include <ButtonDebounce.h>

#define SERIAL_SPEED 115200


//TODO implment BlHeli passthrough: https://github.com/BrushlessPower/BlHeli-Passthrough





int relay = 2;
int esc1Pin = 3;
//4 unused
int powerSwitchPin = 5;
int esc2Pin = 6;
int triggerPin = 7;
int revPin = 8;
//9 unused
//10 unused
int modeSwitchPin = 11;
int powerPin = 12;

int LED = 13;

//pin 15 used for analog input A1


String version = "0.1";


#define CONFIGURATION_VERSION  6

struct Confing {
  int  pusher_pull_time;  //time required for the pusher to move
  int  pusher_push_time;  //time required for the pusher to move
  int  esc_max_power;
  int  min_rampup_time;   //time required for the flywheel to get up to speed
  int  spin_differential; //how much slower the bottom wheel should turn
  int  config_version;   //IMPORTANT: increment config version for every change of this struct
};


Confing configuration = {
  110, //pusher_pull_time -- min: 65 (tested)
  90,  //pusher_push_time -- min: 55 (tested)
  1200, //esc_max_power
  140, //min_rampup_time
  150, //spin_differential
  CONFIGURATION_VERSION
};

struct ConfigParam {
  String  name;
  int     default_value;
  int     max_value;
  int     min_value;
  int*    value;
};

ConfigParam config_params[5] = {
  ConfigParam{"pusher_pull_time",110,500,10,&configuration.pusher_pull_time},
  ConfigParam{"pusher_push_time",90,500,10,&configuration.pusher_push_time},
  ConfigParam{"esc_max_power",1200,2047,200,&configuration.esc_max_power},
  ConfigParam{"min_rampup_time",140,500,0,&configuration.min_rampup_time},
  ConfigParam{"spin_differential",150,300,0,&configuration.spin_differential},
};



 
long timer_rampup = 0;
long timer_power_off = 0;
int power_timeout = 1000;


enum states {Idle, Rampup, Ready, Fire, Hold, Command, PoweringDown,PowerOFF};
const char* statesNames[] = {"Idle", "Rampup", "Ready", "Fire", "Hold", "Command","PoweringDown","PowerOFF"};

enum fireRates {Single, Auto, Burst};

enum states state = Idle;
enum states previous_state = Idle;
enum fireRates fireRate = Burst;

int burstCount = 0;

String serial_command;

DShot ESC1(DShot::Mode::DSHOT300INV);
DShot ESC2(DShot::Mode::DSHOT300INV);

ButtonDebounce revButton(revPin, 15);
ButtonDebounce triggerButton(triggerPin, 15);
ButtonDebounce powerButton(powerSwitchPin, 30);
ButtonDebounce modeButton(modeSwitchPin, 30);  

void setup() {

  Serial.begin(SERIAL_SPEED);

  // Attach the ESC on pin 6 and 3
  ESC1.attach(esc1Pin);
  ESC2.attach(esc2Pin);
  ESC1.setThrottle(0);
  ESC2.setThrottle(0); 


   pinMode(powerPin, OUTPUT);
   digitalWrite(powerPin, HIGH);



  
  //pinMode(revPin, INPUT_PULLUP);
  //pinMode(triggerPin, INPUT_PULLUP);

  //pinMode(revPin, INPUT);
  //pinMode(triggerPin, INPUT);
  
  pinMode(relay, OUTPUT);

  // Define pin #13 as output, for the LED
  pinMode(LED, OUTPUT); 

  digitalWrite(relay, LOW);


  Confing configuration_check;
  EEPROM.get( 0, configuration_check );
  if(configuration_check.config_version !=  configuration.config_version){
     //Config structure has changed reset config.
     Serial.println("Config structure mismatch detected");
     EEPROM.put(0, configuration);
     Serial.println("Re-setted configuration");
  }else{
    EEPROM.get( 0, configuration);
  }


}

void loop() {


  
    float vabt= getLiPoVoltage();
    if(vabt < 3.5){
    //Serial.print("LiPo low:");
    //Serial.println(vabt);
    }

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

      if(strcmp(token, "dump") == 0){
        dump();
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

  if(state == PowerOFF){
    digitalWrite(powerPin, LOW);
  }


  if (state == Rampup || state == Ready || state == Fire ) { 
  //if (state != Idle && state != Click1 && state != Click2_Command && state != Hold) {
    digitalWrite(LED, HIGH);
    ESC1.setThrottle(configuration.esc_max_power);    // Send the signal to the ESC
    ESC2.setThrottle(max(configuration.esc_max_power-configuration.spin_differential,48));    // Send the signal to the ESC
  } else {
    digitalWrite(LED, LOW );
    ESC1.setThrottle(0);
    ESC2.setThrottle(0);
  }

  if (state == Fire ) {
    digitalWrite(relay, HIGH);
    delay(configuration.pusher_pull_time);
    digitalWrite(relay, LOW);
    delay(configuration.pusher_push_time);
  }
  if (state == Command) {
    ESC1.setThrottle(0);
    delay(320); 
     
    if (fireRate == Single) {
      fireRate = Burst;
      //   short - 3
      beep beeps[4];
      beeps[0]= {4, 300};
      beeps[1]= {1, 280};
      beeps[2]= {1, 280};
      beeps[3]= {1, 280};
      ESC1.sequenceBeep(beeps,4);
    } else if (fireRate == Burst) {
      fireRate = Auto;         
      beep beeps[4];
      beeps[0]= {4, 280};
      beeps[1]= {1, 240};
      beeps[2]= {2, 240};
      beeps[3]= {3, 240};
      ESC1.sequenceBeep(beeps,4);
    } else {
      fireRate = Single;
       //   short - short
      beep beeps[2];
      beeps[0]= {4, 300};
      beeps[1]= {1, 260};
      ESC1.sequenceBeep(beeps,2);
      
    }
  }
}

enum states getState() {


    revButton.update();
    triggerButton.update();
    powerButton.update();
    modeButton.update();

    int revValue = !revButton.state();
    int triggerValue = !triggerButton.state(); 
    int powerValue = !powerButton.state();
    int modeValue = !modeButton.state();

  


  switch (state) {

    case PowerOFF:
       return PowerOFF;
       break;

    case Idle:
      if (modeValue == HIGH) {
        return Command;
      }    
      if (powerValue == HIGH) {
        timer_power_off= millis(); 
        return PoweringDown;
      }
      
      if (revValue == HIGH) {
        timer_rampup = millis();
        return Rampup;
      }
      return Idle;
      break;

    case PoweringDown :
       if (powerValue == LOW) {
        return Idle;
       }
       if ((millis() - timer_power_off) > power_timeout) {
         return PowerOFF;
       }
       return PoweringDown;
       break; 
       
    case Rampup :
      if (revValue == HIGH && (millis() - timer_rampup) > configuration.min_rampup_time) {
        return Ready;
      } 
      if (revValue == HIGH) {
        return Rampup;
      }
      break;

    case Ready :
      if (triggerValue == HIGH) {
        burstCount = 1;
        return Fire;
      }
      if (revValue == HIGH && triggerValue == LOW) {
        return Ready;
      }
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

    case Command:
      if (modeValue == HIGH) {
        return Command;
      } 
      return Idle;

    default : break;

  }

  return Idle;
}

void setSerialParam(String param, int value){

  for(int i=0;i<sizeof config_params/sizeof(struct ConfigParam);i++){
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


void dump(){

  for(int i=0;i<sizeof config_params/sizeof(struct ConfigParam);i++){
     Serial.print(config_params[i].name);
     Serial.print("=");
     Serial.println(*config_params[i].value);
  }
}






float getLiPoVoltage(){

   /*
   // read the input on analog pin 0:
  int sensorValue = analogRead(ADC_BATTERY);
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 4.3V):
  float voltage = sensorValue * (4.3 / 1023.0);

  return voltage;
  */

  int value = analogRead(A1); //A3 on previous version
  float voltage = value * (5.0/1023) * ((30.0 + 10.0)/10.0);
  
  return voltage;


  
  }
