/*
  These functions are used to calibrate the dialer and handle servo
*/

//Spins the motor while we tweak the servo up/down to detect binding force and servo position
//Press x to exit
//How to use: Attach cracker to safe.
//As motor spins increase handle pressure using a/z until the gear
//just begins to rub
//Move 10 away and then exit. This will be stored as servoRestPosition
//servoTryPosition is stored as servoRestPosition - 50
//servoPressurePosition is stored as servoRestPosition - 70
void testServo()
{
  Serial.println(F("x to exit"));
  Serial.println(F("a to adjust servo to gear"));
  Serial.println(F("z to adjust servo away from gear"));

  int servo = servoRestingPosition;
  handleServo.write(servo);

  long timeSinceLastMovement = millis();
  int lastStep = steps;

  turnCW();
  setMotorSpeed(50);

  enableMotor(); //Turn on motor controller

  while (1)
  {
    if (lastStep != steps)
    {
      lastStep = steps;
      timeSinceLastMovement = millis();
    }
    if (millis() - timeSinceLastMovement > 25)
    {
      setMotorSpeed(0); //Stop!
      Serial.println("Dial stuck");
      while (1);
    }

    if (Serial.available())
    {
      byte incoming = Serial.read();

      if (incoming == 'a') servo++;
      if (incoming == 'z') servo--;
      if (incoming == 'x') //Exit
      {
        setMotorSpeed(0); //Stop!
        handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)
        delay(timeServoRelease); //Allow servo to move
        break;
      }

      if (servo < 0) servo = 0;
      if (servo > 255) servo = 255;

      handleServo.write(servo); //Goto the resting position (handle horizontal, door closed)
    }
    //int handlePosition = averageAnalogRead(servoPosition); //Old way
    int handlePosition = digitalRead(servoPositionButton); //Look for button being pressed

    Serial.print(F("servo: "));
    Serial.print(servo);
    Serial.print(F(" / handleButton: "));
    Serial.print(servoPositionButton);
    Serial.println();

    delay(100);
  }

  //Record settings to EEPROM
  servoRestingPosition = servo + 10; //We want some clearance 
  servoTryPosition = servoRestingPosition - 50;
  servoHighPressurePosition = servoRestingPosition - 60;

  if (servoHighPressurePosition < 0 || servoTryPosition < 0)
  {
    Serial.println(F("servoPressurePosition or servoTryPosition is negative. Adjust servo higher."));
    Serial.println(F("Freezing"));
    while (1); //Freeze
  }

  EEPROM.put(LOCATION_SERVO_REST, servoRestingPosition);
  EEPROM.put(LOCATION_SERVO_TEST_PRESSURE, servoTryPosition);
  EEPROM.put(LOCATION_SERVO_HIGH_PRESSURE, servoHighPressurePosition);

  Serial.print(F("servo: resting["));
  Serial.print(servoRestingPosition);
  Serial.print(F("] try["));
  Serial.print(servoTryPosition);
  Serial.print(F("] Highpressure["));
  Serial.print(servoHighPressurePosition);
  Serial.print(F("]"));
  Serial.println();

  Serial.println(F("Servo positions stored."));

}

//Pulls down on handle using current settings
//Reports if button has been pressed
//To use: before running function identify the solution slot
//Dial will turn to slot
//Press any letter to cause servo to pull on handle
//If button is pressed, it will print Pressed!!
//Adjust button down just below level of being pressed when handle is normally pulled on
//Press x to exit
void testHandleButton(void)
{
  //Find the indent to test
  int solutionDiscC = 0;
  for (int x = 0 ; x < 12 ; x++)
  {
    if (indentsToTry[x] == true)
      solutionDiscC = convertEncoderToDial(indentLocations[x]);
  }
  setDial(solutionDiscC, false); //Goto this dial location

  Serial.println("x to exit");
  Serial.println("a to pull on handle");
  Serial.println("z to release handle");

  int pressedCounter = 0;
  while (1)
  {
    if (Serial.available())
    {
      byte incoming = Serial.read();
      if (incoming == 'x')
      {
        //Release handle
        handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)
        delay(timeServoRelease); //Allow servo to release

        return; //Exit
      }
      else if (incoming == 'a')
      {
        //Pull on handle
        handleServo.write(servoTryPosition);
        delay(timeServoApply); //Allow servo to move
      }
      else if (incoming == 'z')
      {
        //Release handle
        handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)
        delay(timeServoRelease); //Allow servo to release
      }
    }

    if (digitalRead(servoPositionButton) == LOW)
    {
      pressedCounter++;
      Serial.print("Pressed! ");
      Serial.println(pressedCounter); //To have something that changes
      delay(100);
    }

    //Hang out for 100ms but scan button during that time
    for (byte x = 0 ; x < 100 ; x++)
    {
      if (digitalRead(servoPositionButton) == LOW) break;
      delay(1);
    }
  }

}

//Test to see if we can repeatably go to a dial position
//Turns dial to random CW and CCW position and asks user to verify.
//How to use: Attach cracker to safe. Home the dial using the menu function. Then run this
//and verify the dial goes where it says it is going. If it's wrong, check homeOffset variable.
//If one direction is off, check switchDirectionAdjustment variable.
void positionTesting()
{
  int randomDial;

  for (int x = 0 ; x < 4 ; x++)
  {
    randomDial = random(0, 100);
    randomDial = 25;
    turnCCW();
    setDial(randomDial, false);

    Serial.print(F("Dial should be at: "));
    Serial.println(convertEncoderToDial(steps));
    messagePause("Verify then press key to continue");

    randomDial = random(0, 100);
    randomDial = 75;
    turnCW();
    setDial(randomDial, false);

    Serial.print(F("Dial should be at: "));
    Serial.println(convertEncoderToDial(steps));
    messagePause("Verify then press key to exit");
  }
}

