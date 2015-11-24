#include <rfpr.h>
#include <rfhw.h>

// Global variables
int myVID = -1; // Not allocated

// Global constants
#define RID 1
#define GROUP 20

// Prototypes
int getSample();
void adcSetup();

void setup() {
  adcSetup();
  rf::hw_init((uint8_t)GROUP);
  delay(3000);
  while (myVID == -1)
    registerToBase();
}

void loop() {
  // put your main code here, to run repeatedly:

}

int registerToBase()
{
  rf::pr_send_connectRequest((uint16_t)RID, '5'); // TODO The base needs to returns the satellites' VID

  void* data = malloc(sizeof(10)); // TODO Size?
  rf::pr_receive(data);

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
