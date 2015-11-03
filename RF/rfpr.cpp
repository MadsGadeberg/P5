#include "rfhw.h"

// Define pins
// IRQ port needs to be equal to interrupt no 0
#define RFM_IRQ     2

// SPI ports - change according to processor
// Arduino is SPI master
#define SPI_SS      10 // Slave select -> Can be changed to whatever port needed
#define SPI_MOSI    11 // Master out -> Slave in
#define SPI_MISO    12 // Master in -> Slave out
#define SPI_SCK     13 // Clock

// Configuration Setting Command (frequency = 433 MHz)
#define RF_BAND 0x10

// Frequency Setting Command (frequency correction)
// Fc=430+F*0.0025 MHz = approx 434 MHz
#define RF_FREQUENCY 1600

namespace rf {
	// Init spi
	void hw_initSPI() {
		// Set pinmodes for SPI and IRQ
		pinMode(SPI_SS, OUTPUT);
		pinMode(SPI_MOSI, OUTPUT);
    	pinMode(SPI_MISO, INPUT);
    	pinMode(SPI_SCK, OUTPUT);
    	pinMode(RFM_IRQ, INPUT);
    	
    	// Disable RF (SPI)
    	hw_disableRF();
    	
    	// 0x51
		// SPE - Enables the SPI when 1 (&0x40)
    	// MSTR - Sets the Arduino in master mode when 1, slave mode when 0 (&0x10)
    	// SPR1 and SPR0 - Sets the SPI speed, 00 is fastest (4MHz) 11 is slowest (250KHz) (means it's in between) (&0x03)
    	// https://www.arduino.cc/en/Tutorial/SPIEEPROM
    	SPCR = _BV(SPE) | _BV(MSTR); 
    	bitSet(SPCR, SPR0); // Not required -> Remove for faster transfer (see above comment)
    	
    	// use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    	// Comment from Jeelabs RF12
    	// SPI2x (Double SPI Speed) bit
    	// http://avrbeginners.net/architecture/spi/spi.html#spsr
    	SPSR |= _BV(SPI2X); // Not required -> can be removed (slower speed)
    	
    	// Pull IRQ pin up to prevent IRQ on start
    	digitalWrite(RFM_IRQ, HIGH);
	}
	
	// Identifier handled in protocol
	void hw_init(uint8_t byte_filter) {
		// Init SPI
		hw_initSPI();
		
		
	}
	
	inline void hw_enableRF() {
		digitalWrite(SPI_SS, LOW);
	}
	
	inline void hw_disableRF() {
		digitalWrite(SPI_SS, HIGH);
	}
}