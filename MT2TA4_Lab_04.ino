/*
Karman Sidhu
4000578131
Mechtron 2TA4
Lab 4
*/

// the fsm contains the following states
/*
1. Initialization: Intialization variables and the setup to begin
2. Monitor: This state only monitors when the intial condition currentTemp > maxTemp (otherwise monitoring the  current temp is done in all states using a helper function)
3. Fan Ramp: This is where we increase the speed of the fan gradually until it hits 255 (100% duty)
4. Fan Full: This is where the fan continously keeps spinning at 255 (100% duty) until the current temp is equal to or below the max temp
*/


#include <TimerOne.h> // to generate 100 ms interrupts
#include <Wire.h>
#include "DFRobot_RGBLCD1602.h"

const int POT_PIN = A1; //set to potentiometer to set max temp
const int TEMP_PIN = A0;  // connected to LM35 temperature sensor output
const int FAN_PIN = 3; // PWM pin to control fan through optocoupler/MOSFET circuit

enum StateType {INIT_STATE, MONITOR_STATE, FAN_RAMP_STATE, FAN_FULL_STATE}; 

enum Action { INIT, MONITOR, RAMP_FAN, RUN_FAN}; 

const char* stateNames[] = {"INIT", "MONITOR", "RAMP_FAN", "FAN_FULL"};
const char* actionNames[] = {"INIT", "MONITOR", "RAMP_FAN", "RUN_FAN"}; 

//global variables

// flag for timer interrupt for 100 ms
volatile bool tick100ms = false; 
int lcdCounter =0; // to control how often lcd is updated
int beatCounter = 0;  // to control the periodic serial printing 

DFRobot_RGBLCD1602 lcd(0x6B, 16, 2); 

float maxTemp = 0;
float currentTemp =0; 

int fanSpeed = 0; // current pwm value sent to the fan between 0 and 255
const int rampStep = 15; //amount of speed being added every 100ms






//function prototypes

void InitSystem();  //initalizing the system
void MonitorTemp();  //normal monitoring state
void RampFan(); // gradually increase the fan speed
void RunFan(); // running fan at full speeed
void timerISR(); // isr for interrupt
void updateLCD(); //updating lcd display
void printBeat(); //printing current data to serial monitor (for me)
void readTemp(); // reads temp sensor and converting to celsius


static void (*action_table[])(void) = {
  InitSystem,  // INIT
  MonitorTemp, //MONITOR
  RampFan, // RAMP_FAN
  RunFan // RUN_FAN
}; 

StateType curr_state = INIT_STATE; 
Action curr_action = INIT; 

void setup(){
  Serial.begin(9600); 

  Wire.begin(); 

  //setting fan control pin to output
  pinMode(FAN_PIN, OUTPUT); 

  lcd.init(); 
  lcd.setRGB(0,0,255); 
  lcd.clear(); 

  //initializing interrupt 

  Timer1.initialize(100000);
  Timer1.attachInterrupt(timerISR);

  Serial.println("System Heartbeat Started: 10 Hz");

}

void loop(){
  //runnign every 100 ms when ISR sets the flag as true
  if (tick100ms){
    tick100ms = false;  //reset the flag

    int potValue = analogRead(POT_PIN);  // reading the potentiometer value between 0-1023

    //modifying the reading to temp range between 15 to 30 deg celcius
    maxTemp =  15.0 + ((float)potValue/1023.0)*15.0;

    // just to periodically update serial monitor data every 1 second

    beatCounter++; 
    if(beatCounter>=10){
      printBeat();
      beatCounter = 0; 
    }

    // updating lcd every 0.5 seconds

    lcdCounter++; 
    if(lcdCounter >=5){
      updateLCD(); 
      lcdCounter = 0; 
    }
    //execute the current fsm action
    action_table [curr_action](); 
  }
}


void InitSystem(){

  //read the current tem[]
  readTemp(); 
  fanSpeed = 0; //ensure fan starts from 0
  analogWrite(FAN_PIN, fanSpeed); //sending pwm value to fan

  //transition to monitorcstate
  curr_state = MONITOR_STATE; 
  curr_action = MONITOR;
}

void MonitorTemp(){

 //Read sensors

  readTemp(); 

  //if the temp exceeds the max transition to the fan ramp state

  if (currentTemp> maxTemp){
    curr_state = FAN_RAMP_STATE; 
    curr_action = RAMP_FAN; 
    }
  }

void RampFan(){

  //read temp again

  readTemp(); 

  //increase fan gradually by 15 e
  fanSpeed += rampStep; 

  //ensure min pwm to start spinning fan
  if (fanSpeed>0 && fanSpeed <60){
    fanSpeed = 60; 
  }

  //to stop pwn from passing the max
  if (fanSpeed > 255){
    fanSpeed = 255; 
  }

  //to prevent negative values incase
  if (fanSpeed <0){
    fanSpeed =0; 
  }

  //output the pwm signal to the fan

  analogWrite (FAN_PIN, fanSpeed);

  // if max speed reached transition to the full speed state called run fan
  if (fanSpeed>=255){
    curr_state = FAN_FULL_STATE; 
    curr_action = RUN_FAN; 
  }

  //else if the temp drops below the max, 

  else if (currentTemp < maxTemp){

    //turn the fan off
    fanSpeed = 0;
    analogWrite(FAN_PIN, fanSpeed); 

    //return to the monitoring state
    curr_state = MONITOR_STATE; 
    curr_action = MONITOR;  
  }
}

void RunFan(){

  //read the temp 
  readTemp(); 

  //keep the fan running at full speed
  analogWrite(FAN_PIN, 255); 

  //if temp drops below max

  if (currentTemp <= maxTemp){

    //turn the fan off and go back into the monitor state
    fanSpeed = 0;
    analogWrite(FAN_PIN, fanSpeed); 
    curr_state = MONITOR_STATE; 
    curr_action = MONITOR;  
  }
}

void readTemp(){

  // storing the resistance value coming from the potentiometer
  //reads analog voltage from tempsensor
  int sensorValue = analogRead(TEMP_PIN); 

  //convert adc to voltage
  //corresponds to 0-5 volts
  //voltage = (ADC * 5V) / 1023
  // outputs 10 mV per celcius but amplifier has gain of 3
  //then the output is 30 mV per celsius
  //temp = voltage/0.03

  //assuming that there is a 3x ampliifier gain
  currentTemp = ((float)sensorValue*5.0 / 1023.0) / 0.03;
}


void timerISR(){
  tick100ms = true; 
}

void updateLCD(){
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(currentTemp, 1);
  lcd.print("C         ");

  lcd.setCursor(0,1); 
  lcd.print("Max : ");
  lcd.print(maxTemp, 1); 
  lcd.print("C         "); 
}


void printBeat(){ 
  Serial.print("State: ");
  Serial.print(stateNames[curr_state]); 

  Serial.print(" | Action: "); 
  Serial.print(actionNames[curr_action]); 

  Serial.print(" | Temp:  ");
  Serial.print(currentTemp, 1); 

  Serial.print("C | Max: ");
  Serial.print(maxTemp, 1);  

  Serial.print("C | Fan: ");
  Serial.println(fanSpeed); 



}




















