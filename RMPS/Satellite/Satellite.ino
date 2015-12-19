#include "rfpr.h"
#include "rfphy.h"
#include "rfapp.h"
#include <Arduino.h>

// Constants
#define RID 1                               // Unique ID for this satellite
#define SLEEP_TIME_THRESHOLD 10             // Threshold to compensate for drifting
#define TIME_BETWEEN_SAMPLE TIME_BETWEEN_PING / SAMPLES_PER_PACKET

// Global variables
int myVID = -1;                             // a Virtual ID that gets assigned from the base when connected to it.
int samplesCounter = 0;                     // The current number of samples
uint16_t sampleArray[SAMPLES_PER_PACKET];   // The data being sent to the base

unsigned long int pingReceivedTime = 0;     // Time of last ping
char data[SAMPLE_PACKET_SIZE];

// Prototypes
int  getSample();
void adcSetup();
int  registerToBase();
void adcSetup();
int  getSample();
void prepareDataForBase();

void setup() {
	adcSetup();
	pinMode(2, OUTPUT);
	rf::phy_init((uint8_t)GROUP);  // Initializing the RF module
	delay(100);                    // Power up time (worst case from datasheet)

	while (myVID == -1)
		myVID = registerToBase();    // Waiting for the base to acknowledge us, granting a VID
}

void loop() {
	// Check for ping
	if (pingReceivedTime + TIME_BETWEEN_PING_SEQUENCE - SLEEP_TIME_THRESHOLD < millis()) {
		rf::packetTypes type = rf::pr_receive(data);
		if (type == 2) {
			// Start of new sample sequence
			pingReceivedTime = millis();
			samplesCounter = 0;

			// Send dataPacket
			prepareDataForBase();
			rf::pr_send_samplePacket(sampleArray);
		}
	}

	// Is it time to sample new data? when the absolute time is bigger then the calculated sample time, then sample!
	if (pingReceivedTime + samplesCounter * TIME_BETWEEN_SAMPLE < millis())
		sampleArray[(samplesCounter++) % SAMPLES_PER_PACKET] = getSample();
}

// Function that sends the Real ID to the base and returns a Virtual ID if the base accepts our request
int registerToBase() {
	unsigned long int connectRequestSent;
	int newVID = -1; // -1 indicates that no VID have been assigned

	rf::pr_send_connectRequest((uint16_t)RID);
	connectRequestSent = millis();

	while (millis() < connectRequestSent + 50 && myVID == -1) { // Letting the base process connectRequest, and waiting a bit for the confirmation
		rf::packetTypes type = rf::pr_receive(data);
		if (type == rf::CONNECTED_CONFIRMATION) {
			struct rf::ConnectedConfirmation *confirmation;
			confirmation = (rf::ConnectedConfirmation*)data;
			newVID = (confirmation->VID);
		}
	}
 
	return newVID;
}

// Function that sets up the registers to be able to use the ports as Analog input.
void adcSetup() {
	// PRR - Power Reduction register
	PRR = 0x00;
	// ADMUX - ADC Multiplexer Selection Register
	ADMUX = (1 << MUX3);  // Sets ADC0 as neg and ADC1 as pos input with a gain of 1

  // ADCSRA - ADC Control and Status Register A
  // ADEN enables adc
  // ADPS 0:2 Controls the input clock to the adc
	ADCSRA = (1 << ADEN) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);
}

// Function that reads input from strain gauge.
int getSample() {
	// Power Strain gauge circuit
	digitalWrite(2, LOW);

	// Do single conversion
	ADCSRA |= ((1 << ADSC) | (1 << ADIF));

	// Wait for conversion to finish
	while (!(ADCSRA & (1 << ADIF)));

	// Return the data
	// Get the first 2 LSB from ADCH and ADCL and return them as int
	int value = (ADCL | ((ADCH & 0x03) << 8));

	// Cut power from strain gauge circuit
	digitalWrite(2, HIGH);

	return value;
}

// Function to send the correct base - even if we have sampled for than SAMPLES_PER_PACKET and needs to correct the order
void prepareDataForBase() {
	if (samplesCounter < SAMPLES_PER_PACKET)
		return; // Nothing to worry about
   
  // The beginning of the array might have been overriden
	if (samplesCounter >= SAMPLES_PER_PACKET) {
		uint16_t tempSampleArray[SAMPLES_PER_PACKET];

	// Get the oldest data and add to beginning of temp array
	int i = 0;
	for (i; i < samplesCounter; i++)                          
		tempSampleArray[i] = sampleArray[(samplesCounter + i) % SAMPLES_PER_PACKET];

   // Add the rest of the data to temp array
	 int k = 0;
	 for (int j = i; j < SAMPLES_PER_PACKET; j++)              
		 tempSampleArray[j] = sampleArray[k++];

    // Copy the temp into the "real" array
		memcpy(sampleArray, tempSampleArray, sizeof sampleArray); 
	}
}
