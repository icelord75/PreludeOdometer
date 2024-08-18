#include <Arduino.h>

#ifndef TWI_FREQ
#define TWI_FREQ 100000L    // 100 KHz (max 400 KHz)
#endif

#define USE_INTERNAL_PULLUP 1

// TWI Actions
#define TWI_START           0
#define TWI_RESTART         1
#define TWI_STOP            2
#define TWI_TRANSMIT        3
#define TWI_RECEIVE_ACK     4
#define TWI_RECEIVE_NACK    5

// TWI status codes
#define TW_MT_SLA_ACK     0x18
#define TW_MT_DATA_ACK    0x28
#define TW_MR_SLA_ACK     0x40
#define TW_MR_DATA_ACK    0x50
#define TW_MR_DATA_NACK   0x58

void twi_start(uint32_t twi_freq = TWI_FREQ);

// Twi init
void twi_start(uint32_t twi_freq )
{
#if(USE_INTERNAL_PULLUP)
  // activate internal pullups for twi.
  DDRC  &=  ~ (( 1  <<  4 ) | ( 1  <<  5 )) ;
  PORTC  |=  ( 1  <<  4 ) | ( 1  <<  5 );
#endif

  // set TWI Freq
  TWSR &= ~(_BV(TWPS0) | _BV(TWPS1)); // clear prescaler bits
  TWBR = ((F_CPU / twi_freq) - 16) / 2;
}

// perform TWI action and wait for TWCR change
// return status code
uint8_t twi(uint8_t action)
{
  switch (action)
  {
    case TWI_START:
    case TWI_RESTART:
      TWCR = _BV(TWSTA) | _BV(TWEN) | _BV(TWINT);// Если нужно прерывание | _BV(TWIE);
      break;
    case TWI_STOP:
      TWCR = _BV(TWSTO) | _BV(TWEN) | _BV(TWINT);// | _BV(TWIE);
      break;
    case TWI_TRANSMIT:
      TWCR = _BV(TWEN) | _BV(TWINT);// | _BV(TWIE);
      break;
    case TWI_RECEIVE_ACK:
      TWCR = _BV(TWEN) | _BV(TWINT) | _BV(TWEA);//| _BV(TWIE);
      break;
    case TWI_RECEIVE_NACK:
      TWCR = _BV(TWEN) | _BV(TWINT);// | _BV(TWIE);
      break;
  }
  if (action != TWI_STOP) while (!(TWCR & _BV(TWINT)));
  return (TWSR & 0xF8);
}

// transmit address or data byte via TWI
// return 1 with success, 0 - error
bool transmit_byte_I2C(uint8_t data, uint8_t OK_code)
{
  TWDR = data;
  return (OK_code == twi(TWI_TRANSMIT));
}

// read data from TWI
// return 1 with success, 0 - error
bool receive_byte_I2C(uint8_t* ptr, bool ack)
{ bool res_code = false;

  if (ack) res_code = (TW_MR_DATA_ACK == twi(TWI_RECEIVE_ACK));
  else res_code = (TW_MR_DATA_NACK == twi(TWI_RECEIVE_NACK));
  if (res_code) *ptr = TWDR;
  return res_code;
}


// write data bytes to EEPROM
// disk - 0x50-0x57, startAddress - 0x00 - 0xFF, &data - data ptr, len - (sizeof(data)
// return 1 with success, 0 - error
byte FM24C_write(byte disk, byte startAddress, void *data, unsigned int len) {
  byte* ptr = (byte*) data;
  twi(TWI_START);
  bool twi_OK = false;
  twi_OK = transmit_byte_I2C(disk << 1, TW_MT_SLA_ACK);
  if (twi_OK) twi_OK = transmit_byte_I2C(startAddress, TW_MT_DATA_ACK);

  while (twi_OK && len) {
    twi_OK = transmit_byte_I2C(*ptr, TW_MT_DATA_ACK );
    ptr++;
    len--;
  }

  twi(TWI_STOP);
  return twi_OK;
}


// read data bytes from EEPROM
// disk - 0x50-0x57, startAddress - 0x00 - 0xFF, &data - data ptr, len - (sizeof(data)
// return - number of bytes readed or 0 if error
byte FM24C_read(byte disk, byte startAddress, void *data, unsigned int len) {
  byte rdata = 0;
  byte *p;

  twi(TWI_START);
  bool twi_OK = false;
  twi_OK = transmit_byte_I2C(disk << 1, TW_MT_SLA_ACK);
  if (twi_OK) twi_OK = transmit_byte_I2C(startAddress, TW_MT_DATA_ACK);

  if (twi_OK) {
    twi(TWI_RESTART);
    twi_OK = transmit_byte_I2C(((disk << 1) | 1), TW_MR_SLA_ACK);
    for (rdata = 0, p = (byte*)data; (twi_OK && rdata < len); rdata++, p++) {

      twi_OK = receive_byte_I2C(p, rdata < (len - 1));

    }
  }
  twi(TWI_STOP);
  return (rdata);
}