#include <SoftwareSerial.h>
#include <Servo_ATTinyCore.h>

#define RX PIN_PB4
#define TX PIN_PB3
#define LIGHT PIN_PB2
#define PAN PIN_PB1
#define TILT PIN_PB0

#define FLASHINTERVAL 50
#define TILTMAX 180
#define TILTMIN 45
#define PANMAX 180
#define PANMIN 0

SoftwareSerial softSerial(RX, TX);
Servo pan;
Servo tilt;

uint8_t panAngle = 90;
uint8_t tiltAngle = 90;
uint8_t lightSignal = 0;
uint8_t lightState = 0;

unsigned long start = 0;

void setup() {

  OSCCAL -= 0;

  pinMode(LIGHT, OUTPUT);
  digitalWrite(LIGHT, LOW);

  pan.attach(PAN);
  tilt.attach(TILT);

  pan.write(panAngle);
  tilt.write(tiltAngle);

  delay(4000);

  start = millis();

  softSerial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentTime = millis();

  while(softSerial.available() >= 3){

    panAngle = softSerial.read();
    tiltAngle = softSerial.read();
    lightSignal = softSerial.read();

    if(panAngle > PANMAX) panAngle = PANMAX;
    if(panAngle < PANMIN) panAngle = PANMIN;
    if(tiltAngle > TILTMAX) tiltAngle = TILTMAX;
    if(tiltAngle < TILTMIN) tiltAngle = TILTMIN;

    tiltAngle = 180 - tiltAngle; // Flip angle for the tilt motor so that 0 is at the bottom of travel

    while(softSerial.available() % 3 != 0){
      softSerial.read();
    }

  }

  pan.write(panAngle);
  tilt.write(tiltAngle);

  if(lightSignal >= 2 && currentTime >= (start+FLASHINTERVAL)){
    start = currentTime;
    lightState = !lightState;
  } else if(lightSignal < 2){
    lightState = lightSignal;
  }

  digitalWrite(LIGHT, lightState);

}  
