// Inspired by RF12 - https://github.com/jcw/jeelib

#include <stdint.h>
#include "rfhw.h"
#include <Arduino.h>

// Define pins
// SPI ports - change according to processor
// Arduino is SPI master
#if defined(__AVR_ATtiny84__)
	#define RFM_IRQ     	8	// IRQ port needs to be equal to interrupt no 0

	#define SPI_SS      	9 	// Slave select -> Can be changed to whatever port needed - pin 3
	#define SPI_MOSI    	5 	// Master out -> Slave in - Pin 7 - Reversed?
	#define SPI_MISO    	6 	// Master in -> Slave out - Pin 8
	#define SPI_SCK     	4 	// Clock - Pin 9
#else
	#define RFM_IRQ     	2	// IRQ port needs to be equal to interrupt no 0

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
#define MAX_LEN 		0xf7 // Can be changed to minimize memory consumption

// States
#define STATE_TX_PRE0	0xfa
#define STATE_TX_PRE1	0xfb
#define STATE_TX_PRE2	0xfc
#define STATE_TX_BYTE0	0xfd
#define STATE_TX_FILTER	0xfe


#define STATE_TX_LEN	0xff

#define STATE_IDLE 		0xf8
#define STATE_RX 		0xf9

// Filter
#define FILTER    20

namespace rf {
	volatile uint8_t hw_state = STATE_IDLE;
	uint8_t hw_buffer[MAX_LEN];
	volatile uint8_t hw_buffer_index = 0;
	volatile uint8_t hw_buffer_len = 0;
	
