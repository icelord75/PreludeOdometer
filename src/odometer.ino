/*
//      _______ __  _________ _________
//      \_     |  |/  .  \   |   \ ___/
//        / :  |   \  /   \  |   / _>_
//       /      \ | \ |   /  |  /     \
//      /   |___/___/____/ \___/_     /
//      \___/--------TECH--------\___/
//       ==== ABOVE SCIENCE 1994 ====
//
//   Ab0VE TECH - HONDA Prelude Odometer Version 0.22 / Feb-2017
*/

#define DEBUG YES

#include <avr/pgmspace.h>
#include <Arduino.h>
#include <Wire.h>
#include <LedDisplay.h>
#include <I2C_eeprom.h>
#include <avr/wdt.h>


I2C_eeprom eeprom(0x50,16384/8);

#define PPR 17 // VSS pulses per rotation

#define TRIP_A 0
#define TRIP_B 1

// 205/55R15
#define TIRE_WIDTH_DEFAULT 2
#define TIRE_SIDE_DEFAULT  6
#define TIRE_RIM_DEFAULT   1

const PROGMEM uint8_t TIRE_WIDTH_ARRAY[] = { 185, 195, 205, 215, 225, 235, 245, 255 };
const PROGMEM uint8_t TIRE_SIDE_ARRAY[] =  { 25, 30, 35, 40, 45, 50, 55, 60, 65,  70, 75 };
const PROGMEM uint8_t TIRE_RIM_ARRAY[] =   { 14,15,16,17,18,19,20 };

#define CONFIG_POS_MAX 6
// CONFIG
uint8_t TIRE_RIM;
uint8_t TIRE_WIDTH;
uint8_t TIRE_SIDE;
uint8_t NEEDLE_DIMMED;
uint8_t NEEDLE_UNDIMMED;
uint8_t DISPLAY_DIMMED;
uint8_t DISPLAY_UNDIMMED;
float TOTAL_TRIP;
float DAILY_TRIP_A;
float DAILY_TRIP_B;
int CURRENT_SHOW=TRIP_A;

//
float TIRE_CIRCUMFERENCE;

const byte interruptPin = 2;
volatile byte state = LOW;
unsigned int PULSES = 0;
float LENPERPULSE = 0;

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

// MOVE CONFIG TO EEPROM
#define NEEDLE_DIMMED_DEFAULT     128
#define NEEDLE_UNDIMMED_DEFAULT   255
#define DISPLAY_DIMMED_DEFAULT     12
#define DISPLAY_UNDIMMED_DEFAULT  15

#define VISUAL_DELAY      100

float PulsesToMeters(unsigned long Pulses)
{
        float range;
        range=Pulses*TIRE_CIRCUMFERENCE/PPR/1000;
        return range;
}

void CalcTire()
{
        TIRE_CIRCUMFERENCE = (TIRE_WIDTH_ARRAY[TIRE_WIDTH]/50*TIRE_SIDE_ARRAY[TIRE_SIDE]+TIRE_RIM_ARRAY[TIRE_RIM]*25.4)*3.1415;
#ifdef DEBUG
        Serial.print("TIRE CIRCUMFERENCE: "); Serial.print(TIRE_CIRCUMFERENCE);
        Serial.print('\n');
#endif
}

void VSS() // VSS signal interrupt
{
        PULSES++;
        wdt_reset();
}

//boolean SETUP=true;

