#include "DShot.h"
#include <EEPROM.h>
#include <ButtonDebounce.h>

#define SERIAL_SPEED 115200


//TODO implment BlHeli passthrough: https://github.com/BrushlessPower/BlHeli-Passthrough



int solenoid = 2;
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


#define CONFIGURATION_VERSION  10

struct Confing {
  int  pusher_pull_time;     //time required for the pusher to move
  int  pusher_push_time;     //time required for the pusher to move
  int  esc_max_power;
  int  min_rampup_time;      //time required for the flywheel to get up to speed
  int  spin_differential;    //how much slower the bottom wheel should turn
  int  inactivity_time_out;  //After how much self shutdown
  int  warning_vbat;         //Vbat voltage to trigger warning
  int  critical_vbat;        //Vbat voltage to trigger shutdown
  int  vbat_scale;           //Used to calibrate vbat measurment
  int  config_version;      //IMPORTANT: increment config version for every change of this struct
};


Confing configuration = {
  110,         //pusher_pull_time -- min: 65 (tested)
  90,          //pusher_push_time -- min: 55 (tested)
  1200,        //esc_max_power
  140,         //min_rampup_time
  150,         //spin_differential
  30,          //inactivity_time_out
  10,          //warning_vbat
  9,           //critical_vbat
  12,          //vbat_scale
  CONFIGURATION_VERSION
};

struct ConfigParam {
  String  name;
  int     default_value;
  int     max_value;
  int     min_value;
  int*    value;
};

ConfigParam config_params[9] = {
  ConfigParam{"pusher_pull_time",110,500,10,&configuration.pusher_pull_time},
  ConfigParam{"pusher_push_time",90,500,10,&configuration.pusher_push_time},
  ConfigParam{"esc_max_power",1200,2047,200,&configuration.esc_max_power},
  ConfigParam{"min_rampup_time",140,500,0,&configuration.min_rampup_time},
  ConfigParam{"spin_differential",150,300,0,&configuration.spin_differential},
  ConfigParam{"inactivity_time_out",30,60,0,&configuration.inactivity_time_out},
  ConfigParam{"warning_vbat",10,14,0,&configuration.warning_vbat},
  ConfigParam{"critical_vbat",9,14,0,&configuration.critical_vbat},
  ConfigParam{"vbat_scale",12,100,1,&configuration.vbat_scale},
};



enum states {Idle, Rampup, Ready, Fire, Hold,Command, PoweringDown,PowerOFF};
const char* statesNames[] = {"Idle", "Rampup", "Ready", "Fire", "Hold", "Command","PoweringDown","PowerOFF"};

enum fireRates {Single, Auto, Burst};

enum states state = Idle;
enum states previous_state = Idle;
enum fireRates fireRate = Single;

int burstCount = 0;

String serial_command;

DShot ESC1(DShot::Mode::DSHOT300INV);
DShot ESC2(DShot::Mode::DSHOT300INV);

ButtonDebounce revButton(revPin, 15);
ButtonDebounce triggerButton(triggerPin, 15);
ButtonDebounce powerButton(powerSwitchPin, 30);
ButtonDebounce modeButton(modeSwitchPin, 30);

#if defined(__AVR_ATmega328P__)
  int long_beep=130;
  int short_beep=110;
#endif
#if defined(ARDUINO_ARCH_MEGAAVR)
  int long_beep=260;
  int short_beep=240;
#endif

unsigned long timer_rampup = 0;
unsigned long timer_power_off = 0;
unsigned long power_timeout = short_beep*4 + 300; //shoutdown after 4 beeps

unsigned long inactivity_timer = 0;
unsigned long inactivity_timeout = 0; 

unsigned long vabt_warning_timer = millis(); 

void setup() {

  Serial.begin(SERIAL_SPEED,SERIAL_8N1);


  ESC1.attach(esc1Pin);
  ESC2.attach(esc2Pin);
  ESC1.setThrottle(0);
  ESC2.setThrottle(0);
  setESC1speed(0);
  setESC2speed(0); 

  analogReference(DEFAULT);

  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);
  
  pinMode(solenoid, OUTPUT);
  digitalWrite(solenoid, LOW);
  
  pinMode(LED, OUTPUT); 
  digitalWrite(LED, LOW);

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

  inactivity_timeout = ((unsigned long) configuration.inactivity_time_out) * 1000 * 60;
  inactivity_timer = millis();

  //initialize LiPo voltage
  getAvgLiPoVoltage();
}

