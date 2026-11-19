#include "SerialCommand.h"
#include "BluetoothSerial.h"
#include "EEPROMAnything.h"
#include "EEPROM.h"
#include <math.h>
#include <string.h>

#define Baudrate 115200
#define SENSOR_COUNT 6
#define EEPROM_MAGIC 0x4C463032UL
#define EEPROM_SIZE sizeof(param_t)

BluetoothSerial SerialBT;
SerialCommand sCmd(SerialBT);

// VAR
bool debug = false, run = false, blink = false;
int raw[SENSOR_COUNT], normalised[SENSOR_COUNT], powerLeft = 0, powerRight = 0;
unsigned int kruispunt;
float position, iTerm, lastErr;
unsigned long previous, calculationTime, drivePrevious, driveHelp;

// Motorpinnen
const int MotorLeftIn1  = 16;
const int MotorLeftIn2  = 17;
const int MotorRightIn1 = 18;
const int MotorRightIn2 = 19;
const int MotorLeftPwm  = 5;
const int MotorRightPwm = 21;
const int standbyPin    = 4;

// Pin's
const int startButton = 23;
const int stopButton  = 22;
const int sensor[SENSOR_COUNT] = {39, 36, 34, 35, 32, 33};

// Param die in EEPROM staan
struct param_t {
  uint32_t magic;
  unsigned long cycleTime;
  int white[SENSOR_COUNT], black[SENSOR_COUNT], threshold, power;
  float diff, kp, ki, kd;
} params;

struct legacy_param_t {
  unsigned long cycleTime;
  int black, white[8], power, maxWhite;
  float diff, kp, ki, kd;
};

// --------- Sensor lezen ---------
void sensorRead() {
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    raw[i] = analogRead(sensor[i]);
  }
}

void saveParams() {
  params.magic = EEPROM_MAGIC;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM_writeAnything(0, params);
  EEPROM.end();
}

void setDefaultParams() {
  memset(&params, 0, sizeof(params));
  params.magic = EEPROM_MAGIC;
  params.cycleTime = 1000;
  params.threshold = 500;
  params.power = 100;
  params.kp = 1.0f;
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    params.white[i] = 4095;
    params.black[i] = 0;
  }
}

void loadParams() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM_readAnything(0, params);
  EEPROM.end();

  if (params.magic == EEPROM_MAGIC &&
      params.cycleTime > 0 && params.cycleTime <= 1000000UL &&
      params.threshold >= 1 && params.threshold < 1000 &&
      params.power >= 0 && params.power <= 255) {
    return;
  }

  legacy_param_t legacy;
  EEPROM.begin(sizeof(legacy));
  EEPROM_readAnything(0, legacy);
  EEPROM.end();

  setDefaultParams();
  if (legacy.cycleTime > 0 && legacy.cycleTime <= 1000000UL &&
      legacy.power >= 0 && legacy.power <= 255) {
    params.cycleTime = legacy.cycleTime;
    params.power = legacy.power;
    params.diff = legacy.diff;
    params.kp = legacy.kp;
    params.ki = legacy.ki;
    params.kd = legacy.kd;
  }
  saveParams();
}

int readSensorAverage(uint8_t index) {
  long total = 0;
  const uint8_t sampleCount = 16;
  for (uint8_t sample = 0; sample < sampleCount; sample++) {
    total += analogRead(sensor[index]);
  }
  return total / sampleCount;
}


// --------- Debug ---------
void onDebug() {
  SerialBT.println();
  SerialBT.print("cycle:"); SerialBT.println(params.cycleTime);
  SerialBT.print("threshold:"); SerialBT.println(params.threshold);
  SerialBT.print("white:");
  for (int i = 0; i < SENSOR_COUNT; i++) { SerialBT.print(params.white[i]); SerialBT.print(' '); }
  SerialBT.println();
  SerialBT.print("black:");
  for (int i = 0; i < SENSOR_COUNT; i++) { SerialBT.print(params.black[i]); SerialBT.print(' '); }
  SerialBT.println();

  SerialBT.print("norm:");
  for (int i = 0; i < SENSOR_COUNT; i++) { SerialBT.print(normalised[i]); SerialBT.print(' '); }
  SerialBT.println();

  SerialBT.print("raw:");
  for (int i = 0; i < SENSOR_COUNT; i++) { SerialBT.print(raw[i]); SerialBT.print(' '); }
  SerialBT.println();

  SerialBT.print("pos:");  SerialBT.println(position, 1);
  SerialBT.print("pwr:");  SerialBT.println(params.power);
  SerialBT.print("diff:"); SerialBT.println(params.diff, 2);
  SerialBT.print("kp:");   SerialBT.println(params.kp, 2);
  float cycleSec = params.cycleTime * 1e-6f;
  SerialBT.print("ki:");   SerialBT.println(params.ki / cycleSec, 2);
  SerialBT.print("kd:");   SerialBT.println(params.kd * cycleSec, 2);
  SerialBT.print("left:"); SerialBT.print(powerLeft);
  SerialBT.print(" right:"); SerialBT.println(powerRight);
  SerialBT.print("calc:"); SerialBT.println(calculationTime);
}


