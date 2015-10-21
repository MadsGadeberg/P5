byte testArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

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
      sendData(testArray, sizeof(testArray));
	}

	delay(50); // Debounce button
}

// Start of functions
// Needs to be moved to library

int sendData(int data)
{
  return Serial.write(data);
}

int sendData(byte data[], int size)
{
  return Serial.write(data, size);
}

