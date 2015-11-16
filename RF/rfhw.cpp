// Inspired by RF12 - https://github.com/jcw/jeelib

#include <stdint.h>
#include "rfhw.h"
#include <Arduino.h>

// Define pins
// IRQ port needs to be equal to interrupt no 0
#define RFM_IRQ     	2

// SPI ports - change according to processor
// Arduino is SPI master
#if defined(__AVR_ATtiny84__)
	#define SPI_SS      	9 	// Slave select -> Can be changed to whatever port needed - pin 3
	#define SPI_MOSI    	5 	// Master out -> Slave in - Pin 7 - Reversed?
	#define SPI_MISO    	6 	// Master in -> Slave out - Pin 8
	#define SPI_SCK     	4 	// Clock - Pin 9
#else
	#define SPI_SS      	10 	// Slave select -> Can be changed to whatever port needed
	#define SPI_MOSI    	11 	// Master out -> Slave in
	#define SPI_MISO    	12 	// Master in -> Slave out
	#define SPI_SCK     	13 	// Clock
#endif

// Configuration Setting Command (frequency = 433 MHz)
#define RF_BAND 		0x10

// Frequency Setting Command (frequency correction)
// Fc=430+F*0.0025 MHz = approx 434 MHz
#define RF_FREQUENCY 	1600

// MAX_LEN for packet
// uint8_t used for states
#define MAX_LEN 		0xfa

// States
#define STATE_TX_BYTE0	0xfd
#define STATE_TX_FILTER	0xfe
#define STATE_TX_LEN	0xff

#define STATE_IDLE 		0xfb
#define STATE_RX 		0xfc

// Filter
#define FILTER    20

namespace rf {
	uint8_t hw_state = STATE_IDLE;
	char hw_buffer[MAX_LEN];
	uint8_t hw_buffer_index = 0;
	uint8_t hw_buffer_len = 0;
	
	void hw_interrupt();
	void hw_enableRF();
	void hw_disableRF();
	void hw_setStateRecieve();
	void hw_setStateIdle();
	void hw_setStateTransmitter();
	void hw_setStateSleep();
	void hw_initSPI();
	uint16_t hw_sendCMD(uint16_t command);
	char hw_sendCMDByte(char out);

	// Init spi
	inline void hw_initSPI() {
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
    	#ifdef SPCR
    		SPCR = _BV(SPE) | _BV(MSTR); 
    		// bitSet(SPCR, SPR0); // Not required -> Remove for faster transfer (see above comment)
    		
    		// use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    		// Comment from Jeelabs RF12
    		// SPI2x (Double SPI Speed) bit
    		// http://avrbeginners.net/architecture/spi/spi.html#spsr
    		SPSR |= _BV(SPI2X); // Not required -> can be removed (slower speed)
    	#else
    		// ATTiny does not support  SPCR and SPSR
    		USICR = bit(USIWM0);
    	#endif
    	
    	// Pull IRQ pin up to prevent IRQ on start
    	digitalWrite(RFM_IRQ, HIGH);
	}
	
