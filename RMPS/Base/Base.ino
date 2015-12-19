#include <MemoryFree.h>

#include <Arduino.h>
#include <QueueArray.h>
#include <rfapp.h>
#include <rfhw.h>
#include <rfpr.h>

using namespace rf;

#define BTNRUN_PIN 3
#define BTNLISTEN_PIN 4

#define MAX_CONNECTED_SATELLITES 8
#define TIME_BETWEEN_PING_SEQUENCE 200

typedef enum SystemStates { LISTENINGFORSATELITES, RUNNING, STANDBY } SystemStates;

// the state of the base
SystemStates systemState = LISTENINGFORSATELITES;

// Listening for satellites data
int nrOfSatellitesConected = 0; // This value also shows the next VID to assign to a satellite
uint16_t connectedSatellites[MAX_CONNECTED_SATELLITES]; // the array that holds the RID of the connected satellites

// Run mode data
unsigned long int RUNNINGInitiated = 0;  // the time RUNNING was initiated - used to calculate ping times
unsigned long int pingSatelliteCount = 0;   // The number of satellite pings since RUNNINGInitiated
bool satellitePinged = 0;    // if the current sattelite have ben pinged - ping only once!
unsigned long int pingSequenceCount = 0;

unsigned long int samplePrintCount = 0;

QueueArray<Sample> samples[MAX_CONNECTED_SATELLITES];

// Prototypes
void registerSatellite();
void getDataFromSatellites();
void getDataFromSatellite(int satellite);
void logTimeout(int satellite);
void pingSatellite(int satelliteNr);
void incrementSatellite();
void checkForStateChange();
void initRUNNING();

void setup(){
	Serial.begin(250000);
    rf::phy_init((uint8_t)GROUP); // Initializing the RF module	
    delay(100); // Power up time (worst case from datasheet)
	Serial.println("Init done");

	pinMode(BTNRUN_PIN, INPUT);
	pinMode(BTNLISTEN_PIN, INPUT);
}

void loop(){
	if (systemState == LISTENINGFORSATELITES)
		registerSatellite();
	else if (systemState == RUNNING) {
		getDataFromSatellites();
	    printSamples();
	}

	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite() {
	char data[SAMPLE_PACKET_SIZE];

	// if data is of type request, ad it to base and send confirmation.
	if (pr_receive(data) == CONNECT_REQUEST) {
		// cast
		struct ConnectRequest *request = (ConnectRequest*)data;
		
		uint16_t satelliteRID = (request->RID);
        
        // check if sat allready connected. Return already saved VID
        if (satConnected(satelliteRID) != -1) {
            pr_send_connectedConfirmation(satelliteRID, satConnected(satelliteRID)); 
            Serial.print("Satellite "); Serial.print(satConnected(satelliteRID)); Serial.println(" REconnected");           
        }
        else {
        	// If not save RID and return VID
            connectedSatellites[nrOfSatellitesConected] = satelliteRID;
            // send confirmation
            pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
            Serial.print("Satellite ");
            Serial.print(nrOfSatellitesConected);
            Serial.println(" connected");
            nrOfSatellitesConected++;      
        }
	}
}

// function that checks if satellite is allready connected. Returns index if it is. -1 if not.
int satConnected(int RID) {
    // iterate all connected satellites to see if RID is among them.
    for (int i = 0 ; i < nrOfSatellitesConected; i++) {
        if (connectedSatellites[i] == RID)
            return i;
    }
    return -1;
}

// this function initiates a ping sequence.
void getDataFromSatellites() {
	unsigned long int time = millis();
	// the nr of Ping sequences elapsed
	pingSequenceCount = nrOfSatellitesConected == 0 ? 0 : pingSatelliteCount / nrOfSatellitesConected;
	// the sattelite that needs to be pinged in the current ping sequence
	unsigned long int satelliteToGetDataFrom = nrOfSatellitesConected == 0 ? 0 : pingSatelliteCount % nrOfSatellitesConected;

	// caclulating the timewindow for satelliteToGetDataFrom.
	unsigned long int timeWindowStart = RUNNINGInitiated + (pingSequenceCount * TIME_BETWEEN_PING_SEQUENCE) + (satelliteToGetDataFrom * TIME_BETWEEN_PING);
	unsigned long int timeWindowEnd = timeWindowStart + TIME_BETWEEN_PING;

	// if we are inside in the timeslice of the current satellite to ping.
	if (timeWindowStart < time && time < timeWindowEnd)
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
        SamplePacketVerified* tmpSamples = (SamplePacketVerified*) data;
        for(int i = 0; i< SAMPLES_PER_PACKET; i++) {
            QueueArray<Sample> *queue = &samples[satellite];
            Sample s = tmpSamples->data[i];
            queue->enqueue(s);
        }
		incrementSatellite();
	}
}

