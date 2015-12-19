#include <Arduino.h>
#include <QueueArray.h>
#include <rfapp.h>
#include <rfphy.h>
#include <rfpr.h>

// Using the namespace rf which is used in protocol and physical layer
using namespace rf;

// Defines of pins
#define PIN_RUNBTN 3
#define PIN_LISTENBTN 4

// Configuration of the system
#define MAX_CONNECTED_SATELLITES 8
#define TIME_BETWEEN_PING_SEQUENCE 200

// Keeps control of current number of satellites connected
uint8_t nrOfSatellitesConected = 0;

// All connected satellites and their RID (Real ID - unique id for each satelite)
uint16_t connectedSatellites[MAX_CONNECTED_SATELLITES];

// Data from connected satellites
QueueArray<Sample> samples[MAX_CONNECTED_SATELLITES];


// Registrers if base is running and collecting data from satelites. If it's not running it would try to connect to sattelites
typedef enum SystemStates { LISTENINGFORSATELLITES, RUNNING } SystemStates;
SystemStates systemState = LISTENINGFORSATELLITES;

// Run mode data
unsigned long int runningInitiated   = 0;   // the time running was initiated - used to calculate ping times
unsigned long int pingSatelliteCount = 0;   // The number of satellite pings since runningInitiated
unsigned long int pingSequenceCount  = 0;

bool satellitePinged = 0; // if the current sattelite have been pinged - ping only once!

unsigned long int samplePrintCount = 0;

// Prototypes 
void registerSatellite();
void getDataFromSatellites();
void getDataFromSatellite(int satellite);
void logTimeout(int satellite);
void pingSatellite(int satelliteNr);
void incrementSatellite();
void checkForStateChange();
void initRunning();

void setup() {
	// Initilize Serial
	Serial.begin(250000);
	
	// Initializing the RF module
  rf::phy_init((uint8_t)GROUP);
    
  // Power up time (worst case from datasheet)
  delay(100);

	// Set pinModes for buttons to input
	pinMode(PIN_RUNBTN, INPUT);
	pinMode(PIN_LISTENBTN, INPUT);
}

void loop() {
	switch (systemState) {
	case LISTENINGFORSATELLITES:
		registerSatellite();
		break;
	case RUNNING:
		getDataFromSatellites();
	  printSamples();
		break;
	}

	checkForStateChange();
}

// function that listens for incomming sattelite requests.
void registerSatellite() {
	char data[SAMPLE_PACKET_SIZE];

	// if data is of type request, add it to base and send confirmation.
	if (pr_receive(data) == CONNECT_REQUEST) {
		// Cast to struct
		struct ConnectRequest *request = (ConnectRequest*)data;
		
		// Get sateliteID
		uint16_t satelliteRID = request->RID;
        
        // Check if satellite allready connected. If it's already connected return already saved VID
        if (satelliteConnected(satelliteRID) != -1) {
        	// Send confirmation with VID
            pr_send_connectedConfirmation(satelliteRID, satelliteConnected(satelliteRID));
            
            // Write information to serial
            Serial.print("Satellite "); 
            Serial.print(satelliteConnected(satelliteRID)); 
            Serial.println(" Reconnected");           
        }
        
        else {
        	// Save RID and return VID for the new connected satellite
            connectedSatellites[nrOfSatellitesConected] = satelliteRID;
            
            // Send confirmation with VID
            pr_send_connectedConfirmation(satelliteRID, nrOfSatellitesConected);
            
            // Write information to 
            Serial.print("Satellite ");
            Serial.print(nrOfSatellitesConected);
            Serial.println(" connected");
            
            // Increment number of satellites connected
            nrOfSatellitesConected++;      
        }
	}
}

