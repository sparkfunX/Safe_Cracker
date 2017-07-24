/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain. Then we begin running through
  solution domain, pulling on the handle and measuring it as we go.

  Motor current turning dial, speed = 255, ~350 to 560mA
  Motor current turning dial, speed = 50, = ~60 to 120mA

  TODO:
  servo rest, pull, and open settings to eeprom
  current combination to nvm
*/

#include "nvm.h" //EEPROM locations for settings
#include <EEPROM.h> //For storing settings and things to NVM

#include <Servo.h>
Servo handleServo;

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
const byte servoPositionButton = A1;
const byte displayLatch = A2;
const byte displayClock = A3;
const byte displayData = A4;

//Servo values are found using the testServo() function while the
//cracker was deattached from the safe

//Settings for 0.8 cubic ft. safe
//On the 1st configuration increasing numbers cause handle to go down
//const byte servoRestingPosition = 15; //Position not pulling/testing on handle
//const byte servoPressurePosition = 50; //Position when doing indent measuring
//const byte servoTryPosition = 80; //Position when testing handle

//Settings for 1.2 cubic ft. safe
//On the 2nd larger configuration, decreasing numbers cause handle to go down
int servoRestingPosition = 100; //Position not pulling/testing on handle
int servoTryPosition = 50; //Position when testing handle
int servoHighPressurePosition = 40; //Position when doing indent measuring

const int timeServoApply = 350;  //ms for servo to apply pressure. 350 works
const int timeServoRelease = 200;  //Allow servo to release. 250 works
const int timeMotorStop = 125; //ms for motor to stop spinning after stop command. 200 works

int handlePosition; //Used to see how far handle moved when pulled on
//const int handleOpenPosition = 187; //Analog value. Must be less than analog value from servo testing.

#define CCW 0
#define CW 1

volatile int steps = 0; //Keeps track of encoder counts. 8400 per revolution so this can get big.
boolean direction = CW; //steps goes up or down based on direction
boolean previousDirection = CW; //Detects when direction changes to add some steps for encoder slack
byte homeOffset = 0; //Found by running findFlag(). Stored in nvm.

//Because we're switching directions we need to add extra steps to take
//up the slack in the encoder
//The greater the adjustment the more negative it goes
int switchDirectionAdjustment = (84 * 0) + 0; //Use 'Test dial control' to determine adjustment size
//84 * 1 - 20 = Says 34 but is actually 33.5 (undershoot)
//84 * 0 = Says 85 but is actually 85.9 (overshoot)

//DiscA goes in CCW fashion during testing, increments 3 each new try.
//We don't know if this is the center of the notch. If you exhaust the combination domain, adjust up or down one.
#define DISCA_START 0

//DiscB goes in CW fashion during testing, increments 3 each new try.
#define DISCB_START (DISCA_START - 3)

//DiscC goes in CCW fashion during testing. 12 indentations, 12 raised bits. Indents are 4.17 numbers wide.
#define DISCC_START 0

//These are the combination numbers we are attempting
int discA = DISCA_START;
int discB = DISCB_START;
int discC = DISCC_START;

//int discA = 30; //Testing only. Don't use.
//int discB = 55;
//int discC = 47;

//Keeps track of the combos we need to try for each disc
//byte maxAAttempts = 33; //Assume solution notch is 3 digits wide
//byte maxBAttempts = 33; //Assume solution notch is 3 digits wide
byte maxCAttempts = 0; //Calculated at startup

//Keeps track of how many combos we've tried on a given disc
//byte discAAttempts = 0;
//byte discBAttempts = 0;
byte discCAttempts = 0;

long startTime; //Used to measure amount of time taken per test

boolean buttonPressed = false; //Keeps track of the 'GO' button.

