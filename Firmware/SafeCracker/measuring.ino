/*
  These functions are used to measure the internal indents on disc C
*/


//Locate and measure the 12 indents on disc C
//We believe the width of the solution slot on disc C is smaller than the other 11
//The depth of the indents is interesting but I worry the indent depth will vary
//based on too many factors
void measureDiscC(int numberOfTests)
{
  //Clear arrays
  for (int x = 0 ; x < 12 ; x++)
  {
    indentLocations[x] = 0;
    indentWidths[x] = 0;
    indentDepths[x] = 0;
  }

  for (int testNumber = 0 ; testNumber < numberOfTests ; testNumber++)
  {
    for (int indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
    {
      checkForUserPause(); //Pause if user presses a button

      findFlag(); //Recal every loop. Measuring causes havoc with the encoder

      //Look up where we should start looking
      int dialValue;
      if (indentNumber == 0) dialValue = 0;
      else dialValue = convertEncoderToDial(indentLocations[indentNumber - 1]) + 10; //Move to the middle of the high part following the last indent
      if (dialValue >= 100) dialValue -= 100;

      setDial(dialValue, false); //Go to the middle of this indent's location

      measureIndent(indentLocations[indentNumber], indentWidths[indentNumber], indentDepths[indentNumber]);

      Serial.print(F("Test "));
      Serial.print(testNumber + 1);
      Serial.print(F(" of "));
      Serial.print(numberOfTests);
      Serial.print(F(". IndentNum["));
      Serial.print(indentNumber);
      Serial.print(F("] Encoder["));
      Serial.print(indentLocations[indentNumber]);
      Serial.print(F("] / Dial["));
      Serial.print(convertEncoderToDial(indentLocations[indentNumber]));
      Serial.print(F("] / Width["));
      Serial.print(indentWidths[indentNumber]);
      Serial.print(F("] / Depth["));
      Serial.print(indentDepths[indentNumber]);
      Serial.print("]");
      Serial.println();
    }
  }

  //Find the largest indent
  int biggestWidth = 0;
  int biggestIndent = 0;
  for (int x = 0 ; x < 12 ; x++)
  {
    if (indentWidths[x] > biggestWidth)
    {
      biggestWidth = indentWidths[x];
      biggestIndent = x;
    }
  }
  Serial.print(F("Largest indent number: "));
  Serial.println(biggestIndent);

  //Turn off all indents
  for (int x = 0 ; x < 12 ; x++)
  {
    indentsToTry[x] = false; //Clear local array
    EEPROM.put(LOCATION_TEST_INDENT_0 + x, false); //Record settings to EEPROM
  }

  //Turn on the one largest indent
  indentsToTry[biggestIndent] = true;
  EEPROM.put(LOCATION_TEST_INDENT_0 + biggestIndent, true); //Record settings to EEPROM
  Serial.println(F("Largest indent is now set to test"));

  //Record these indent values to EEPROM
  for (byte x = 0 ; x < 12 ; x++)
    EEPROM.put(LOCATION_INDENT_DIAL_0 + (x * 2), indentLocations[x]); //adr, data
  
  Serial.println(F("Indent locations recorded to EEPROM"));

  messagePause("Press key to continue");
}

//From our current position, begin looking for and take internal measurement of the indents
//on the blocking disc C
//Caller has set the dial to wherever they want to start measuring from
//Caller passes in two addresses where to record the data
void measureIndent(int &indentLocation, int &indentWidth, int &indentDepth)
{
  byte searchSlow = 50; //We don't want 255 fast, just a jaunt speed

  int edgeFar = 0;
  int edgeNear = 0;

  //Apply pressure to handle
  handleServo.write(servoHighPressurePosition);
  delay(timeServoApply); //Wait for servo to move

  //Spin until we hit the edge of the indent
  turnCW();
  setMotorSpeed(searchSlow); //Begin turning dial

  long timeSinceLastMovement = millis();
  int lastStep = steps;
  while (1)
  {
    if (lastStep != steps)
    {
      lastStep = steps;
      timeSinceLastMovement = millis();
    }
    if (millis() - timeSinceLastMovement > 10) break;
  }
  setMotorSpeed(0); //Stop!

  delay(timeMotorStop); //Allow motor to stop spinning

  //Add offset because we're switching directions and we need to take up
  //slack in the encoder
  steps += switchDirectionAdjustment;
  if (steps > 8400) steps -= 8400;

  edgeFar = steps; //Take measurement


  //Ok, we've made it to the far edge of an indent. Now move backwards towards the other edge.
  turnCCW();

  //Spin until we hit the opposite edge of the indent
  setMotorSpeed(searchSlow); //Begin turning dial

  timeSinceLastMovement = millis();
  lastStep = steps;
  while (1)
  {
    if (lastStep != steps)
    {
      lastStep = steps;
      timeSinceLastMovement = millis();
    }
    if (millis() - timeSinceLastMovement > 10) break;
  }
  setMotorSpeed(0); //Stop!

  delay(timeMotorStop); //Alow motor to stop spinning

  //Take a reading on how deep the handle got
  //handlePosition = averageAnalogRead(servoPosition);

  steps -= switchDirectionAdjustment;
  if (steps < 0) steps += 8400;

  edgeNear = steps; //Take measurement


  //Ok, we've made it to the near edge of an indent. Now move towards the other edge for a 2nd reading
  turnCW();
  previousDirection = CW; //Last adjustment to dial was in CW direction

  //Now go back to original edge
  setMotorSpeed(searchSlow); //Begin turning dial

  timeSinceLastMovement = millis();
  lastStep = steps;
  while (1)
  {
    if (lastStep != steps)
    {
      lastStep = steps;
      timeSinceLastMovement = millis();
    }
    if (millis() - timeSinceLastMovement > 10) break;
  }
  setMotorSpeed(0); //Stop!

  delay(timeMotorStop); //Alow motor to stop spinning

  //Take a reading on how deep the handle got
  //handlePosition = averageAnalogRead(servoPosition);

  steps -= switchDirectionAdjustment;
  if (steps < 0) steps += 8400;

  int edgeFar2 = steps; //Take measurement



  //Measurement complete
  int sizeOfIndent;
  //if (edgeFar > edgeNear) sizeOfIndent = 8400 - edgeFar + edgeNear;
  //else sizeOfIndent = edgeNear - edgeFar;

  if (edgeFar2 > edgeNear) sizeOfIndent = 8400 - edgeFar2 + edgeNear;
  else sizeOfIndent = edgeNear - edgeFar2;

  //indentDepth += handlePosition; //Record handle depth
  indentWidth += sizeOfIndent; //Record this value to what the user passed us

  indentLocation = edgeFar2 + (sizeOfIndent / 2); //Find center of indent
  if (indentLocation >= 8400) indentLocation -= 8400;


  //Move away from edge of indent so we don't pinch the plunger
  turnCCW();
  previousDirection = CCW; //Last adjustment to dial was in CW direction

  gotoStep(indentLocation, false); //Move to middle of this indent

  handleServo.write(servoRestingPosition); //Release servo
  delay(timeServoRelease * 2); //Give servo extra time to release because we are using extra pressure

  /*
       Serial.print("edgeNear: ");
       Serial.print(edgeNear);
       Serial.print(" / ");
       Serial.println(convertEncoderToDial(edgeNear));
       Serial.print("edgeFar2: ");
       Serial.print(edgeFar2);
       Serial.print(" / ");
       Serial.println(convertEncoderToDial(edgeFar2));

         //Display where the center of this indent is on the dial
         int centerOfIndent = sizeOfIndent / 2 + edgeFar;
         Serial.print("centerOfIndent: ");
         Serial.print(centerOfIndent);
         Serial.print(" / ");
         Serial.println(convertEncoderToDial(centerOfIndent));
  */
}

