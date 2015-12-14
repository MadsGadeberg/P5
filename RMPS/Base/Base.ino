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

// Debug Data allocation
LinkedList<int> recieveTimes;
unsigned long int pingTime = 0;

// Prototypes
void registerSatellite();
void getDataFromSatellites();
void getDataFromSatellite(int satellite);
void logTimeout(int satellite);
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
	else if (systemState == RUNMODE){
		getDataFromSatellites();
	    printSampesToSerial();
	}
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
            Serial.print("Satt ");
            Serial.print(satConnected(satelliteRID));
            Serial.println(" REconnected");           
        }
        // If not save RID and return VID
        else {
            connectedSatellites[nrOfSatellitesConected] = satelliteRID;
            // send confirmation
            rf::pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
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

	if (rf::pr_receive(data) == rf::DATA) {
        rf::SamplePacketVerified* samplePacket = (rf::SamplePacketVerified*) data;
        
		// save data to dataSet
		for (int i = 0; i < SAMPLES_PER_PACKET; i++){

            // get the samples
            LinkedList<rf::Sample> samples = dataSet.get(satellite);
            
            // add samples to the samples
            samples.add(samplePacket->data[i]);

            Serial.print("samples size: ");
            Serial.println(samples.size());

            // print the saved samples
            for (int j = 0; j < samples.size(); j++){
                Serial.print("samples.get(");   Serial.print(j);    Serial.print(").value: ");
                Serial.println(samples.get(j).value);                
            }
            Serial.println();
            
            // save the samples again
            dataSet.set(satellite, samples);
            ///////////////////////////////////////////////////////////
            samples = dataSet.get(satellite);

            Serial.print("samples size: ");
            Serial.println(samples.size());

            // print the saved samples
            for (int j = 0; j < samples.size(); j++){
                Serial.print("samples.get(");   Serial.print(j);    Serial.print(").value: ");
                Serial.println(samples.get(j).value);                
            }

                        
            
            //Serial.print("dataSet.get(satellite).get(");    Serial.print(i);Serial.print(").value: ");
            //Serial.print(dataSet.get(satellite).get(i).value);            Serial.println();

            Serial.println("____________________");
            
            //Serial.println(dataSet.get(satellite).get(dataSet.size() - 1).valid);
            //Serial.println(dataSet.get(satellite).get(dataSet.size() - 1).value);
		}

		incrementSatellite();
	}
}

// when timeout occurs save invalid data in data structure and increment satt
void logTimeout(int satellite){
        // save invalid dummydata to dataSet
        for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
            rf::Sample s;
            s.valid = false;
            dataSet.get(satellite).add(s);
        }
        incrementSatellite();
}

// pings the satellite and the timer of the ping
void pingSatellite(int satellite) {
	rf::pr_send_ping((char)satellite);	
    pingTime = millis();
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
	// clear datastructure for new dataSet
	dataSet.clear();

    // create Sample lists for all connected satellites
	for (int i = 0; i < nrOfSatellitesConected; i++)
		dataSet.add(LinkedList<rf::Sample>());

	systemState = RUNMODE;
	// resetting ping counts
	pingSatelliteCount = 0;
	satellitePinged = 0;
	pingSent = 0;
	runmodeInitiated = millis();

    Serial.println("________________________________________________");
    Serial.println("runmode initiated");
}

void printSampesToSerial() {
    unsigned long int sampleNrToPrint = sampleToPrint();

    //Serial.print("sample to print: ");
    //Serial.println(sampleNrToPrint);
    
    // due to a ping delay between the different satellites we need to calculate the corect data
	for (int i = 0; i < nrOfSatellitesConected; i++) {
        
		// the corrected sample to print. The samples for each satellite in dataSet is indexed with a difference of SAMPLES_BETWEEN_PINGS
		int indexCorrectedSampleToPrint = sampleNrToPrint - SAMPLES_BETWEEN_PINGS * i;
        
        //Serial.print("indexCorrected to print: ");
        //Serial.println(indexCorrectedSampleToPrint);
        
        //dataSet.get(i).get(indexCorrectedSampleToPrint).value = 10;
        //dataSet.get(satellite).add(samplePacket->data[i]);
        
        rf::Sample sample = dataSet.get(i).get(indexCorrectedSampleToPrint);

        //Serial.print("Sat ");
        //Serial.print(i);
        //Serial.print(" Valid: ");
        //Serial.print(sample.valid);
        //Serial.print(" value: ");
        //Serial.println(sample.value);
        //Serial.println();
	}
}

// this function gets the sample number fra runmode initiated we want to print
unsigned long int sampleToPrint(){
    // Get the delay of how old data we want to print
    // timedelay is the time in the past we want to print. We need to wait for the data to be available from all the satellites before we can print them. The time we need to wait is the time from the newest dataset that is available for all satellites. To ensure we have all data it is easier to wait for at least TIME_BETWEEN_PING_SEQUENCE + the time it takes the satellite to return the data which is less than TIME_BETWEEN_PING. That way we always know we have the data needed.
    unsigned long int timeDelay = (TIME_BETWEEN_PING_SEQUENCE + TIME_BETWEEN_PING);   
    
    // Get the time we have ben in runmode
    unsigned long int runModeTime = millis() - runmodeInitiated;
    //Serial.print("runModeTime: ");
    //Serial.println(runModeTime);

    // get the time from runmode we want to print
    unsigned long int timeOfSamplesToPrint = (runModeTime < timeDelay ? 0 : (runModeTime - timeDelay));
    //Serial.print("timeOfSamplesToPrint: ");
    //Serial.println(timeOfSamplesToPrint);
    
    // the sample number we want to print in the past
    return timeOfSamplesToPrint / TIME_BETWEEN_SAMPLES;
}

