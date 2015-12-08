#include "../Libraries/rfpr.h"
#include "../Libraries/rfhw.h"
#include "../Libraries/rfapp.h"
#include <Arduino.h>
#include <string.h>
#include <LinkedList.h>

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
rf::SamplePacketVerified *samplePacketArray[8];

//LinkedList<LinkedList<Sample>> dataSet(10, LinkedList<Sample>(0));	// all data retrieved - Memmory issues at some point!!!!!!
LinkedList<LinkedList<rf::Sample>> dataSet;	// all data retrieved - Memmory issues at some point!!!!!!


// Prototypes
void registerSatellite();
void getDataFromSatellite();
void pingSatellite(int satelliteNr);
void incrementSatellite();
void checkForStateChange();
void initRunMode();
void printSampesToSerial();

void setup() {
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module

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
	printSampesToSerial();

	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite()
{
	char data[SAMPLE_PACKET_SIZE];

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
	char data[SAMPLE_PACKET_SIZE];

	// ping satellite
	if (satellitePinged == 0)
		pingSatellite(satelliteNr);

	// IF valid data returned
	if (rf::pr_receive(data) == rf::DATA) { 	// check for responce
		rf::SamplePacketVerified* samplePacket = (rf::SamplePacketVerified*) data;

		// save data to dataSet
		for (int i = 0; i < SAMPLE_PACKET_SIZE; i++)
			dataSet.get(satelliteNr).add(samplePacket->data[i]);

		incrementSatellite();
	}
	// If timeout
	else if (pingSent + TIME_OUT_TIME < millis()) {  // timeout and data is marked as invalid
		// save invalid dummydata to dataSet
		for (int i = 0; i < SAMPLE_PACKET_SIZE; i++) {
			rf::Sample s;
			s.valid = false;
			dataSet.get(satelliteNr).add(s);
		}

		incrementSatellite();
	}
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
void checkForStateChange() {
	if (digitalRead(RUNPIN))			// Run button
		systemState = LISTENINGFORSATS;
	else if (digitalRead(LISTENPIN)) {	// Listen for satellites
		initRunMode();
	}
}

// setting runmode configuration.
void initRunMode() {
	// prepare datastructure for satellite data
	dataSet.clear();
	for (int i = 0; i < nrOfSatellitesConected; i++) {
		dataSet.add(LinkedList<rf::Sample>());
	}

	systemState = RUNMODE;
	// resetting ping counts
	pingSatelliteCount = 0;
	satellitePinged = 0;
	pingSent = 0;
	runmodeInitiated = millis();
}

void printSampesToSerial() {
	// iterate satellites
	for (int i = 0; i < nrOfSatellitesConected; i++) {
		// timedelay is the time in the past we want to print. We need to wait for the data to be available from all the satellites before we can print them. The time we need to wait is the time from the newest dataset that is available for all satellites. To ensure we have all data it is easier to wait for at least TIME_BETWEEN_PING_SEQUENCE + the time it takes the satellite to return the data which is less than TIME_BETWEEN_PING. That way we always know we have the data needed.
		int timeDelay = (TIME_BETWEEN_PING_SEQUENCE + TIME_BETWEEN_PING);
		
		// the amount of samples douring timeDelay. 
		int sampleDelay = timeDelay / TIME_BETWEEN_SAMPLES;
	
		// The time system have ben in RunMode
		unsigned long int runModeTime = millis() - runmodeInitiated;

		// the samples from beginning of runmode. time/TIME_BETWEEN_SAMPLES - pingDelay
		int samplesDouringRunmode = runModeTime / TIME_BETWEEN_SAMPLES;

		// the sample that is safe to print for all satellites connected. This is respect to the schedueling algorithm. The base needs the data before we can print it.
		int samplesToPrint = samplesDouringRunmode - sampleDelay;

		// the corrected sample to print. The samples for each satellite in dataSet is indexed with a difference of SAMPLES_BETWEEN_PINGS
		int indexCorrectedSampleToPrint = samplesToPrint - SAMPLES_BETWEEN_PINGS * i;

		dataSet.get(nrOfSatellitesConected).get(indexCorrectedSampleToPrint);
	}
}