#include <rfpr.h>
#include <rfhw.h>

// Global constants
#define RID 1
#define GROUP 20
#define SAMPLE_ARRAY_SIZE 50
#define TIME_BETWEEN_PING 200 // We will get pinged for data at least every x ms
#define WAIT_TIME_FOR_ADC 3
#define RF_POWER_UP_TIME 3000

// Global variables
int myVID = -1; // Not allocated
int samplesCounter = 0; // Needed in order to check how much data we're sending
uint16_t sampleArray[SAMPLE_ARRAY_SIZE]; // The data being sent to the base
unsigned long int lastSleep = 0; // We must sleep between each ping in order to save battery
bool pingReceived;

// Prototypes
int getSample();
void adcSetup();
int registerToBase();

void setup() {
  adcSetup(); // Setting the correct ports of ADC
  rf::hw_init((uint8_t)GROUP); // Initializing the RF module
  delay(RF_POWER_UP_TIME); // Waiting for the RF module to power up
  while (myVID == -1)
    myVID = registerToBase(); // Waiting for the base to acknowledge us, granting a VID
}

void loop() {
	if (millis() - lastSleep > TIME_BETWEEN_PING) // TODO Need a threshold
	{
		// Turn on RF module, receive data, check if data is a ping

		// If true:
		pingReceived = true;
		// Let the RF module sleep
		lastSleep = millis();

		// If false: Do nothing
	}

	if (pingReceived)
	{
		rf::pr_send_dataSending(sampleArray);
		samplesCounter = 0;
	}
	
	sampleArray[samplesCounter < SAMPLE_ARRAY_SIZE ? samplesCounter++ : samplesCounter] = (uint16_t)getSample(); // Getting value from the straingauge
	delay(WAIT_TIME_FOR_ADC);
}

int registerToBase()
{
  rf::pr_send_connectRequest((uint16_t)RID, '5'); // TODO The base needs to returns the satellites' VID

  void* data = malloc(sizeof(10)); // TODO Size?
  rf::pr_receive(data);

  // Do something with the data

  free(data);

  // If no confirmation (and a VID) is received from the base
  return -1;
}

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

int getSample() {
  // do single conversion
  ADCSRA |= ((1 << ADSC) | (1 << ADIF));

  // wait for conversion to finish-
  // busywait until bit is smth
  while (!(ADCSRA & (1 << ADIF)));

  //Return the data
  //get the first 2 lsb from ADCH and ADCL and return them as int
  int value = (ADCL | ((ADCH & 0x03) << 8));
  return value;
}