boolean indentsToTry[12]; //Keeps track of the indents we want to try
int indentLocations[12]; //Encoder click for a given indent
int indentWidths[12]; //Calculated width of a given indent
int indentDepths[12]; //Not really used

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Safe Cracker");

  pinMode(motorReset, OUTPUT);
  disableMotor();

  pinMode(encoderA, INPUT);
  pinMode(encoderB, INPUT);

  pinMode(motorPWM, OUTPUT);
  pinMode(motorDIR, OUTPUT);

  pinMode(LED, OUTPUT);

  pinMode(currentSense, INPUT);
  pinMode(servoPositionButton, INPUT_PULLUP);

  pinMode(photo, INPUT_PULLUP);

  pinMode(button, INPUT_PULLUP);

  pinMode(displayClock, OUTPUT);
  pinMode(displayLatch, OUTPUT);
  pinMode(displayData, OUTPUT);

  digitalWrite(displayClock, LOW);
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayData, LOW);

  //Setup the encoder interrupts.
  attachInterrupt(digitalPinToInterrupt(encoderA), countA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB), countB, CHANGE);

  //Load settings from EEPROM
  homeOffset = EEPROM.read(LOCATION_HOME_OFFSET); //After doing a findFlag calibration, adjust this number up or down until dial is at zero
  Serial.print(F("Home Offset: "));
  Serial.println(homeOffset);

  Serial.println(F("Indent data"));
  for (int indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
  {
    indentsToTry[indentNumber] = EEPROM.read(LOCATION_TEST_INDENT_0 + indentNumber); //Boolean
    EEPROM.get(LOCATION_INDENT_DIAL_0 + (indentNumber * 2), indentLocations[indentNumber]); //Addr, loc. Encoder click for a given indent

    Serial.print(F("IndentNum["));
    Serial.print(indentNumber);
    Serial.print(F("] Encoder["));
    Serial.print(indentLocations[indentNumber]);
    Serial.print(F("] / Dial["));
    Serial.print(convertEncoderToDial(indentLocations[indentNumber]));
    Serial.print(F("] / Width["));
    Serial.print(indentWidths[indentNumber]);
    Serial.print(F("] / Depth["));
    Serial.print(indentDepths[indentNumber]);
    Serial.print(F("] Test["));

    //Print Test if indent will be tested
    if (indentsToTry[indentNumber] == true) Serial.print("Y");
    else Serial.print("N");
    Serial.print(F("]"));

    Serial.println();

  }

  //Get servo settings
  EEPROM.get(LOCATION_SERVO_REST, servoRestingPosition);
  EEPROM.get(LOCATION_SERVO_TEST_PRESSURE, servoTryPosition);
  EEPROM.get(LOCATION_SERVO_HIGH_PRESSURE, servoHighPressurePosition);

  //Validate settings
  if (servoRestingPosition > 250 || servoRestingPosition < 0)
  {
    //New board and/or EEPROM has not been set
    //Assign defaults
    servoRestingPosition = 100;
    servoTryPosition = 50;
    servoHighPressurePosition = 40;

    //Record these defaults to EEPROM
    EEPROM.put(LOCATION_SERVO_REST, servoRestingPosition);
    EEPROM.put(LOCATION_SERVO_TEST_PRESSURE, servoTryPosition);
    EEPROM.put(LOCATION_SERVO_HIGH_PRESSURE, servoHighPressurePosition);
  }

  Serial.print(F("servo: resting["));
  Serial.print(servoRestingPosition);
  Serial.print(F("] try["));
  Serial.print(servoTryPosition);
  Serial.print(F("] HighPressure["));
  Serial.print(servoHighPressurePosition);
  Serial.print(F("]"));
  Serial.println();

  //Setup servo
  handleServo.attach(servo);
  handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)
  delay(timeServoRelease * 3); //Allow servo to release, may be in solution slot

  randomSeed(analogRead(A5));

  //Calculate how many indents we need to attempt on discC
  maxCAttempts = 0;
  for (byte x = 0 ; x < 12 ; x++)
    if (indentsToTry[x] == true) maxCAttempts++;

  //At startup discB may be negative. Fix it.
  if (discB < 0) discB += 100;

  //Tell dial to go to zero
  enableMotor(); //Turn on motor controller
  findFlag(); //Find the flag
  //Adjust steps with the real-world offset
  steps = (84 * homeOffset); //84 * the number the dial sits on when 'home'
  setDial(0, false); //Make dial go to zero

  clearDisplay();
  showCombination(24, 0, 66); //Display winning combination
}

