/*
   //      _______ __  _________ _________
   //      \_     |  |/  .  \   |   \ ___/
   //        / :  |   \  /   \  |   / _>_
   //       /      \ | \ |   /  |  /     \
   //      /   |___/___/____/ \___/_     /
   //      \___/--------TECH--------\___/
   //        ==== ABOVE SINCE 1994 ====
   //
   //   Ab0VE TECH - HONDA Prelude Odometer
 */

/* Controller connections
   //            +-----------+
   //            • TX    Vin •
   //            • RX  A Gnd •  <- GND
   //            • RST R RST •
   //     GND -> • GND D  +5 •  <- +5V Reg. LM2596HV
   //     VSS -> • 2   U  A7 •
   //     RPM -> • 3   I  A6 •
   // LED DAT <- • 4   N  A5 • -> FRAM SCL
   // LED Reg <- • 5   O  A4 • -> FRAM SDA
   // LED Clk <- • 6      A3 •
   //  LED En <- • 7   N  A2 •
   // LED RST -> • 8   A  A1 •
   //  Needle <- • 9   N  A0 • <- DIMMING
   // Indiglo <- • 10  O Arf •
   //   Setup -> • 11    3V3 •
   //  Button -> • 12 ||| 13 • <- OneWire DS1820
   //            +-----------+
 */

// TODO
//   racelogic bum edition )))
//   longterm l/100km
//   instant l/100km

#define DEBUG
//#define OPTIBOOT    // OPTIBOOT installed on board

#include <avr/pgmspace.h>
#include <Arduino.h>
#include <font5x7.h> // Local font lib
#include <LedDisplay.h>
#include <I2C_eeprom.h> // platformio lib install "I2C_EEPROM"
#ifdef OPTIBOOT
#include <avr/wdt.h>
#endif

#include <OneWire.h> // platformio lib install "OneWire"
OneWire ds(13);                   // DS1820 Temperature sensor

I2C_eeprom eeprom(0x50,16384/8);  // FM24C16A FRAM

#define PPR 15 // VSS pulses per axle revolution

#define TRIP_A              0
#define TRIP_B              1
#define MOTOR_HOUR          2
#define OUTSIDE_TEMP        3

#define MAX_SHOW            3

// default tire size 205/55R15
#define TIRE_WIDTH_DEFAULT 4
#define TIRE_SIDE_DEFAULT  6
#define TIRE_RIM_DEFAULT   2

uint16_t TIRE_WIDTH_ARRAY[] = { 165, 175, 185, 195, 205, 215, 225, 235, 245, 255, 265, 275, 285, 295, 305 };
uint8_t TIRE_SIDE_ARRAY[] =   { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85 };
uint8_t TIRE_RIM_ARRAY[] =    { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };

#define CONFIG_POS_MAX 11

// CONFIG
uint8_t TIRE_RIM;
uint8_t TIRE_WIDTH;
uint8_t TIRE_SIDE;
#define NEEDLE_STEP 16
#define NEEDLE_DIMMED_DEFAULT     127
#define NEEDLE_UNDIMMED_DEFAULT   255
uint8_t NEEDLE_DIMMED;
uint8_t NEEDLE_UNDIMMED;

#define MIN_DISPLAY 1 // Minimal visiable display
#define MAX_DISPLAY 15
#define DISPLAY_DIMMED_DEFAULT    10
#define DISPLAY_UNDIMMED_DEFAULT  15
uint8_t DISPLAY_DIMMED;
uint8_t DISPLAY_UNDIMMED;

#define INDIGLO_STEP 16
#define INDIGLO_DIMMED_DEFAULT    127
#define INDIGLO_UNDIMMED_DEFAULT  255
uint8_t INDIGLO_DIMMED;
uint8_t INDIGLO_UNDIMMED;

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
uint8_t DIMMING;

// PINS CONFIG
#define BUTTON 12
#define NEEDLE 9
#define DIMPIN 14 // A0 as DIGITAL_PIN
#define SETUP_PIN 11
#define INDIGLO 10

