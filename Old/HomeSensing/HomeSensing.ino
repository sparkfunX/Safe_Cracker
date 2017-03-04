/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain.

  To write:
    int measureIndents()

  To test:
    boolean tryHandle(void)
    int readMotorCurrent(void)

  Done-ish:
    gotoStep(int) - Goes to a given step value
    setHome(void) - Detects reed switch and clears steps to zero
    resetDial(void) - Resets discs to a ready to set (turn CW to set disc C)

  Measure where the indents start/stop on the test safe
    Pull on handle, spin wheel until either servo moves or current goes way up

*/

const byte encoderA = 2;
const byte encoderB = 3;
const byte reed = 5;
const byte servo = 9;
const byte motorReset = 8;
const byte motorPWM = 6;
const byte motorDIR = 10;
const byte buzzer = 11;
const byte LED = 13;
const byte currentSense = A0;
const byte servoPosition = A1;

#include <Servo.h>
Servo handleServo;

const int servoRestingPosition = 100; //Found by trial/error once apparatus is attached to handle
const int servoTryPosition = servoRestingPosition + 45; //TODO change as needed

#define CCW 0
#define CW 1

volatile int steps = 0; //Keeps track of encoder counts. 8400 per revolution so this can get big.
boolean direction = CW; //steps goes up or down based on direction


//DiscA goes in CW fashion during testing. 12 indentations, each 8.3 numbers wide.
//Each combination progresses at 30 numbers (12*8.3=99.6)
//Center of first indent (98 to 1 inclusive) is 0
#define DISCA_START 0

//DiscB goes in CCW fashion during testing. Increments of 3.
#define DISCB_START 0

//DiscC goes in CW fashion during testing. First valid number is 97.
#define DISCC_START 97

//These are the combination numbers we are attempting
int discA = DISCA_START;
int discB = DISCB_START;
int discC = DISCC_START;

void setup()
{
  Serial.begin(115200);
  Serial.println("Safe Cracker");

  pinMode(motorReset, OUTPUT);
  disableMotor();

  pinMode(encoderA, INPUT);
  pinMode(encoderB, INPUT);

  pinMode(motorPWM, OUTPUT);
  pinMode(motorDIR, OUTPUT);

  pinMode(LED, OUTPUT);

  pinMode(currentSense, INPUT);
  pinMode(servoPosition, INPUT);

  pinMode(reed, INPUT_PULLUP);

  //Setup the encoder interrupts.
  attachInterrupt(digitalPinToInterrupt(encoderA), countA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB), countB, CHANGE);

  //Setup servo
  handleServo.attach(servo);
  handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)

  enableMotor(); //Turn on motor controller

  randomSeed(analogRead(A5)); //Needed only for testing
}

void loop()
{
  Serial.println();
  Serial.println("Press key to start");
  while (!Serial.available()); //Wait for user input
  Serial.read(); //Throw away character

  turnCCW();
  int randomDial = random(0, 100);
  setDial(randomDial, false);

  setHome(); //Find home
}

//Finds width of magnetic field and cuts it in half
//Doesn't seem to find exactly center
void setHome()
{
  turnCW();

  //Begin spinning
  setMotorSpeed(255); //Go!

  //Sometimes when we start we're already near the magnet, spin until we are out
  if (magnetDetected() == true)
  {
    int currentDial = convertEncoderToDial(steps);
    currentDial += 50;
    if (currentDial > 100) currentDial -= 100;
    setDial(currentDial, false); //Advance to 50 dial ticks away from here

    setMotorSpeed(255); //Go!
  }

  while(magnetDetected() == false) delayMicroseconds(1);
  int startPosition = steps; //Record first instance of magnet

  //Spin until we don't sense magnet
  for (int checks = 0 ; checks < 100 ; ) //There are a lot of false checks. Wait for 100 of the same.
  {
    if (magnetDetected() == false) checks++; //No magnet
    else checks = 0;
  }

  setMotorSpeed(0);

  steps = 0;
  return; //We detected the center of the reed. We're done.

  delay(250);

  setMotorSpeed(100); //Slow

  //Spin until we don't sense magnet
  while(magnetDetected() == true) delayMicroseconds(1);
  int endPosition = steps; //Record first instance of no magnet

  setMotorSpeed(0);

  //Calculate width of magnetic field
  int difference = 0; 
  if(startPosition >= endPosition) difference = (8400 - startPosition + endPosition) / 2;
  else difference = (endPosition - startPosition) / 2;

  int centerPosition = startPosition + difference;
  if(centerPosition > 8399) centerPosition -= 8400;
  if(centerPosition < 0) centerPosition += 8400;

  gotoStep(centerPosition, false);

  steps = 0;
}