void loop() {


  controlESCs();

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

      if(strcmp(token, "vbat") == 0){
          Serial.print("vbat=");
          Serial.println(getLiPoVoltage());
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


  if(configuration.warning_vbat> 0 && getAvgLiPoVoltage() < configuration.warning_vbat){
      if((millis() - vabt_warning_timer)  > 10000){
        beep beeps[2];
        beeps[0]= {1, short_beep};
        beeps[1]= {1, short_beep};
        ESC1.sequenceBeep(beeps,2);
        vabt_warning_timer = millis();
      }
  }
  if(configuration.critical_vbat > 0 && getAvgLiPoVoltage() < configuration.critical_vbat){
    state = PowerOFF;
  }
  
  
  if(state == PoweringDown && previous_state != PoweringDown){
      beep beeps[4];
      beeps[0]= {4, short_beep};
      beeps[1]= {3, short_beep};
      beeps[2]= {2, short_beep};
      beeps[3]= {1, short_beep};
      ESC1.sequenceBeep(beeps,4);
  }
  if(state == Idle && previous_state==PoweringDown){
      ESC1.sequenceBeepClear();
      ESC2.setThrottle(0);
  }
  
  if(state == PowerOFF){
    digitalWrite(powerPin, LOW);
  }


  if (state == Rampup || state == Ready || state == Fire ) { 

    digitalWrite(LED, HIGH);

    setESC1speed(configuration.esc_max_power);
    setESC2speed(max(configuration.esc_max_power-configuration.spin_differential,48));
    
  } else {
    digitalWrite(LED, LOW );

    setESC1speed(0);
    setESC2speed(0);
  }
  
  if (state == Fire ) {
    
    digitalWrite(solenoid, HIGH);
    delay(configuration.pusher_pull_time);
    digitalWrite(solenoid, LOW);
    delay(configuration.pusher_push_time);
  }
  if (state == Command) {

    
    setESC1speed(0);
    delay(320);

     
    if (fireRate == Single) {
      fireRate = Burst;
      //   short - 3
      beep beeps[4];
      beeps[0]= {4, long_beep};
      beeps[1]= {1, short_beep};
      beeps[2]= {1, short_beep};
      beeps[3]= {1, short_beep};
      ESC1.sequenceBeep(beeps,4);
    } else if (fireRate == Burst) {
      fireRate = Auto;         
      beep beeps[4];
      beeps[0]= {4, long_beep};
      beeps[1]= {1, short_beep};
      beeps[2]= {2, short_beep};
      beeps[3]= {3, short_beep};
      ESC1.sequenceBeep(beeps,4);
    } else {
      fireRate = Single;
       //   short - short
      beep beeps[2];
      beeps[0]= {4, long_beep};
      beeps[1]= {1, short_beep};
      ESC1.sequenceBeep(beeps,2);
      
    }
  }

  //keep this statement at end of the loop
  if (previous_state != state) {
    Serial.println(statesNames[state]);
    previous_state = state;
    if(state!=PoweringDown){
      inactivity_timer = millis(); 
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
      if (inactivity_timeout > 0 && ((millis() - inactivity_timer) > inactivity_timeout)) {
        timer_power_off= millis(); 
        return PoweringDown;
      }
      return Idle;
      break;

    case PoweringDown :
       if (powerValue == LOW && ((millis() - inactivity_timer) < inactivity_timeout)) {
        return Idle;
       }
       //pressing any button will exit from timout outo power down
       if ((revValue == HIGH || revValue == HIGH || powerValue == HIGH || modeValue == HIGH) && ((millis() - inactivity_timer) > inactivity_timeout)) {
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


int esc1ActualSpeed = 0;
int esc2ActualSpeed = 0;
int esc1TargetSpeed = 0;
int esc2TargetSpeed = 0;

 
long timer_break_esc1 = 0;
long timer_break_esc2 = 0;

void setESC1speed(int speed){
   esc1TargetSpeed = speed;
}

void setESC2speed(int speed){
    esc2TargetSpeed = speed;
}

void controlESCs(){


  //Gradual speed decrease is required to avoid voltage spikes that would accidentally trigger the solenoid
  
  if(esc1ActualSpeed<esc1TargetSpeed){
    esc1ActualSpeed = esc1TargetSpeed;
    ESC1.setThrottle(esc1ActualSpeed);
  }else if(esc1ActualSpeed>esc1TargetSpeed && (millis() - timer_break_esc1 > 25)){
      timer_break_esc1 = millis();
      esc1ActualSpeed-=100;
      if(esc1ActualSpeed < esc1TargetSpeed){
        esc1ActualSpeed = esc1TargetSpeed;
      }
      ESC1.setThrottle(esc1ActualSpeed);
  }

  if(esc2ActualSpeed<esc2TargetSpeed){
    esc2ActualSpeed = esc2TargetSpeed;
    ESC2.setThrottle(esc2ActualSpeed);
  }else if(esc2ActualSpeed>esc2TargetSpeed && (millis() - timer_break_esc2 > 25)){
    timer_break_esc2 = millis();
    esc2ActualSpeed-=100;
    if(esc2ActualSpeed < esc2TargetSpeed){
      esc2ActualSpeed = esc2TargetSpeed;
    }
    ESC2.setThrottle(esc2ActualSpeed);
  }

  

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

  //https://www.arduino.cc/reference/en/language/functions/analog-io/analogread/
  //https://skillbank.co.uk/arduino/measure.htm


  float value = analogRead(A1); 
  float voltage = (value + 0.5) * 5.0 / 1023.0 *   ((float) configuration.vbat_scale); //adj_multiplier
  //Calibration formula: current_scale * (actual_vbat / measured_vbat) = new_scale

  return voltage;
  }


float vbat_avg = 0;

float getAvgLiPoVoltage(){

  float voltage = getLiPoVoltage();

  //To deal with battery sag voltage is measured as exponential moving average.
  if(vbat_avg == 0){
    vbat_avg = voltage;
  }else{
    static float alpha = 0.001; //must be between 0 and 1, the higher the faster it converges 
    vbat_avg = (alpha * voltage) + (1.0 - alpha) * vbat_avg;
  }

  return vbat_avg;
  }
