#include <rfhw.h>
#include "Arduino.h"

// Size of packages - needs to match with sender
#define SEND_PACKET 85
#define RECEIVE_PACKET 10

uint8_t arraySend[SEND_PACKET];

void setup() {
  // Init array
  for (int i = 0; i < SEND_PACKET; i++) {
    arraySend[i] = 'a';
  }

  // Init RF module
  rf::phy_init((uint8_t)20);

  // Give time for bootup
  delay(500);
}

void loop() {
  // receive data
  uint8_t len = 0;
  uint8_t* rdata = rf::phy_receive(&len);
  if (rdata != NULL) {
      // When data is received (and it has correct length, send data again)
      if (len == RECEIVE_PACKET) {
        SendData();
      }
  }
}

void SendData() {
  // Give receiver time to turn on the receiver on again
  delayMicroseconds(3);

  // Send data
  if (!rf::phy_send(arraySend, SEND_PACKET))
  {
    return;
  }
}
