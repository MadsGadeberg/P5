#include <rfpr.h>
#include <rfhw.h>
#include <rfapp.h>

// Global variables
int nextVID = 0;

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
	delay(RF_POWER_UP_TIME); // Waiting for the RF module to power up
}

void loop() {
	if (millis() < RF_POWER_UP_TIME + WAIT_FOR_CONNECTS_TIME) // TODO Overhead at skulle tjekke hver gang?
	{
		// Turn on RF, receive data, if connectRequest send confirmation
	}

	for (auto i = 0; i < nextVID; i++)
	{
		rf::pr_send_ping((char)i);
		delay(TIME_BETWEEN_PING / nextVID);
	}

	// Receive, check if DataSending packet. Do some stuff with that.
}