#define LED_dataPin 4
#define LED_registerSelect 5
#define LED_clockPin 6
#define LED_enable 7
#define LED_reset 8

#define LED_displayLength 16  // Two HCMS-297x led matrix displays
LedDisplay myDisplay = LedDisplay(LED_dataPin, LED_registerSelect, LED_clockPin, LED_enable,
                                  LED_reset, LED_displayLength);

#define LOGO_DELAY        500 // firt logo output delay
#define LONGPRESS_TIME    1000

float LEN=0;
float TIRE_CIRCUMFERENCE;
float TEMPERATURE=0;
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
uint8_t data[12];
uint8_t addr[8];
uint8_t type_s;

#define DISPLAY_TRIP 0
#define DISPLAY_SETUP 1
uint8_t DISPLAY_MODE=DISPLAY_TRIP;
uint8_t DEFAULT_BRIGHTNESS;
uint8_t DEFAULT_NEEDLE;
uint8_t DEFAULT_INDIGLO;

void CalcTire()
{
        TIRE_CIRCUMFERENCE = (TIRE_WIDTH_ARRAY[TIRE_WIDTH]*TIRE_SIDE_ARRAY[TIRE_SIDE]/100+TIRE_RIM_ARRAY[TIRE_RIM]*25.4)*3.1416/1000;
        LENPERPULSE=TIRE_CIRCUMFERENCE/PPR; // in Meters
#ifdef DEBUG
        Serial.print("TIRE CIRCUMFERENCE: "); Serial.print(TIRE_CIRCUMFERENCE);
        Serial.println("m");
        Serial.print("LEN PER PULSE: "); Serial.print(LENPERPULSE);
        Serial.println("m");
#endif
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
        for ( int a=1; a<LED_displayLength/4; a++ )
                myDisplay.loadControlRegister(B01110000 + (bright & B00001111));
}

