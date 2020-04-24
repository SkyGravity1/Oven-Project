// Temperature stuff using adafruit MAX6675 library : https://github.com/adafruit/MAX6675-library
#include <MAX6675.h>

int CS = 10;             // CS pin on MAX6675
int SO = 8;              // SO pin of MAX6675
int SCKK = 13;             // SCK pin of MAX6675
int units = 1;            // Units to readout temp (0 = raw, 1 = ˚C, 2 = ˚F)
float temperature = 0.0;  // Temperature output variable

// Initialize the MAX6675 Library for our chip
MAX6675 temp(CS,SO,SCKK,units);

// LCD Stuff
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const uint8_t rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//Process stuff
const uint8_t relay_1 = 6, relay_2 = 7, select = A0, start = A1;

//new_time is for present, op_time is for time when the operation started 
//(operation being state the oven is)
unsigned long new_time,op_time;

//{end_target_temp,ideal_operation_time,max_time,hyst} for {pre_soak,soak,pre_flow,reflow,cooling}
int profile[4][5][4] = {
  {{150.0,60,90,3},{175,60,120,3},{217,30,60,3},{249,30,45,3},{25,40,250,6}},
  {0,0,0,0,0,0},
  {0,0,0,0,0,0},
  {0,0,0,0,0,0}
};

String profileNames[4] = {
  "Quik Chip       ", "Profile 2       ", "Profile 3       ", "Profile 4       "
};

typedef enum {
  menu=0,
  pre_soak=1,
  soak=2,
  pre_flow=3,
  reflow=4,
  cooling=5
}oven_state;

oven_state state = menu;

uint8_t profile_select = 0;

float temp_diff = 0.0; //Difference between starting temperature(of operation) and target temperature (at end of operation)

unsigned long start_time = 0;

float start_temp = 0.0;

float ramp = 0;
float actual_target_temp = 0;

//SETUP

void setup() {
  //Relay pins (by default make them turned off)
  pinMode(relay_1,OUTPUT);
  pinMode(relay_2,OUTPUT);

  digitalWrite(relay_1,HIGH);
  digitalWrite(relay_2,HIGH);

  pinMode(select,INPUT);
  pinMode(start,INPUT);
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Temp :");

  //Process stuff
}

//LOOP
int start_op = 0;
void loop() {
  // Read the temp from the MAX6675
  temperature = temp.read_temp();

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(6, 0);
  
  if(temperature < 0 || !isDigit(temperature)) {                   
    // print error
    lcd.print("error");
    //Retake the temperature
    return;
  } else {
    // print temperature
    lcd.print(temperature);
    lcd.print("*C  ");
  }

  lcd.setCursor(0, 1);
  
  if(state == menu){
    menu_func();
  }else{
    if(state == pre_soak){
      if(temperature <= profile[profile_select][state][0]/*end_target_temp*/ + profile[profile_select][state][3]/*hyst*/ && temperature >= profile[profile_select][state][0]/*end_target_temp*/ - profile[profile_select][state][3]/*pre_soak_hyst*/){
        state = soak;
        start_op = 0;
      }
    }else if(state == soak){
      if(temperature <= profile[profile_select][state][0]/*end_target_temp*/ + profile[profile_select][state][3]/*hyst*/ && temperature >= profile[profile_select][state][0]/*end_target_temp*/ - profile[profile_select][state][3]/*pre_soak_hyst*/){
        state = pre_flow;
        start_op = 0;
      }
    }else if(state == pre_flow){
      if(temperature <= profile[profile_select][state][0]/*end_target_temp*/ + profile[profile_select][state][3]/*hyst*/ && temperature >= profile[profile_select][state][0]/*end_target_temp*/ - profile[profile_select][state][3]/*pre_soak_hyst*/){
        state = reflow;
        start_op = 0;
      }
    }else if(state == reflow){
      if(temperature <= profile[profile_select][state][0]/*end_target_temp*/ + profile[profile_select][state][3]/*hyst*/ && temperature >= profile[profile_select][state][0]/*end_target_temp*/ - profile[profile_select][state][3]/*pre_soak_hyst*/){
        state = cooling;
        start_op = 0;
      }
    }else if(state == cooling){
      if(temperature <= profile[profile_select][state][0]/*end_target_temp*/ + profile[profile_select][state][3]/*hyst*/ && temperature >= profile[profile_select][state][0]/*end_target_temp*/ - profile[profile_select][state][3]/*pre_soak_hyst*/){
        state = menu;
        start_op = 0;
      }
    }
    if(!start_op){
      start_temp = temperature;
      start_time = (millis()/1000);
      temp_diff = profile[profile_select][state][0]/*end_target_temp*/ - start_temp;
      if(profile[profile_select][state-1][0] == profile[profile_select][state][0])
        ramp = 0;
      else
        ramp = temp_diff/profile[profile_select][state][1]/*ideal_operation_time*/;
      start_op = 1;   
    }
    temp_control();
    if(digitalRead(start)){
      state=menu;
      while(digitalRead(start));
    }
  }
   
  delay(1000);
}

//FUNCTIONS

void menu_func(){
  //When in menu, everything should be off
  oven_off();

  lcd.print(profileNames[profile_select]);

  if(digitalRead(select)){
    profile_select++;
    while(digitalRead(select));
  }

  if(digitalRead(start)){
    state=pre_soak;
    while(digitalRead(start));
  }
}

void oven_on(){
  digitalWrite(relay_1,LOW);
  digitalWrite(relay_2,LOW);
}

void oven_off(){
  digitalWrite(relay_1,HIGH);
  digitalWrite(relay_2,HIGH);
}

int error=0;
void temp_control(){
    actual_target_temp = ramp * ((millis()/1000) - start_time) + start_temp;
    if(ramp>0){
      if(temperature <= actual_target_temp+profile[profile_select][state][3]/*hyst*/ && temperature >= actual_target_temp - profile[profile_select][state][3]/*hyst*/){
        oven_on();
        error=0;
      }else if(temperature >= actual_target_temp+profile[profile_select][state][3]/*hyst*/){
        oven_off();
      }else if(temperature <= actual_target_temp - profile[profile_select][state][3]/*hyst*/){
        oven_on();
        error++;        
      }
    }else if (ramp==0){
      error=0;
      if(temperature <= actual_target_temp+profile[profile_select][state][3]/*hyst*/ && temperature >= actual_target_temp - profile[profile_select][state][3]/*hyst*/){
        oven_on();        
      }else if(temperature >= actual_target_temp+profile[profile_select][state][3]/*hyst*/){
        oven_off();
      }else if(temperature <= actual_target_temp-profile[profile_select][state][3]/*hyst*/){
        oven_on();
      }
    }else{
      if(temperature <= actual_target_temp+profile[profile_select][state][3]/*hyst*/ && temperature >= actual_target_temp - profile[profile_select][state][3]/*hyst*/){
        oven_off();
        error=0;
      }else if(temperature >= actual_target_temp+profile[profile_select][state][3]/*hyst*/){
        oven_on();
      }else if(temperature <= actual_target_temp - profile[profile_select][state][3]/*hyst*/){
        oven_off();
        error++;        
      }
    }   

    //If we take too long to heat up, stop process
    //If for 10 seconds straight we can't get back to the curve, stop process
    if((millis()/1000) - start_time >= profile[profile_select][state][2] || error>10)
      state = menu;
}
