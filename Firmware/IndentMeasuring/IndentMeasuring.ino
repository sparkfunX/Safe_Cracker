/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain.

  Motor current turning dial, speed = 255, ~350 to 560mA
  Motor current turning dial, speed = 50, = ~60 to 120mA

  To write:
    int measureIndents()

  To test:
    boolean tryHandle(void)

  Done:
    gotoStep(int) - Goes to a given step value
    setHome(void) - Detects reed switch and clears steps to zero
    resetDial(void) - Resets discs to a ready to set (turn CW to set disc C)
    int readMotorCurrent(void)
*/

const byte encoderA = 2;
const byte encoderB = 3;
const byte servo = 9;
const byte motorReset = 8;
const byte motorPWM = 6;
const byte motorDIR = 10;
const byte buzzer = 11;
const byte LED = 13;
const byte currentSense = A4; //Should be A0 but trace is broken
const byte servoPosition = A1;

const byte photoHigh = A3;
const byte photoLow = A2;
const byte photo = A1;

#include <Servo.h>
Servo handleServo;

const int servoRestingPosition = 85; //Found by trial/error once apparatus is attached to handle
const int servoPressurePosition = servoRestingPosition + 40; //Found by trial/error
const int servoTryPosition = servoRestingPosition + 50; //Found by trial/error

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

  pinMode(photo, INPUT_PULLUP);
  pinMode(photoHigh, OUTPUT);
  pinMode(photoLow, OUTPUT);
  digitalWrite(photoHigh, HIGH);
  digitalWrite(photoLow, LOW);

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

  turnCW();
  int randomDial = random(0, 100);
  setDial(randomDial, false);

  messagePause("Time to find home");
  goHome(); //Detect magnet and center the dial

  for (int x = 0 ; x < 4 ; x++)
  {
    Serial.print("Dial is at: ");
    Serial.println(convertEncoderToDial(steps));

    messagePause("Goto random");

    randomDial = random(0, 100);
    setDial(randomDial, false);
//    setDial(58, false);
  }


  /*  turnCW();
    randomDial = random(0, 100);
    setDial(randomDial, false);
    Serial.print("Dial is at: ");
    Serial.println(convertEncoderToDial(steps));
    messagePause("");
  */

  //345 seems to be avg of high?

  //Measure indent heights
  /*messagePause("Motor done. Adjust dial to high indent");

    //Apply pressure to handle
    handleServo.write(servoPressurePosition);
    delay(500); //Allow servo to move to position
    int handlePosition = averageAnalogRead(servoPosition);
    handleServo.write(servoRestingPosition); //Return to resting position (handle horizontal, door closed)

    Serial.print("handlePosition high: ");
    Serial.println(handlePosition);

    //  setDial(randomDial + 5, false);
    messagePause("Adjust dial to low indent");

    //Apply pressure to handle
    handleServo.write(servoPressurePosition);
    delay(500); //Allow servo to move to position
    handlePosition = averageAnalogRead(servoPosition);
    handleServo.write(servoRestingPosition); //Return to resting position (handle horizontal, door closed)

    Serial.print("handlePosition low: ");
    Serial.println(handlePosition);
  */


  //measureDiscA(); //Try to measure the idents in disc A

  //setDiscsToStart(); //Put discs into starting position

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
}

//Attempt to measure the 12 indents in disc A
void measureDiscA()
{
  goHome(); //Detect magnet and center the dial
  //delay(1000);
  messagePause("Home");

  resetDial(); //Clear out everything
  //delay(1000);
  messagePause("Cleared");

  int indents[12];

  for (int x = 0 ; x < 12 ; x++)
  {
    indents[x] = measureIndent();

    //Advance 3 dial numbers

  }
}

//From our current position, begin looking for and take internal measurement of the indents
//on the blocking disc A
//Caller has set the dial to wherever they want to start measuring from
int measureIndent(void)
{
#define SEARCH_PRESSURE 120 //Todo Find by trial error. We don't want massive pressure that may burn out servo
#define SEARCH_SPEED 100 //We don't want 255 fast, just a jaunt speed
#define HIGH_INDENT_HANDLE_POSITION 145 //TODO Wild guess until tested
  int edgeTweedleDee = 0;
  int edgeTweedleDumb = 0;

  //This won't work: the motor is not turning. Don't measure current here

  float startingCurrent = readMotorCurrent(); //Take initial reading
  Serial.print("startingCurrent: ");
  Serial.println(startingCurrent, 3);

  //Apply pressure to handle
  handleServo.write(SEARCH_PRESSURE);

  //TODO change as needed
  delay(300); //Wait for servo to move, but don't let it stall for too long and burn out

  //Spin until we hit the edge of the indent
  turnCW();
  setMotorSpeed(SEARCH_SPEED); //Begin turning dial
  while (readMotorCurrent() < (startingCurrent * 1.5))
  {
    if (averageAnalogRead(servoPosition) < HIGH_INDENT_HANDLE_POSITION)
      Serial.println("Indent detected");
    delay(10);
  }
  setMotorSpeed(0); //Stop!

  delay(100); //Alow motor to stop spinning
  messagePause("Detected edge of indent");

  edgeTweedleDee = steps; //Take measurement

  //Ok, we've made it to the far edge of an indent. Now move backwards towards the other edge.
  turnCCW();

  //Spin until we hit the opposite edge of the indent
  setMotorSpeed(SEARCH_SPEED); //Begin turning dial
  while (readMotorCurrent() < (startingCurrent * 1.5))
  {
    delay(10);
  }
  setMotorSpeed(0); //Stop!

  delay(100); //Alow motor to stop spinning
  messagePause("Detected opposite edge of indent");

  edgeTweedleDumb = steps; //Take measurement

  int sizeOfIndent = abs(edgeTweedleDee - edgeTweedleDumb);
  return (sizeOfIndent);
}

//Set the discs to their starting positions
void setDiscsToStart()
{
  //Go to starting conditions
  goHome(); //Detect magnet and center the dial
  delay(1000);

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

//Given the current state of discs, advance to the next numbers
void nextCombination()
{
  discA = lookupAValues(((discA + 2) / 8) + 1); //There are 12 indentations. Disc A changes by 8.3.
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