void setup()
{
#ifdef OPTIBOOT
        wdt_disable();
#endif
#ifdef DEBUG
        Serial.begin(115200);
#endif
        eeprom.begin();
        myDisplay.begin();
        setBrightness(0);
#ifdef DEBUG
        Serial.print("\n\nAb0VE-TECH\nHonda Prelude Oddometer\n\n");
#endif
        pinMode(DIMPIN, INPUT);
        pinMode(NEEDLE, OUTPUT);
        pinMode(INDIGLO, OUTPUT);
        pinMode(BUTTON, INPUT_PULLUP);
        pinMode(SETUP_PIN, INPUT_PULLUP);
        pinMode(VSS_PIN, INPUT_PULLUP);
        pinMode(RPM_PIN, INPUT_PULLUP);

        attachInterrupt(digitalPinToInterrupt(VSS_PIN), VSS, RISING);  // positive tiggering
        attachInterrupt(digitalPinToInterrupt(RPM_PIN), RPM, FALLING); // negative tiggering

        // Start DS
        if ( !ds.search(addr))
                ds.reset_search();
        if (addr[0] == 0x10)
                type_s=1;
        else
                type_s=0;
        ds.reset();
        ds.select(addr);
        ds.write(0x44, 1);

        // read config from EEPROM
        eeprom.readBlock(0, (uint8_t*) &TOTAL_TRIP, 4);
        eeprom.readBlock(4, (uint8_t*) &DAILY_TRIP_A, 4);
        eeprom.readBlock(9, (uint8_t*) &DAILY_TRIP_B, 4);
        eeprom.readBlock(200,(uint8_t*) &CURRENT_SHOW, 1);
        eeprom.readBlock(37,(uint8_t*) &TIRE_WIDTH, 1);
        eeprom.readBlock(18,(uint8_t*) &TIRE_SIDE, 1);
        eeprom.readBlock(19,(uint8_t*) &TIRE_RIM, 1);
        eeprom.readBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
        eeprom.readBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
        eeprom.readBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
        eeprom.readBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
        eeprom.readBlock(24,(uint8_t*) &MOTOR_HOURS, 4);
        eeprom.readBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
        eeprom.readBlock(32,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);
        eeprom.readBlock(34,(uint8_t*) &LEADING_ZERO, 1);
        eeprom.readBlock(35,(uint8_t*) &INDIGLO_DIMMED, 1);
        eeprom.readBlock(36,(uint8_t*) &INDIGLO_UNDIMMED, 1);

#ifdef DEBUG
        Serial.print("EEPROM TOTAL: "); Serial.print(TOTAL_TRIP);
        Serial.print(", A: "); Serial.print(DAILY_TRIP_A);
        Serial.print(", B: "); Serial.print(DAILY_TRIP_B);
        Serial.print('\n');
        Serial.print("EEPROM TIRES: "); Serial.print(TIRE_WIDTH);
        Serial.print("/"); Serial.print(TIRE_SIDE);
        Serial.print("R"); Serial.print(TIRE_RIM);
        Serial.print('\n');
        Serial.print("EEPROM NEEDLE Day:"); Serial.print(NEEDLE_UNDIMMED);
        Serial.print(" Night:"); Serial.print(NEEDLE_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM DISPLAY: Day:"); Serial.print(DISPLAY_UNDIMMED);
        Serial.print(" Night:"); Serial.print(DISPLAY_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM INDIGLO: Day:"); Serial.print(INDIGLO_UNDIMMED);
        Serial.print(" Night:"); Serial.print(INDIGLO_DIMMED);
        Serial.print('\n');
        Serial.print("EEPROM MOTOR: Hours"); Serial.print(MOTOR_HOURS);
        Serial.print(" RPM:"); Serial.print(NOMINAL_RPM);
        Serial.print(" Limit:"); Serial.print(MOTOR_HOURS_LIMIT);
        Serial.print('\n');
#endif

// fix posible eeprom errata
        if (TIRE_WIDTH>=sizeof(TIRE_WIDTH_ARRAY)/2) TIRE_WIDTH=TIRE_WIDTH_DEFAULT;
        if (TIRE_SIDE>=sizeof(TIRE_SIDE_ARRAY)) TIRE_SIDE=TIRE_SIDE_DEFAULT;
        if (TIRE_RIM>=sizeof(TIRE_RIM_ARRAY)) TIRE_RIM=TIRE_RIM_DEFAULT;
        if (NEEDLE_DIMMED<1) NEEDLE_DIMMED=NEEDLE_DIMMED_DEFAULT;
        if (NEEDLE_UNDIMMED<1) NEEDLE_UNDIMMED=NEEDLE_UNDIMMED_DEFAULT;
        if (DISPLAY_DIMMED<1) DISPLAY_DIMMED=DISPLAY_DIMMED_DEFAULT;
        if (DISPLAY_UNDIMMED<1) DISPLAY_UNDIMMED=DISPLAY_UNDIMMED_DEFAULT;
        if (INDIGLO_DIMMED<1) INDIGLO_DIMMED=INDIGLO_DIMMED_DEFAULT;
        if (INDIGLO_UNDIMMED<1) INDIGLO_UNDIMMED=INDIGLO_UNDIMMED_DEFAULT;
        if (NOMINAL_RPM<500) NOMINAL_RPM=2000;
        if (MOTOR_HOURS_LIMIT<100) MOTOR_HOURS_LIMIT=DEFAULT_MOTOR_HOURS_LIMIT;
        LEADING_ZERO=LEADING_ZERO & 1;
        if (CURRENT_SHOW>MAX_SHOW) CURRENT_SHOW=0;

        CalcTire();
        myDisplay.home();
        setBrightness(0);
        myDisplay.print("   HondaPrelude ");
        for ( int a=1; a<=DISPLAY_UNDIMMED; a++) {
                setBrightness(a);
                analogWrite(NEEDLE,  a * 17);
                analogWrite(INDIGLO, a * 17);
                delay(50);
        }
        DEFAULT_BRIGHTNESS=DISPLAY_UNDIMMED;
        DEFAULT_NEEDLE=NEEDLE_UNDIMMED;
        DEFAULT_INDIGLO=INDIGLO_UNDIMMED;
        delay(LOGO_DELAY);
#ifdef OPTIBOOT
        wdt_enable(WDTO_500MS);
#endif
}

void ReadTemp()
{
        ds.reset();
        ds.select(addr);
        ds.write(0xBE); // Read Scratchpad

        for ( uint8_t i = 0; i < 9; i++) {   // we need 9 bytes
                data[i] = ds.read();
        }
        int16_t raw = (data[1] << 8) | data[0];
        if (type_s) {
                raw = raw << 3; // 9 bit resolution default
                if (data[7] == 0x10) {
                        // "count remain" gives full 12 bit resolution
                        raw = (raw & 0xFFF0) + 12 - data[6];
                }
        } else {
                byte cfg = (data[4] & 0x60);
                // at lower res, the low bits are undefined, so let's zero them
                if (cfg == 0x00) raw = raw & ~7; // 9 bit resolution, 93.75 ms
                else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
                else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
                //// default is 12 bit resolution, 750 ms conversion time
        }
        TEMPERATURE = (float)raw / 16.0;
}

void loop()
{
#ifdef OPTIBOOT
        wdt_reset();
#endif

// Temperature
        ReadTemp();

// RPM calculation
        if (RPM_COUNT!=0)
        {
                RPMs = ( RPM_COUNT / (float) (millis() - timeold)) * 1000.0 * 60 / 4; // 4 pulses per one revolution
                MOTOR_TIME = RPMs/NOMINAL_RPM*(millis()- timeold)/1000; // in SEC
                RPM_COUNT = 0;
                timeold = millis();
                MOTOR_HOURS += MOTOR_TIME/3600; // in HOURS
        }

// TRIP calculation
        if (PULSES!=0) {
#ifdef DEBUG
                Serial.print("PULSES: "); Serial.print(PULSES);
                Serial.println("");
                Serial.print("TIRE CIRCUMFERENCE: "); Serial.print(TIRE_CIRCUMFERENCE);
                Serial.println("m");
#endif
                LEN=PULSES*LENPERPULSE;
                PULSES=0;
                TOTAL_TRIP+=LEN/1000; // in Km
                DAILY_TRIP_A+=LEN/1000; // in Km
                DAILY_TRIP_B+=LEN/1000; // in Km
                if (DAILY_TRIP_A>=10000) DAILY_TRIP_A-=10000;
                if (DAILY_TRIP_B>=10000) DAILY_TRIP_B-=10000;
        }

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
        Serial.print(" TEMP:"); Serial.print(TEMPERATURE);
        Serial.println("");
#endif

        DIMMING=digitalRead(DIMPIN);
        if (DIMMING == HIGH) {
                if (!DIMMED) { // JUST DIMMED and fade in
                        if (DEFAULT_BRIGHTNESS>DISPLAY_DIMMED) DEFAULT_BRIGHTNESS--;
                        if (DEFAULT_NEEDLE>NEEDLE_DIMMED) DEFAULT_NEEDLE-=NEEDLE_STEP;
                        if (DEFAULT_INDIGLO>INDIGLO_DIMMED) DEFAULT_INDIGLO-=INDIGLO_STEP;
                        if ((DEFAULT_BRIGHTNESS==DISPLAY_DIMMED) && (DEFAULT_NEEDLE<=NEEDLE_DIMMED) && (DEFAULT_INDIGLO<=INDIGLO_DIMMED)) DIMMED=true;
                        setBrightness(DEFAULT_BRIGHTNESS);
                        analogWrite(NEEDLE, DEFAULT_NEEDLE);
                        analogWrite(INDIGLO, DEFAULT_INDIGLO);
                }
          } else {
                if (DIMMED) { // JUST UNDIMMED and fade out
                        if (DEFAULT_BRIGHTNESS<DISPLAY_UNDIMMED) DEFAULT_BRIGHTNESS++;
                        if (DEFAULT_NEEDLE<NEEDLE_UNDIMMED) DEFAULT_NEEDLE+=NEEDLE_STEP;
                        if (DEFAULT_INDIGLO<INDIGLO_UNDIMMED) DEFAULT_INDIGLO+=INDIGLO_STEP;
                        if ((DEFAULT_BRIGHTNESS>=DISPLAY_UNDIMMED) && (DEFAULT_NEEDLE>=NEEDLE_UNDIMMED) && (DEFAULT_INDIGLO>=INDIGLO_UNDIMMED)) DIMMED=false;
                        setBrightness(DEFAULT_BRIGHTNESS);
                        analogWrite(NEEDLE, DEFAULT_NEEDLE);
                        analogWrite(INDIGLO, DEFAULT_INDIGLO);
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
                                        //BACK FROM SETUP
                                        eeprom.writeBlock(37,(uint8_t*) &TIRE_WIDTH, 1);
                                        eeprom.writeBlock(18,(uint8_t*) &TIRE_SIDE, 1);
                                        eeprom.writeBlock(19,(uint8_t*) &TIRE_RIM, 1);
                                        eeprom.writeBlock(20,(uint8_t*) &NEEDLE_DIMMED, 1);
                                        eeprom.writeBlock(21,(uint8_t*) &NEEDLE_UNDIMMED, 1);
                                        eeprom.writeBlock(22,(uint8_t*) &DISPLAY_DIMMED, 1);
                                        eeprom.writeBlock(23,(uint8_t*) &DISPLAY_UNDIMMED, 1);
                                        eeprom.writeBlock(28,(uint8_t*) &NOMINAL_RPM, 4);
                                        eeprom.writeBlock(32,(uint8_t*) &MOTOR_HOURS_LIMIT, 2);
                                        eeprom.writeBlock(34,(uint8_t*) &LEADING_ZERO, 1);
                                        eeprom.writeBlock(35,(uint8_t*) &INDIGLO_DIMMED, 1);
                                        eeprom.writeBlock(36,(uint8_t*) &INDIGLO_UNDIMMED, 1);
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
                                        if (CURRENT_SHOW>MAX_SHOW) CURRENT_SHOW=0; // A/B/H/T
                                        eeprom.writeBlock(200,(uint8_t*) &CURRENT_SHOW,2);
                                } else
                                        LONGPRESS=false;
                        }
                }

                myDisplay.home();
                if ((MOTOR_HOURS>=MOTOR_HOURS_LIMIT) && !LIMIT_BLINK) // Motor hours limit
                {
                        sprintf(buffer,"MOTORLMT");
                        LIMIT_BLINK=true;
                        setBrightness(DISPLAY_UNDIMMED_DEFAULT);
                        analogWrite(NEEDLE,NEEDLE_UNDIMMED_DEFAULT);
                        analogWrite(INDIGLO,INDIGLO_UNDIMMED_DEFAULT);
                } else  // Show total trip
                {
                        if (LEADING_ZERO)
                        {
                                sprintf(buffer,"%08f",TOTAL_TRIP);
                        } else {
                                dtostrf(TOTAL_TRIP,8, 0, buffer);
                        }
                        if (LIMIT_BLINK)
                        {
                                if (DIMMING == HIGH)
                                {
                                        setBrightness(DISPLAY_DIMMED);
                                        analogWrite(NEEDLE, NEEDLE_DIMMED);
                                        analogWrite(INDIGLO, INDIGLO_DIMMED);
                                } else {
                                        setBrightness(DISPLAY_UNDIMMED);
                                        analogWrite(NEEDLE, NEEDLE_UNDIMMED);
                                        analogWrite(INDIGLO, INDIGLO_UNDIMMED);
                                }
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
                        dtostrf(MOTOR_HOURS,7, 1, buffer+9);
                        break;
                case OUTSIDE_TEMP:
                        buffer[8]='T';
                        dtostrf(TEMPERATURE,5, 1, buffer+9);
                        buffer[14]=127; // &deg;
                        buffer[15]='C';
                        break;
                }
                buffer[16]='\0';
                myDisplay.print(buffer);
#ifdef DEBUG
                Serial.print("DISPLAY:  ");
                Serial.println(buffer);
#endif
                break;
// SETUP MODE
        case DISPLAY_SETUP:
                sprintf(buffer,"                ");
                switch (SETUP_POS) {
                case 0: sprintf(buffer,"   Tires  % 3dmm",TIRE_WIDTH_ARRAY[TIRE_WIDTH]);
                        break;
                case 1: sprintf(buffer,"   Tires    /%d%%",TIRE_SIDE_ARRAY[TIRE_SIDE]);
                        break;
                case 2: sprintf(buffer,"   Tires    R%d\"",TIRE_RIM_ARRAY[TIRE_RIM]);
                        break;
                case 3: sprintf(buffer,"  NeedleDay% 5d ",NEEDLE_UNDIMMED);
                        break;
                case 4: sprintf(buffer,"  NeedleNght% 3d ",NEEDLE_DIMMED);
                        break;
                case 5: sprintf(buffer," DisplayDay% 5d ",DISPLAY_UNDIMMED);
                        break;
                case 6: sprintf(buffer," DisplayNight% 3d ",DISPLAY_DIMMED);
                        break;
                case 7: sprintf(buffer," IndigloDay% 5d ",INDIGLO_UNDIMMED);
                        break;
                case 8: sprintf(buffer," IndigloNght% 3d ",INDIGLO_DIMMED);
                        break;
                case 9: sprintf(buffer," NominalRPM% 5d ",NOMINAL_RPM);
                        break;
                case 10: sprintf(buffer,"MotrHourLmt% 4d ",MOTOR_HOURS_LIMIT);
                        break;
                case 11: sprintf(buffer," LeadingZero=%d  ",LEADING_ZERO);
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
                                                if (TIRE_WIDTH>=sizeof(TIRE_WIDTH_ARRAY)/2) TIRE_WIDTH=0;
                                                break;
                                        case 1:
                                                TIRE_SIDE++;
                                                if (TIRE_SIDE>=sizeof(TIRE_SIDE_ARRAY)) TIRE_SIDE=0;
                                                break;
                                        case 2:
                                                TIRE_RIM++;
                                                if (TIRE_RIM>=sizeof(TIRE_RIM_ARRAY)) TIRE_RIM=0;
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
                                                if (DISPLAY_UNDIMMED>MAX_DISPLAY) DISPLAY_UNDIMMED=MIN_DISPLAY;
                                                setBrightness(DISPLAY_UNDIMMED);
                                                break;
                                        case 6:
                                                DISPLAY_DIMMED++;
                                                if (DISPLAY_DIMMED>MAX_DISPLAY) DISPLAY_DIMMED=MIN_DISPLAY;
                                                setBrightness(DISPLAY_DIMMED);
                                                break;
                                        case 7:
                                                INDIGLO_UNDIMMED+=INDIGLO_STEP;
                                                analogWrite(INDIGLO,INDIGLO_UNDIMMED);
                                                break;
                                        case 8:
                                                INDIGLO_DIMMED+=INDIGLO_STEP;
                                                analogWrite(INDIGLO,INDIGLO_DIMMED);
                                                break;
                                        case 9:
                                                NOMINAL_RPM+=NOMINAL_STEP;
                                                if (NOMINAL_RPM>MAX_NOMINAL) NOMINAL_RPM=NOMINAL_STEP;
                                                break;
                                        case 10:
                                                MOTOR_HOURS_LIMIT+=MOTOR_HOURS_STEP;
                                                if (MOTOR_HOURS_LIMIT>MAX_MOTOR_HOURS) MOTOR_HOURS_LIMIT=MOTOR_HOURS_STEP;
                                                break;
                                        case 11:
                                                LEADING_ZERO=!LEADING_ZERO;
                                                break;
                                        }
                                } else
                                        LONGPRESS=false;
                        }
                }
                myDisplay.home();
                myDisplay.print(buffer);
#ifdef DEBUG
                Serial.print("DISPLAY:  ");
                Serial.println(buffer);
#endif
                break;
        }
}
