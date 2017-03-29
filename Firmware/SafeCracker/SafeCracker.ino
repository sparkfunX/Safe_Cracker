/*
  Opening a safe using a motor and a servo
  By: Nathan Seidle @ SparkFun Electronics
  Date: February 24th, 2017

  We use a motor with encoder feedback to try to glean the internal workings of the
  safe to reduce the overall solution domain. Then we begin running through
  solution domain, pulling on the handle and measuring it as we go.

  Motor current turning dial, speed = 255, ~350 to 560mA
  Motor current turning dial, speed = 50, = ~60 to 120mA
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
const byte homeOffset = 98; //After doing a goHome calibration, adjust this number up or down until dial is at zero

//Because we're switching directions we need to add extra steps to take
//up the slack in the encoder
//The greater the adjustment the more negative it goes
int switchDirectionAdjustment = (84 * 1) + 0; //Use 'Test dial control' to determine adjustment size
//84*10 says 90 but is actually 85 (too negative)
//84 * 2 = Says 33 but at actually 32 (too negative)

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
  indentsToTry[1] = false; //8
  indentsToTry[2] = true; //16
  indentsToTry[3] = false; //24
  indentsToTry[4] = false; //33
  indentsToTry[5] = false; //41
  indentsToTry[6] = false; //50
  indentsToTry[7] = false; //58
  indentsToTry[8] = true; //66
  indentsToTry[9] = false; //74
  indentsToTry[10] = false; //83
  indentsToTry[11] = false; //91

  //Calculate how many indents we need to attempt on discC
  maxCAttempts = 0;
  for (byte x = 0 ; x < 12 ; x++)
    if (indentsToTry[x] == true) maxCAttempts++;

  //At startup discB may be negative. Fix it.
  if(discB < 0) discB += 100;

  clearDisplay();
}

void loop()
{
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
  Serial.println(F("3) Measure indents"));
  Serial.println(F("4) Calibrate handle servo"));
  Serial.println(F("5) Set starting combos"));
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
  else if (incoming == '4')
  {
    testServo(); //Infinite loop to test servo
  }
  else if (incoming == '5')
  {
    //Get starting values from user
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
  else if (incoming == 's')
  {
    startTime = millis();

    resetDiscsWithCurrentCombo(false); //Set the discs to the current combinations (user can set if needed from menu)

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
