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
  -home offset setting in eeprom
  -indent testing in eeprom
  servo pull force
  find indents automatically
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
const byte servoPosition = A1;
const byte displayLatch = A2;
const byte displayClock = A3;
const byte displayData = A4;

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
byte homeOffset = EEPROM.read(LOCATION_HOME_OFFSET); //After doing a goHome calibration, adjust this number up or down until dial is at zero

//Because we're switching directions we need to add extra steps to take
//up the slack in the encoder
//The greater the adjustment the more negative it goes
int switchDirectionAdjustment = (84 * 0) + 84/2; //Use 'Test dial control' to determine adjustment size
//84 * 10 = Says 90 but is actually 85 (too negative)
//84 * 2 = Says 33 but is actually 32 (undershoot)
//84 * 1 - 20 = Says 34 but is actually 33.5 (undershoot)
//84 * 1 + 0 = Says 28 but is actually 27.5 (undershoot)
//84 * 0 = Says 85 but is actually 85.9 (overshoot)
//84 * 0 + 84/2 = Good

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

boolean indentsToTry[12]; //Keeps track of the indents we want to try

//Keeps track of the combos we need to try for each disc
//byte maxAAttempts = 33; //Assume solution notch is 3 digits wide
//byte maxBAttempts = 33; //Assume solution notch is 3 digits wide
byte maxCAttempts = 0; //Calculated at startup

//Keeps track of how many combos we've tried on a given disc
//byte discAAttempts = 0;
//byte discBAttempts = 0;
byte discCAttempts = 0;

long startTime; //Used to measure amount of time taken per test

int handlePosition; //Used to see how far handle moved when pulled on

boolean buttonPressed = false; //Keeps track of the 'GO' button.

int indentValues[12]; //Encoder click for a given indent
int indentWidths[12]; //Calculated width of a given indent
int indentDepths[12]; //Not really used
byte indentNumber[12]; //Used to sort indents once values are found

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

  //Setup servo
  handleServo.attach(servo);
  handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)

  randomSeed(analogRead(A5));

  enableMotor(); //Turn on motor controller

  //Use the measure indents function to see which indents are skinniest
  //Disable all the idents that are largest or ones you don't want to test
  indentsToTry[0] = EEPROM.read(LOCATION_TEST_INDENT_0); //0
  indentsToTry[1] = EEPROM.read(LOCATION_TEST_INDENT_1); //8
  indentsToTry[2] = EEPROM.read(LOCATION_TEST_INDENT_2); //16
  indentsToTry[3] = EEPROM.read(LOCATION_TEST_INDENT_3); //24
  indentsToTry[4] = EEPROM.read(LOCATION_TEST_INDENT_4); //33
  indentsToTry[5] = EEPROM.read(LOCATION_TEST_INDENT_5); //41
  indentsToTry[6] = EEPROM.read(LOCATION_TEST_INDENT_6); //50
  indentsToTry[7] = EEPROM.read(LOCATION_TEST_INDENT_7); //58
  indentsToTry[8] = EEPROM.read(LOCATION_TEST_INDENT_8); //66
  indentsToTry[9] = EEPROM.read(LOCATION_TEST_INDENT_9); //74
  indentsToTry[10] = EEPROM.read(LOCATION_TEST_INDENT_10); //83
  indentsToTry[11] = EEPROM.read(LOCATION_TEST_INDENT_11); //91

  //Calculate how many indents we need to attempt on discC
  maxCAttempts = 0;
  for (byte x = 0 ; x < 12 ; x++)
    if (indentsToTry[x] == true) maxCAttempts++;

  //At startup discB may be negative. Fix it.
  if (discB < 0) discB += 100;

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

  Serial.println(F("Menu:"));
  Serial.println(F("1) Go home and reset dial"));
  Serial.println(F("2) Test dial control"));
  Serial.println(F("3) View indent positions"));
  Serial.println(F("4) Find indent positions"));
  Serial.println(F("5) Measure indents"));
  Serial.println(F("6) Set indents to test"));
  Serial.println(F("7) Set starting combos"));
  Serial.println(F("8) Calibrate handle servo"));
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
    goHome(); //Detect magnet and center the dial
    delay(100);

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

    Serial.print(F("\n\rSetting home offset to: "));
    Serial.println(zeroLocation);

    EEPROM.write(LOCATION_HOME_OFFSET, zeroLocation);

    //setDial(0, true); //Turn to zero with an extra spin
    resetDial(); //Go to dial position zero

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
    findIndents(); //Locate the center of the 12 indents
  }
  else if (incoming == '5')
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
  else if (incoming == '6')
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
          if (indentsToTry[x] == true) Serial.print("Test");
          else Serial.print("-");
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
  else if (incoming == '7')
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
  else if (incoming == '8')
  {
    testServo(); //Infinite loop to test servo
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

      handleServo.write(servoPressurePosition); //Apply pressure to handle
      delay(300); //Wait for servo to move
      handleServo.write(servoRestingPosition); //Release servo
      delay(200); //Allow servo to release
    }
    Serial.println();

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