// when timeout occurs save invalid data in data structure and increment satt
void logTimeout(int satellite) {
    // save invalid dummydata to dataSet
    for(int i = 0; i< SAMPLES_PER_PACKET; i++) {
        Sample s;
        s.valid = false;
        samples[satellite].enqueue(s);
    }
    incrementSatellite();
}

// pings the satellite and the timer of the ping
void pingSatellite(int satellite) {
	pr_send_ping((char)satellite);	
	satellitePinged = 1;
}

// Function that increments satellite
void incrementSatellite() {
	pingSatelliteCount++;
	satellitePinged = 0;
}

// function that checks for buttonpresses to change systemState
void checkForStateChange() {
	if (digitalRead(BTNLISTEN_PIN))      // Listen for satellites
        initLISTENINGFORSATELITESMode();
	else if (digitalRead(BTNRUN_PIN))  // Run button
		initRUNNING();
}

// setting Listening for sats configuration
void initLISTENINGFORSATELITESMode() {
    delay(500);
    systemState = LISTENINGFORSATELITES;

    // clear all connected satellites
    for (int i = 0; i < MAX_CONNECTED_SATELLITES; i++)
        connectedSatellites[i] = -1;
    nrOfSatellitesConected = 0;
        
    Serial.println("________________________________________________");
    Serial.println("Listening for sats");
}

// setting RUNNING configuration.
void initRUNNING() {
    if (nrOfSatellitesConected > 0) {
        delay(500);
    	systemState = RUNNING;
    
        RUNNINGInitiated = millis();
    	pingSatelliteCount = 0;
    	satellitePinged = 0;
        pingSequenceCount = 0;
        samplePrintCount = 0;
        
        // clear samples queue
        for(int i = 0; i < MAX_CONNECTED_SATELLITES; i++) {
            while(samples[i].isEmpty() == false)
                samples[i].pop();
        }
        
        syncSamples();
        Serial.println("RUNNING initiated");
    }
    else {
        delay(500);
        Serial.println("No satellites connected!");
    }
        
}

// this function is padding samples to our samplequeue to make sure that the saved samples is syncronized.
void syncSamples() {
    for (int i = 0; i < nrOfSatellitesConected; i++) {
        int samplesToAdd = i * SAMPLES_BETWEEN_PINGS;
        for (int j = 0; j < samplesToAdd; j++)
            samples[i].enqueue(Sample());    
    }
}

void printSamples() {
    unsigned long int timeFromRUNNINGInitiated = (millis() - RUNNINGInitiated) < 0 ? 0 : (millis() - RUNNINGInitiated);

    // after 1 second we start to print samples from queue
    if (timeFromRUNNINGInitiated > 50) {

        // calculate timewindow
        unsigned long int timeWindowStart = 50 + samplePrintCount * TIME_BETWEEN_SAMPLES;

        Sample samplesToPrint[8];
        for(int i = 0; i < 8; i++)
            samplesToPrint[i].valid = false;
        
        // if time to print sample
        if(timeFromRUNNINGInitiated > timeWindowStart) {
            // check if there are samples to print
            if (samples[0].isEmpty() == false) {
                for(int satellite = 0; satellite < nrOfSatellitesConected; satellite++) {
                    samplesToPrint[satellite] = samples[satellite].dequeue();
                        Serial.println("-----------");
                    if (samplesToPrint[satellite].valid){
                        Serial.print("Sat "); 
                        Serial.print(satellite); 
                        Serial.print(": "); 
                        Serial.println(samplesToPrint[satellite].value);
                    }
                    else{
                        Serial.print("Sat "); 
                        Serial.print(satellite); 
                        Serial.println(":-");
                    }
                }

                samplePrintCount++;
            }
        }
    }
}

