// QTR-6A testprogramma voor ESP32
// Leest 6 analoge sensoren en print ze in één regel

const int qtrPins[6] = {39, 36, 34, 35, 32, 33};
const int ledonpin = 13; // LED pin voor indicatie

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Serial.println("QTR-6A test gestart...");
  pinMode(ledonpin, OUTPUT);
  digitalWrite(ledonpin, HIGH); // Zet de LED aan om aan te geven dat het programma draait
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

