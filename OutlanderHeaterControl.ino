/* Arduino & MCP2515 canbus controller for Outlander heater. Forked from original by @JamieJones85.
 * Updated for latest version of MCP_CAN library and changed to send data via canbus rather than to display
 * Heater control itself is now done by Zombieverter: this controller just sends the pot's HeatReq
 * switch state and value on CAN ID 509 (0x1FD) for Zombieverter to act on. HV/DC-DC status is still
 * monitored and reported for telemetry, but no longer gates HeatReq - Zombieverter is trusted to
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
unsigned long hvLastRec;
byte hvStatus;
unsigned long temperatureLastRec;
long unsigned int rxId;

#define ZOMBIE_HEATER_CAN_ID 509 // 0x1FD
bool enabled = false;
bool hvPresent = false;
bool heating = false;
int currentTemperature = 0;
const int potPin = A0;
const int ledPin = 3;
const int pumpRelay = 5;
const int powerSwitch = 6;

int heartbeat = 0;

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN); 

void ms10Task();
void ms100Task();
void ms1000Task();

Task ms10(10, -1, &ms10Task);
Task ms100(100, -1, &ms100Task);
Task ms1000(1000, -1, &ms1000Task);

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
  
  runner.addTask(ms10);
  ms10.enable();

  runner.addTask(ms100);
  ms100.enable();

  runner.addTask(ms1000);
  ms1000.enable();
}

void loop() {
  unsigned char len = 0;
  unsigned char buf[8];
  // put your main code here, to run repeatedly:
  runner.execute();
  if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
  {
        CAN.readMsgBuf(&rxId, &len, buf);    // read data,  len: data length, buf: data buf

        if (rxId == 0x398) {
          //Heater status
          if (buf[5] == 0x00) {
            heating = false;
          } else if (buf[5] > 0) {
            heating = true;
          }
          //hv status
          if (buf[6] == 0x09) {
            hvPresent = false;
          } else if (buf[6] == 0x00) {
            hvPresent = true;
          }

          //temperatures
          unsigned int temp1 = buf[3] - 40;
          unsigned int temp2 = buf[4] - 40;
          if (temp2 > temp1) {
            currentTemperature = temp2;
          } else {
            currentTemperature = temp1;
          }
          temperatureLastRec = millis();
        }
        if (rxId == 0x377) {
          hvLastRec = millis();
          hvStatus = buf[7];
        }
        if (rxId == 0x285) {
          if (buf[2] == 0xB6) {
            heartbeat = 1;
          } else {
          heartbeat = 0;
          }
        }
    }
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

void ms10Task() {
  if (heartbeat == 0) {
  //send 0x285
   uint8_t canData[8];
   canData[0] = 0x00;
   canData[1] = 0x00;
   canData[2] = 0x14;
   canData[3] = 0x21;
   canData[4] = 0x90;
   canData[5] = 0xFE;
   canData[6] = 0x0C;
   canData[7] = 0x10;

   CAN.sendMsgBuf(0x285, 0, sizeof(canData), canData);
  }
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

  CAN.sendMsgBuf(ZOMBIE_HEATER_CAN_ID, 0, sizeof(canData), canData);
}

void ms1000Task() {
  #ifdef DEBUG
    Serial.print("Enabled: ");
    Serial.println(enabled);
    Serial.println("Heater Status");
    Serial.print("HV Present: ");
    Serial.print(hvPresent);
    Serial.print(" Heater Active: ");
    Serial.print(heating);
    Serial.print(" Water Temperature: ");
    Serial.print(currentTemperature);
    Serial.println("C");
    Serial.println("");
    Serial.println("Settings");
    Serial.print(" Heating: ");
    Serial.print(enabled);
    Serial.println("");
    Serial.println("");
  #endif

  //send information on canbus via caninfoID

   uint8_t canData[8];
   canData[0] = hvPresent; // HV Present
   canData[1] = enabled; // Heater enabled
   canData[2] = heating; // Heater active
   canData[3] = currentTemperature; // Water Temp
   canData[4] = 0x00; // Not used
   canData[5] = 0x00; // Not used
   canData[6] = 0x00; // Not used
   canData[7] = 0x00; // Not used

   CAN.sendMsgBuf(0x300, 0, sizeof(canData), canData);
}