	static void hw_interrupt();
	void hw_enableRF();
	void hw_disableRF();
	void hw_setStateRecieve();
	void hw_setStateIdle();
	void hw_setStateTransmitter();
	void hw_setStateSleep();
	void hw_initSPI();
	void hw_enableIRQ();
	void hw_disableIRQ();
	bool hw_canSend();
	uint16_t hw_sendCMD(uint16_t command);
	uint16_t hw_sendCMDIRQ(uint16_t command);
	uint8_t hw_sendCMDByte(uint8_t out);

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
    		bitSet(SPCR, SPR0); // Not required -> Remove for faster transfer (see above comment)
    		
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
    	hw_setStateSleep();

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
    	hw_sendCMD(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    	hw_sendCMD(0x9850); // !mp,90kHz,MAX OUT
    	hw_sendCMD(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    	
    	// hw_sendCMD(0xE000); // NOT USE
    	// hw_sendCMD(0xC800); // NOT USE
    	// hw_sendCMD(0xC049); // 1.66MHz,3.1V
    	
    	// Set state idle
    	hw_setStateSleep();
    	
    	// Setup interrupt
    	attachInterrupt(0, hw_interrupt, LOW);
	}
	
	static void hw_interrupt() {
		hw_sendCMD(0x0000); // Wake up
	
		if (hw_state == STATE_RX) {
			// Read from reciever
			uint8_t in = hw_sendCMD(0xB000);
			
			if (hw_buffer_index == 0 && hw_buffer_len == 0) {
				// Read length of package
				hw_buffer_len = in; // Implicit casted to uint8_t
				
				// Detect if length is bigger than max length
				if (hw_buffer_len > MAX_LEN) {
					hw_buffer_len = 0;
					// Go to idle and change state
					hw_setStateSleep();
				}
			} else {
				// Read to byte array
				hw_buffer[hw_buffer_index++] = in;
				
				// Detect end of recieve
				if (hw_buffer_index >= hw_buffer_len) {
					// Go to sleep but do not change state
					hw_sendCMD(0x8201);
				}
			}
		} else if (hw_state != STATE_IDLE) {
			// Should never interrupt in IDLE mode - but just to be sure
			// Chooce what to send
			uint8_t out;
			
			// Read and update state
			uint8_t state = hw_state++;
			
			if (state == STATE_TX_BYTE0) {
				out = 0x2D;
			} else if (state == STATE_TX_FILTER) {
				out = FILTER;
			} else if (state == STATE_TX_LEN) {
				out = hw_buffer_len;
			} else if (state < hw_buffer_len) {
				// state is 0 indexed
				// hw_buffer_len is 1 indexed
				out = hw_buffer[state];
			} else if (state == hw_buffer_len || state == STATE_TX_PRE0 || state == STATE_TX_PRE1 || state == STATE_TX_PRE2) {
				out = 0xAA;
			} else {
				out = 0xAA;
			
				// Sleep RF module
				hw_setStateSleep();				
			}
			
			// Send data
			hw_sendCMD(0xB800 | out);
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
		
		// Power Management Command
		// Enable receiver: &0x80
		// Enable base band block: &0x40
		// Enable synthesizer: &0x10
		// Enable crystal oscillator: &0x8
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		hw_sendCMD(0x82D9);
	}
	
	/*
	inline void hw_setStateIdle() {
		hw_state = STATE_IDLE;
		hw_sendCMD(0x820D);
	}*/
	
	inline void hw_setStateTransmitter() {
		hw_state = STATE_TX_BYTE0;//STATE_TX_PRE0;
		
		// Power Management Command
		// Enable transmitter: &0x20
		// Enable synthesizer: &0x10
		// Enable crystal oscillator: &0x8
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		//hw_sendCMD(0x8239);
		
		hw_sendCMD(0x823D);
	}
	
	inline void hw_setStateSleep() {
		hw_state = STATE_IDLE;
		
		// Power Management Command
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		hw_sendCMD(0x8201);
	}
	
	bool hw_sendWait(const uint8_t buffer[], uint8_t len) {
		if(hw_send(buffer, len) == false)
			return false;
			
		while (hw_state != STATE_IDLE);
		return true;
	}
	
	bool hw_send(uint8_t byte) {
		uint8_t data[] = { byte };
		return hw_send(data, 1);
	}
	
	bool hw_send(const uint8_t buffer[], uint8_t len) {
		if (len < 0)
			return false;
			
		if (len > MAX_LEN)
			return false;
		
		if (!hw_canSend())
			return false;
			
		// Copy buffer in buffer
		memcpy((void*)hw_buffer, buffer, len);
		
		// Setup buffer index and len
		hw_buffer_index = 0;
		hw_buffer_len = len;
		
		// Set transmitting mode
		hw_setStateTransmitter();
		
		return true;
	}
	
	bool hw_canSend() {
		// Check if we can stop the reciever
		// Status Read Command
		// Antenna tuning signal strength: &0x100
		// Serial.println(hw_sendCMD(0x0000); & 0x0100 == 0);
		// Remember to disable IRQ
		
		// Possible Race condition on hw_state and hw_buffer_index
		if (hw_state == STATE_RX && hw_buffer_index == 0) {// && hw_sendCMDIRQ(0x0000) & 0x0100 == 0) {
			// Set state to sleep - sets hw_state to IDLE
			hw_setStateSleep();
		}
		
		return hw_state == STATE_IDLE;
	}
	
	uint8_t* hw_recieve(uint8_t* length) {
		if (hw_state == STATE_RX && hw_buffer_index >= hw_buffer_len && hw_buffer_index != 0) {
			hw_state = STATE_IDLE;
			
			*length = hw_buffer_len;
			hw_buffer_index = 0;
			hw_buffer_len = 0;
			
			return hw_buffer;
		} else if (hw_state == STATE_IDLE) {
			hw_buffer_index = 0;
			hw_buffer_len = 0;
		
			hw_setStateRecieve();
		}
		
		return NULL;
	}
	
	void hw_enableIRQ() {
		#ifdef EIMSK
			// Arduino
    		bitClear(EIMSK, INT0);
    	#else
    		// ATtiny
    		bitClear(GIMSK, INT0);
    	#endif
	}
	
	void hw_disableIRQ() {
		#ifdef EIMSK
			// Arduino
    		bitSet(EIMSK, INT0);
    	#else
    		// ATtiny
    		bitSet(GIMSK, INT0);
    	#endif
	}
	
	uint16_t hw_sendCMDIRQ(uint16_t command) {
		hw_disableIRQ();
		
		uint16_t result = hw_sendCMD(command);
		
		hw_enableIRQ();
		
		return result;
	}
	
	uint16_t hw_sendCMD (uint16_t command) {
		// Change spi speed - see https://www.arduino.cc/en/Tutorial/SPIEEPROM
		#if F_CPU > 10000000
    		bitSet(SPCR, SPR0);
		#endif
	
    	hw_enableRF(); // Chip select (activate SPI)
    
    	// Send first 8 bytes and read first 8 bytes reply - then the next 8 bytes
    	uint16_t response = hw_sendCMDByte(command >> 8) << 8;
    	response |= hw_sendCMDByte(command);
    
    	hw_disableRF(); // Chip select (deactivate SPI)
    	
    	#if F_CPU > 10000000
    		bitClear(SPCR, SPR0);
		#endif
    	
    	return response;
	}
	
	// Sends one byte and returns one byte. Please note that RFM12 is expecting 2 bytes for each command
	// https://www.arduino.cc/en/Tutorial/SPIEEPROM
	uint8_t hw_sendCMDByte (uint8_t out) {
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