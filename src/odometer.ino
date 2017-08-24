/*
   //      _______ __  _________ _________
   //      \_     |  |/  .  \   |   \ ___/
   //        / :  |   \  /   \  |   / _>_
   //       /      \ | \ |   /  |  /     \
   //      /   |___/___/____/ \___/_     /
   //      \___/--------TECH--------\___/
   //       ==== ABOVE SCIENCE 1994 ====
   //
   //   Ab0VE TECH - HONDA Prelude Odometer
 */

/******** TODO **********
   Motor hours alaem     ❏
 *************************/

#define DEBUG

#include <avr/pgmspace.h>
#include <Arduino.h>
#include <Wire.h>
#include <LedDisplay.h>
#include <I2C_eeprom.h>
#include <avr/wdt.h>


I2C_eeprom eeprom(0x50,16384/8); /* FM24C16A FRAM */

#define PPR 4 // VSS pulses per axle revolution


#define TRIP_A 0
#define TRIP_B 1
#define MOTOR_HOUR 2

// default tire size 205/55R15
#define TIRE_WIDTH_DEFAULT 4
#define TIRE_SIDE_DEFAULT  6
#define TIRE_RIM_DEFAULT   2

const PROGMEM uint16_t TIRE_WIDTH_ARRAY[] = { 165, 175, 185, 195, 205, 215, 225, 235, 245, 255, 265, 275, 285, 295, 305 };
const PROGMEM uint8_t TIRE_SIDE_ARRAY[] =   { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85 };
const PROGMEM uint8_t TIRE_RIM_ARRAY[] =    { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };

#define CONFIG_POS_MAX 8

// CONFIG
uint8_t TIRE_RIM;
uint16_t TIRE_WIDTH;
uint8_t TIRE_SIDE;
#define NEEDLE_STEP 16
uint8_t NEEDLE_DIMMED;
uint8_t NEEDLE_UNDIMMED;
#define NEEDLE_DIMMED_DEFAULT     128
#define NEEDLE_UNDIMMED_DEFAULT   255
#define MAX_DISPLAY 15
uint8_t DISPLAY_DIMMED;
uint8_t DISPLAY_UNDIMMED;
#define DISPLAY_DIMMED_DEFAULT    10
#define DISPLAY_UNDIMMED_DEFAULT  15
float TOTAL_TRIP;
float DAILY_TRIP_A;
float DAILY_TRIP_B;
int CURRENT_SHOW=TRIP_A;

//
float TIRE_CIRCUMFERENCE;

// VSS input pin
#define VSS_PIN 2
unsigned int PULSES = 0;
float LENPERPULSE = 0;

// RPM input pin
#define RPM_PIN 3
volatile unsigned int RPM_COUNT=0;
unsigned long timeold=0;
unsigned int RPMs=0;
float MOTOR_TIME;
unsigned int NOMINAL_RPM; // RPM with 1 hour == 1 motor hour
#define MAX_NOMINAL 6000
#define NOMINAL_STEP 200
float MOTOR_HOURS;
uint16_t MOTOR_HOURS_LIMIT;
#define DEFAULT_MOTOR_HOURS_LIMIT 200
#define MOTOR_HOURS_STEP 10
#define MAX_MOTOR_HOURS 1000
boolean LIMIT_BLINK=true;

// PINS CONFIG
#define BUTTON 12
#define NEEDLE 8
#define DIMPIN 10
#define SETUP_PIN 11

#define LED_dataPin 3
#define LED_registerSelect 4
#define LED_clockPin 5
#define LED_enable 6
#define LED_reset 7

#define LED_displayLength 16 // Two HCMS-297x led matrix displays
LedDisplay myDisplay = LedDisplay(LED_dataPin, LED_registerSelect, LED_clockPin, LED_enable,
                                  LED_reset, LED_displayLength);

#define VISUAL_DELAY      100 // for visual pleasure and correct timed features
#define LOGO_DELAY        500 // firt logo output delay

float LEN=0;
unsigned long TIME,TIMES;
char buffer[20];
bool PRESSED=false;
bool LONGPRESS=false;
bool DIM=false;
bool DIMMED=false;
bool SETUP_PRESSED=false;
uint8_t val;
uint8_t SETUP_POS=0;

