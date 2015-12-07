#include <rfhw.h>
#include <rfpr.h>
#include "Arduino.h"

int state = 0;
uint8_t args[4];

char output[50];
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
      Serial.println();
      Serial.println("Hex:");

      for (int i = 0; i < len; i++) {
        Serial.print(" 0x");
        Serial.print(rdata[i], HEX);
      }

      Serial.println();

      // Read from protocol layer
      uint8_t len;
      rf::PacketTypes ptype = rf::pr_receive(output, rdata, len);
      if (ptype == rf::CONNECT_REQUEST) {
        rf::ConnectRequest* request = (rf::ConnectRequest*)output;
        
        Serial.println();
        Serial.println("Connect request");
        Serial.print("RID ");
        Serial.println(request->RID);
      } else if (ptype == rf::CONNECTED_CONFIRMATION) {
        rf::ConnectedConfirmation* request = (rf::ConnectedConfirmation*)output;
        
        Serial.println();
        Serial.println("Connected confirmation");
        Serial.print("VID ");
        Serial.println(request->VID);
        Serial.print("RID ");
        Serial.println((char) request->RID);
      } else if (ptype == rf::PING) {
        rf::Ping* request = (rf::Ping*)output;
        
        Serial.println();
        Serial.println("Ping");
        Serial.print("VID ");
        Serial.println(request->VID);
      } else if (ptype == rf::DATA) {
        rf::SamplePacketVerified* request = (rf::SamplePacketVerified*)output;
        
        Serial.println();
        Serial.println("Data");
        for (int i = 0; i < 20; i++) {
          Serial.print("Nr ");
          Serial.println(i + 1);
          Serial.print("Valid ");
          Serial.println(request->data[i].valid);
          Serial.print("Data ");
          Serial.println(request->data[i].value);
          Serial.println();
        }
      }
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

  if (state > 8) state = 0;
}

bool ReadSerial() {
   while (Serial.available() > 0) {
    currentSerialRead[++currentSerialReadIndex] = (char) Serial.read();
    
    // Detect type of read
    if (currentSerialRead[currentSerialReadIndex] == '\n') {
      // Terminate with null sign - replace newline
      currentSerialRead[currentSerialReadIndex--] = '\0';
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
  } else if (currentSerialRead[0] == 'c' && currentSerialReadIndex > 1) {
    return currentSerialRead[2];
  } else if (currentSerialRead[0] == 'd' && currentSerialReadIndex > 1) {
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
