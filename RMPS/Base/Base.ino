#include "../Libraries/rfpr.h"
#include "../Libraries/rfhw.h"
#include "../Libraries/rfapp.h"
#include <Arduino.h>

#define RUNPIN 0
#define LISTENPIN 1
#define MAX_CONNECTED_SATELLITES 10
#define TIME_BETWEEN_PING_SEQUENCE 200
#define TIME_OUT_TIME 18

// Enums
typedef enum SystemStates { LISTENINGFORSATS, RUNMODE, STANDBY } SystemStates;

// the state of the base
SystemStates systemState = STANDBY;

// connected satellites information
int nrOfSatellitesConected = 0; // This value also shows how many connected satellites we have
uint16_t connectedSatellites[MAX_CONNECTED_SATELLITES]; // the array that holds the RID of the connected satellites

// satellite ping operation information
unsigned long int runmodeInitiated = 0;	// the time runmode was initiated - used to calculate ping times
unsigned long int pingSent = 0; // time of last ping sent
int pingSatelliteCount = 0;		// The number op satellite pings in lifetime of base
int satellitePinged = 0;		// if the current sattelite have ben pinged - ping only once!

// Data location
struct rf::SamplePacketVerified *dataPacket;
rf::SamplePacketVerified samplePacketArray[8];

// Prototypes
void registerSatellite();
void getDataFromSatellite();
void checkForStateChange();
void useData(rf::SamplePacketVerified* data);

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
	delay(100); // Power up time (worst case from datasheet)

	pinMode(RUNPIN, INPUT);
	pinMode(LISTENPIN, INPUT);
}

void loop() {
	if (LISTENINGFORSATS)
		registerSatellite();
	else if (RUNMODE) {
		int pingSequenceCount = pingSatelliteCount / nrOfSatellitesConected;		// the nr of ping sequences elapsed
		int satelliteCount = pingSatelliteCount % nrOfSatellitesConected;			// the sattelite that needs to be pinged

		if (runmodeInitiated + (pingSequenceCount * TIME_BETWEEN_PING_SEQUENCE) + (satelliteCount * TIME_BETWEEN_PING) < millis()) {	// ping the satellite when the calculated ping time is smaller than the actual time.
			getDataFromSatellite(satelliteCount);
		}
	}
	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite()
{
	char data[SAMPLE_ARRAY_SIZE];

	// if data is of type request, ad it to base and send confirmation.
	if (rf::pr_receive(data) == rf::CONNECT_REQUEST)
	{
		// cast
		struct rf::ConnectRequest *request = (rf::ConnectRequest*)data;
		// save RID
		uint16_t satelliteRID = (request->RID);
		connectedSatellites[nrOfSatellitesConected] = satelliteRID;

		// send confirmation
		rf::pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
		
		nrOfSatellitesConected++;
	}
}

// this function initiates a ping sequence.
void getDataFromSatellite(int satelliteNr) {
	char data[SAMPLE_ARRAY_SIZE];

	// ping satellite
	if (satellitePinged == 0)
		pingSatellite(satelliteNr);

	// Mark all data as invalid
	for (int i = 0; i < SAMPLE_ARRAY_SIZE; i++)
		samplePacketArray[satelliteNr].data[i].valid = false;

	// check for responce and timeout
	if (rf::pr_receive(data) == rf::DATA) { 	// check for responce
		rf::SamplePacketVerified* samplePacket = (rf::SamplePacketVerified*) data;

		useData(samplePacket);

		incrementSatellite();
	}
	else if (pingSent + TIME_OUT_TIME < millis())  // timeout and data is marked as invalid
		incrementSatellite();
}

// poings the satellite and the timer of the ping
void pingSatellite(int satelliteNr) {
	rf::pr_send_ping((char)satelliteNr);
	pingSent = millis();
	satellitePinged = 1;

	// Need to wait for satellites RF module
	delayMicroseconds(3);
}

// Function that increments satellite 
void incrementSatellite() {
	pingSatelliteCount++;
	satellitePinged = 0;
}

// function that checks for buttonpresses to change systemState
void checnForStateChange() {
	if (digitalRead(RUNPIN))			// Run button
		systemState = LISTENINGFORSATS;
	else if (digitalRead(LISTENPIN)) {	// Listen for satellites
		initRunMode();
	}
}

// this function uses data gotten from satellite
void useData(rf::SamplePacketVerified* data) {

}

// setting runmode configuration.
void initRunMode() {
	systemState = RUNMODE;
	// resetting ping counts
	pingSatelliteCount = 0;
	satellitePinged = 0;
	pingSent = 0;
	runmodeInitiated = millis();
}