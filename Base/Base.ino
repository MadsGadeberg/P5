// Arduino digital pins
#define LED_PIN     13

// Button hardware is setup so the button goes LOW when pressed
#define BUTTON_PRESSED LOW
#define BUTTON_NOT_PRESSED HIGH

void setup()
{
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, LOW);

	Serial.begin(1200);  // Hardware supports up to 2400, but 1200 gives longer range
}

void loop()
{
	boolean light_led = false;

	while (Serial.available() == 0);

	int value = Serial.read();
	if (value == 5)
	{
		light_led = true;
		Serial.println(value);
	}

	if (light_led)
	{
		digitalWrite(LED_PIN, HIGH);
		delay(5);
		digitalWrite(LED_PIN, LOW);
	}
}
