// QTR-6A TESTPROGRAMMA – Arduino Leonardo
// Leest 6 analoge sensoren en print ze in één regel

int qtrPins[6] = {A0, A1, A2, A3, A4, A5};

void setup() {
  Serial.begin(115200);
  Serial.println("QTR-6A test gestart...");
}

void loop() {
  int values[6];

  // Lees alle sensoren
  for (int i = 0; i < 6; i++) {
    values[i] = analogRead(qtrPins[i]);
  }

  // Print in één nette lijn
  Serial.print("QTR: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(values[i]);
    Serial.print("\t");
  }
  Serial.println();

  delay(50); // 20 Hz update
}

