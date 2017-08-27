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
   Motor hours alarm     ❏
 *************************/

/* Controller connections
   //            +-----------+
   //            • TX    Vin •
   //            • RX  A Gnd •  <- GND
   //            • RST R RST •
   //     GND -> • GND D  +5 •  <- +5V Reg. LM2596HV
   //     VSS -> • 2   U  A7 •
   //   Tacho -> • 3   I  A6 •
   // LED DAT <- • 4   N  A5 • -> FRAM SCL
   // LED Reg <- • 5   O  A4 • -> FRAM SDA
   // LED Clk <- • 6      A3 •
   //  LED En <- • 7   N  A2 •
   // LED RST -> • 8   A  A1 •
   //  Needle <- • 9   N  A0 •
   //  DIMING -> • 10  O Arf •
   //   Setup -> • 11    3V3 •
   //  Button -> • 12 ||| 13 •
   //            +-----------+
 */

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

uint16_t TIRE_WIDTH_ARRAY[] = { 165, 175, 185, 195, 205, 215, 225, 235, 245, 255, 265, 275, 285, 295, 305 };
uint8_t TIRE_SIDE_ARRAY[] =   { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85 };
uint8_t TIRE_RIM_ARRAY[] =    { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };

#define CONFIG_POS_MAX 9

// CONFIG
uint8_t TIRE_RIM;
uint8_t TIRE_WIDTH;
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
double TOTAL_TRIP;
double DAILY_TRIP_A;
double DAILY_TRIP_B;
int CURRENT_SHOW=TRIP_A;
boolean LEADING_ZERO;

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
#define NEEDLE 9
#define DIMPIN 10
#define SETUP_PIN 11

#define LED_dataPin 4
#define LED_registerSelect 5
#define LED_clockPin 6
#define LED_enable 7
#define LED_reset 8

#define LED_displayLength 16 // Two HCMS-297x led matrix displays
LedDisplay myDisplay = LedDisplay(LED_dataPin, LED_registerSelect, LED_clockPin, LED_enable,
                                  LED_reset, LED_displayLength);

#define VISUAL_DELAY      10 // for visual pleasure and correct timed features
#define LOGO_DELAY        500 // firt logo output delay
#define LONGPRESS_TIME    1000

float LEN=0;
float TIRE_CIRCUMFERENCE;
unsigned long TIME,TIMES;
char buffer[20];
bool PRESSED=false;
bool LONGPRESS=false;
bool DIM=false;
bool DIMMED=false;
bool SETUP_PRESSED=false;
bool SETUP_DO=false;
uint8_t val;
uint8_t SETUP_POS=0;

#define DISPLAY_TRIP 0
#define DISPLAY_SETUP 1
uint8_t DISPLAY_MODE=DISPLAY_TRIP;

