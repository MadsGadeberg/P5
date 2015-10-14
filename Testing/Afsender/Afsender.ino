

/// @dir pingPong
/// Demo of a sketch which sends and receives packets.
// 2010-05-17 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// with thanks to Peter G for creating a test sketch and pointing out the issue
// see http://jeelabs.org/2010/05/20/a-subtle-rf12-detail/

#include <JeeLib.h>
int inc = 0;
int arr = (5 + 1) * sizeof(char);
char* payload = (char*)malloc(arr);

void setup () {
    Serial.begin(57600);
    Serial.println(57600);
    Serial.println("Send only");
    rf12_initialize(1, RF12_433MHZ, 33);
    for (int i = 0; i < arr; i++)
    {
      payload[i] = ' ';
    }
}

void loop () {
    rf12_recvDone();

    if (rf12_canSend()) {
        sprintf(payload, "%d", inc++);
        rf12_sendStart(0, payload, arr);
        Serial.println(inc);
        delay(100);
    }
}
