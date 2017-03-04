//Given a step value, go to that step
//Assume user has set direction prior to calling
//Returns the delta from what you asked and where it ended up
//Adding a full rotation will add a 360 degree full rotation
int gotoStep(int stepGoal, boolean addAFullRotation)
{
  //Serial.print("Going to: ");
  //Serial.println(stepGoal);

  int coarseWindow = 1000;
  int fineWindow = 32;

  int diff = stepsRequired(steps, stepGoal);
  Serial.print("steps: ");
  Serial.println(steps);
  Serial.print("stepgoal: ");
  Serial.println(stepGoal);
  Serial.print("1diff: ");
  Serial.println(diff);
  messagePause("");

  setMotorSpeed(150); //Go!
  //  while (stepsRequired(steps, stepGoal) > coarseWindow) delayMicroseconds(10); //Spin until coarse window is closed
  while (1)
  {
    int diff = stepsRequired(steps, stepGoal);
    Serial.print("diff: ");
    Serial.println(diff);
    if (diff < coarseWindow) break;
    delayMicroseconds(10); //Spin until coarse window is closed
  }

  //After we have gotten close to the first coarse window, proceed past the goal, then proceed to the goal
  if (addAFullRotation == true)
  {
    delay(500); //This should spin us past the goal
    //Now proceed to the goal
    while (stepsRequired(steps, stepGoal) > coarseWindow) delayMicroseconds(10); //Spin until coarse window is closed
  }

  setMotorSpeed(50); //Slowly approach

  //while (stepsRequired(steps, stepGoal) > fineWindow) delayMicroseconds(10); //Spin until fine window is closed
  while (1)
  {
    int diff = stepsRequired(steps, stepGoal);
    Serial.print("fine diff: ");
    Serial.println(diff);
    if (diff < fineWindow) break;
    delayMicroseconds(10); //Spin until coarse window is closed
  }

  setMotorSpeed(0); //Stop

  delay(500); //Wait for motor to stop

  int delta = steps - stepGoal;

  Serial.print("stepGoal: ");
  Serial.print(stepGoal);
  Serial.print(" / Final steps: ");
  Serial.print(steps);
  Serial.print(" / delta: ");
  Serial.println(delta);

  //Serial.print("Arrived: ");
  //Serial.println(steps);

  return (delta);
}

//Calculate number of steps to get to our goal
//Based on current position, goal, and direction
//Corrects for 8400 roll over
int stepsRequired(int currentSteps, int goal)
{
  /*Serial.print("currentSteps: ");
    Serial.print(currentSteps);
    Serial.print(" / goal: ");
    Serial.println(goal);*/

  if (direction == CW)
  {
    if (currentSteps >= goal) return (currentSteps - goal);
    else if (currentSteps < goal) return (8400 - goal + currentSteps);
  }
  else if (direction == CCW)
  {
    if (currentSteps >= goal) return (8400 - currentSteps + goal);
    else if (currentSteps < goal) return (goal - currentSteps);
  }

  Serial.println("stepRequired failed"); //We shouldn't get this far
  Serial.print("Goal: ");
  Serial.println(goal);
  Serial.print("currentSteps: ");
  Serial.print(currentSteps);

  return (0);
}

//Given a dial number, goto that value
//Assume user has set the direction before calling
//Returns the dial value we actually ended on
int setDial(int dialValue, boolean extraSpin)
{
  Serial.print("Want dialValue: ");
  Serial.println(dialValue);

  int encoderValue = convertDialToEncoder(dialValue); //Get encoder value
  Serial.print("Want encoderValue: ");
  Serial.println(encoderValue);

  gotoStep(encoderValue, extraSpin); //Goto that encoder value
  Serial.print("After movement, steps: ");
  Serial.println(steps);

  int actualDialValue = convertEncoderToDial(steps); //Convert back to dial values
  Serial.print("After movement, dialvalue: ");
  Serial.println(actualDialValue);

  return (actualDialValue);
}

//Spin until we detect both sides of reed switch
//Split the difference, then rotate back to that new center
void goHome()
{
  const byte searchSpeed = 50; //50 to 255 is allowed

  turnCW();

  //Begin spinning
  setMotorSpeed(searchSpeed);

  //Sometimes when we start we're already near the magnet, spin until we are out
  if (magnetDetected() == true)
  {
    Serial.println("We're too close to the magnet");
    int currentDial = convertEncoderToDial(steps);
    currentDial += 50;
    if (currentDial > 100) currentDial -= 100;
    setDial(currentDial, false); //Advance to 50 dial ticks away from here

    setMotorSpeed(searchSpeed);
  }

  while (magnetDetected() == false) delayMicroseconds(1);

  setMotorSpeed(0);
  delay(500); //Wait for motor to stop

  //Adjust steps with the real-world offset
  steps = (84 * 0); //84 * the number the dial sits on when 'home'
}

//Given a dial value, covert to an encoder value (0 to 8400)
//If there are 100 numbers on the dial, each number is 84 ticks wide
int convertDialToEncoder(int dialValue)
{
  int encoderValue = dialValue * 84;

  if (encoderValue > 8400)
  {
    Serial.print("Whoa! Your trying to go to a dial value that is illegal. encoderValue: ");
    Serial.println(encoderValue);
    while (1);
  }

  return (84 * dialValue);
}