void loop()
{
  char incoming;

  //Main serial control menu
  Serial.println();
  Serial.print(F("Combo to start at: "));
  Serial.print(discA);
  Serial.print("/");
  Serial.print(discB);
  Serial.print("/");
  Serial.print(discC);
  Serial.println();

  Serial.println(F("1) Go home and reset dial"));
  Serial.println(F("2) Test dial control"));
  Serial.println(F("3) View indent positions"));
  Serial.println(F("4) Measure indents"));
  Serial.println(F("5) Set indents to test"));
  Serial.println(F("6) Set starting combos"));
  Serial.println(F("7) Calibrate handle servo"));
  Serial.println(F("8) Test handle button"));
  Serial.println(F("9) Test indent centers"));
  Serial.println(F("s) Start cracking"));

  while (!Serial.available())
  {
    if (digitalRead(button) == LOW)
    {
      delay(50);
      if (digitalRead(button) == LOW)
      {
        buttonPressed = true;
        break;
      }
      else
      {
        buttonPressed = false;
      }
    }
    else
    {
      buttonPressed = false;
    }
  }

  if (buttonPressed == true)
  {
    Serial.println(F("Button pressed!"));

    while (digitalRead(button) == false); //Wait for user to stop pressing button
    buttonPressed = false; //Reset variable

    incoming = 's'; //Act as if user pressed start cracking
  }
  else
  {
    incoming = Serial.read();
  }

  if (incoming == '1')
  {
    //Go to starting conditions
    findFlag(); //Detect the flag and center the dial

    Serial.print(F("Home offset is: "));
    Serial.println(homeOffset);

    int zeroLocation = 0;
    while (1) //Loop until we have good input
    {
      Serial.print(F("Enter where dial is actually at: "));

      while (!Serial.available()); //Wait for user input

      Serial.setTimeout(1000); //Must be long enough for user to enter second character
      zeroLocation = Serial.parseInt(); //Read user input

      Serial.print(zeroLocation);
      if (zeroLocation >= 0 && zeroLocation <= 99) break;
      Serial.println(F(" out of bounds"));
    }

    homeOffset = zeroLocation;

    Serial.print(F("\n\rSetting home offset to: "));
    Serial.println(homeOffset);

    EEPROM.write(LOCATION_HOME_OFFSET, homeOffset);

    //Adjust steps with the real-world offset
    steps = (84 * homeOffset); //84 * the number the dial sits on when 'home'

    setDial(0, false); //Turn to zero

    Serial.println(F("Dial should be at: 0"));
  }
  else if (incoming == '2')
  {
    positionTesting(); //Pick random places on dial and prove we can go to them
  }
  else if (incoming == '3')
  {
    //View indent center values
    for (int x = 0 ; x < 12 ; x++)
    {
      Serial.print(x);
      Serial.print(F(": ["));
      Serial.print(lookupIndentValues(x));
      Serial.print(F("]"));
      Serial.println();
    }
  }
  else if (incoming == '4')
  {
    //Measure indents

    int measurements = 0;
    while (1) //Loop until we have good input
    {
      Serial.print(F("How many measurements would you like to take? (Start with 1)"));
      while (!Serial.available()); //Wait for user input
      Serial.setTimeout(1000); //Must be long enough for user to enter second character
      measurements = Serial.parseInt(); //Read user input

      if (measurements >= 1 && measurements <= 20) break;

      Serial.print(measurements);
      Serial.println(F(" out of bounds"));
    }
    Serial.println(measurements);

    measureDiscC(measurements); //Try to measure the idents in disc C. Give function the number of tests to run (get average).
  }
  else if (incoming == '5')
  {
    while (1) //Loop until exit
    {
      int indent = 0;
      while (1) //Loop until we have valid input
      {
        Serial.println("Indents to test:");
        for (int x = 0 ; x < 12 ; x++)
        {
          Serial.print(x);
          Serial.print(": ");

          //Print Test if indent will be tested
          if (indentsToTry[x] == true) Serial.print("Y");
          else Serial.print("N");

          //Print dial value of this indent
          Serial.print(" / ");
          Serial.print(convertEncoderToDial(indentLocations[x]));

          Serial.println();
        }
        Serial.println("Which indent to change?");
        Serial.println("Type 99 to exit");

        while (!Serial.available()); //Wait for user input

        Serial.setTimeout(500); //Must be long enough for user to enter second character
        indent = Serial.parseInt(); //Read user input

        if (indent >= 0 && indent <= 11) break;
        if (indent == 99) break;

        Serial.print(indent);
        Serial.println(F(" out of bounds"));
      }

      if (indent == 99) break; //User wants to exit

      //Flip it
      if (indentsToTry[indent] == true) indentsToTry[indent] = false;
      else indentsToTry[indent] = true;

      //Record current settings to EEPROM
      EEPROM.put(LOCATION_TEST_INDENT_0 + (indent * sizeof(byte)), indentsToTry[indent]);
    }

    //Calculate how many indents we need to attempt on discC
    maxCAttempts = 0;
    for (byte x = 0 ; x < 12 ; x++)
      if (indentsToTry[x] == true) maxCAttempts++;
  }
  else if (incoming == '6')
  {
    //Set starting combos
    for (byte x = 0 ; x < 3 ; x++)
    {
      int combo = 0;
      while (1) //Loop until we have good input
      {
        Serial.print(F("Enter Combination "));
        if (x == 0) Serial.print("A");
        else if (x == 1) Serial.print("B");
        else if (x == 2) Serial.print("C");
        Serial.print(F(" to start at: "));
        while (!Serial.available()); //Wait for user input

        Serial.setTimeout(1000); //Must be long enough for user to enter second character
        combo = Serial.parseInt(); //Read user input

        if (combo >= 0 && combo <= 99) break;

        Serial.print(combo);
        Serial.println(F(" out of bounds"));
      }
      Serial.println(combo);
      if (x == 0) discA = combo;
      else if (x == 1) discB = combo;
      else if (x == 2) discC = combo;
    }

  }
  else if (incoming == '7')
  {
    testServo();
  }
  else if (incoming == '8')
  {
    testHandleButton(); //Just pull down on handle and vary distance to hit open button
  }
  else if (incoming == '9')
  {

    setDial(0, false); //Turn to zero without extra spin

    //Test center point of each indent
    for (int indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
    {

      //int dialValue = lookupIndentValues(indentNumber); //Get this indent's dial value
      //setDial(dialValue, false); //Go to the middle of this indent's location

      int encoderValue;
      EEPROM.get(LOCATION_INDENT_DIAL_0 + (indentNumber * 2), encoderValue);

      //84 allows bar to hit indent but just barely
      //* 2 lines the bar with indent nicely
      encoderValue += 84 * 2;

      gotoStep(encoderValue, false); //Goto that encoder value, no extra spin

      Serial.print("dialValue: ");
      Serial.print(convertEncoderToDial(encoderValue));

      Serial.print(" encoderValue: ");
      Serial.println(encoderValue);

      handleServo.write(servoTryPosition); //Apply pressure to handle
      delay(timeServoApply); //Wait for servo to move
      handleServo.write(servoRestingPosition); //Release servo
      delay(timeServoRelease); //Allow servo to release
    }
    Serial.println();

  }
  else if (incoming == 'a')
  {
    handleServo.write(servoRestingPosition);
  }
  else if (incoming == 'z')
  {
    handleServo.write(servoTryPosition);
  }
  else if (incoming == 's') //Start cracking!
  {
    clearDisplay();
    showCombination(discA, discB, discC); //Update display

    startTime = millis();

    //Set the discs to the current combinations (user can set if needed from menu)
    resetDiscsWithCurrentCombo(false); //Do not pause with messages

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
    } //End eombination loop
  } //End incoming == 's'
}
