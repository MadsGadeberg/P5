#include <rfhw.h>
#include "Arduino.h"

#define SEND_PACKET 10
#define receive_SIZE 85
//#define WAIT 1

uint8_t arraySend[SEND_PACKET];
uint8_t arrayreceive[receive_SIZE];
uint32_t lastSendTime = 0;

void setup() {
  // Init array
   for (int i = 0; i < SEND_PACKET; i++) {
    arraySend[i] = 'a';
  }  

  // Init serial
  Serial.begin(57600);

  // Init RF module
  rf::phy_init((uint8_t)20);

  // Send data
  SendData();
}

void loop() {
  // receive data
  uint8_t len = 0;
  volatile uint8_t* rdata = rf::phy_receive(&len);
  if (rdata != NULL) {
    if (len == receive_SIZE) {
      uint32_t current = millis();
      
      int calculatedTime = current - lastSendTime;

      #ifndef WAIT
      Serial.println(calculatedTime);
      #endif
      SendData();
    }
  }

    if (Serial.available() > 0) {

      // read the incoming byte:
      char incomingByte = Serial.read();

      if (incomingByte == 's')
        SendData();
    }

    if (millis() - lastSendTime > 200) {
      Serial.println("r");
      SendData();
    }
}

void SendData() {
  lastSendTime = millis();
  delay(5);

#ifdef WAIT
  uint32_t start = millis();
#endif

#ifndef WAIT
  if (!rf::phy_send(arraySend, SEND_PACKET))
#else
  if (!rf::phy_sendWait(arraySend, SEND_PACKET))
#endif
  {
    Serial.println("NOT SEND");
    return;
  }

#ifdef WAIT
  uint32_t result = millis() - start;
  Serial.println(result);
#endif
  
  lastSendTime = millis();
}