#define DISPLAY_TRIP 0
#define DISPLAY_SETUP 1
uint8_t DISPLAY_MODE=DISPLAY_TRIP;

void CalcTire()
{
        TIRE_CIRCUMFERENCE = (TIRE_WIDTH_ARRAY[TIRE_WIDTH]*TIRE_SIDE_ARRAY[TIRE_SIDE]/100+TIRE_RIM_ARRAY[TIRE_RIM]*25.4)*3.1416;
#ifdef DEBUG
        Serial.print("TIRE CIRCUMFERENCE: "); Serial.print(TIRE_CIRCUMFERENCE);
        Serial.print('\n');
#endif
        LENPERPULSE=TIRE_CIRCUMFERENCE/PPR/1000; // in Meters
}

void VSS() // VSS signal interrupt
{
        PULSES++;
}

void RPM() // VSS signal interrupt
{
        RPM_COUNT++;
}

void setup()
{
        wdt_enable(WDTO_1S);
#ifdef DEBUG
        Serial.begin(115200);
#endif
        eeprom.begin();
        myDisplay.begin();
        myDisplay.setBrightness(0);
#ifdef DEBUG
        Serial.print("\n\n-=Ab0VE TECH=-\nHonda Prelude Oddometer\n\n");
#endif
        pinMode(DIMPIN, INPUT);
        pinMode(NEEDLE, OUTPUT);
        pinMode(BUTTON, INPUT_PULLUP);
        pinMode(SETUP_PIN, INPUT_PULLUP);
        pinMode(VSS_PIN, INPUT_PULLUP);
        pinMode(RPM_PIN, INPUT_PULLUP);

        attachInterrupt(digitalPinToInterrupt(VSS_PIN), VSS, RISING);
        attachInterrupt(digitalPinToInterrupt(RPM_PIN), RPM, RISING);

// read config from EEPROM
        eeprom.readBlock(0, (uint8_t*) &TOTAL_TRIP, 4);
        eeprom.readBlock(4, (uint8_t*) &DAILY_TRIP_A, 4);
        eeprom.readBlock(9, (uint8_t*) &DAILY_TRIP_B, 4);
        eeprom.readBlock(15,(uint8_t*) &CURRENT_SHOW, 1);
        eeprom.readBlock(16,(uint8_t*) &TIRE_WIDTH, 2);
        eeprom.readBlock(18,(uint8_t*) &TIRE_SIDE, 1);
        eeprom.readBlock(19,(uint8_t*) &TIRE_RIM, 1);
        eeprom.readBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
        eeprom.readBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
        eeprom.readBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
        eeprom.readBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
        eeprom.readBlock(24,(uint8_t*) &MOTOR_HOURS, 4);
        eeprom.readBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
        eeprom.readBlock(32,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);

        if (isnan(TOTAL_TRIP)) TOTAL_TRIP=0;
        if (isnan(DAILY_TRIP_A)) DAILY_TRIP_A=0;
        if (isnan(DAILY_TRIP_B)) DAILY_TRIP_B=0;
        if (isnan(CURRENT_SHOW)) CURRENT_SHOW=TRIP_A;
        if (isnan(TIRE_WIDTH)) TIRE_WIDTH=TIRE_WIDTH_DEFAULT;
        if (isnan(TIRE_SIDE)) TIRE_SIDE=TIRE_SIDE_DEFAULT;
        if (isnan(TIRE_RIM)) TIRE_RIM=TIRE_RIM_DEFAULT;
        if (isnan(NEEDLE_DIMMED)) NEEDLE_DIMMED=NEEDLE_DIMMED_DEFAULT;
        if (isnan(NEEDLE_UNDIMMED)) NEEDLE_UNDIMMED=NEEDLE_UNDIMMED_DEFAULT;
        if (isnan(DISPLAY_DIMMED)) DISPLAY_DIMMED=DISPLAY_DIMMED_DEFAULT;
        if (isnan(DISPLAY_UNDIMMED)) DISPLAY_UNDIMMED=DISPLAY_UNDIMMED_DEFAULT;
        if (isnan(MOTOR_HOURS)) MOTOR_HOURS=0;
        if (isnan(NOMINAL_RPM)) NOMINAL_RPM=2000;
        if (isnan(MOTOR_HOURS_LIMIT)) MOTOR_HOURS_LIMIT=DEFAULT_MOTOR_HOURS_LIMIT;

#ifdef DEBUG
        Serial.print("EEPROM TOTAL: "); Serial.print(TOTAL_TRIP);
        Serial.print(", A: "); Serial.print(DAILY_TRIP_A);
        Serial.print(", B: "); Serial.print(DAILY_TRIP_B);
        Serial.print('\n');
        Serial.print("EEPROM TIRES: "); Serial.print(TIRE_WIDTH_ARRAY[TIRE_WIDTH]);
        Serial.print("/"); Serial.print(TIRE_SIDE_ARRAY[TIRE_SIDE]);
        Serial.print("R"); Serial.print(TIRE_RIM_ARRAY[TIRE_RIM]);
        Serial.print('\n');
        Serial.print("EEPROM ARROW: Day:"); Serial.print(NEEDLE_UNDIMMED);
        Serial.print("Night:"); Serial.print(NEEDLE_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM DISPLAY: Day:"); Serial.print(DISPLAY_UNDIMMED);
        Serial.print("Night:"); Serial.print(DISPLAY_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM MOTOR: Hours"); Serial.print(MOTOR_HOURS);
        Serial.print("RPM:"); Serial.print(NOMINAL_RPM);
        Serial.print("Limit:"); Serial.print(MOTOR_HOURS_LIMIT);
        Serial.print('\n');
#endif

        CalcTire();
        wdt_reset();

        myDisplay.home();
        myDisplay.print("   HondaPrelude ");
        for (int a=0; a<DISPLAY_UNDIMMED; a++)
        {
                myDisplay.setBrightness(a);
                analogWrite(NEEDLE, a*17);
                delay(60);
        }

        wdt_reset();
        delay(LOGO_DELAY);

// DEFAULT LIGHTS
        uint8_t val=digitalRead(DIMPIN);
        if (val == HIGH) {
                myDisplay.setBrightness(DISPLAY_DIMMED);
                analogWrite(NEEDLE, NEEDLE_DIMMED);
        } else {
                myDisplay.setBrightness(DISPLAY_UNDIMMED);
                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
        }

        wdt_reset();
}

void loop()
{
        wdt_reset();
// RPM calculation
        RPMs = ( RPM_COUNT / (float) (millis() - timeold)) * 1000.0 * 60 / 4; // 4 pulses per one revolution
        timeold = millis();
        RPM_COUNT = 0;
        MOTOR_TIME+=RPMs/NOMINAL_RPM*(millis()- timeold)/1000; // in SEC
        timeold = millis();
        MOTOR_HOURS+=MOTOR_TIME/3600; // in HOURS

// TRIP calculation
        LEN=PULSES*LENPERPULSE;
        PULSES=0;
        TOTAL_TRIP+=LEN;
        DAILY_TRIP_A+=LEN;
        DAILY_TRIP_B+=LEN;
        if (DAILY_TRIP_A>=10000) DAILY_TRIP_A-=10000;
        if (DAILY_TRIP_B>=10000) DAILY_TRIP_B-=10000;
        eeprom.writeBlock(0, (uint8_t*) &TOTAL_TRIP, 4);
        eeprom.writeBlock(4, (uint8_t*) &DAILY_TRIP_A, 4);
        eeprom.writeBlock(9, (uint8_t*) &DAILY_TRIP_B, 4);
        eeprom.writeBlock(24, (uint8_t*) &MOTOR_HOURS, 4);


#ifdef DEBUG
        Serial.print("RPM:"); Serial.print(RPMs);
        Serial.print(" T:"); Serial.print(TOTAL_TRIP);
        Serial.print(" A:"); Serial.print(DAILY_TRIP_A);
        Serial.print(" B:"); Serial.print(DAILY_TRIP_B);
        Serial.print(" M:"); Serial.print(MOTOR_HOURS);
        Serial.println("");
#endif
//

        val=digitalRead(DIMPIN);
        if (val == HIGH) {
                if (!DIMMED) { // JUST DIMMED
                        myDisplay.setBrightness(DISPLAY_DIMMED);
                        analogWrite(NEEDLE, NEEDLE_DIMMED);
                        //**************************************
                        DIMMED=true;
                } else {
                        if (DIMMED) { // JUST UNDIMMED
                                myDisplay.setBrightness(DISPLAY_UNDIMMED);
                                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                //**********************************
                                DIMMED=false;
                        }
                }
        }


        val=digitalRead(SETUP_PIN);
        if (val == HIGH) {
                if (!SETUP_PRESSED) {
                        SETUP_PRESSED=true;
                        TIMES=millis();
                } else {
                        if ((millis()-TIMES)>2000) {
                                TIMES=millis();
                                switch (DISPLAY_MODE) {
                                case DISPLAY_TRIP:
                                        DISPLAY_MODE=DISPLAY_SETUP;
                                        break;
                                default:
                                        //BACK FROM SETUP
                                        eeprom.writeBlock(16,(uint8_t*) &TIRE_WIDTH, 2);
                                        eeprom.writeBlock(18,(uint8_t*) &TIRE_SIDE, 1);
                                        eeprom.writeBlock(19,(uint8_t*) &TIRE_RIM, 1);
                                        eeprom.writeBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
                                        eeprom.writeBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
                                        eeprom.writeBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
                                        eeprom.writeBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
                                        eeprom.writeBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
                                        eeprom.writeBlock(28,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);
                                        DISPLAY_MODE=DISPLAY_TRIP;
                                        CalcTire(); // Recalc circumference size
                                        break;
                                }
                        }
                }
        } else {
                SETUP_PRESSED=false;
        }


        switch (DISPLAY_MODE) {
// TRIP MODE
        case DISPLAY_TRIP:
                val=digitalRead(BUTTON);
                if (val == HIGH) {
                        if (!PRESSED) { // JUST PRESSED
                                PRESSED=true;
                                TIME=millis();
                        } else { // HOLDING
                                if ((millis()-TIME)>2000) {// LOGPRESS
                                        LONGPRESS=true;
                                        switch (CURRENT_SHOW) {
                                        case TRIP_A:
                                                DAILY_TRIP_A=0; break;
                                        case TRIP_B:
                                                DAILY_TRIP_B=0; break;
                                        case MOTOR_HOUR:
                                                MOTOR_HOURS=0; break;
                                        }
                                }
                        }
                } else {
                        if (PRESSED) // JUST RELEASED
                        {
                                PRESSED=false;
                                if (!LONGPRESS) { // SHORTPRESS
                                        CURRENT_SHOW++;
                                        if (CURRENT_SHOW>2) CURRENT_SHOW=0; // A/B/H
                                        eeprom.writeBlock(15,(uint8_t*) &CURRENT_SHOW,2);
                                } else
                                        LONGPRESS=false;
                        }
                }
                myDisplay.home();
                if ((MOTOR_HOURS>=MOTOR_HOURS_LIMIT) && !LIMIT_BLINK) // Motor hours limit
                {
                        sprintf(buffer,"MOTORLMT");
                        LIMIT_BLINK=true;
                } else  // Show total trip
                {
                        dtostrf(TOTAL_TRIP,8, 0, buffer);
                        LIMIT_BLINK=false;
                }
                switch (CURRENT_SHOW) {
                case TRIP_A:
                        buffer[8]='A';
                        dtostrf(DAILY_TRIP_A,7, 1, buffer+9);
                        break;
                case TRIP_B:
                        buffer[8]='B';
                        dtostrf(DAILY_TRIP_B,7, 1, buffer+9);
                        break;
                case MOTOR_HOUR:
                        buffer[8]='M';
                        dtostrf(MOTOR_HOURS,7, 1, buffer+9);
                        break;
                }
                //buffer[16]='\0';
                myDisplay.print(buffer);
                break;

// SETUP MODE
        case DISPLAY_SETUP:

                switch (SETUP_POS) {
                case 0: sprintf(buffer,"Tires  W% 6dmm",TIRE_WIDTH_ARRAY[TIRE_WIDTH]);
                        break;
                case 1: sprintf(buffer,"Tires  S% 7d%%",TIRE_SIDE_ARRAY[TIRE_SIDE]);
                        break;
                case 2: sprintf(buffer,"Tires  R% 7d\"",TIRE_RIM_ARRAY[TIRE_RIM]);
                        break;
                case 3: sprintf(buffer,"ArrowDAY% 8d",NEEDLE_UNDIMMED);
                        break;
                case 4: sprintf(buffer,"ArrowNHT% 8d",NEEDLE_DIMMED);
                        break;
                case 5: sprintf(buffer,"DsplyDAY% 8d",DISPLAY_UNDIMMED);
                        break;
                case 6: sprintf(buffer,"DsplyNHT% 8d",DISPLAY_DIMMED);
                        break;
                case 7: sprintf(buffer,"NrmalRPM% 8d",NOMINAL_RPM);
                        break;
                case 8: sprintf(buffer,"MH Limit% 8d",MOTOR_HOURS_LIMIT);
                        break;
                }

                val=digitalRead(BUTTON);
                if (val == HIGH) {
                        if (!PRESSED) { // JUST PRESSED
                                PRESSED=true;
                                TIME=millis();
                        } else { // HOLDING
                                if ((millis()-TIME)>2000) {// LOGPRESS
                                        LONGPRESS=true;
                                        SETUP_POS++;
                                        if (SETUP_POS > CONFIG_POS_MAX) SETUP_POS=0;
                                        TIME=millis();
                                }
                        }
                } else {
                        if (PRESSED) // JUST RELEASED
                        {
                                PRESSED=false;
                                if (!LONGPRESS) { // SHORTPRESS
                                        switch (SETUP_POS) {
                                        case 0:
                                                TIRE_WIDTH++;
                                                if (TIRE_WIDTH>sizeof(TIRE_WIDTH_ARRAY)/2) TIRE_WIDTH=0;
                                                break;
                                        case 1:
                                                TIRE_SIDE++;
                                                if (TIRE_SIDE>sizeof(TIRE_SIDE_ARRAY)) TIRE_SIDE=0;
                                                break;
                                        case 2:
                                                TIRE_RIM++;
                                                if (TIRE_RIM>sizeof(TIRE_RIM_ARRAY)) TIRE_RIM=0;
                                                break;
                                        case 3:
                                                NEEDLE_UNDIMMED=+NEEDLE_STEP;
                                                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                                break;
                                        case 4:
                                                NEEDLE_DIMMED=+NEEDLE_STEP;
                                                analogWrite(NEEDLE, NEEDLE_DIMMED);
                                                break;
                                        case 5:
                                                DISPLAY_UNDIMMED++;
                                                if (DISPLAY_UNDIMMED>MAX_DISPLAY) DISPLAY_UNDIMMED=0;
                                                myDisplay.setBrightness(DISPLAY_UNDIMMED);
                                                break;
                                        case 6:
                                                DISPLAY_DIMMED++;
                                                if (DISPLAY_DIMMED>MAX_DISPLAY) DISPLAY_DIMMED=0;
                                                myDisplay.setBrightness(DISPLAY_DIMMED);
                                                break;
                                        case 7:
                                                NOMINAL_RPM+=NOMINAL_STEP;
                                                if (NOMINAL_RPM>MAX_NOMINAL) NOMINAL_RPM=NOMINAL_STEP;
                                                break;
                                        case 8:
                                                MOTOR_HOURS_LIMIT+=MOTOR_HOURS_STEP;
                                                if (MOTOR_HOURS_LIMIT>MAX_MOTOR_HOURS) MOTOR_HOURS_LIMIT=MOTOR_HOURS_STEP;
                                                break;
                                        }
                                } else
                                        LONGPRESS=false;
                        }
                }
                myDisplay.home();
                myDisplay.print(buffer);
                break;

        }
        delay(VISUAL_DELAY);
}
