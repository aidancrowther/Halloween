/* This Program is written for an ATmega8 microcontroller connected to the 
*  12V shared UART bus of the halloween controller. Its task is to monitor
*  the bus for a frame signifying it to listen (DMXADDR) to the data stream.
*  It then relays the data as DMX channels/packets to the primary DMX network.
*/

#include <Arduino.h>
#include <DmxSimple.h>
#include <Timeout.h>

#define TIMER2_INTERRUPT_ENABLE() TIMSK |= _BV(TOIE2)
#define TIMER2_INTERRUPT_DISABLE() TIMSK &= ~_BV(TOIE2)

#define DMXADDR 0xA0
#define DMXPIN 9
#define NUMPACKETS 8
#define MUXENABLE 18

#define READING 0b10000000
#define COUNTING 0b01000000

volatile bool pausedXFR = false;
uint16_t packetCount = 0;
unsigned long recvTime = 0;

uint8_t buffer[7];

void setup() {
  delay(2000);
  // put your setup code here, to run once:
  Serial.begin(57600);
  Serial.setTimeout(2);
  Serial.print("Starting on address ");
  Serial.print(DMXADDR, HEX);
  Serial.print("\n");
  DmxSimple.usePin(DMXPIN);

  DDRC |= 0x1F;
  PORTC |= 0x10;
}

bool initSerial(){
  uint8_t packet = Serial.read();
  if((packet & 0xF0) == DMXADDR){
    return true;
  } else {
    return false;
  }
}

bool readData = false;

void loop() {

  if(Serial.available()){
    if(packetCount % 8 == 0){
      packetCount++;
      if(initSerial()){
        TIMER2_INTERRUPT_DISABLE();
        while(packetCount < DMX_SIZE){
          while(!Serial.available());
          DmxSimple.write(packetCount, Serial.read());
          packetCount++;
        }
        packetCount = 0;
      } else {
        while(packetCount%8){
          while(!Serial.available());
          Serial.read();
          packetCount++;
        }
        packetCount = 0;
      }
    }
    TIMER2_INTERRUPT_ENABLE();
  }

}
