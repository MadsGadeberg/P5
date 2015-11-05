/// @file
/// RFM12B driver implementation
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include "RF12.h"
#include <avr/io.h>
#include <util/crc16.h>
#include <avr/sleep.h>
#include <Arduino.h> // Arduino 1.0

#define rf12_rawlen     rf12_len
#define crc_initVal     ~0
#define crc_update      _crc16_update

// maximum transmit / receive buffer: 3 header + data + 2 crc bytes
#define RF_MAX   (RF12_MAXDATA + 5)

// pins used for the RFM12B interface - yes, there *is* logic in this madness:
//
//  - leave RFM_IRQ set to the pin which corresponds with INT0, because the
//    current driver code will use attachInterrupt() to hook into that
//  - use SS_DDR, SS_PORT, and SS_BIT to define the pin you will be using as
//    select pin for the RFM12B (you're free to set them to anything you like)
//  - please leave SPI_SS, SPI_MOSI, SPI_MISO, and SPI_SCK as is, i.e. pointing
//    to the hardware-supported SPI pins on the ATmega, *including* SPI_SS !
// ATmega168, ATmega328, etc.
#define RFM_IRQ     2     // 2=JeeNode, 18=JeeNode pin change

#define SPI_SS      10    // PB2, pin 16
#define SPI_MOSI    11    // PB3, pin 17
#define SPI_MISO    12    // PB4, pin 18
#define SPI_SCK     13    // PB5, pin 19

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000

// RF12 status bits
#define RF_RSSI_BIT     0x0100

// bits in the node id configuration byte
#define NODE_ID         RF12_HDR_MASK // id of this node, as A..Z or 1..31

// transceiver states, these determine what to do with each interrupt
enum {
    TXCRC1, TXCRC2, TXTAIL, TXDONE, TXIDLE,
    TXRECV,
    TXPRE1, TXPRE2, TXPRE3, TXSYN1, TXSYN2,
};

static uint8_t nodeid;              // address of this node
static uint8_t group;               // network group
static volatile uint8_t rxfill;     // number of data bytes in rf12_buf
static volatile int8_t rxstate;     // current transceiver state

volatile uint16_t rf12_crc;         // running crc value
volatile uint8_t rf12_buf[RF_MAX];  // recv/xmit buf, including hdr & crc bytes
static uint8_t rf12_fixed_pkt_len;  // fixed packet length reception

