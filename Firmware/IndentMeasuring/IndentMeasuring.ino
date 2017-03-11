/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain.

  Motor current turning dial, speed = 255, ~350 to 560mA
  Motor current turning dial, speed = 50, = ~60 to 120mA

  To write:
    record combinations to eeprom

  To test:
    boolean tryHandle(void)

  Done:
    gotoStep(int) - Goes to a given step value
    setHome(void) - Detects reed switch and clears steps to zero
    resetDial(void) - Resets discs to a ready to set (turn CW to set disc C)
    int readMotorCurrent(void)
    int measureIndents()

*/

const byte encoderA = 2;
const byte encoderB = 3;
const byte photo = 5;
const byte motorPWM = 6;
const byte button = 7;
const byte motorReset = 8;
const byte servo = 9;
const byte motorDIR = 10;
const byte buzzer = 11;
const byte LED = 13;

const byte currentSense = A0;
const byte servoPosition = A1;
const byte displayClock = A2;
const byte displayEnable = A3;
const byte displaySerial = A4;

#include <Servo.h>
Servo handleServo;

//Servo values were found using the testServo() function while the
//cracker was deattached from the safe
//Increase in numbers cause handle to go down
//Resting is 5, which is analog read 133/106
//Good pressure is 50, analog 203
//Open is 75 and analog greater than 254
//Max open is 175, analog around 450
const byte servoRestingPosition = 5;
const byte servoPressurePosition = 40;
const byte servoTryPosition = 70;
const byte handleOpenPosition = 250; //Analog value. Door handle is in the open position (45 degrees down) if servo got this far.

#define CCW 0
#define CW 1

volatile int steps = 0; //Keeps track of encoder counts. 8400 per revolution so this can get big.
boolean direction = CW; //steps goes up or down based on direction
boolean previousDirection = CW; //Detects when direction changes to add some steps for encoder slack
const byte homeOffset = 0; //Where does the dial actually land after doing a goHome calibration

//DiscA goes in CCW fashion during testing. 12 indentations, each 8.3 numbers wide.
//Each combination progresses at 30 numbers (12*8.3=99.6)
//Center of first indent (98 to 1 inclusive) is 0
#define DISCA_START 0

//DiscB goes in CW fashion during testing. Increments of 3.
#define DISCB_START 98

//DiscC goes in CCW fashion during testing. First valid number is 3.
#define DISCC_START 3

//These are the combination numbers we are attempting
int discA = DISCA_START;
int discB = DISCB_START;
int discC = DISCC_START;

int testNumber = 0; //Keeps track of the 12 indent tests

long startTime;

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

  pinMode(photo, INPUT_PULLUP);

  pinMode(displayClock, OUTPUT);
  pinMode(displayEnable, OUTPUT);
  pinMode(displaySerial, OUTPUT);

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
  messagePause("Press key to start");

  //testServo(); //Infinite loop to test servo
  //measureDiscA(); //Try to measure the idents in disc A
  //positionTesting(); //Pick random places on dial and prove we can go to them

  startTime = millis();
  setDiscsToStart(); //Put discs into starting position

  while (1)
  {
    nextCombination(); //Try the next combo!

    if(Serial.available())
    {
      byte incoming = Serial.read();
      if(incoming == 'p')
      {
        Serial.println("Pausing");
        while(!Serial.available()) delay(1);
      }
    }
  }
}

//Set the discs to their starting positions
void setDiscsToStart()
{
  //Go to starting conditions
  goHome(); //Detect magnet and center the dial
  delay(1000);

  resetDial(); //Clear out everything
  delay(1000);

  //Set discs to their starting position
  turnCCW();
  int discCIsAt = setDial(DISCC_START, false);
  Serial.print("DiscC is at: ");
  Serial.println(discCIsAt);

  turnCW();
  //Turn past disc B one extra spin
  int discBIsAt = setDial(DISCB_START, true);
  Serial.print("DiscB is at: ");
  Serial.println(discBIsAt);

  turnCCW();
  int discAIsAt = setDial(DISCA_START, false);
  Serial.print("DiscA is at: ");
  Serial.println(discAIsAt);

  Serial.println("Discs are at starting positions");
}

//Given the current state of discs, advance to the next numbers
void nextCombination()
{
  testNumber++;
  if (testNumber == 12) //Idents are exhuasted, time to adjust discB
  {
    testNumber = 0;
    discB -= 3; //Disc B changes by 3
    if (discB < 0)
    {
      Serial.println("Freeze");
      while (1); //Freeze

      discA = DISCA_START; //Reset discA
      discB = DISCB_START; //Reset discB
      discC += 3; //DiscC changes by 3
      if (discC > 99)
      {
        Serial.println("Damn. We exhausted the combination domain. Try changing the center values.");
        disableMotor(); //Power down
        while (1); //Freeze
      }
      else
      {
        //Perhaps do a resetDial() here and re-set discs

        //Adjust discC to this new value
        turnCCW();
        int discCIsAt = setDial(discC, false);
        Serial.print("DiscC is at: ");
        Serial.println(discCIsAt);

        //Adjust discB to this new value
        turnCW();
        int discBIsAt = setDial(discB, true);
        Serial.print("DiscB is at: ");
        Serial.println(discBIsAt);

        turnCCW();
        int discAIsAt = setDial(discA, false);
        Serial.print("DiscA is at: ");
        Serial.println(discAIsAt);
      }
    }
    else
    {
      discA = getNextIndent(discB); //Get the first indent after B

      long delta = millis() - startTime;
      startTime = millis();
      Serial.print("Time required to run discA: ");
      Serial.println(delta);

      //Adjust discB to this new value
      turnCW();

      //Turn 50 dial ticks CW away from here
      int currentDial = convertEncoderToDial(steps);
      currentDial -= 50;
      if (currentDial < 0) currentDial += 100;
      setDial(currentDial, false); 

      int discBIsAt = setDial(discB, false);
      Serial.print("DiscB is at: ");
      Serial.println(discBIsAt);

      turnCCW();
      int discAIsAt = setDial(discA, false);
      Serial.print("DiscA is at: ");
      Serial.println(discAIsAt);
    }
  }
  else
  {
    discA = getNextIndent(discA);

    //Adjust discA to this new value
    turnCCW();
    int discAIsAt = setDial(discA, false);
    Serial.print("DiscA is at: ");
    Serial.println(discAIsAt);
  }

  Serial.print("Trying combination: ");
  Serial.print(discC);
  Serial.print("/");
  Serial.print(discB);
  Serial.print("/");
  Serial.print(discA);
  Serial.println();

  //Try the handle
  if (tryHandle() == true)
  {
    Serial.println("Door is open!!!");
    announceSuccess();
    disableMotor(); //Power down
    while (1); //Freeze
  }
}

//Given a spot on the dial, what is the next available indent in the CCW direction
//Takes care of wrap conditions
//Returns the dial position of the next ident
int getNextIndent(int currentDialPosition)
{
  for (int x = 0 ; x < 12 ; x++)
  {
    byte nextDialPosition = lookupIndentValues(x);
    if (nextDialPosition > currentDialPosition) return (nextDialPosition);
  }

  //If we never found a next dial value then we have wrap around situation
  return (lookupIndentValues(0));
}

