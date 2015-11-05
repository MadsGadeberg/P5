void setup() {
  adcSetup();
}

void adcSetup(){
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

void loop() {
  int value = getSample();
  // do whatever with the value
}

int getSample(){
  // activates databuffers for AIN0 and AIN1
  //DIDR0 &= ~(1 << 1);
  //DIDR0 &= ~(1 << 0);

  // do single conversion
  ADCSRA |= ((1<<ADSC) | (1 << ADIF));  

  // wait for conversion to finish-
  // busywait until bit is smth
  while(!(ADCSRA & (1 << ADIF))){}

  // Close buffer
  //DIDR0 |= 1 << 1;
  //DIDR0 |= 1 << 0;

  //Return the data
  //get the first 2 lsb from ADCH and ADCL and return them as int
  char tmpL = ADCL; //tmp værdierne virker ikke!!
  char tmpH = ADCH;
  //return (ADCH & 0x03); // returnerer 0x03
  //return ADCL; // returnerer Mange
  int value = (ADCL | ((ADCH & 0x03) << 8));
  return value;
}
