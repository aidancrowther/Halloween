/* This Program is written for an ATmega8 microcontroller connected to the 
*  DMX bus of the halloween controller. Its task is to monitor the bus for any 
*  data written to DEVICE_ID + (TURRET_CHANNELS * NUM_TURRETS) channels and
*  retransmit data for each turret whenever an update is detected. Turrets
*  receive commands over 9600 BAUD TX-only SoftUART interfaces. 
*/

#include <TXOnlySerial.h>
#include <DMXSerial.h>

#define BAUD 9600
#define TURRET_CHANNELS 3

#define ID_TWO 9
#define ID_THREE 10
#define ID_FOUR 11
#define ID_FIVE 12
#define ID_SIX 13
#define ID_SEVEN 14
#define ID_EIGHT 15
#define ID_NINE 16

#define NUM_TURRETS 4
#define TURRET_ONE 2
#define TURRET_TWO 3
#define TURRET_THREE 4
#define TURRET_FOUR 5
#define TURRET_FIVE 6
#define TURRET_SIX 7
#define TURRET_SEVEN 8

TXOnlySerial SerialOne(TURRET_ONE);
TXOnlySerial SerialTwo(TURRET_TWO);
TXOnlySerial SerialThree(TURRET_THREE);
TXOnlySerial SerialFour(TURRET_FOUR);

uint8_t Turret_One[TURRET_CHANNELS];
uint8_t Turret_Two[TURRET_CHANNELS];
uint8_t Turret_Three[TURRET_CHANNELS];
uint8_t Turret_Four[TURRET_CHANNELS];

TXOnlySerial TurretInterfaces[NUM_TURRETS] = {SerialOne, SerialTwo, SerialThree, SerialFour};

uint8_t* TurretData[NUM_TURRETS] = {Turret_One, Turret_Two, Turret_Three, Turret_Four};
uint8_t TurretUpdates = 0;

uint16_t DeviceID = 1;

void setup() {

  for(uint8_t i=0; i<8; i++){
    pinMode(ID_TWO+i, INPUT_PULLUP);
  }

  delay(1000);

  for(uint8_t i=0; i<8; i++){
    DeviceID |= ((!digitalRead(ID_TWO+i)) << i+1);
  }

  DMXSerial.init(DMXReceiver);

  for(uint8_t i=0; i<NUM_TURRETS; i++){
    TurretInterfaces[i].begin(BAUD);
    TurretInterfaces[i].println("Interface started!");
    TurretInterfaces[i].println(DeviceID+TURRET_CHANNELS*i);
  }

  delay(1000);

}

void loop() {

  if(DMXSerial.noUpdateSince() >= 23){

    for(uint8_t i=0; i<NUM_TURRETS; i++){
      uint8_t Buffer[TURRET_CHANNELS];
      
      Buffer[0] = DMXSerial.read((i*TURRET_CHANNELS) + 0 + DeviceID);
      Buffer[1] = DMXSerial.read((i*TURRET_CHANNELS) + 1 + DeviceID);
      Buffer[2] = DMXSerial.read((i*TURRET_CHANNELS) + 2 + DeviceID);

      bool Updated = (!(Buffer[0] == TurretData[i][0]) | !(Buffer[1] == TurretData[i][1]) | !(Buffer[2] == TurretData[i][2]));
      TurretUpdates |= Updated << i;
      if(Updated){
        TurretData[i][0] = Buffer[0];
        TurretData[i][1] = Buffer[1];
        TurretData[i][2] = Buffer[2];
      }
    }

  }

  for(uint8_t i=0; i<NUM_TURRETS; i++){
    if((TurretUpdates>>i)&1){
      TurretInterfaces[i].write(TurretData[i][0]);
      TurretInterfaces[i].write(TurretData[i][1]);
      TurretInterfaces[i].write(TurretData[i][2]);
      TurretUpdates &= ~(1 << i);
    }
  }

}
