/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain. Then we begin running through
  solution domain, pulling on the handle and measuring it as we go.

  Motor current turning dial, speed = 255, ~350 to 560mA
  Motor current turning dial, speed = 50, = ~60 to 120mA

  To write:
    record combinations to eeprom
    build calibration routine that takes in first reading (to calc home offset)
    and 2nd reading (to calc reverse direction slack)

*/

//Pin definitions
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
const byte displayLatch = A2;
const byte displayClock = A3;
const byte displayData = A4;

#include <Servo.h>
Servo handleServo;

//Servo values were found using the testServo() function while the
//cracker was deattached from the safe
//Increase in numbers cause handle to go down
//Resting is 5, which is analog read 133/106
//Good pressure is 50, analog 203
//Open is 75 and analog greater than 254
//Max open is 175, analog around 450
const byte servoRestingPosition = 15;
const byte servoPressurePosition = 50; //40 has slippage during indent measuring
const byte servoTryPosition = 80; //80 has handle position of 273
const int handleOpenPosition = 260; //Analog value. Must be less than analog value from servo testing.

#define CCW 0
#define CW 1

volatile int steps = 0; //Keeps track of encoder counts. 8400 per revolution so this can get big.
boolean direction = CW; //steps goes up or down based on direction
boolean previousDirection = CW; //Detects when direction changes to add some steps for encoder slack
const byte homeOffset = 98; //Where does the dial actually land after doing a goHome calibration

//Because we're switching directions we need to add extra steps to take
//up the slack in the encoder
//The greater the adjustment the more negative it goes
int switchDirectionAdjustment = (84 * 1) + 0; //Use 'Test dial control' to determine adjustment size
//84*10 says 90 but is actually 85 (too negative)
//84 * 2 = Says 33 but at actually 32 (too negative)

//DiscC goes in CCW fashion during testing. 12 indentations, each 8.3 numbers wide.
//Each combination progresses at 30 numbers (12*8.3=99.6)
//Center of first indent (98 to 1 inclusive) is 0
#define DISCC_START 0

//DiscB goes in CW fashion during testing. Increments of 3.
#define DISCB_START 98

//DiscA goes in CCW fashion during testing. First valid number is 3.
#define DISCA_START 3

//These are the combination numbers we are attempting
int discA = DISCA_START;
int discB = DISCB_START;
int discC = DISCC_START;

boolean indentsToTry[12]; //Keeps track of the indents we want to try
byte maxIndentAttempts = 0; //Keeps track of the indents we need to try
byte currentIndent = 0; //Keeps track on the indent we are on

long startTime; //Used to measure amount of time taken per test

int handlePosition; //Used to see how far handle moved when pulled on

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
  pinMode(displayLatch, OUTPUT);
  pinMode(displayData, OUTPUT);

  digitalWrite(displayClock, LOW);
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayData, LOW);

  //Setup the encoder interrupts.
  attachInterrupt(digitalPinToInterrupt(encoderA), countA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB), countB, CHANGE);

  //Setup servo
  handleServo.attach(servo);
  handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)

  randomSeed(analogRead(A5));

  enableMotor(); //Turn on motor controller

  //Use the measure indents function to see which indents are skinniest
  //Disable all the idents that are largest or one you don't want to test
  indentsToTry[0] = false; //0
  indentsToTry[1] = true; //8
  indentsToTry[2] = false; //16
  indentsToTry[3] = true; //24
  indentsToTry[4] = false; //33
  indentsToTry[5] = false; //41
  indentsToTry[6] = false; //50
  indentsToTry[7] = false; //58
  indentsToTry[8] = true; //66
  indentsToTry[9] = false; //74
  indentsToTry[10] = false; //83
  indentsToTry[11] = false; //91

  //Calculate how many indents we need to attempt
  maxIndentAttempts = 0;
  for (byte x = 0 ; x < 12 ; x++)
    if (indentsToTry[x] == true) maxIndentAttempts++;

  clearDisplay();
}

