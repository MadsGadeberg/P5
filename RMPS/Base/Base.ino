#include "../Libraries/rfpr.h"
#include "../Libraries/rfhw.h"
#include "../Libraries/rfapp.h"
#include <Arduino.h>

using namespace rf;

// Global variables
int nextVID = 0;
char data[20];
int i = 0;
unsigned long int pingSent = 0;
uint16_t connectedSatellites[10];

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
	delay(RF_POWER_UP_TIME); // Waiting for the RF module to power up
}

void loop() {
	while (millis() < RF_POWER_UP_TIME + WAIT_FOR_CONNECTS_TIME) // Letting satellites register for WAIT_FOR_CONNECTS_TIME ms
	{
		rf::packetTypes type = rf::pr_receive(data);
		if (type == rf::CONNECT_REQUEST)
		{
			struct rf::connectRequest *request;
			request = (rf::connectRequest*)data;
			uint16_t satelliteRID = (request->RID);

			rf::pr_send_connectedConfirmation(satelliteRID, nextVID++);
		}
	}

	rf::pr_send_ping((char)i);
	pingSent = millis();

	// Receive, check if DataSending packet. Do some stuff with that.

	delay(WAIT_FOR_CONNECTS_TIME - (millis() - pingSent)); 
	i++;
}