	// Identifier handled in protocol
	void hw_init(uint8_t byte_filter) {
		// Init SPI
		hw_initSPI();
    
    	hw_sendCMD(0x0000); // initial SPI transfer added to avoid power-up problem
    	hw_setStateSleep(); // DC (disable clk pin), enable lbd

    	// wait until RFM12B is out of power-up reset, this takes several *seconds*
    	hw_sendCMD(0xB800); // in case we're still in OOK mode
    	while (digitalRead(RFM_IRQ) == 0)
        	hw_sendCMD(0x0000);

  		// Configuration Setting Command
  		// Band = 1
  		// Enable TX and Enable RX FIFO buffer
  		// Crystal load not in data sheet
    	hw_sendCMD(0x80C7 | (1 << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    
    	// Frequency Setting Command
    	// 12 bytes
    	// FC = 430 + F * 0,0025 MHz
    	// FC is carrier frequency and F is frequency parameter 36 <= F <= 3903
    	hw_sendCMD(0xA000 + 1600); // 96-3960 freq range of values within band
    
    	// Data Rate Command
    	// BR = 10000000/29/(R+1)/(1+cs*7)
    	// cs = &0x80
    	// R = &0x7F
    	hw_sendCMD(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps -- 10000000/29/((0x06)+1)/(1+(fist bit in last byte)*7)
    
    	// Receiver Control Command
    	// VDI output (or interrupt input): p16 = &0x400
    	// Bandwith - see datasheet: &0xE0
    	// VDI response time: &0x300 -> Fast
    	// Select LNA gain: &0x18 -> 0dBm
    	// Select DRSSI treshold: 0x7 -> -91dBm
    	hw_sendCMD(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    
    	// Data Filter Command
    	// Enable clock recovery auto-lock: 0x80
    	// Enable clock recovery fast mode (OFF): 0x40
    	// Filter type: Digital filter: 0x10 
    	// Set DQD threshold: 0x7
    	hw_sendCMD(0xC2AC); // AL,!ml,DIG,DQD4
    
    	// Group
    	// FIFO and Reset Mode Command
    	// Set FIFO interrupt level: 0xF0
    	// Select the length of the synchron pattern: See datasheet -> 0xC (is 0) -> Reprogrammed in next command
    	// Enable FIFO fill -> 0x2 (ON)
    	// Disable hi sensitivity reset mode -> 0x1 (ON)
    	hw_sendCMD(0xCA83); // FIFO8,2-SYNC,!ff,DR
    
    	// Synchron pattern Command - Reprograms byte 0
    	// Byte 0: 0xFF
    	hw_sendCMD(0xCE00 | FILTER); // SYNC=2DXX；
        
    	// AFC Command
    	// 
    	hw_sendCMD(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    	hw_sendCMD(0x9850); // !mp,90kHz,MAX OUT
    	hw_sendCMD(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    	//hw_sendCMD(0xE000); // NOT USE
    	//hw_sendCMD(0xC800); // NOT USE
    	//hw_sendCMD(0xC049); // 1.66MHz,3.1V
    	
    	// Setup interrupt
    	attachInterrupt(0, hw_interrupt, LOW);
	}
	
	void hw_interrupt() {
		if (hw_state == STATE_RX) {
			// Read from reciever
			char in = hw_sendCMD(0xB000);
			
			if (hw_buffer_index == 0 && hw_buffer_len == 0) {
				// Read length of package
				hw_buffer_len = in; // Implicit casted to uint8_t
				
				// Detect if length is bigger than max length
				if (hw_buffer_len > MAX_LEN) {
					hw_buffer_len = 0;
				}
			} else {
				// Read to byte array
				hw_buffer[hw_buffer_index++] = in;
				
				// Detect end of recieve
				if (hw_buffer_index >= hw_buffer_len) {
					// Go to idle but do not change state
					hw_sendCMD(0x820D);
				}
			}
		} else if (hw_state != STATE_IDLE) {
			// Should never interrupt in IDLE mode - but just to be sure
			// Chooce what to send
			char out;
			if (hw_state == STATE_TX_BYTE0) {
				out = 0x2D;
			} else if (hw_state == STATE_TX_FILTER) {
				out = FILTER;
			} else if (hw_state == STATE_TX_LEN) {
				out = hw_buffer_len;
			} else if (hw_state < hw_buffer_len) {
				// hw_state is 0 indexed
				// hw_buffer_len is 1 indexed
				out = hw_buffer[hw_state];
			} else {
				hw_setStateIdle();
				return;
			}
			
			// Send data
			hw_sendCMD(0xB800 | out);
			
			// Change state
			hw_state++;
		}
	}
	
	inline void hw_enableRF() {
		digitalWrite(SPI_SS, LOW);
	}
	
	inline void hw_disableRF() {
		digitalWrite(SPI_SS, HIGH);
	}

	inline void hw_setStateRecieve() {
		hw_state = STATE_RX;
		hw_sendCMD(0x82DD);
	}
	
	inline void hw_setStateIdle() {
		hw_state = STATE_IDLE;
		hw_sendCMD(0x820D);
	}
	
	inline void hw_setStateTransmitter() {
		hw_state = STATE_TX_BYTE0;
		hw_sendCMD(0x823D);
	}
	
	inline void hw_setStateSleep() {
	
	}
	
	inline bool hw_send(char byte) {
		char data[] = { byte };
		return hw_send(data, 1);
	}
	
	bool hw_send(const char buffer[], uint8_t len) {
		if (len < 0)
			return false;
			
		if (len > MAX_LEN)
			return false;
			
		if (hw_state != STATE_IDLE) 
			return false;
			
		// Copy buffer in buffer
		memcpy(hw_buffer, buffer, len);
		
		// Setup buffer index and len
		hw_buffer_index = 0;
		hw_buffer_len = len;
		
		// Set transmitting mode
		hw_setStateTransmitter();
		
		return true;
	}
	
	bool hw_canSend() {
		return hw_state == STATE_IDLE;
	}
	
	char* hw_recieve(uint8_t* length) {
		if (hw_state == STATE_RX && hw_buffer_index >= hw_buffer_len) {
			hw_state = STATE_IDLE;
			
			*length = hw_buffer_len;
			hw_buffer_index = 0;
			
			return hw_buffer;
		} else if (hw_state == STATE_IDLE) {
			hw_setStateRecieve();
		}
		
		return NULL;
	}
	
	uint16_t hw_sendCMD (uint16_t command) {
    	hw_enableRF(); // Chip select (activate SPI)
    
    	// Send first 8 bytes and read first 8 bytes reply - then the next 8 bytes
    	uint16_t response = hw_sendCMDByte(command >> 8) << 8;
    	response |= hw_sendCMDByte(command);
    
    	hw_disableRF(); // Chip select (deactivate SPI)
    	
    	return response;
	}
	
	// Sends one byte and returns one byte. Please note that RFM12 is expecting 2 bytes for each command
	// https://www.arduino.cc/en/Tutorial/SPIEEPROM
	char hw_sendCMDByte (char out) {
		// Arduino
		#ifdef SPDR
			SPDR = out;                    // Start the transmission
  			while (!(SPSR & (1<<SPIF)));    // Wait for the end of the transmission
  			return SPDR;                   	// return the received byte
  		#else
  			// ATtiny
    		USIDR = out;
    		byte v1 = bit(USIWM0) | bit(USITC);
    		byte v2 = bit(USIWM0) | bit(USITC) | bit(USICLK);
    		
    		for (uint8_t i = 0; i < 8; ++i) {
        		USICR = v1;
        		USICR = v2;
    		}
    		return USIDR;
  		#endif
	}
}