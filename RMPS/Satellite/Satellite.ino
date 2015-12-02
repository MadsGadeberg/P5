#include "rfpr.h"
#include "rfhw.h"
#include "rfapp.h"
#include <Arduino.h>

// Constants
#define RID 1
#define WAIT_TIME_FOR_ADC 3

// Global variables
int myVID = -1; // a Virtual ID that gets assigned from the base when connected to it.
int samplesCounter = 0; // The current nr of samples
uint16_t sampleArray[SAMPLE_ARRAY_SIZE]; // The data being sent to the base

unsigned long int lastSleepTime = 0; // The time of last sleep. Needed because we want to sleep between each ping to save battery
bool pingReceived = false;
char data[20];

// Prototypes
int getSample();
void adcSetup();
int registerToBase();

void setup() {
    adcSetup();
	pinMode(2, OUTPUT);
    rf::pr_initRF();
  
	while (myVID == -1)
		myVID = registerToBase(); // Waiting for the base to acknowledge us, granting a VID
}

void loop() {
	if (millis() - lastSleepTime > TIME_BETWEEN_PING) // TODO Need a threshold
	{
		rf::packetTypes type = rf::pr_receive(data);
		if (type == rf::PING && ((rf::ping*)data)->VID == myVID)
		{
			pingReceived = true;
			lastSleepTime = millis(); // The RF module automatically sleeps after pr_receive()
		}
	}

	if (pingReceived)
	{
		rf::pr_send_sampleDataPacket(sampleArray);
		samplesCounter = 0;
	}

	sampleArray[samplesCounter < SAMPLE_ARRAY_SIZE ? samplesCounter++ : samplesCounter] = (uint16_t)getSample(); // Getting value from the straingauge
	delay(WAIT_TIME_FOR_ADC);
}

// function that sends the Real ID to the base and returns a Virtual ID if tha base accepts our request
int registerToBase(){
	uint16_t newVID = -1; // -1 indicates that no VID have been assigned

	rf::pr_send_connectRequest((uint16_t)RID);

	rf::packetTypes type = rf::pr_receive(data);
	if (type == rf::CONNECTED_CONFIRMATION)
	{
		struct rf::connectedConfirmation *confirmation;
		confirmation = (rf::connectedConfirmation*)data;
		newVID = (confirmation->VID);
	}

	return newVID;
}

// Function that sets up the registers to be able to use the ports as Analog input.
void adcSetup() {
	// PRR - Power Reduction register
	PRR = 0x00;
	//PRR |= 1 << 1; // USI (Universal serial interface) disable
	//PRR |= 1 << 2; // Timer 0 disable
	//PRR |= 1 << 3; // Timer 1 disable

	// ADMUX - ADC Multiplexer Selection Register
	ADMUX = (1 << MUX3); // Sets ADC0 as neg and ADC1 as pos input with a gain of 1

	// ADCSRA - ADC Control and Status Register A
	// ADEN enables adc
	// ADPS 0:2 Controls the input clock to the adc
	ADCSRA = (1 << ADEN) | (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);

	// ADCSRB - ADC Control and Status Register B
	// "BIN" or "7" selects unipolar mode
	//ADCSRB = 0x00; // should be BIN but is reserved for a keyword whatever!??!!

	// DIDR0 - Digital input disable register 0 - 0 is on
	//DIDR0 = 0x00; //
	//DIDR0 |= 1 << 0;
	//DIDR0 |= 1 << 1;
}

// Function that reads input from strain gauge.
int getSample() {
    // Power Strain gauge circuit
    digitalWrite(2, LOW);
  
	// do single conversion
	ADCSRA |= ((1 << ADSC) | (1 << ADIF));

	// wait for conversion to finish-
	// busywait until bit is smth
	while (!(ADCSRA & (1 << ADIF)));

	//Return the data
	//get the first 2 lsb from ADCH and ADCL and return them as int
	int value = (ADCL | ((ADCH & 0x03) << 8));
  
    // Cut power from strain gauge circuit
    digitalWrite(2, HIGH);

	return value;
}
