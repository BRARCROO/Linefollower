#include "SerialCommand.h"
#include "EEPROMAnything.h"

#define SerialPort Serial
#define Baudrate 115200

SerialCommand sCmd(SerialPort);
bool debug;
unsigned long previous, calculationTime;

struct param_t
{
  unsigned long cycleTime;
  /* andere parameters die in het eeprom geheugen moeten opgeslagen worden voeg je hier toe ... */
} params;

void setup()
{
  SerialPort.begin(Baudrate);

  sCmd.addCommand("set", onSet);
  sCmd.addCommand("debug", onDebug);
  sCmd.setDefaultHandler(onUnknownCommand);

  EEPROM_readAnything(0, params);

  pinMode(13, OUTPUT);
  SerialPort.println("ready");
}

void loop()
{
  sCmd.readSerial();
 
  unsigned long current = micros();
  if (current - previous >= params.cycleTime)
  {
    previous = current;

    /* code die cyclisch moet uitgevoerd worden programmeer je hier ... */
  }

  unsigned long difference = micros() - current;
  if (difference > calculationTime) calculationTime = difference;
}

void onUnknownCommand(char *command)
{
  SerialPort.print("unknown command: \"");
  SerialPort.print(command);
  SerialPort.println("\"");
}

void onSet()
{
  char* param = sCmd.next();
  char* value = sCmd.next();  
  
  if (strcmp(param, "cycle") == 0) params.cycleTime = atol(value);
  /* parameters een nieuwe waarde geven via het set commando doe je hier ... */
  
  EEPROM_writeAnything(0, params);
}

void onDebug()
{
  SerialPort.print("cycle time: ");
  SerialPort.println(params.cycleTime);
  
  /* parameters weergeven met behulp van het debug commando doe je hier ... */
  
  SerialPort.print("calculation time: ");
  SerialPort.println(calculationTime);
  calculationTime = 0;
}
