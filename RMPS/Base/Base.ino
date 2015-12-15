#include <rfapp.h>
#include <rfhw.h>
#include <rfpr.h>
#include <MemoryFree.h>

#include <Arduino.h>
#include <string.h>
#include <LinkedList.h>

#define RUNPIN 3
#define LISTENPIN 4
#define MAX_CONNECTED_SATELLITES 8
#define TIME_BETWEEN_PING_SEQUENCE 200
using namespace rf;

// Enums
typedef enum SystemStates { LISTENINGFORSATS, RUNMODE, STANDBY } SystemStates;

// the state of the base
SystemStates systemState = STANDBY;

// connected satellites information
int nrOfSatellitesConected = 0; // This value also shows the next VID to assign to a satellite
uint16_t connectedSatellites[MAX_CONNECTED_SATELLITES]; // the array that holds the RID of the connected satellites

// satellite ping operation information
unsigned long int runmodeInitiated = 0;  // the time runmode was initiated - used to calculate ping times
//unsigned long int pingSent = 0; // time of last ping sent
int pingSatelliteCount = 0;   // The number of satellite pings since runModeInitiated
int satellitePinged = 0;    // if the current sattelite have ben pinged - ping only once!

// Data location
struct SamplePacketVerified *dataPacket;
// Prototypes
void registerSatellite();
void getDataFromSatellites();
void getDataFromSatellite(int satellite);
void logTimeout(int satellite);
void pingSatellite(int satelliteNr);
void incrementSatellite();
void checkForStateChange();
void initRunMode();

void setup() {
	Serial.begin(250000);
    rf::phy_init((uint8_t)GROUP); // Initializing the RF module	
    delay(100); // Power up time (worst case from datasheet)
	Serial.println("Init done");

	pinMode(RUNPIN, INPUT);
	pinMode(LISTENPIN, INPUT);
}

void loop() {
	if (systemState == LISTENINGFORSATS)
		registerSatellite();
	else if (systemState == RUNMODE){
		getDataFromSatellites();
	}
	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite(){
	char data[SAMPLE_PACKET_SIZE];

	// if data is of type request, ad it to base and send confirmation.
	if (pr_receive(data) == CONNECT_REQUEST)
	{
		// cast
		struct ConnectRequest *request = (ConnectRequest*)data;
		
		uint16_t satelliteRID = (request->RID);
        
        // check if sat allready connected. Return already saved VID
        if (satConnected(satelliteRID) != -1){
            pr_send_connectedConfirmation(satelliteRID, satConnected(satelliteRID)); 
            Serial.print("Satt ");
            Serial.print(satConnected(satelliteRID));
            Serial.println(" REconnected");           
        }
        // If not save RID and return VID
        else {
            connectedSatellites[nrOfSatellitesConected] = satelliteRID;
            // send confirmation
            pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
            Serial.print("Satt ");
            Serial.print(nrOfSatellitesConected);
            Serial.println(" connected");
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
	unsigned long int timeWindowEnd = timeWindowStart + TIME_BETWEEN_PING;
    //Serial.print("                             Start: "); Serial.print(timeWindowStart);Serial.print(" End: "); Serial.print(timeWindowEnd);Serial.print(" time: "); Serial.println(millis());


	// if we are inside in the timeslice of the current satellite to ping.
	if ( timeWindowStart < time && time < timeWindowEnd)
		getDataFromSatellite(satelliteToGetDataFrom);
       
	// If we didnt recieve data in the timeslice
	else if (time >= timeWindowEnd)
        logTimeout(satelliteToGetDataFrom);
}

// pings satellite for data and saves it to dataSet
void getDataFromSatellite(int satellite) {
	// ping satellite
	if (satellitePinged == 0)
		pingSatellite(satellite);
       
	// datasource for returned data
	char data[SAMPLE_PACKET_VERIFIED_SIZE];

	if (pr_receive(data) == DATA) {
        //Serial.println("                             recieve: ");
    
        SamplePacketVerified* samplePacket = (SamplePacketVerified*) data;

		incrementSatellite();
	}
}

// when timeout occurs save invalid data in data structure and increment satt
void logTimeout(int satellite){
        //Serial.println("                             timeout: ");
        // save invalid dummydata to dataSet

        incrementSatellite();
}

// pings the satellite and the timer of the ping
void pingSatellite(int satellite) {
	pr_send_ping((char)satellite);	
    //pingSent = millis();
    //Serial.println("____________________");
    //Serial.print("Sat ");
    //Serial.print(satellite);
    //Serial.print(" Ping at :");
    //Serial.println(millis());
	satellitePinged = 1;
}

// Function that increments satellite
void incrementSatellite() {
	pingSatelliteCount++;
	satellitePinged = 0;

    //Serial.println("_____________________________");
    //Serial.print("Sat incremented to: ");
    //Serial.print(pingSatelliteCount);
    //Serial.print(" at ");
    //Serial.println(millis());



    //Serial.print("S: ");
    //Serial.print(timeWindowStart);
    //Serial.print(" T: ");
    //Serial.print(millis());
    //Serial.print(" E: ");
    //Serial.println(timeWindowEnd);
}

// function that checks for buttonpresses to change systemState
void checkForStateChange() {
	if (digitalRead(LISTENPIN))      // Listen for satellites
        initListeningForSatsMode();
	else if (digitalRead(RUNPIN))  // Run button
		initRunMode();
}

// setting Listening for sats configuration
void initListeningForSatsMode(){
        systemState = LISTENINGFORSATS;

        // clear all connected satellites
        for (int i = 0; i < MAX_CONNECTED_SATELLITES; i++)
            connectedSatellites[i] = -1;
        nrOfSatellitesConected = 0;
        
        Serial.println("________________________________________________");
        Serial.println("Listening for sats");
        delay(500);
}

// setting runmode configuration.
void initRunMode() {

	systemState = RUNMODE;
	// resetting ping counts
	pingSatelliteCount = 0;
	satellitePinged = 0;
	//pingSent = 0;
	runmodeInitiated = millis();

    Serial.println("________________________________________________");
    Serial.println("runmode initiated");
}