// --------- Help ---------
void onHelp() {
  SerialBT.println();
  SerialBT.println("---------------------------------------------------------------------------");
  SerialBT.println("Possible commands:                   <Argument1>   <Argument2>");
  SerialBT.println();
  SerialBT.println("start      (starts program)          <NONE>        <NONE>");
  SerialBT.println("stop       (stops program)           <NONE>        <NONE>");
  SerialBT.println();
  SerialBT.println("set        (sets params)");
  SerialBT.println("                                     <cycle>       <value (int)>");
  SerialBT.println("                                     <power>       <value (int)>");
  SerialBT.println("                                     <diff>        <value (float)>");
  SerialBT.println("                                     <kp>          <value (float)>");
  SerialBT.println("                                     <ki>          <value (float)>");
  SerialBT.println("                                     <kd>          <value (float)>");
  SerialBT.println("                                     <threshold>   <value (1-999)>");
  SerialBT.println();
  SerialBT.println("calibrate  (sensor calibration)      <white|black>");
  SerialBT.println("debug      (shows EEPROM values)     <NONE>        <NONE>");
  SerialBT.println("---------------------------------------------------------------------------");
}


// --------- Calibratie ---------
void onCalibrate() {
  char* surface = sCmd.next();
  if (!surface || (strcmp(surface, "white") != 0 && strcmp(surface, "black") != 0)) {
    SerialBT.println("Use: calibrate white or calibrate black");
    return;
  }

  int readings[SENSOR_COUNT];
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    readings[i] = readSensorAverage(i);
    int otherSurface = (strcmp(surface, "white") == 0) ? params.black[i] : params.white[i];
    if (readings[i] == otherSurface) {
      SerialBT.println("Calibration rejected: white and black readings must differ.");
      return;
    }
  }

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (strcmp(surface, "white") == 0) params.white[i] = readings[i];
    else params.black[i] = readings[i];
  }
  saveParams();
  SerialBT.print("Calibrated ");
  SerialBT.println(surface);
}


// --------- Start / Stop ---------
void onStart() {
  run = true;
  SerialBT.println("The car is started");
  iTerm = 0;
  drivePrevious = micros();
  driveHelp     = drivePrevious;
  kruispunt     = 0;
}

void onStop() {
  run = false;
  SerialBT.println("The car is stopt");
}


// --------- Set parameters ---------
void onSet() {
  char* param = sCmd.next();
  char* value = sCmd.next();
  if (!param || !value) return;

  if (strcmp(param, "cycle") == 0) {
    long newCycle = atol(value);
    if (newCycle > 0 && newCycle <= 1000000L) {
      float ratio = (float)newCycle / params.cycleTime;
      params.ki *= ratio;
      params.kd /= ratio;
      params.cycleTime = newCycle;
    }
  } else if (strcmp(param, "power") == 0) {
    params.power = constrain(atoi(value), 0, 255);
  } else if (strcmp(param, "threshold") == 0) {
    int threshold = atoi(value);
    if (threshold < 1 || threshold >= 1000) {
      SerialBT.println("Threshold must be between 1 and 999.");
      return;
    }
    params.threshold = threshold;
  } else if (strcmp(param, "diff") == 0) {
    params.diff = atof(value);
  } else if (strcmp(param, "kp") == 0) {
    params.kp = atof(value);
  } else if (strcmp(param, "ki") == 0) {
    params.ki = atof(value) * (params.cycleTime * 1e-6f);
  } else if (strcmp(param, "kd") == 0) {
    params.kd = atof(value) / (params.cycleTime * 1e-6f);
  }

  saveParams();
}


// --------- Unknown command ---------
void onUnknownCommand(char *command) {
  SerialBT.print("Unknown Command: \"");
  SerialBT.print(command);
  SerialBT.println("\"");
}


