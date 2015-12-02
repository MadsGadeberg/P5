#include "../Libraries/rfpr.h"
#include "../Libraries/rfhw.h"
#include "../Libraries/rfapp.h"
#include <Arduino.h>

#define RUNPIN 0
#define LISTENPIN 1

// Enums
typedef enum SystemStates { LISTENINGFORSATS, RUNMODE, STANDBY } SystemStates;

// Global variables
SystemStates systemState = STANDBY;
int nextVID = 0; // This value also shows how many connected satellites we have
char data[20];
int satelliteNumber = 0;
unsigned long int pingSent = 0;
uint16_t connectedSatellites[10];

// Prototypes
void registerSatellite();
void getDataFromSatellite();

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module

	pinMode(RUNPIN, INPUT);
	pinMode(LISTENPIN, INPUT);
}

void loop() {
	if (LISTENINGFORSATS)
		registerSatellite(); // Letting satellites register for WAIT_FOR_CONNECTS_TIME ms
	else if (RUNMODE)
		getDataFromSatellite();

	// check for state change
	if (digitalRead(RUNPIN))			// Run button
		systemState = LISTENINGFORSATS;
	else if (digitalRead(LISTENPIN))	// Listen for satellites
		systemState = RUNMODE;
}

void registerSatellite()
{
	rf::packetTypes type = rf::pr_receive(data);
	if (type == rf::CONNECT_REQUEST)
	{
		struct rf::ConnectRequest *request;
		request = (rf::ConnectRequest*)data;
		uint16_t satelliteRID = (request->RID);

		rf::pr_send_connectedConfirmation(satelliteRID, nextVID++);
	}
}

void getDataFromSatellite() {
	rf::SamplePacketVerified samplePacket[8];

	//get sampleDataPacket from all connected satellites
	for (int i = 0; nextVID < i; i++) {
		// ping satellite
		rf::pr_send_ping((char)satelliteNumber);
		pingSent = millis();

		// Need to wait for satellites RF module
		delayMicroseconds(3);

		// get data
		struct rf::SamplePacket *dataPacket;

		// if datatype is data save it, else mark it as invalid
		if (rf::pr_receive(data) == rf::DATA)
			dataPacket = (rf::SamplePacket*)data;
		else
			samplePacket[i].data->valid = false;
	}

	// count up sat number
	satelliteNumber = (satelliteNumber + 1) % nextVID;
}