/// @details
/// Initialise the SPI port for use by the RF12 driver.
void rf12_spiInit () {
	// Set pinmodes for SPI and IRQ
	pinMode(SPI_SS, OUTPUT);
	pinMode(SPI_MOSI, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_SCK, OUTPUT);
    pinMode(RFM_IRQ, INPUT);

	// Chip select (deactivate SPI)
	digitalWrite(SPI_SS, HIGH);
	
	// 0x50 
	// SPE - Enables the SPI when 1 
    // MSTR - Sets the Arduino in master mode when 1, slave mode when 0
    // SPR1 and SPR0 - Sets the SPI speed, 00 is fastest (4MHz) 11 is slowest (250KHz)
    SPCR = _BV(SPE) | _BV(MSTR); 
    bitSet(SPCR, SPR0);
    
    // use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    // SPI2x (Double SPI Speed) bit
    // http://avrbeginners.net/architecture/spi/spi.html#spsr
    SPSR |= _BV(SPI2X);
    
    //digitalWrite(RFM_IRQ, HIGH); // pull-up
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

static uint16_t rf12_xfer (uint16_t cmd) {
    digitalWrite(SPI_SS, LOW); // Chip select (activate SPI)
    
    // Get reply from SPI - read first 8 bytes and then the next 8 bytes
    uint16_t reply = rf12_byte(cmd >> 8) << 8;
    reply |= rf12_byte(cmd);
    
    digitalWrite(SPI_SS, HIGH); // Chip select (deactivate SPI)
    return reply;
}

/// @details
/// This call provides direct access to the RFM12B registers. If you're careful
/// to avoid configuring the wireless module in a way which stops the driver
/// from functioning, this can be used to adjust frequencies, power levels,
/// RSSI threshold, etc. See the RFM12B wireless module documentation.
///
/// This call will briefly disable interrupts to avoid clashes on the SPI bus.
///
/// Returns the 16-bit value returned by SPI. Probably only useful with a
/// "0x0000" status poll command.
/// @param cmd RF12 command, topmost bits determines which register is affected.
uint16_t rf12_control(uint16_t cmd) {
    bitClear(EIMSK, INT0); // Interrupt 0 off
   	uint16_t r = rf12_xfer(cmd);
    bitSet(EIMSK, INT0); // Interrupt 0 on
    return r;
}

static void rf12_interrupt () {
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    // correction: now takes 2 + 8 µs, since sending can be done at 8 MHz
    rf12_xfer(0x0000);

    if (rxstate == TXRECV) {
        uint8_t in = rf12_xfer(RF_RX_FIFO_READ);

        if (rxfill == 0 && group != 0)
            rf12_buf[rxfill++] = group;

        rf12_buf[rxfill++] = in;
        rf12_crc = crc_update(rf12_crc, in);

        if (rxfill >= rf12_len + 5 || rxfill >= RF_MAX)
            rf12_xfer(RF_IDLE_MODE);
    } else {
        uint8_t out;

        if (rxstate < 0) {
            uint8_t pos = 3 + rf12_len + rxstate++;
            out = rf12_buf[pos];
            rf12_crc = crc_update(rf12_crc, out);
        } else {
            switch (rxstate) {
                case TXSYN1: out = 0x2D; break;
                case TXSYN2: out = group;
                             rxstate = - (3 + rf12_len);
                             break;
                case TXCRC1: out = rf12_crc; break;
                case TXCRC2: out = rf12_crc >> 8; break;
                case TXDONE: rf12_xfer(RF_IDLE_MODE); // fall through
                default:     out = 0xAA;
            }
            ++rxstate;
        }

        rf12_xfer(RF_TXREG_WRITE + out);
    }
}

static void rf12_recvStart () {
    if (rf12_fixed_pkt_len) {
        rf12_rawlen = rf12_fixed_pkt_len;
        rf12_grp = rf12_hdr = 0;
        rxfill = 3;
    } else
        rxfill = rf12_rawlen = 0;
    rf12_crc = crc_update(crc_initVal, group);
    rxstate = TXRECV;

    rf12_xfer(RF_RECEIVER_ON);
}

#include <RF12.h>
#include <Ports.h> // needed to avoid a linker error :(

/// @details
/// The timing of this function is relatively coarse, because SPI transfers are
/// used to enable / disable the transmitter. This will add some jitter to the
/// signal, probably in the order of 10 µsec.
///
/// If the result is true, then a packet has been received and is available for
/// processing. The following global variables will be set:
///
/// * volatile byte rf12_hdr -
///     Contains the header byte of the received packet - with flag bits and
///     node ID of either the sender or the receiver.
/// * volatile byte rf12_len -
///     The number of data bytes in the packet. A value in the range 0 .. 66.
/// * volatile byte rf12_data -
///     A pointer to the received data.
/// * volatile byte rf12_crc -
///     CRC of the received packet, zero indicates correct reception. If != 0
///     then rf12_hdr, rf12_len, and rf12_data should not be relied upon.
///
/// To send an acknowledgement, call rf12_sendStart() - but only right after
/// rf12_recvDone() returns true. This is commonly done using these macros:
///
///     if(RF12_WANTS_ACK){
///        rf12_sendStart(RF12_ACK_REPLY,0,0);
///      }
/// @see http://jeelabs.org/2010/12/11/rf12-acknowledgements/
uint8_t rf12_recvDone () {
    if (rxstate == TXRECV &&
            (rxfill >= rf12_len + 5 + RF12_COMPAT || rxfill >= RF_MAX)) {
        rxstate = TXIDLE;
        if (rf12_len > RF12_MAXDATA)
            rf12_crc = 1; // force bad crc if packet length is invalid
        if (!(rf12_hdr & RF12_HDR_DST) || (nodeid & NODE_ID) == 31 ||
                (rf12_hdr & RF12_HDR_MASK) == (nodeid & NODE_ID))
        {
            return 1; // it's a broadcast packet or it's addressed to this node
        }
    }
    if (rxstate == TXIDLE)
        rf12_recvStart();
    return 0;
}

/// @details
/// Call this when you have some data to send. If it returns true, then you can
/// use rf12_sendStart() to start the transmission. Else you need to wait and
/// retry this call at a later moment.
///
/// Don't call this function if you have nothing to send, because rf12_canSend()
/// will stop reception when it returns true. IOW, calling the function
/// indicates your intention to send something, and once it returns true, you
/// should follow through and call rf12_sendStart() to actually initiate a send.
/// See [this weblog post](http://jeelabs.org/2010/05/20/a-subtle-rf12-detail/).
///
/// Note that even if you only want to send out packets, you still have to call
/// rf12_recvDone() periodically, because it keeps the RFM12B logic going. If
/// you don't, rf12_canSend() will never return true.
uint8_t rf12_canSend () {
    // need interrupts off to avoid a race (and enable the RFM12B, thx Jorg!)
    // see http://openenergymonitor.org/emon/node/1051?page=3
    if (rxstate == TXRECV && rxfill == 0 &&
            (rf12_control(0x0000) & RF_RSSI_BIT) == 0) {
        rf12_control(RF_IDLE_MODE); // stop receiver
        rxstate = TXIDLE;
        return 1;
    }
    return 0;
}

void rf12_sendStart (uint8_t hdr) {
    rf12_hdr = hdr & RF12_HDR_DST ? hdr :
                (hdr & ~RF12_HDR_MASK) + (nodeid & NODE_ID);

	rf12_crc = crc_update(crc_initVal, group);
    rxstate = TXPRE1;
    rf12_xfer(RF_XMITTER_ON); // bytes will be fed via interrupts
}

/// @details
/// Switch to transmission mode and send a packet.
/// This can be either a request or a reply.
///
/// Notes
/// -----
///
/// The rf12_sendStart() function may only be called in two specific situations:
///
/// * right after rf12_recvDone() returns true - used for sending replies /
///   acknowledgements
/// * right after rf12_canSend() returns true - used to send requests out
///
/// Because transmissions may only be started when there is no other reception
/// or transmission taking place.
///
/// The short form, i.e. "rf12_sendStart(hdr)" is for a special buffer-less
/// transmit mode, as described in this
/// [weblog post](http://jeelabs.org/2010/09/15/more-rf12-driver-notes/).
///
/// @param hdr The header contains information about the destination of the
///            packet to send, and flags such as whether this should be
///            acknowledged - or if it actually is an acknowledgement.
/// @param ptr Pointer to the data to send as packet.
/// @param len Number of data bytes to send. Must be in the range 0 .. 65.
void rf12_sendStart (uint8_t hdr, const void* ptr, uint8_t len) {
    rf12_rawlen = len;
    memcpy((void*) rf12_data, ptr, len);
    rf12_sendStart(hdr);
}

/// @details
/// Call this once with the node ID (0-31), frequency band (0-3), and
/// optional group (0-255 for RFM12B, only 212 allowed for RFM12).
/// @param id The ID of this wireless node. ID's should be unique within the
///           netGroup in which this node is operating. The ID range is 0 to 31,
///           but only 1..30 are available for normal use. You can pass a single
///           capital letter as node ID, with 'A' .. 'Z' corresponding to the
///           node ID's 1..26, but this convention is now discouraged. ID 0 is
///           reserved for OOK use, node ID 31 is special because it will pick
///           up packets for any node (in the same netGroup).
/// @param band This determines in which frequency range the wireless module
///             will operate. The following pre-defined constants are available:
///             RF12_433MHZ, RF12_868MHZ, RF12_915MHZ. You should use the one
///             matching the module you have, to get a useful TX/RX range.
/// @param g Net groups are used to separate nodes: only nodes in the same net
///          group can communicate with each other. Valid values are 1 to 212.
///          This parameter is optional, it defaults to 212 (0xD4) when omitted.
///          This is the only allowed value for RFM12 modules, only RFM12B
///          modules support other group values.
/// @param f Frequency correction to apply. Defaults to 1600, per RF12 docs.
///          This parameter is optional, and was added in February 2014.
/// @returns the nodeId, to be compatible with rf12_config().
uint8_t rf12_initialize (uint8_t id, uint8_t band, uint8_t g, uint16_t frequency) {
    nodeid = id;
    group = g;
	// caller should validate!    if (frequency < 96) frequency = 1600;

    rf12_spiInit();
    rf12_xfer(0x0000); // initial SPI transfer added to avoid power-up problem
    //rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd

    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    rf12_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    while (digitalRead(RFM_IRQ) == 0)
        rf12_xfer(0x0000);

    rf12_xfer(0x80C7 | (band << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    //rf12_xfer(0xA000 + frequency); // 96-3960 freq range of values within band
    rf12_xfer(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps -- 10000000/29/((0x06)+1)/(1+(fist bit in last byte)*7)
    rf12_xfer(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    //rf12_xfer(0xC2AC); // AL,!ml,DIG,DQD4
    
    // Group
    rf12_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR
    rf12_xfer(0xCE00 | group); // SYNC=2DXX；
        
    rf12_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    rf12_xfer(0x9850); // !mp,90kHz,MAX OUT
    rf12_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    //rf12_xfer(0xE000); // NOT USE
    //rf12_xfer(0xC800); // NOT USE
    //rf12_xfer(0xC049); // 1.66MHz,3.1V

    rxstate = TXIDLE;
    if ((nodeid & NODE_ID) != 0)
        attachInterrupt(0, rf12_interrupt, LOW);
    else
        detachInterrupt(0);

    return nodeid;
}