#include <rfhw.h>
#include <rfpr.h>
#include "Arduino.h"

#define PIN_STRAIN 10

void setup() {
  // Init RF module
  rf::hw_init((uint8_t)20);

  // Set pinmode for strain and disable
  pinMode(PIN_STRAIN, OUTPUT);
  digitalWrite(PIN_STRAIN, HIGH);

  // Setup adc
  adcSetup();
}

void loop() {
  uint8_t len = 0;
  uint8_t* data = rf::hw_recieve(&len);
  if (data != NULL) {
    if (len == 4) {
      switch (data[0]) {
        case 'g':
          getData(data[1], (((uint16_t)data[2]) << 8) | (uint16_t)  data[3]);
          break;
        case 'p':
          switch (data[1]) {
            case 'r':
              while (!rf::pr_send_connectRequest(50));
              break;
            case 'c':
              while (!rf::pr_send_connectedConfirmation('a', 0x4));
              break;
            case 'p':
              while (!rf::pr_send_ping(0x4));
              break;
            case 's':
              uint16_t data[20];
              for (int i = 0; i < 20; i++) {
                data[i] = i;
              }
              while (!rf::pr_send_samplePacket(data));
              break;
          }
          break;
        default:
          while (!rf::hw_send(data, len));
      }
    }
    else {
      while (!rf::hw_send(data, len));
    }
  }
}

void getData(uint8_t count, uint16_t delayMicros) {  
  // Add temp buffer for sending - length 30 chooced by random
  char sendData[30];
  
  // Enable strain
  digitalWrite(PIN_STRAIN, LOW);

  // Keep tracks of errors
  int errors = 0;
  
  for (int i = 0; i < count; i++) {
    // Get sample from strain
    int sample = getSample();

    // Update sendData
    sprintf(sendData, "Måling: %d", sample);

    // Send data and increment errors if not sending
    if (!rf::hw_send((uint8_t*)sendData, strlen(sendData))) {
      errors++;
    }

    // Delays selected micro seconds
    delayMicroseconds(delayMicros);
  }

  // Disable strain
  digitalWrite(PIN_STRAIN, HIGH);

  // Send errors
  sprintf(sendData, "Fejl: %d", errors);
  while (!rf::hw_sendWait((uint8_t*)sendData, strlen(sendData)));
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
