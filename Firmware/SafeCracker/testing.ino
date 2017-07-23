/*
  These functions are used to calibrate the dialer and handle servo
*/

//Spins the motor while we tweak the servo up/down to detect binding force and servo position
//Press x to exit
//How to use: Attach cracker to safe. As motor spins increase handle pressure using a/z until the gear binds in an indent.
//Add 10 or 20 and this is servoPressurePosition global variable.
//Add 40 or 50 and this is servoTryPosition.
//*Remove* cracker from safe and adjust the servo up/down. Get the 3d printed handle to 45 degrees. The analog reading
//is the handleOpenPosition global variable. If the handle gets this far then the door is considered open.
//You want the hOP to be large enough to be different from just a try, but you want sTP to be not so large that
//we stress the servo on every handle try.
void testServo()
{
  handleServo.attach(servo);

  enableMotor(); //Turn on motor controller

  int servo = servoRestingPosition;
  handleServo.write(servo);

  long timeSinceLastMovement = millis();
  int lastStep = steps;

  turnCW();
  setMotorSpeed(50);

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

      if (incoming == 'a') servo += 5;
      if (incoming == 'z') servo -= 5;
      if (incoming == 'x') //Exit
      {
        setMotorSpeed(0); //Stop!
        handleServo.write(servoRestingPosition); //Goto the resting position (handle horizontal, door closed)
        delay(timeServoRelease); //Allow servo to move
        return;
      }

      if (servo < 0) servo = 0;
      if (servo > 255) servo = 255;

      handleServo.write(servo); //Goto the resting position (handle horizontal, door closed)
    }
    int handlePosition = averageAnalogRead(servoPosition);

    Serial.print(F("servo: "));
    Serial.print(servo);
    Serial.print(F(" / handlePosition: "));
    Serial.print(handlePosition);
    Serial.println();

    delay(100);
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

