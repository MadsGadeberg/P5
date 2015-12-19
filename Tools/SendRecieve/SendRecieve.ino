#include <rfhw.h>
#include "Arduino.h"

// Size of packages - needs to match with sender
#define SEND_PACKET 10
#define RECEIVE_PACKET 85

// Timeout of packet to be received
#define RECEIVE_TIMEOUT 200

uint8_t arraySend[SEND_PACKET];
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
  uint8_t* rdata = rf::phy_receive(&len);
  if (rdata != NULL) {
    // When data is received (and it has correct length, send data again)
    if (len == RECEIVE_PACKET) {
      // Get time it took to recieve package
      int calculatedTime = millis() - lastSendTime;
      Serial.println(calculatedTime);

      // Send data - function handles delay
      SendData();
    }
  }

  // Read from serial to support sending on recieving a s
  if (Serial.available() > 0) {
    // read the incoming byte:
    char incomingByte = Serial.read();

    // Send data if byte is s
    if (incomingByte == 's')
      SendData();
  }

  // Handle timeout of packet and send a new if not received any answer
  if (millis() - lastSendTime > RECEIVE_TIMEOUT) {
    Serial.println("r");
    SendData();
  }
}

void SendData() {
  // Wait 5 ms to delay execution of sending next packet
  delay(5);

  // Update last time sent
  lastSendTime = millis();

  // Send package
  if (!rf::phy_send(arraySend, SEND_PACKET))
  {
    Serial.println("NOT SEND");
    return;
  }
}
