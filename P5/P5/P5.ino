#include <VirtualWire.h>

const int led_pin = 13;
const int transmit_pin = 4;


void setup()
{
	// Initialise the IO and ISR
	vw_set_tx_pin(transmit_pin);

	vw_setup(8000);	 // Bits per sec

	pinMode(13, OUTPUT);

	Serial.begin(57600);
	Serial.println("Starting...");
}

int count = 1;

void loop()
{
	char msg[7] = { 'h','e','l','l','o',' ','#' };

	msg[6] = count;
	digitalWrite(led_pin, HIGH); // Flash a light to show transmitting
	vw_send((uint8_t *)msg, 7);
	Serial.println(count);
	vw_wait_tx(); // Wait until the whole message is gone
	digitalWrite(led_pin, LOW);
	count = count + 1;
	delay(500);
}
