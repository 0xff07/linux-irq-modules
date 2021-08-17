#define ARDUINO_IRQ_GPIO A0

void setup() {
	pinMode(ARDUINO_IRQ_GPIO, OUTPUT);
	Serial.begin(9600);
	Serial.println("Press Enter to send IRQ...");
}

void loop() {
	char in;
	if (Serial.available() > 0) {
		in = Serial.read();
		if (in == '\n') {
			digitalWrite(ARDUINO_IRQ_GPIO, HIGH);
			delay(100);
			digitalWrite(ARDUINO_IRQ_GPIO, LOW);
			Serial.println("IRQ signal sent!");
		}
	}
}
