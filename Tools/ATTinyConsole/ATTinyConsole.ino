#include <rfhw.h>
#include "Arduino.h"

int state = 0;
uint8_t args[4];

char currentSerialRead[20];
int currentSerialReadIndex = -1;

void setup() {
  // Init serial
  Serial.begin(57600);

  // Init RF module
  rf::hw_init((uint8_t)20);
}

void loop() {
  // Recieve data
  uint8_t len = 0;
  uint8_t* rdata = rf::hw_recieve(&len);
  if (rdata != NULL) {
      Serial.print("Recieved ");
      Serial.println(len);
      
      for (int i = 0; i < len; i++) {
        Serial.print((char) rdata[i]);
      }
      Serial.println();
  }

  switch (state) {
    case 0:
      currentSerialReadIndex = -1;
      Serial.println("Skriv venligst en kommando");
      state++;
      break;
    case 2: case 4: case 6:
      currentSerialReadIndex = -1;
      Serial.print("Indtast argument ");
      Serial.println(state++ / 2);
      break;
    case 8:
      // Send kommando
      SendData(args, 4);
      state = 0;
      
      break;
    default:
      if (ReadSerial()) {
        args[state / 2] = GetArg();

        Serial.print("Argument HEX: ");
        Serial.print(args[state / 2], HEX);
        Serial.print(" Char: ");
        Serial.println((char) args[state / 2]);

        state++;
      }
  }
}

bool ReadSerial() {
   while (Serial.available() > 0) {
    currentSerialRead[++currentSerialReadIndex] = (char) Serial.read();
    
    // Detect type of read
    if (currentSerialRead[currentSerialReadIndex] == '\n') {
      // Terminate with null sign - replace newline
      currentSerialRead[currentSerialReadIndex--] = '\0';
      return true;
    }

    /*
      if (currentSerialRead[0] == '0' && currentSerialRead[1] == 'x') {
      if (currentSerialReadIndex >= 2) {
        return true;
      }
      }
     */
   }
   return false;
}

uint8_t GetArg() {
  if (currentSerialReadIndex < 0) {
    return 0;
  } else if (currentSerialRead[0] == 'c') {
    return currentSerialRead[2];
  } else if (currentSerialRead[0] == 'd') {
    return atoi(currentSerialRead + 2);
  } else {
    return currentSerialRead[0];
  }
}

void SendData(const uint8_t arg[], int len) {
  if (rf::hw_send(arg, len))
  {
    Serial.println("Sent");
    return;
  } else {
    Serial.println("Not sent");
    return;
  }
}
