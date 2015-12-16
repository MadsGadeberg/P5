// Inspired by RF12 - https://github.com/jcw/jeelib

#include <stdint.h>
#include "rfhw.h"
#include <Arduino.h>

// Define pins
// SPI ports - change according to processor
// Arduino is SPI master
#if defined(__AVR_ATtiny84__)
	#define RFM_IRQ     	8	// IRQ port (INT0)

	#define SPI_SS      	9 	// Slave select -> Can be changed - pin 3
	#define SPI_MOSI    	5 	// Master out -> Slave in - Pin 7
	#define SPI_MISO    	6 	// Master in -> Slave out - Pin 8
	#define SPI_SCK     	4 	// Clock - Pin 9
#else
	#define RFM_IRQ     	2	// IRQ port (INT0)

	#define SPI_SS      	10 	// Slave select -> Can be changed
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
#define MAX_LEN 		0xfa // Can be changed to minimize memory consumption

// States
#define STATE_TX_BYTE1	0xfd
#define STATE_TX_BYTE0	0xfe
#define STATE_TX_LEN	0xff

#define STATE_IDLE 		0xfb
#define STATE_RX 		0xfc

namespace rf {
	volatile uint8_t phy_state = STATE_IDLE;
	uint8_t phy_buffer[MAX_LEN];
	volatile uint8_t phy_buffer_index = 0;
	volatile uint8_t phy_buffer_len = 0;
	
	uint8_t hw_filter;
	
	static void phy_interrupt();
	void phy_enableRF();
	void phy_disableRF();
	void phy_setStatereceive();
	void phy_setStateIdle();
	void phy_setStateTransmitter();
	void phy_setStateSleep();
	void phy_initSPI();
	void phy_enableIRQ();
	void phy_disableIRQ();
	bool phy_canSend();
	uint16_t phy_sendCMD(uint16_t command);
	uint16_t phy_sendCMDIRQ(uint16_t command);
	uint8_t phy_sendCMDByte(uint8_t out);

	// Init spi
	inline void phy_initSPI() {
		// Set pinmodes for SPI and IRQ
		pinMode(SPI_SS, OUTPUT);
		pinMode(SPI_MOSI, OUTPUT);
    	pinMode(SPI_MISO, INPUT);
    	pinMode(SPI_SCK, OUTPUT);
    	pinMode(RFM_IRQ, INPUT);
    	
    	// Disable RF (SPI)
    	phy_disableRF();
    	
    	#ifdef SPCR
    		// 0x51
			// SPE - Enables the SPI when 1 (&0x40)
    		// MSTR - Sets the Arduino in master mode when 1, slave mode when 0 (&0x10)
    		// SPR1 and SPR0 - Sets the SPI speed, 00 is fastest (4MHz) 11 is slowest (250KHz) (means it's in between) (&0x03)
    		// https://www.arduino.cc/en/Tutorial/SPIEEPROM
    		SPCR = _BV(SPE) | _BV(MSTR); 
    		bitSet(SPCR, SPR0);
    		
    		// use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    		// Comment from Jeelabs RF12
    		// SPI2x (Double SPI Speed) bit
    		// http://avrbeginners.net/architecture/spi/spi.html#spsr
    		SPSR |= _BV(SPI2X);
    	#else
    		// ATTiny does not support  SPCR and SPSR
    		USICR = bit(USIWM0);
    	#endif
    	
    	// Pull IRQ pin up to prevent IRQ on start
    	digitalWrite(RFM_IRQ, HIGH);
	}
	
