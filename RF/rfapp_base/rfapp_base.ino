#include <rfpr.h>
#include <rfhw.h>

// Global variables
int nextVID = 0;

// Global constants
#define GROUP 20
#define WAIT_FOR_CONNECTS_TIME 5000 // 5 seconds
#define RF_POWER_UP_TIME 3000 // 3 seconds

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
	delay(RF_POWER_UP_TIME); // Waiting for the RF module to power up
}

void loop() {
	if (millis() < RF_POWER_UP_TIME + WAIT_FOR_CONNECTS_TIME) // TODO Overhead at skulle tjekke hver gang?
	{
		// Turn on RF, receive data, if connectRequest send confirmation
	}


}