#include "P5-lib.cpp"

void setup()
{
	pinMode(12, INPUT);

	Serial.begin(1200);  // Hardware supports up to 2400, but 1200 gives longer range
	Serial.println("Starting transmitter...");
}

void loop()
{

	if (digitalRead(12))
	{
		Serial.write(5);
	}

	delay(50); // Debounce button
  int j = test();
}
