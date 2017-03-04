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

  setDiscsToStart(); //Put discs into starting position

  /*while (1)
  {
    nextCombination(); //Try the next combo!
    while (!Serial.available()); //Wait for user input
    Serial.read(); //Throw away character
  }*/

  //Try to open the door
  /*boolean doorOpen = tryHandle();
    if(doorOpen == true)
    {
    announceSuccess();
    Serial.println("Holy smokes!");
    }
    else
    {
    Serial.println("Failed: ");
    }*/

  //Measure motor current
  /*while (1)
    {
    //int motorCurrent = readMotorCurrent();

    //Serial.print(motorCurrent);
    //Serial.println(" mA");

    Serial.print("Step: ");
    Serial.println(steps);
    }*/
}

//Given the current state of discs, advance to the next numbers
void nextCombination()
{
  discA = lookupAValues(((discA+2)/8) + 1); //There are 12 indentations. Disc A changes by 8.3.
  if (discA > 99)
  {
    discA = DISCA_START; //Reset discA
    discB += 3; //Disc B changes by 3
    if (discB > 99)
    {
      Serial.println("Freeze");
      while (1); //Freeze

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
        //Adjust discC to this new value

        //Perhaps do a resetDial() here and re-set discs
      }
    }
    else
    {
      //Adjust discB to this new value
      turnCCW();
      int discBIsAt = setDial(discB, false);
      Serial.print("DiscB is at: ");
      Serial.println(discBIsAt);

      turnCW();
      int discAIsAt = setDial(discA, false);
      Serial.print("DiscA is at: ");
      Serial.println(discAIsAt);
    }
  }
  else
  {
    //Adjust discA to this new value
    turnCW();
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

//Set the discs to their starting positions
void setDiscsToStart()
{
  //Go to starting conditions
  setHome(); //Find home
  delay(1000);
  steps = 0;

  resetDial(); //Clear out everything
  delay(1000);

  return;

  //Set discs to their starting position
  turnCW();
  int discCIsAt = setDial(DISCC_START, false);
  Serial.print("DiscC is at: ");
  Serial.println(discCIsAt);

  turnCCW();
  //Turn past disc B one extra spin
  int discBIsAt = setDial(DISCB_START, true);
  Serial.print("DiscB is at: ");
  Serial.println(discBIsAt);

  turnCW();
  int discAIsAt = setDial(DISCA_START, false);
  Serial.print("DiscA is at: ");
  Serial.println(discAIsAt);

  Serial.println("Discs at starting positions");
}

//Disc A is the safety disc that prevents you from feeling the edges of the wheels
//It has 12 upper and 12 low indents which means 100/24 = 4.16 per lower indent
//So it moves a bit across the wheel. We could do floats, instead we'll do a lookup
//Values were found by hand: What number is in the middle of the indent?
int lookupAValues(int indentNumber)
{
  switch(indentNumber)
  {
    case 0: return(0); //98 to 1 on the wheel
    case 1: return(8); //6-9
    case 2: return(16); //14-17
    case 3: return(24); //23-26
    case 4: return(32); //31-34
    case 5: return(41); //39-42
    case 6: return(49); //48-51
    case 7: return(58); //56-59
    case 8: return(66); //64-67
    case 9: return(74); //73-76
    case 10: return(83); //81-84
    case 11: return(91); //90-93
    case 12: return(100); //End
  }
}