// Checks if satellite with specific RID is connected, and returns VID if it is. If not returns -1
// Returns a int16_t since -1 should be supported and therefore uint8_t (which could have all VID values)
int16_t satelliteConnected(uint16_t RID) {
    // Iterate all connected satellites to see if RID is among them.
    for (uint8_t i = 0; i < nrOfSatellitesConected; i++) {
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
	unsigned long int timeWindowStart = runningInitiated + (pingSequenceCount * TIME_BETWEEN_PING_SEQUENCE) + (satelliteToGetDataFrom * TIME_BETWEEN_PING);
	unsigned long int timeWindowEnd = timeWindowStart + TIME_BETWEEN_PING;

	// if we are inside in the timeslice of the current satellite to ping.
	if (timeWindowStart < time && time < timeWindowEnd)
		getDataFromSatellite(satelliteToGetDataFrom);
       
	// If we didnt recieve data in the timeslice
	else if (time >= timeWindowEnd)
      logTimeout(satelliteToGetDataFrom);
}

// Pings satellite for data and saves it to dataSet
void getDataFromSatellite(int satellite) {
	// Ping satellite if is not pinged before
	if (!satellitePinged)
		pingSatellite(satellite);
       
	// Datasource for returned data
	char data[SAMPLE_PACKET_VERIFIED_SIZE];

	if (pr_receive(data) == DATA) {
        SamplePacketVerified* tmpSamples = (SamplePacketVerified*) data;
        for (int i = 0; i< SAMPLES_PER_PACKET; i++) {
            QueueArray<Sample> *queue = &samples[satellite];
            Sample s = tmpSamples->data[i];
            queue->enqueue(s);
        }
	 incrementSatellite();
	}
}

// pings the satellite and the timer of the ping
void pingSatellite(uint8_t satellite) {
	 pr_send_ping(satellite);	
	 satellitePinged = true;
}

// when timeout occurs save invalid data in data structure and increment satt
void logTimeout(uint8_t satellite) {
   // save invalid dummydata to dataSet
   for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
       Sample s;
       s.valid = false;
       samples[satellite].enqueue(s);
   }
   incrementSatellite();
}

// Function that increments satellite
void incrementSatellite() {
	 pingSatelliteCount++;
	 satellitePinged = 0;
}

// function that checks for buttonpresses to change systemState
void checkForStateChange() {
	// Check if one of the buttons indicate a change has to be made
	if (digitalRead(PIN_LISTENBTN))
     initListeningForSatellitesMode();
  else if (digitalRead(PIN_RUNBTN))
     initRunning();
}

// setting Listening for sats configuration
void initListeningForSatellitesMode() {
    delay(500);
    systemState = LISTENINGFORSATELLITES;

    // clear all connected satellites
    for (int i = 0; i < MAX_CONNECTED_SATELLITES; i++)
       connectedSatellites[i] = -1;
    nrOfSatellitesConected = 0;
        
    Serial.println("________________________________________________");
    Serial.println("Listening for satellites");
}

// setting running configuration.
void initRunning() {
   if (nrOfSatellitesConected > 0) {
      delay(500);
   	 systemState = RUNNING;
   
      runningInitiated = millis();
   	 pingSatelliteCount = 0;
   	 satellitePinged = 0;
      pingSequenceCount = 0;
      samplePrintCount = 0;
        
      // clear samples queue
      for (int i = 0; i < MAX_CONNECTED_SATELLITES; i++) {
          while (samples[i].isEmpty() == false)
              samples[i].pop();
      }
        
      syncSamples();
      Serial.println("Running initiated");
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
    unsigned long int timeFromRunningInitiated = (millis() - runningInitiated) < 0 ? 0 : (millis() - runningInitiated);

    // after 1 second the samples are printet
    if (timeFromRunningInitiated > 50) {
        // calculate timewindow
        unsigned long int timeWindowStart = 50 + samplePrintCount * TIME_BETWEEN_SAMPLES;

        Sample samplesToPrint[8];
        for (int i = 0; i < 8; i++)
            samplesToPrint[i].valid = false;
        
        // if time to print sample
        if (timeFromRunningInitiated > timeWindowStart) {
            // check if there are samples to print
            if (samples[0].isEmpty() == false) {
                for (int satellite = 0; satellite < nrOfSatellitesConected; satellite++) {
                    samplesToPrint[satellite] = samples[satellite].dequeue();
                    
                    // Print sample information to serial
                    Serial.println("-----------");
                    if (samplesToPrint[satellite].valid) {
                        Serial.print("Sat "); 
                        Serial.print(satellite); 
                        Serial.print(": "); 
                        Serial.println(samplesToPrint[satellite].value);
                    }
                    
                    else {
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

