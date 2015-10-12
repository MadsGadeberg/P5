/// @dir pingPong
/// Demo of a sketch which sends and receives packets.
// 2010-05-17 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// with thanks to Peter G for creating a test sketch and pointing out the issue
// see http://jeelabs.org/2010/05/20/a-subtle-rf12-detail/

#include <JeeLib.h>
int lastPackage = -1;
int noPackages = 0;
int lostPackages = 0;
bool isStart = true;

MilliTimer secTimer;

void setup () {
    Serial.begin(57600);
    Serial.println(57600);
    Serial.println("Receive only");
    rf12_initialize(2, RF12_433MHZ, 33);
}

void loop () {
    if (rf12_recvDone() && rf12_crc == 0) {
        int input = atoi((char*)rf12_data);
        if (input == lastPackage + 1 || isStart){
          lastPackage++;
          noPackages++;
          isStart = false;
        }
        else {
          lostPackages  = lostPackages + (input - lastPackage - 1);
          lastPackage = input;
        }
    }

    if (secTimer.poll(1000)) {
       Serial.print("Packages sent: ");
       Serial.print(noPackages);
       Serial.println("");
       Serial.print("Packages lost: ");
       Serial.print(lostPackages);
       Serial.println("");
       Serial.println("");

       noPackages = 0;
       lostPackages = 0;
    }
           
}