void CalcTire()
{
        TIRE_CIRCUMFERENCE = (TIRE_WIDTH_ARRAY[TIRE_WIDTH]*TIRE_SIDE_ARRAY[TIRE_SIDE]/100+TIRE_RIM_ARRAY[TIRE_RIM]*25.4)*3.1416/1000;
#ifdef DEBUG
        Serial.print("TIRE CIRCUMFERENCE: "); Serial.print(TIRE_CIRCUMFERENCE);
        Serial.println("m");
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

void setBrightness(uint8_t bright)
{
        // set the brightness: for each 4 chars
        for (int a=1; a<LED_displayLength/4; a++ )
                myDisplay.loadControlRegister(B01110000 + (bright && B01111));
}

void setup()
{
        wdt_enable(WDTO_1S);
#ifdef DEBUG
        Serial.begin(115200);
#endif
        eeprom.begin();
        myDisplay.begin();
        setBrightness(0);
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
        eeprom.readBlock(16,(uint8_t*) &TIRE_WIDTH, 1);
        eeprom.readBlock(18,(uint8_t*) &TIRE_SIDE, 1);
        eeprom.readBlock(19,(uint8_t*) &TIRE_RIM, 1);
        eeprom.readBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
        eeprom.readBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
        eeprom.readBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
        eeprom.readBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
        eeprom.readBlock(24,(uint8_t*) &MOTOR_HOURS, 4);
        eeprom.readBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
        eeprom.readBlock(32,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);
        eeprom.readBlock(33,(uint8_t*) &LEADING_ZERO, 1);

#ifdef DEBUG
        Serial.print("EEPROM TOTAL: "); Serial.print(TOTAL_TRIP);
        Serial.print(", A: "); Serial.print(DAILY_TRIP_A);
        Serial.print(", B: "); Serial.print(DAILY_TRIP_B);
        Serial.print('\n');
        Serial.print("EEPROM TIRES: "); Serial.print(TIRE_WIDTH);
        Serial.print("/"); Serial.print(TIRE_SIDE);
        Serial.print("R"); Serial.print(TIRE_RIM);
        Serial.print('\n');
        Serial.print("Night:"); Serial.print(NEEDLE_DIMMED);
        Serial.print("EEPROM NEEDLE: Day:"); Serial.print(NEEDLE_UNDIMMED);
        Serial.print('\n');
        Serial.print("EEPROM DISPLAY: Day:"); Serial.print(DISPLAY_UNDIMMED);
        Serial.print("Night:"); Serial.print(DISPLAY_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM MOTOR: Hours"); Serial.print(MOTOR_HOURS);
        Serial.print("RPM:"); Serial.print(NOMINAL_RPM);
        Serial.print("Limit:"); Serial.print(MOTOR_HOURS_LIMIT);
        Serial.print('\n');
#endif

        if (TIRE_WIDTH>=sizeof(TIRE_WIDTH_ARRAY)/2) TIRE_WIDTH=TIRE_WIDTH_DEFAULT;
        if (TIRE_SIDE>=sizeof(TIRE_SIDE_ARRAY)) TIRE_SIDE=TIRE_SIDE_DEFAULT;
        if (TIRE_RIM>=sizeof(TIRE_RIM_ARRAY)) TIRE_RIM=TIRE_RIM_DEFAULT;
        if (NEEDLE_DIMMED<1) NEEDLE_DIMMED=NEEDLE_DIMMED_DEFAULT;
        if (NEEDLE_UNDIMMED<1) NEEDLE_UNDIMMED=NEEDLE_UNDIMMED_DEFAULT;
        if (DISPLAY_DIMMED<1) DISPLAY_DIMMED=DISPLAY_DIMMED_DEFAULT;
        if (DISPLAY_UNDIMMED<1) DISPLAY_UNDIMMED=DISPLAY_UNDIMMED_DEFAULT;
        if (NOMINAL_RPM<500) NOMINAL_RPM=2000;
        if (MOTOR_HOURS_LIMIT<100) MOTOR_HOURS_LIMIT=DEFAULT_MOTOR_HOURS_LIMIT;
        LEADING_ZERO=LEADING_ZERO & 1;
        CalcTire();
        wdt_reset();

        myDisplay.home();
        setBrightness(0);
        myDisplay.print("   HondaPrelude ");
        for (int a=0; a<DISPLAY_UNDIMMED; a++)
        {
                setBrightness(a);
                analogWrite(NEEDLE, a*17);
                delay(60);
        }

        wdt_reset();
        delay(LOGO_DELAY);

// DEFAULT LIGHTS
        uint8_t val=digitalRead(DIMPIN);
        if (val == HIGH) {
                setBrightness(DISPLAY_DIMMED);
                analogWrite(NEEDLE, NEEDLE_DIMMED);
        } else {
                setBrightness(DISPLAY_UNDIMMED);
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

/// Store in FRAM in each cycle
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

        val=digitalRead(DIMPIN);
        if (val == HIGH) {
                if (!DIMMED) { // JUST DIMMED
                        setBrightness(DISPLAY_DIMMED);
                        analogWrite(NEEDLE, NEEDLE_DIMMED);
                        DIMMED=true;
                } else {
                        if (DIMMED) { // JUST UNDIMMED
                                setBrightness(DISPLAY_UNDIMMED);
                                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                DIMMED=false;
                        }
                }
        }

        val=digitalRead(SETUP_PIN);
        if (val == LOW) {
                if (!SETUP_PRESSED) {
                        SETUP_PRESSED=true;
                        SETUP_DO=true;
                        TIMES=millis();
                } else {
                        if ((millis()-TIMES)>LONGPRESS_TIME) {
                                TIMES=millis();
                                switch (DISPLAY_MODE) {
                                case DISPLAY_TRIP:
                                        DISPLAY_MODE=DISPLAY_SETUP;
                                        SETUP_DO=false;
                                        break;
                                default:
                                #ifdef DEBUG
                                        Serial.println("Write SETUP");
                                        #endif
                                        //BACK FROM SETUP
                                        eeprom.writeBlock(16,(uint8_t*) &TIRE_WIDTH, 1);
                                        eeprom.writeBlock(18,(uint8_t*) &TIRE_SIDE, 1);
                                        eeprom.writeBlock(19,(uint8_t*) &TIRE_RIM, 1);
                                        eeprom.writeBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
                                        eeprom.writeBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
                                        eeprom.writeBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
                                        eeprom.writeBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
                                        eeprom.writeBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
                                        eeprom.writeBlock(32,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);
                                        eeprom.writeBlock(34,(uint8_t*) &LEADING_ZERO, 1);
                                        DISPLAY_MODE=DISPLAY_TRIP;
                                        SETUP_DO=false;
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
                if (val == LOW) {
                        if (!PRESSED) { // JUST PRESSED
                          #ifdef DEBUG
                                Serial.println("MODE CHANGE");
                          #endif
                                PRESSED=true;
                                TIME=millis();
                        } else { // HOLDING
                                if ((millis()-TIME)>LONGPRESS_TIME) {// LOGPRESS
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
                        if (LEADING_ZERO)
                        {
                                sprintf(buffer,"%08d",(int)TOTAL_TRIP);
                        } else {
                                dtostrf(TOTAL_TRIP,8, 0, buffer);
                        }
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
                        dtostrf(MOTOR_HOURS,7, 2, buffer+9);
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
                case 3: sprintf(buffer,"NeedlDAY% 8d",NEEDLE_UNDIMMED);
                        break;
                case 4: sprintf(buffer,"NeedlNHT% 8d",NEEDLE_DIMMED);
                        break;
                case 5: sprintf(buffer,"DsplyDAY% 8d",DISPLAY_UNDIMMED);
                        break;
                case 6: sprintf(buffer,"DsplyNHT% 8d",DISPLAY_DIMMED);
                        break;
                case 7: sprintf(buffer,"NrmalRPM% 8d",NOMINAL_RPM);
                        break;
                case 8: sprintf(buffer,"MH Limit% 8d",MOTOR_HOURS_LIMIT);
                        break;
                case 9: sprintf(buffer,"Leading Zero=%d  ",LEADING_ZERO);
                        break;
                }

                val=digitalRead(BUTTON);
                if ((val == LOW) || SETUP_DO) {
                        if (!PRESSED && (val==LOW)) { // JUST PRESSED
                                PRESSED=true;
                                TIME=millis();
                        } else { // HOLDING
                                if (( SETUP_DO && !SETUP_PRESSED) || (millis()-TIME)>LONGPRESS_TIME) {// LOGPRESS
                                        SETUP_DO=false;
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
                                                NEEDLE_UNDIMMED+=NEEDLE_STEP;
                                                analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                                break;
                                        case 4:
                                                NEEDLE_DIMMED+=NEEDLE_STEP;
                                                analogWrite(NEEDLE, NEEDLE_DIMMED);
                                                break;
                                        case 5:
                                                DISPLAY_UNDIMMED++;
                                                if (DISPLAY_UNDIMMED>MAX_DISPLAY) DISPLAY_UNDIMMED=0;
                                                setBrightness(DISPLAY_UNDIMMED);
                                                break;
                                        case 6:
                                                DISPLAY_DIMMED++;
                                                if (DISPLAY_DIMMED>MAX_DISPLAY) DISPLAY_DIMMED=0;
                                                setBrightness(DISPLAY_DIMMED);
                                                break;
                                        case 7:
                                                NOMINAL_RPM+=NOMINAL_STEP;
                                                if (NOMINAL_RPM>MAX_NOMINAL) NOMINAL_RPM=NOMINAL_STEP;
                                                break;
                                        case 8:
                                                MOTOR_HOURS_LIMIT+=MOTOR_HOURS_STEP;
                                                if (MOTOR_HOURS_LIMIT>MAX_MOTOR_HOURS) MOTOR_HOURS_LIMIT=MOTOR_HOURS_STEP;
                                                break;
                                        case 9:
                                                LEADING_ZERO=!LEADING_ZERO;
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
