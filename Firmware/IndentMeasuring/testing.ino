//Spins the motor while we tweak the servo up/down to detect binding force
//and servo position
void testServo()
{
  handleServo.attach(servo);

  enableMotor(); //Turn on motor controller

  int servo = 5;
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

      if (servo < 0) servo = 0;
      if (servo > 255) servo = 255;

      handleServo.write(servo); //Goto the resting position (handle horizontal, door closed)
    }
    int handlePosition = averageAnalogRead(servoPosition);

    Serial.print("servo: ");
    Serial.print(servo);
    Serial.print(" / handlePosition: ");
    Serial.print(handlePosition);
    Serial.println();

    delay(100);

  }

}

//Test to see if we can repeatably go to a dial position
//Goes both CW and CCW
//Run this and verify the dial goes where it says it is going
void positionTesting()
{
  int randomDial;
  //messagePause("Time to find home");
  goHome(); //Detect the home flag and center the dial
  
  for (int x = 0 ; x < 4 ; x++)
  {
    Serial.print("Dial is at: ");
    Serial.println(convertEncoderToDial(steps));

    messagePause("Goto next spot");

    randomDial = random(0, 100);
    turnCCW();
    setDial(randomDial, false);

    Serial.print("Dial is at: ");
    Serial.println(convertEncoderToDial(steps));

    messagePause("Goto next spot");

    randomDial = random(0, 100);
    turnCW();
    setDial(randomDial, false);
  }

}