// --------- setup ---------
void setup() {
  SerialBT.begin("BrancoDD Auto");

  sCmd.addCommand("set",       onSet);
  sCmd.addCommand("start",     onStart);
  sCmd.addCommand("stop",      onStop);
  sCmd.addCommand("calibrate", onCalibrate);
  sCmd.addCommand("help",      onHelp);
  sCmd.addCommand("debug",     onDebug);
  sCmd.setDefaultHandler(onUnknownCommand);

  loadParams();

  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);
  pinMode(MotorLeftIn1, OUTPUT);
  pinMode(MotorLeftIn2, OUTPUT);
  pinMode(MotorRightIn1, OUTPUT);
  pinMode(MotorRightIn2, OUTPUT);
  pinMode(MotorLeftPwm, OUTPUT);
  pinMode(MotorRightPwm, OUTPUT);
  pinMode(standbyPin, OUTPUT);
  analogWrite(MotorLeftPwm, 0);
  analogWrite(MotorRightPwm, 0);
  digitalWrite(standbyPin, HIGH);

  SerialBT.println("ready");
}


// --------- loop ---------
void loop() {
  sCmd.readSerial();

  unsigned long current = micros();
  if (current - previous < params.cycleTime) return;
  previous = current;

  sensorRead();

  // normalisatie
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (params.white[i] == params.black[i]) normalised[i] = 0;
    else normalised[i] = constrain(map(raw[i], params.white[i], params.black[i], 0, 1000), 0, 1000);
  }

  uint8_t index = 0;
  int maxVal = normalised[0];
  int zwartTeller = (normalised[0] >= params.threshold) ? 1 : 0;
  for (uint8_t i = 1; i < SENSOR_COUNT; i++) {
    if (normalised[i] > maxVal) { maxVal = normalised[i]; index = i; }
    if (normalised[i] >= params.threshold) zwartTeller++;
  }

  if (maxVal < params.threshold && run) {
    position = (position < 0) ? -70 : 70;
  } else if (zwartTeller >= SENSOR_COUNT - 1) {
    unsigned long drivetime = current;
    if (kruispunt % 6 == 0 && run && kruispunt) {
      float time = (drivetime - drivePrevious) * 1e-6f;
      drivePrevious = drivetime;
      SerialBT.print("lap:"); SerialBT.print(time, 3); SerialBT.println("s");
    } else if (!kruispunt) drivePrevious = drivetime;
    if (drivetime - driveHelp > 1000000UL) { kruispunt++; driveHelp = drivetime; }
  } else if (index == 0) {
    position = -50;
  } else if (index == SENSOR_COUNT - 1) {
    position = 50;
  } else {
    int sNul    = normalised[index];
    int sLinks  = (index > 0) ? normalised[index - 1] : 0;
    int sRechts = (index < SENSOR_COUNT - 1) ? normalised[index + 1] : 0;
    float b = (sRechts - sLinks) * 0.5f;
    float a = sRechts - b - sNul;
    position = (fabs(a) < 0.001f) ? (index - 2.5f) : (-b / (2 * a) + index - 2.5f);
    position *= 15;
  }

  float error  = -position;
  float output = error * params.kp;
  iTerm += params.ki * error;
  iTerm = constrain(iTerm, -510, 510);
  output += iTerm + params.kd * (error - lastErr);
  lastErr = error;
  output  = constrain(output, -510, 510);

  powerLeft = powerRight = 0;
  if (run) {
    if (output >= 0) {
      powerLeft  = constrain(params.power + params.diff * output, -255, 255);
      powerRight = constrain(powerLeft - output, -255, 255);
    } else {
      powerRight = constrain(params.power - params.diff * output, -255, 255);
      powerLeft  = constrain(powerRight + output, -255, 255);
    }
  }

  if (powerLeft > 0) {
    digitalWrite(MotorLeftIn1, HIGH);
    digitalWrite(MotorLeftIn2, LOW);
  } else if (powerLeft < 0) {
    digitalWrite(MotorLeftIn1, LOW);
    digitalWrite(MotorLeftIn2, HIGH);
  } else {
    digitalWrite(MotorLeftIn1, LOW);
    digitalWrite(MotorLeftIn2, LOW);
  }
  analogWrite(MotorLeftPwm, abs(powerLeft));

  if (powerRight > 0) {
    digitalWrite(MotorRightIn1, HIGH);
    digitalWrite(MotorRightIn2, LOW);
  } else if (powerRight < 0) {
    digitalWrite(MotorRightIn1, LOW);
    digitalWrite(MotorRightIn2, HIGH);
  } else {
    digitalWrite(MotorRightIn1, LOW);
    digitalWrite(MotorRightIn2, LOW);
  }
  analogWrite(MotorRightPwm, abs(powerRight));

  unsigned long diff = micros() - current;
  if (diff > calculationTime) calculationTime = diff;
}
