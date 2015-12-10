#include <rfapp.h>
#include <rfhw.h>
#include <rfpr.h>
#include <MemoryFree.h>

#include <Arduino.h>
#include <string.h>
#include <LinkedList.h>

#define RUNPIN 3
#define LISTENPIN 4
#define MAX_CONNECTED_SATELLITES 10
#define TIME_BETWEEN_PING_SEQUENCE 200
#define TIME_OUT_TIME 18

// the size of the largest possible packet in Protocol Layer
#define MAX_PACKET_SIZE sizeof(struct SamplePacketVerified)

// Enums
typedef enum SystemStates { LISTENINGFORSATS, RUNMODE, STANDBY } SystemStates;

// the state of the base
SystemStates systemState = STANDBY;

// connected satellites information
int nrOfSatellitesConected = 0; // This value also shows the next VID to assign to a satellite
uint16_t connectedSatellites[MAX_CONNECTED_SATELLITES]; // the array that holds the RID of the connected satellites

														// satellite ping operation information
unsigned long int runmodeInitiated = 0;  // the time runmode was initiated - used to calculate ping times
unsigned long int pingSent = 0; // time of last ping sent
int pingSatelliteCount = 0;   // The number of satellite pings since runModeInitiated
int satellitePinged = 0;    // if the current sattelite have ben pinged - ping only once!

// Data location
struct rf::SamplePacketVerified *dataPacket;
rf::SamplePacketVerified *samplePacketArray[8];
// all data retrieved - Memmory issues at some point!!!!!!
LinkedList<LinkedList<rf::Sample>> dataSet;

// Prototypes
void registerSatellite();
void getDataFromSatellites();
void getDataFromSatellite(int satellite);
void pingSatellite(int satelliteNr);
void incrementSatellite();
void checkForStateChange();
void initRunMode();
void printSampesToSerial();

void setup() {
	Serial.begin(250000);
	rf::hw_init((uint8_t)GROUP); // Initializing the RF module
	delay(100); // Power up time (worst case from datasheet)
	Serial.println("Init done");

	pinMode(RUNPIN, INPUT);
	pinMode(LISTENPIN, INPUT);
}

void loop() {
	if (systemState == LISTENINGFORSATS)
		registerSatellite();
	else if (systemState == RUNMODE)
		getDataFromSatellites();

	printSampesToSerial();

	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite(){
	char data[SAMPLE_PACKET_SIZE];

	// if data is of type request, ad it to base and send confirmation.
	if (rf::pr_receive(data) == rf::CONNECT_REQUEST)
	{
		// cast
		struct rf::ConnectRequest *request = (rf::ConnectRequest*)data;
		
		uint16_t satelliteRID = (request->RID);
        
        // check if sat allready connected. Return already saved VID
        if (satConnected(satelliteRID) != -1){
            rf::pr_send_connectedConfirmation(satelliteRID, satConnected(satelliteRID));            
        }
        // If not save RID and return VID
        else {
            connectedSatellites[nrOfSatellitesConected] = satelliteRID;
            // send confirmation
            rf::pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
            Serial.print("Sattelite nr: ");
            Serial.print(nrOfSatellitesConected);
            Serial.println(" connected:");
            nrOfSatellitesConected++;      
        }
	}
}

// function that checks if satellite is allready connected. Returns index if it is. -1 if not.
int satConnected(int RID){
    // iterate all connected satellites to see if RID is among them.
    for (int i = 0 ; i < nrOfSatellitesConected; i++){
        if (connectedSatellites[i] == RID)
            return i;
    }
    return -1;
}

// this function initiates a ping sequence.
void getDataFromSatellites() {
	unsigned long int time = millis();

	// the nr of Ping sequences elapsed
	unsigned long int pingSequenceCount = nrOfSatellitesConected == 0 ? 0 : pingSatelliteCount / nrOfSatellitesConected;
	// the sattelite that needs to be pinged in the current ping sequence
	unsigned long int satelliteToGetDataFrom = nrOfSatellitesConected == 0 ? 0 : pingSatelliteCount % nrOfSatellitesConected;

	// caclulating the timewindow for satelliteToGetDataFrom.
	unsigned long int timeWindowStart = runmodeInitiated + (pingSequenceCount * TIME_BETWEEN_PING_SEQUENCE) + (satelliteToGetDataFrom * TIME_BETWEEN_PING);
	unsigned long int timeWindowEnd = timeWindowStart + TIME_OUT_TIME;

  // For debug purposes - to be deleted
    /*
    Serial.print("S: ");
    Serial.print(timeWindowStart);
    Serial.print(" T: ");
    Serial.print(time);
    Serial.print(" E: ");
    Serial.println(timeWindowEnd);
    Serial.println();
*/
    Serial.print("mem: ");
    Serial.println(freeMemory());
	// if we are inside in the timeslice of the current satellite to ping.
	if ( timeWindowStart < time && time < timeWindowEnd)
		getDataFromSatellite(satelliteToGetDataFrom);
	// If we didnt recieve data in the timeslice
	else if (time >= timeWindowEnd) {
        Serial.print("TIMEOUT");
		// save invalid dummydata to dataSet
		for (int i = 0; i < SAMPLE_PACKET_SIZE; i++) {
			rf::Sample s;
			s.valid = false;
			dataSet.get(satelliteToGetDataFrom).add(s);
		}
        //Serial.print("Sat: ");
        //Serial.print(satelliteToGetDataFrom);
        //Serial.println(" IMEDOUT");
        
		incrementSatellite();
    }
}

// pings satellite for data and saves it to dataSet
void getDataFromSatellite(int satellite) {
	// ping satellite
	if (satellitePinged == 0)
		pingSatellite(satellite);
   
	// datasource for returned data
	char data[SAMPLE_PACKET_SIZE * 2 + 1];

	if (false || rf::pr_receive(data) == rf::DATA) {
        delay(5000);
	    Serial.print("");
        //Serial.print(" from satellite number ");
     
		//rf::SamplePacketVerified* samplePacket = (rf::SamplePacketVerified*) data;

		// save data to dataSet
		/*for (int i = 0; i < SAMPLE_PACKET_SIZE; i++)
		dataSet.get(satellite).add(samplePacket->data[i]);*/

		//Serial.println((samplePacket->data[12]).value);

		//Serial.println("Inc Data returned");
		incrementSatellite();
	}
}

// pings the satellite and the timer of the ping
void pingSatellite(int satellite) {
	rf::pr_send_ping((char)satellite);
    
    Serial.print("Sat: ");
    Serial.print(satellite);
    Serial.println(" Pinged!");
	
	pingSent = millis();

	satellitePinged = 1;
}

// Function that increments satellite
void incrementSatellite() {
	pingSatelliteCount++;
	satellitePinged = 0;
}

// function that checks for buttonpresses to change systemState
void checkForStateChange() {
	if (digitalRead(LISTENPIN))      // Listen for satellites
		systemState = LISTENINGFORSATS;
	else if (digitalRead(RUNPIN)) {  // Run button
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

    // delay needed because else the runModeInitiatet is incrementing for unknown reason.
    delay(500);
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