void loop()
{
  Serial.println(F("Menu:"));
  Serial.println(F("1) Go home and reset dial"));
  Serial.println(F("2) Test dial control"));
  Serial.println(F("3) Measure indents"));
  Serial.println(F("4) Calibrate handle servo"));
  Serial.println(F("s) Start cracking"));

  while (!Serial.available());
  char incoming = Serial.read();

  if (incoming == '1')
  {
    //Go to starting conditions
    goHome(); //Detect magnet and center the dial
    delay(100);

    resetDial(); //Clear out everything

    Serial.println("Should be at: 0");
  }
  else if (incoming == '2')
  {
    positionTesting(); //Pick random places on dial and prove we can go to them
  }
  else if (incoming == '3')
  {
    measureDiscC(1); //Try to measure the idents in disc C. Give function the number of tests to run (get average).
  }
  else if (incoming == '4')
  {
    testServo(); //Infinite loop to test servo
  }
  else if (incoming == 's')
  {
    startTime = millis();

    //For testing the indent roll over start disc B at 68
    discB = 68 + 3;
    currentIndent = maxIndentAttempts; //Trigger the adjusting of B

    setDiscsToStart(); //Put discs into starting position

    while (1)
    {
      nextCombination(); //Try the next combo!

      if (Serial.available())
      {
        byte incoming = Serial.read();
        if (incoming == 'p')
        {
          Serial.println(F("Pausing"));
          while (!Serial.available());
          Serial.read();
          Serial.println(F("Running"));
        }
        else if (incoming == 'x' || incoming == 's')
        {
          Serial.println(F("Cracking stopped"));
          break; //User wants to stop
        }
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

  //Set discs to their initialized value
  turnCCW();
  int discAIsAt = setDial(discA, false);
  Serial.print("DiscA is at: ");
  Serial.println(discAIsAt);

  turnCW();
  //Turn past disc B one extra spin
  int discBIsAt = setDial(discB, true);
  Serial.print("DiscB is at: ");
  Serial.println(discBIsAt);

  turnCCW();
  int discCIsAt = setDial(discC, false);
  Serial.print("DiscC is at: ");
  Serial.println(discCIsAt);

  Serial.println("Discs are at starting positions");
}

//Given the current state of discs, advance to the next numbers
void nextCombination()
{
  currentIndent++;

  if (currentIndent >= maxIndentAttempts) //Idents are exhuasted, time to adjust discB
  {
    currentIndent = 0; //Reset count

    discB -= 3; //Disc B changes by 3
    if (discB < 0) //disc B is exhauted, time to adjust discA
    {
      Serial.println("Freeze");
      while (1); //Freeze

      discC = DISCC_START; //Reset discC
      discB = DISCB_START; //Reset discB
      discA += 3; //Disc A changes by 3
      if (discC > 99)
      {
        Serial.println("Damn. We exhausted the combination domain. Try changing the center values.");
        disableMotor(); //Power down
        while (1); //Freeze
      }
      else
      {
        //Go to starting conditions
        goHome(); //Detect magnet and center the dial
        delay(1000);

        resetDial(); //Clear out everything
        delay(1000);

        //Adjust discA to this new value
        turnCCW();
        int discAIsAt = setDial(discA, false);
        Serial.print("DiscA is at: ");
        Serial.println(discAIsAt);
        messagePause("Verify disc position and press a key");

        //Adjust discB to this new value
        turnCW();
        int discBIsAt = setDial(discB, true); //Add extra spin
        Serial.print("DiscB is at: ");
        Serial.println(discBIsAt);
        messagePause("Verify disc position and press a key");

        turnCCW();
        int discCIsAt = setDial(discC, false);
        Serial.print("DiscC is at: ");
        Serial.println(discCIsAt);
        messagePause("Verify disc position and press a key");
      }
    }
    else
    {
      discC = getNextIndent(discB); //Get the first indent after B

      long delta = millis() - startTime;
      startTime = millis();
      Serial.print("Time required to run discC: ");
      Serial.println(delta);

      //Adjust discB to this new value
      turnCW();

      //Turn 50 dial ticks CW away from here
      //This is not a good idea. Indent might be
      //within 50, which would push us past the catch on DiscB
      int currentDial = convertEncoderToDial(steps);
      currentDial -= 30;
      if (currentDial < 0) currentDial += 100;
      setDial(currentDial, false);

      int discBIsAt = setDial(discB, false);
      //Serial.print("DiscB is at: ");
      //Serial.println(discBIsAt);
      //messagePause("Check dial position");

      turnCCW();
      int discCIsAt = setDial(discC, false);
      //Serial.print("DiscC is at: ");
      //Serial.println(discCIsAt);
      //messagePause("Check dial position");
    }
  }
  else
  {
    discC = getNextIndent(discC);

    //Adjust discC to this new value
    turnCCW();
    int discCIsAt = setDial(discC, false);
    //Serial.print("DiscC is at: ");
    //Serial.println(discCIsAt);
  }

  showCombination(discA, discB, discC); //Update display

  Serial.print("Time, ");
  Serial.print(millis()); //Show timestamp

  Serial.print(", Combination, ");
  Serial.print(discA);
  Serial.print("/");
  Serial.print(discB);
  Serial.print("/");
  Serial.print(discC);

  //Try the handle
  if (tryHandle() == true)
  {
    Serial.print(", Handle position, ");
    Serial.print(handlePosition);

    Serial.println();
    Serial.println("Door is open!!!");
    announceSuccess();
    disableMotor(); //Power down motor
    while (1); //Freeze
  }

  Serial.print(", Handle position, ");
  Serial.print(handlePosition);

  Serial.println();
}

//Given a spot on the dial, what is the next available indent in the CCW direction
//Takes care of wrap conditions
//Returns the dial position of the next ident
/*int getNextIndent(int currentDialPosition)
  {
  //Have we exhausted the indents to attempt on this dial?
  indentsAttempted--;
  if (indentsAttempted <= 0)
  {
    //Calculate how many indents we need to attempt
    indentsAttempted = 0;
    for (byte x = 0 ; x < 12 ; x++)
      if (indentsToTry[x] == true) indentsAttempted++;

    return (-1); //We have exhausted all the indents. Go to next discB position
  }

  //No? Ok, go to next indent available
  for (int x = 0 ; x < 12 ; x++)
  {
    if (indentsToTry[x] == true) //Are we allowed to use this indent?
    {
      byte nextDialPosition = lookupIndentValues(x);
      if (nextDialPosition > currentDialPosition)
        return (nextDialPosition);
    }
  }

  return (-1); //We have exhausted all the indents. Go to next discB position
  }*/

//Given a spot on the dial, what is the next available indent in the CCW direction
//Takes care of wrap conditions
//Returns the dial position of the next ident
int getNextIndent(int currentDialPosition)
{
  for (int x = 0 ; x < 12 ; x++)
  {
    if (indentsToTry[x] == true) //Are we allowed to use this indent?
    {
      byte nextDialPosition = lookupIndentValues(x);
      if (nextDialPosition > currentDialPosition) return (nextDialPosition);
    }
  }

  //If we never found a next dial value then we have wrap around situation
  //Find the first indent we can use
  for (int x = 0 ; x < 12 ; x++)
  {
    if (indentsToTry[x] == true) //Are we allowed to use this indent?
    {
      return (lookupIndentValues(x));
    }
  }
}

