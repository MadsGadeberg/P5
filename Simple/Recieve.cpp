#include "RF12.h"
#include <avr/io.h>
#include <util/crc16.h>
#include <avr/sleep.h>
#include <Arduino.h> // Arduino 1.0

// Ports
#define RFM_IRQ     2     // 2=JeeNode, 18=JeeNode pin change
#define SPI_SS      10    // PB2, pin 16
#define SPI_MOSI    11    // PB3, pin 17
#define SPI_MISO    12    // PB4, pin 18
#define SPI_SCK     13    // PB5, pin 19

// Filter
#define FILTER    33

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000

void initRF() {
    Serial.println("INIT RF");
  
    int band = 1;

    // Set pinmodes for SPI and IRQ
    pinMode(SPI_SS, OUTPUT);
    pinMode(SPI_MOSI, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_SCK, OUTPUT);
    pinMode(RFM_IRQ, INPUT);
    
    // Chip select (enable/disable SPI)
    disableSPI();
    
  // 0x50 
  // SPE - Enables the SPI when 1 
    // MSTR - Sets the Arduino in master mode when 1, slave mode when 0
    // SPR1 and SPR0 - Sets the SPI speed, 00 is fastest (4MHz) 11 is slowest (250KHz)
    SPCR = _BV(SPE) | _BV(MSTR); 
    bitSet(SPCR, SPR0);
    
    // use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    SPSR |= _BV(SPI2X);

    digitalWrite(RFM_IRQ, HIGH); // pull-up
    
    // initial SPI transfer added to avoid power-up problem
    sendSPI(0x0000); 
    
    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    sendSPI(RF_TXREG_WRITE); // in case we're still in OOK mode
    while (digitalRead(RFM_IRQ) == 0)
        sendSPI(0x0000);

    sendSPI(0x80C7 | (band << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    sendSPI(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps -- 10000000/29/((0x06)+1)/(1+(fist bit in last byte)*7)
    sendSPI(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    
    // Filter
    sendSPI(0xCA83); // FIFO8,2-SYNC,!ff,DR
    sendSPI(0xCE00 | FILTER); // SYNC=2DXX；
        
    sendSPI(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    sendSPI(0x9850); // !mp,90kHz,MAX OUT
    sendSPI(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0    
    
    // Setup interrupt
    attachInterrupt(0, rf_interrupt, LOW);
}

// Chip select (enable SPI)
void enableSPI() {
  digitalWrite(SPI_SS, LOW);
}

// Chip select (disable SPI)
void disableSPI() {
  digitalWrite(SPI_SS, HIGH);
}

// Send SPI signal
uint16_t sendSPI(uint16_t msg) {
    enableSPI(); // Chip select (activate SPI)
    
    // Get reply from SPI - read first 8 bytes and then the next 8 bytes
    uint16_t reply = rf12_byte(msg >> 8) << 8;
    reply |= rf12_byte(msg);
    
    disableSPI(); // Chip select (deactivate SPI)
    return reply;
}

static uint8_t rf12_byte (uint8_t out) {
  // Data to be transmitted via SPI
    SPDR = out;
    
    // this loop spins 4 usec with a 2 MHz SPI clock
    // Wait for transmission to complete
    while (!(SPSR & _BV(SPIF)))
        ;
        
    // Return recieved bytes
    return SPDR;
}

void rf_interrupt () {
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    // correction: now takes 2 + 8 µs, since sending can be done at 8 MHz
    sendSPI(0x0000);

    uint8_t in = sendSPI(RF_RX_FIFO_READ);

  //if (in == 'a')
    Serial.println((char) in);

  //sendSPI(RF_TXREG_WRITE + out);
}

void setup() {
  Serial.begin(57600);
  initRF();
}

void loop() {
  sendSPI(RF_IDLE_MODE);
  delay(3000);
  Serial.println("Recieve");
  sendSPI(RF_RECEIVER_ON);
  delay(10000);
  Serial.println("Recieve off");
}