//Given an encoder value, tell me where on the dial that equates
//Returns 0 to 99
//If there are 100 numbers on the dial, each number is 84 ticks wide
int convertEncoderToDial(int encoderValue)
{
  int dialValue = encoderValue / 84; //2388/84 = 28.43
  int partial = encoderValue % 84; //2388%84 = 36

  if (partial >= (84 / 2)) dialValue++; //36 < 42, so 28 is our final answer

  if (dialValue > 99) dialValue = 0;

  return (dialValue);
}

//Reset the dial
void resetDial()
{
  //Turn CW, past zero, then return to zero
  turnCW();
  int dialValue = setDial(0, false); //Turn to zero with an extra spin

  Serial.print("resetDial ended at: ");
  Serial.println(dialValue);
  Serial.print("steps: ");
  Serial.println(steps);
}

//Tells the servo to pull down on the handle
//After a certain amount of time we test to see the actual
//position of the servo. If it's properly moved all the way down
//then the handle has moved and door is open!
//If position is not more than a determined amount, then return failure
boolean tryHandle()
{
  //Attempt to pull down on handle
  handleServo.write(servoTryPosition);

  //TODO change as needed
  delay(300); //Wait for servo to move, but don't let it stall for too long and burn out

  //Check if we're there
  int handlePosition = analogRead(servoPosition);

  if (handlePosition > 500) //TODO change as needed
  {
    //Holy smokes we're there!
    handleServo.detach(); //Turn off servo
    return (true);
  }

  //Ok, we failed
  //Return to resting position
  handleServo.write(servoRestingPosition);

  return (false);
}

//Sets the motor speed
void setMotorSpeed(int speedValue)
{
  analogWrite(motorPWM, speedValue);
}

//Gives the current being used by the motor in milliamps
int readMotorCurrent()
{
  const int VOLTAGE_REF = 5;  //This board runs at 5V
  const float RS = 0.1;          //Shunt resistor value (in ohms)

  float sensorValue = averageAnalogRead(currentSense);

  // Remap the ADC value into a voltage number (5V reference)
  sensorValue = (sensorValue * VOLTAGE_REF) / 1023;

  // Follow the equation given by the INA169 datasheet to
  // determine the current flowing through RS. Assume RL = 10k
  // Is = (Vout x 1k) / (RS x RL)
  float current = sensorValue / (10 * RS);

  return (current * 1000);
}

//Tell motor to turn dial clock wise
void turnCW()
{
  direction = CW;
  digitalWrite(motorDIR, HIGH);
}

//Tell motor to turn dial counter-clock wise
void turnCCW()
{
  direction = CCW;
  digitalWrite(motorDIR, LOW);
}

//Turn on the motor controller
void enableMotor()
{
  digitalWrite(motorReset, HIGH);
}

void disableMotor()
{
  digitalWrite(motorReset, LOW);
}

//Interrupt routine when encoderA pin changes
void countA()
{
  if (direction == CW) steps--;
  else steps++;
  if (steps < 0) steps = 8399; //Limit variable to zero
  if (steps > 8399) steps = 0; //Limit variable to 8399
}

//Interrupt routine when encoderB pin changes
void countB()
{
  if (direction == CW) steps--;
  else steps++;
  if (steps < 0) steps = 8399; //Limit variable to zero
  if (steps > 8399) steps = 0; //Limit variable to 8399
}

//Checks to see if we detect the magnet
boolean magnetDetected()
{
  if (digitalRead(photo) == LOW) return (true);
  return (false);


  //Take a few readings to really verify
  byte checks = 0;
  for (byte x = 0 ; x < 10 ; x++) //There are a lot of false checks. Wait for 100 of the same.
  {
    if (digitalRead(photo) == LOW) checks++; //Magnet detected!
  }

  //Serial.print("magnet detected: ");
  //Serial.println(checks);

  if (checks > 7) return (true); //Mostly detected

  return (false); //No magnet
}

//Play a music tone to indicate door is open
void announceSuccess()
{
  //Beep! Piano keys to frequencies: http://www.sengpielaudio.com/KeyboardAndFrequencies.gif
  tone(buzzer, 2093, 150); //C
  delay(150);
  tone(buzzer, 2349, 150); //D
  delay(150);
  tone(buzzer, 2637, 150); //E
  delay(150);
}

//Disc A is the safety disc that prevents you from feeling the edges of the wheels
//It has 12 upper and 12 low indents which means 100/24 = 4.16 per lower indent
//So it moves a bit across the wheel. We could do floats, instead we'll do a lookup
//Values were found by hand: What number is in the middle of the indent?
int lookupAValues(int indentNumber)
{
  switch (indentNumber)
  {
    case 0: return (0); //98 to 1 on the wheel
    case 1: return (8); //6-9
    case 2: return (16); //14-17
    case 3: return (24); //23-26
    case 4: return (32); //31-34
    case 5: return (41); //39-42
    case 6: return (49); //48-51
    case 7: return (58); //56-59
    case 8: return (66); //64-67
    case 9: return (74); //73-76
    case 10: return (83); //81-84
    case 11: return (91); //90-93
    case 12: return (100); //End
  }
}

//Print a message and wait for user to press a button
//Good for stepping through actions
void messagePause(char* message)
{
  Serial.println(message);
  while (!Serial.available()); //Wait for user input
  Serial.read(); //Throw away character
}

//Takes an average of readings on a given pin
//Returns the average
int averageAnalogRead(byte pinToRead)
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return (runningValue);
}