	void phy_init(uint8_t byte_filter) {
		// Set filter
		hw_filter = byte_filter;
	
		// Init SPI
		phy_initSPI();
    
    	phy_sendCMD(0x0000); // initial SPI transfer added to avoid power-up problem
    	phy_setStateSleep();

    	// wait until RFM12B is out of power-up reset, this takes several *seconds*
    	phy_sendCMD(0xB800); // in case we're still in OOK mode
    	while (digitalRead(RFM_IRQ) == 0)
        	phy_sendCMD(0x0000);

  		// Configuration Setting Command
  		// Band = 1
  		// Enable TX and Enable RX FIFO buffer
  		// Crystal load not in data sheet
    	phy_sendCMD(0x80C7 | (1 << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    
    	// Frequency Setting Command
    	// 12 bytes
    	// FC = 430 + F * 0,0025 MHz
    	// FC is carrier frequency and F is frequency parameter 36 <= F <= 3903
    	phy_sendCMD(0xA000 + 1600); // 96-3960 freq range of values within band
    
    	// Data Rate Command
    	// BR = 10000000/29/(R+1)/(1+cs*7)
    	// cs = &0x80
    	// R = &0x7F
    	phy_sendCMD(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps -- 10000000/29/((0x06)+1)/(1+(fist bit in last byte)*7)
    
    	// Receiver Control Command
    	// VDI output (or interrupt input): p16 = &0x400
    	// Bandwith - see datasheet: &0xE0
    	// VDI response time: &0x300 -> Fast
    	// Select LNA gain: &0x18 -> 0dBm
    	// Select DRSSI treshold: 0x7 -> -91dBm
    	phy_sendCMD(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    
    	// Data Filter Command
    	// Enable clock recovery auto-lock: 0x80
    	// Enable clock recovery fast mode (OFF): 0x40
    	// Filter type: Digital filter: 0x10 
    	// Set DQD threshold: 0x7
    	phy_sendCMD(0xC2AC); // AL,!ml,DIG,DQD4
    
    	// Group
    	// FIFO and Reset Mode Command
    	// Set FIFO interrupt level: 0xF0
    	// Select the length of the synchron pattern: See datasheet -> 0xC (is 0) -> Reprogrammed in next command
    	// Enable FIFO fill -> 0x2 (ON)
    	// Disable hi sensitivity reset mode -> 0x1 (ON)
    	phy_sendCMD(0xCA83); // FIFO8,2-SYNC,!ff,DR
    
    	// Synchron pattern Command - Reprograms byte 0
    	// Byte 0: 0xFF
    	phy_sendCMD(0xCE00 | hw_filter); // SYNC=2DXX；
        
    	// AFC Command
    	// Keep offset when VDI high
    	// Range limit: no restriction
    	// Disable AFC hi accurency mode
    	// Enable AFC output register
    	// Enable AFC function
    	phy_sendCMD(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    	
    	// TX Configuration Control Command
    	// MP (no documentation off what that is - therefore off)
    	// Frequency derivation: 90kHz
    	// Output power: -0dBm (should be max according to RF12)
    	phy_sendCMD(0x9850); // !mp,90kHz,MAX OUT
    	
    	// PLL Setting Command
    	// Selected uC CLK frequency: 2.5 MHz or less
    	// Low power mode for crystal (bootup time is 2 ms (compared to 1 ms as fast time) but uses less power)
    	// Disables the dithering in the PLL loop
    	// Max bit rate: 256 kbps
    	phy_sendCMD(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    	
    	// Set state idle
    	phy_setStateSleep();
    	
    	// Setup interrupt
    	attachInterrupt(0, phy_interrupt, LOW);
	}
	
	static void phy_interrupt() {
		phy_sendCMD(0x0000); // Wake up
	
		if (phy_state == STATE_RX) {
			// Read from receiver
			uint8_t in = phy_sendCMD(0xB000);
			
			if (phy_buffer_index == 0 && phy_buffer_len == 0) {
				// Read length of package
				phy_buffer_len = in; // Implicit casted to uint8_t
				
				// Detect if length is bigger than max length
				if (phy_buffer_len > MAX_LEN) {
					phy_buffer_len = 0;
					// Go to idle and change state
					phy_setStateSleep();
				}
			} else {
				// Read to byte array
				phy_buffer[phy_buffer_index++] = in;
				
				// Detect end of receive
				if (phy_buffer_index >= phy_buffer_len) {
					// Go to sleep but do not change state
					phy_sendCMD(0x8201);
				}
			}
		} else if (phy_state != STATE_IDLE) {
			// Should never interrupt in IDLE mode - but just to be sure
			// Chooce what to send
			uint8_t out;
			
			// Read and update state
			uint8_t state = phy_state++;
			
			if (state == STATE_TX_BYTE1) {
				out = 0x2D;
			} else if (state == STATE_TX_BYTE0) {
				// Byte 0 is the filter set on initilize (Remember byte 1 is sent before byte 0 - see datasheet)
				out = hw_filter;
			} else if (state == STATE_TX_LEN) {
				out = phy_buffer_len;
			} else if (state < phy_buffer_len) {
				// state is 0 indexed
				// phy_buffer_len is 1 indexed
				out = phy_buffer[state];
			} else if (state == phy_buffer_len) {
				// Also send dummy 0xAA byte one time after all data is sent or the data is not received correctly
				// It is possible to perform this sequence without sending a dummy byte (step i.) but after loading the last data byte to the transmit
				// register the PA turn off should be delayed for at least 16 bits time. The clock source of the microcontroller (if the clock is not supplied
				// by the RFM12B) should be stable enough over temperature and voltage to ensure this minimum delay under all
				// operating circumstances. 
				// From http://www.hoperf.com/upload/rf/rfm12b.pdf page 30
				
				// 0xAA chooced by RF12 and is also the default value used in the datasheet
				out = 0xAA;
			} else {
				// Also send byte after RF going to sleep
				out = 0xAA;
			
				// Sleep RF module
				phy_setStateSleep();				
			}
			
			// Send data
			phy_sendCMD(0xB800 | out);
		}
	}
	
	inline void phy_enableRF() {
		digitalWrite(SPI_SS, LOW);
	}
	
	inline void phy_disableRF() {
		digitalWrite(SPI_SS, HIGH);
	}

	inline void phy_setStatereceive() {
		phy_state = STATE_RX;
		
		// Power Management Command
		// Enable receiver: &0x80
		// Enable base band block: &0x40
		// Enable synthesizer: &0x10
		// Enable crystal oscillator: &0x8
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		phy_sendCMD(0x82D9);
	}
	
	/*
	inline void phy_setStateIdle() {
		phy_state = STATE_IDLE;
		phy_sendCMD(0x820D);
	}*/
	
	inline void phy_setStateTransmitter() {
		phy_state = STATE_TX_BYTE1;
		
		// Power Management Command
		// Enable transmitter: &0x20
		// Enable synthesizer: &0x10
		// Enable crystal oscillator: &0x8
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		//phy_sendCMD(0x8239);
		phy_sendCMD(0x823D);
	}
	
	inline void phy_setStateSleep() {
		phy_state = STATE_IDLE;
		
		// Power Management Command
		// Disable clock output of CLK pin: &0x1 (Clock is generated from master)
		phy_sendCMD(0x8201);
	}
	
	bool phy_sendWait(const uint8_t buffer[], uint8_t len) {
		if(phy_send(buffer, len) == false)
			return false;
			
		while (phy_state != STATE_IDLE);
		return true;
	}
	
	bool phy_send(uint8_t byte) {
		uint8_t data[] = { byte };
		return phy_send(data, 1);
	}
	
	bool phy_send(const uint8_t buffer[], uint8_t len) {
		if (len < 0)
			return false;
			
		if (len > MAX_LEN)
			return false;
		
		if (!phy_canSend())
			return false;
			
		// Copy buffer in buffer
		memcpy((void*)phy_buffer, buffer, len);
		
		// Setup buffer index and len
		phy_buffer_index = 0;
		phy_buffer_len = len;
		
		// Set transmitting mode
		phy_setStateTransmitter();
		
		return true;
	}
	
	bool phy_canSend() {
		// Check if we can stop the receiver
		// Status Read Command
		// Antenna tuning signal strength: &0x100
		// Serial.println(phy_sendCMD(0x0000); & 0x0100 == 0);
		// Remember to disable IRQ
		
		// Possible Race condition on phy_state
		if (phy_state == STATE_RX && phy_buffer_index == 0) {// && phy_sendCMDIRQ(0x0000) & 0x0100 == 0) {
			// Set state to sleep - sets phy_state to IDLE
			phy_setStateSleep();
		}
		
		return phy_state == STATE_IDLE;
	}
	
	uint8_t* phy_receive(uint8_t* length) {
		if (phy_state == STATE_RX && phy_buffer_index >= phy_buffer_len && phy_buffer_index != 0) {
			phy_state = STATE_IDLE;
			
			*length = phy_buffer_len;
			phy_buffer_index = 0;
			phy_buffer_len = 0;
			
			return phy_buffer;
		} else if (phy_state == STATE_IDLE) {
			phy_buffer_index = 0;
			phy_buffer_len = 0;
		
			phy_setStatereceive();
		}
		
		return NULL;
	}
	
	void phy_enableIRQ() {
		#ifdef EIMSK
			// Arduino
    		bitClear(EIMSK, INT0);
    	#else
    		// ATtiny
    		bitClear(GIMSK, INT0);
    	#endif
	}
	
	void phy_disableIRQ() {
		#ifdef EIMSK
			// Arduino
    		bitSet(EIMSK, INT0);
    	#else
    		// ATtiny
    		bitSet(GIMSK, INT0);
    	#endif
	}
	
	uint16_t phy_sendCMDIRQ(uint16_t command) {
		phy_disableIRQ();
		
		uint16_t result = phy_sendCMD(command);
		
		phy_enableIRQ();
		
		return result;
	}
	
	uint16_t phy_sendCMD (uint16_t command) {
		// Change spi speed - see https://www.arduino.cc/en/Tutorial/SPIEEPROM
		#if F_CPU > 10000000
    		bitSet(SPCR, SPR0);
		#endif
	
    	phy_enableRF(); // Chip select (activate SPI)
    
    	// Send first 8 bytes and read first 8 bytes reply - then the next 8 bytes
    	uint16_t response = phy_sendCMDByte(command >> 8) << 8;
    	response |= phy_sendCMDByte(command);
    
    	phy_disableRF(); // Chip select (deactivate SPI)
    	
    	#if F_CPU > 10000000
    		bitClear(SPCR, SPR0);
		#endif
    	
    	return response;
	}
	
	// Sends one byte and returns one byte. Please note that RFM12 is expecting 2 bytes for each command
	// https://www.arduino.cc/en/Tutorial/SPIEEPROM
	uint8_t phy_sendCMDByte (uint8_t out) {
		// Arduino
		#ifdef SPDR
			SPDR = out;                    	// Start the transmission
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