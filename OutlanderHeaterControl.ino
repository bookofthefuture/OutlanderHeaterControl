/* Arduino & MCP2515 canbus controller for Outlander heater. Forked from original by @JamieJones85.
 * Updated for latest version of MCP_CAN library and changed to send data via canbus rather than to display
 * Heater control itself is now done by Zombieverter: this controller just sends the pot's HeatReq
 * switch state and value on CAN ID 509 (0x1FD) for Zombieverter to act on. Zombieverter is trusted to
 * apply its own HV/contactor safety check before enabling the heater.
 * Note: SPI pins for Arduino Pro Mini:
 * CS/SS: 10
 * MOSI: 11
 * MISO: 12
 * SCK: 13
 *
 * Other I/O:
 * Pot: A0 Green 
 * LED: 3
 * Pump Relay: 5 Grey internal | White External
 * Power Switch: 6` Purple
*/ 

#include <mcp_can.h> // https://github.com/coryjfowler/MCP_CAN_lib
#include <SPI.h>
#include <TaskScheduler.h> // https://github.com/arkhipenko/TaskScheduler
#include <Wire.h>

#define DEBUG

#define INVERTPOT true

#define ZOMBIE_HEATER_CAN_ID 509 // 0x1FD
bool enabled = false;
const int potPin = A0;
const int ledPin = 3;
const int pumpRelay = 5;
const int powerSwitch = 6;

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

void ms100Task();

Task ms100(100, -1, &ms100Task);

Scheduler runner;

void setup() {
  #ifdef DEBUG
    Serial.begin(115200);
   Serial.println("Outlander Heater Control");
  #endif
  pinMode(ledPin, OUTPUT);
  pinMode(pumpRelay, OUTPUT);
  pinMode(powerSwitch, INPUT_PULLUP);
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ))              // init can bus : baudrate = 500k
    {
      #ifdef DEBUG
        Serial.println("CAN bus init fail");
        Serial.println(" Init CAN bus interface again");
      #endif
      delay(100);
    }
  #ifdef DEBUG
    Serial.println("CAN BUS Shield init ok!");
  #endif
  CAN.setMode(MCP_NORMAL);   // Change to normal mode to allow messages to be transmitted
 
  runner.init();

  runner.addTask(ms100);
  ms100.enable();
}

void loop() {
  runner.execute();
}

void pumpOn() {
  digitalWrite(pumpRelay, HIGH);
  #ifdef DEBUG
    Serial.println("Pump on");
  #endif
}

void pumpOff() {
  digitalWrite(pumpRelay, LOW);
  #ifdef DEBUG
    Serial.println("Pump on");
  #endif
}

void ms100Task() {
  int sensorValue = analogRead(potPin);
  int powerValue = digitalRead(powerSwitch);

  bool heatReq = (powerValue == 0);
  enabled = heatReq;

  digitalWrite(ledPin, enabled);

  unsigned int potValue;
  if (INVERTPOT) {
      potValue = map(sensorValue, 1023, 0, 0, 4095);
  } else {
      potValue = map(sensorValue, 0, 1023, 0, 4095);
  }

  uint8_t canData[8] = {0};
  canData[0] = heatReq ? 0x01 : 0x00;
  canData[1] = potValue & 0xFF;
  canData[2] = (potValue >> 8) & 0xFF;

  byte sendStatus = CAN.sendMsgBuf(ZOMBIE_HEATER_CAN_ID, 0, sizeof(canData), canData);

  #ifdef DEBUG
    if (sendStatus != CAN_OK) {
      Serial.print("CAN send failed, status: ");
      Serial.println(sendStatus);
    }
    Serial.print("HeatReq: ");
    Serial.print(heatReq);
    Serial.print(" Pot value: ");
    Serial.println(potValue);
  #endif
}