void setup()
{
        wdt_enable(WDTO_2S);
        Serial.begin(115200);
        eeprom.begin();
        myDisplay.begin();
        myDisplay.setBrightness(0);
        Serial.print("\n\n-=Ab0VE TECH=-\nHonda Prelude Oddometer\n\n");

        pinMode(DIMPIN, INPUT);
        pinMode(BUTTON, INPUT);
        pinMode(NEEDLE, OUTPUT);
        pinMode(SETUP_PIN, INPUT);
        pinMode(interruptPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(interruptPin), VSS, RISING);

        eeprom.readBlock(0, (uint8_t*) &TOTAL_TRIP, 4);   if (isnan(TOTAL_TRIP)) TOTAL_TRIP=0;
        eeprom.readBlock(4, (uint8_t*) &DAILY_TRIP_A, 4); if (isnan(DAILY_TRIP_A)) DAILY_TRIP_A=0;
        eeprom.readBlock(9, (uint8_t*) &DAILY_TRIP_B, 4); if (isnan(DAILY_TRIP_B)) DAILY_TRIP_B=0;
        eeprom.readBlock(15,(uint8_t*) &CURRENT_SHOW, 1);
        eeprom.readBlock(16,(uint8_t*) &TIRE_WIDTH, 1); if (isnan(TIRE_WIDTH)) TIRE_WIDTH=TIRE_WIDTH_DEFAULT;
        eeprom.readBlock(17,(uint8_t*) &TIRE_SIDE, 1);  if (isnan(TIRE_SIDE)) TIRE_SIDE=TIRE_SIDE_DEFAULT;
        eeprom.readBlock(18,(uint8_t*) &TIRE_RIM, 1);   if (isnan(TIRE_RIM)) TIRE_RIM=TIRE_RIM_DEFAULT;
        eeprom.readBlock(19,(uint8_t*) &NEEDLE_DIMMED, 1); if (isnan(NEEDLE_DIMMED)) NEEDLE_DIMMED=NEEDLE_DIMMED_DEFAULT;
        eeprom.readBlock(20,(uint8_t*) &NEEDLE_UNDIMMED, 1); if (isnan(NEEDLE_UNDIMMED)) NEEDLE_UNDIMMED=NEEDLE_UNDIMMED_DEFAULT;
        eeprom.readBlock(21,(uint8_t*) &DISPLAY_DIMMED, 1); if (isnan(DISPLAY_DIMMED)) DISPLAY_DIMMED=DISPLAY_DIMMED_DEFAULT;
        eeprom.readBlock(22,(uint8_t*) &DISPLAY_UNDIMMED, 1); if (isnan(DISPLAY_UNDIMMED)) DISPLAY_UNDIMMED=DISPLAY_UNDIMMED_DEFAULT;

#ifdef DEBUG
        Serial.print("EEPROM TOTAL: "); Serial.print(TOTAL_TRIP);
        Serial.print(", A: "); Serial.print(DAILY_TRIP_A);
        Serial.print(", B: "); Serial.print(DAILY_TRIP_B);
        Serial.print('\n');
        Serial.print("EEPROM TIRES: "); Serial.print(TIRE_WIDTH_ARRAY[TIRE_WIDTH]);
        Serial.print("/"); Serial.print(TIRE_SIDE_ARRAY[TIRE_SIDE]);
        Serial.print("R"); Serial.print(TIRE_RIM_ARRAY[TIRE_RIM]);
        Serial.print('\n');
        Serial.print("EEPROM ARROW: Day:"); Serial.print(NEEDLE_UNDIMMED);  Serial.print("Night:");  Serial.print(NEEDLE_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM DISPLAY: Day:"); Serial.print(DISPLAY_UNDIMMED);  Serial.print("Night:");  Serial.print(DISPLAY_DIMMED);
        Serial.print('\n');

#endif

        CalcTire();
        wdt_reset();
        myDisplay.home();
        myDisplay.print("  Honda Prelude");
        for (int a=0; a<DISPLAY_UNDIMMED; a++)
        {
                myDisplay.setBrightness(a);
                analogWrite(NEEDLE, a*17);
                delay(60);
        }
        wdt_reset();
        delay(500);

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
        LENPERPULSE=0.1;

}

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

void loop()
{
        wdt_reset();
// TRIP CALCULATION
        LEN=PULSES*LENPERPULSE;
        // LEN=PulsesToMeters(PULSES);
        PULSES=0;
        TOTAL_TRIP=TOTAL_TRIP+LEN;
        DAILY_TRIP_A+=LEN;
        DAILY_TRIP_B+=LEN;
        if (DAILY_TRIP_A>=10000) DAILY_TRIP_A-=10000;
        if (DAILY_TRIP_B>=10000) DAILY_TRIP_B-=10000;
        eeprom.writeBlock(0, (uint8_t*) &TOTAL_TRIP, 4);
        eeprom.writeBlock(4, (uint8_t*) &DAILY_TRIP_A, 4);
        eeprom.writeBlock(9, (uint8_t*) &DAILY_TRIP_B, 4);

#ifdef DEBUG
        Serial.print(TOTAL_TRIP);  Serial.print(" ");
        Serial.print(DAILY_TRIP_A);  Serial.print(" ");
        Serial.print(DAILY_TRIP_B);  Serial.println("");
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
                                        eeprom.writeBlock(16,(uint8_t*) &TIRE_WIDTH, 1);
                                        eeprom.writeBlock(17,(uint8_t*) &TIRE_SIDE, 1);
                                        eeprom.writeBlock(18,(uint8_t*) &TIRE_RIM, 1);
                                        eeprom.writeBlock(19,(uint8_t*) &NEEDLE_DIMMED, 1);
                                        eeprom.writeBlock(20,(uint8_t*) &NEEDLE_UNDIMMED, 1);
                                        eeprom.writeBlock(21,(uint8_t*) &DISPLAY_DIMMED, 1);
                                        eeprom.writeBlock(22,(uint8_t*) &DISPLAY_UNDIMMED, 1);
                                        DISPLAY_MODE=DISPLAY_TRIP;
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
                                        if (CURRENT_SHOW==TRIP_A) { DAILY_TRIP_A=0; } else { DAILY_TRIP_B=0;}
                                }
                        }
                } else {
                        if (PRESSED) // JUST RELEASED
                        {
                                PRESSED=false;
                                if (!LONGPRESS) { // SHORTPRESS
                                        if (CURRENT_SHOW==TRIP_A) { CURRENT_SHOW=TRIP_B; }
                                        else { CURRENT_SHOW=TRIP_A; }
                                        eeprom.writeBlock(15,(uint8_t*) &CURRENT_SHOW,2);
                                } else
                                        LONGPRESS=false;
                        }
                }
                myDisplay.home();
                dtostrf(TOTAL_TRIP,8, 0, buffer);
                if (CURRENT_SHOW==TRIP_A) {
                        buffer[8]='A';
                        dtostrf(DAILY_TRIP_A,7, 1, buffer+9);
                } else
                {
                        buffer[8]='B';
                        dtostrf(DAILY_TRIP_B,7, 1, buffer+9);
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
                                                if (TIRE_WIDTH>sizeof(TIRE_WIDTH_ARRAY)) TIRE_WIDTH=0;
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
                                                NEEDLE_UNDIMMED++;
                                                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                                break;
                                        case 4:
                                                NEEDLE_DIMMED++;
                                                analogWrite(NEEDLE, NEEDLE_DIMMED);
                                                break;
                                        case 5:
                                                DISPLAY_UNDIMMED++;
                                                if (DISPLAY_UNDIMMED>15) DISPLAY_UNDIMMED=1;
                                                myDisplay.setBrightness(DISPLAY_UNDIMMED);
                                                break;
                                        case 6:
                                                DISPLAY_DIMMED++;
                                                if (DISPLAY_DIMMED>15) DISPLAY_DIMMED=1;
                                                myDisplay.setBrightness(DISPLAY_DIMMED);
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
