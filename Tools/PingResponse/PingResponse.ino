#include <rfhw.h>
#include "Arduino.h"

#define SEND_PACKET 85
#define RECIEVE_SIZE 10
//#define WAIT 1

uint8_t arraySend[SEND_PACKET];

void setup() {
  // Init array
  for (int i = 0; i < SEND_PACKET; i++) {
    arraySend[i] = 'a';
  }  

  // Init serial
//  Serial.begin(57600);

  // Init RF module
  rf::hw_init((uint8_t)20);

  // Give time for bootup
  delay(500);
  
//  Serial.println("Init done");
}

void loop() {
  // Recieve data
  uint8_t len = 0;
  volatile uint8_t* rdata = rf::hw_recieve(&len);
  if (rdata != NULL) {
      if (len == RECIEVE_SIZE) {
        SendData();
      }
  }
}

void SendData() {
  delayMicroseconds(3);

#ifdef WAIT
  uint32_t start = millis();
#endif

#ifndef WAIT
  if (!rf::hw_send(arraySend, SEND_PACKET))
#else
  if (!rf::hw_sendWait(arraySend, SEND_PACKET))
#endif
  {
//    Serial.println("NOT SEND");
    return;
  }

#ifdef WAIT
  uint32_t result = millis() - start;
  Serial.println(result);
#endif
}
