#include <rfpr.h>
#include <rfhw.h>

// Global variables
int nextVID = 0;

// Global constants
#define GROUP 20

void setup() {
  rf::hw_init((uint8_t)GROUP);
  delay(3000);
}

void loop() {
  // put your main code here, to run repeatedly:

}
