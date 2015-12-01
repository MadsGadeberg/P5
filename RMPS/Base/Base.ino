#include "../Libraries/rfpr.h"
#include "../Libraries/rfhw.h"
#include "../Libraries/rfapp.h"
#include <Arduino.h>

using namespace rf;

// Global variables
int nextVID = 0;
char data[20];
int satelliteNumber = 0;
unsigned long int pingSent = 0;
uint16_t connectedSatellites[10];

// Prototypes
void registerSatellite();

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
}

void loop() {
	while (millis() < RF_POWER_UP_TIME + WAIT_FOR_CONNECTS_TIME) 
		registerSatellite(); // Letting satellites register for WAIT_FOR_CONNECTS_TIME ms

	rf::pr_send_ping((char)satelliteNumber);
	pingSent = millis();
	
	delayMicroseconds(3); // Need to wait for other device's RF module

	rf::packetTypes type = rf::pr_receive(data);
	if (type == rf::DATA)
	{
		struct rf::sampleDataPacket *request;
		request = (rf::sampleDataPacket*)data;
		auto data = (request->data);
	}

	for (int i = 0; i < SAMPLE_ARRAY_SIZE; i++)
	{
		Serial.print(satelliteNumber);
		Serial.print(data[i]); // TODO Check for out of bounds
		Serial.print(millis());
	}

	delay(WAIT_FOR_CONNECTS_TIME - (millis() - pingSent)); 
	satelliteNumber++;
}

void registerSatellite()
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