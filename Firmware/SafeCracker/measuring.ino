/*
  These functions are used to measure the internal indents on disc C
*/


//Measure the 12 indents on disc C
//We believe the width of the solution slot on disc C is smaller than the other 11
//The depth of the indents is interesting but I worry the indent depth will vary
//based on too many factors
void measureDiscC(int numberOfTests)
{
  //Clear arrays
  for (int x = 0 ; x < 12 ; x++)
  {
    indentNumber[x] = x; //Assign indent number
    indentValues[x] = 0;
    indentWidths[x] = 0;
    indentDepths[x] = 0;
  }

  for (int testNumber = 0 ; testNumber < numberOfTests ; testNumber++)
  {
    for (int indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
    {
      checkForUserPause(); //Pause if user presses a button

      goHome(); //Recal every loop. Measuring causes havoc with the encoder

      int dialValue = lookupIndentValues(indentNumber); //Get this indent's dial value
      setDial(dialValue, false); //Go to the middle of this indent's location

      //int encoderValue = convertDialToEncoder(dialValue); //Get encoder value

      measureIndent(indentValues[indentNumber], indentWidths[indentNumber], indentDepths[indentNumber]);

      Serial.print(F("Test "));
      Serial.print(testNumber + 1);
      Serial.print(" of ");
      Serial.print(numberOfTests);
      Serial.println(F(". [Location]/[Width]/[Depth]"));

      for (int x = 0 ; x < 12 ; x++)
      {
        Serial.print(x);
        Serial.print(F(": ["));
        Serial.print(indentValues[x]);
        Serial.print(F("] / ["));
        Serial.print(indentWidths[x]);
        Serial.print(F("] / ["));
        Serial.print(indentDepths[x]);
        Serial.print("]");
        Serial.println();
      }
    }
  }

  //Sort array from smallest to largest
  sort(indentNumber, indentWidths, indentDepths, 12);

  Serial.println();
  Serial.println(F("Measuring complete"));
  Serial.println(F("Smallest to largest width [Width]/[Depth]"));

  for (int x = 0 ; x < 12 ; x++)
  {
    Serial.print(F("Indent "));
    Serial.print(indentNumber[x]);
    Serial.print(F(": ["));
    Serial.print(indentValues[x]);
    Serial.print(F("] / ["));
    Serial.print(indentWidths[x]);
    Serial.print(F("] / ["));
    Serial.print(indentDepths[x]);
    Serial.print("]");
    Serial.println();
  }

  messagePause("Press key to continue");
}

//From: http://www.hackshed.co.uk/arduino-sorting-array-integers-with-a-bubble-sort-algorithm/
//Sorts a given array from smallest to largest
//Modified to sort a second array as well
void sort(byte spotNumber[], int numbersToSort[], int secondValue[], int numberOfThingsToSort)
{
  for (int i = 0 ; i < (numberOfThingsToSort - 1) ; i++)
  {
    for (int p = 0 ; p < (numberOfThingsToSort - (i + 1)) ; p++)
    {
      if (numbersToSort[p] > numbersToSort[p + 1])
      {
        int tempSpot = spotNumber[p];
        int tempFirst = numbersToSort[p];
        int tempSecond = secondValue[p];

        spotNumber[p] = spotNumber[p + 1];
        numbersToSort[p] = numbersToSort[p + 1];
        secondValue[p] = secondValue[p + 1];

        spotNumber[p + 1] = tempSpot;
        numbersToSort[p + 1] = tempFirst;
        secondValue[p + 1] = tempSecond;
      }
    }
  }
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
  handleServo.write(servoPressurePosition);
  delay(300); //Wait for servo to move

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

  delay(100); //Allow motor to stop spinning

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

  delay(100); //Alow motor to stop spinning

  //Take a reading on how deep the handle got
  handlePosition = averageAnalogRead(servoPosition);

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

  delay(100); //Alow motor to stop spinning

  //Take a reading on how deep the handle got
  handlePosition = averageAnalogRead(servoPosition);

  steps -= switchDirectionAdjustment;
  if (steps < 0) steps += 8400;

  int edgeFar2 = steps; //Take measurement


  //Measurement complete
  handleServo.write(servoRestingPosition); //Release servo
  delay(200); //Allow servo to release

  int sizeOfIndent;
  //if (edgeFar > edgeNear) sizeOfIndent = 8400 - edgeFar + edgeNear;
  //else sizeOfIndent = edgeNear - edgeFar;

  if (edgeFar2 > edgeNear) sizeOfIndent = 8400 - edgeFar2 + edgeNear;
  else sizeOfIndent = edgeNear - edgeFar2;

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

  indentDepth += handlePosition; //Record handle depth
  indentWidth += sizeOfIndent; //Record this value to what the user passed us

  indentLocation = edgeFar2 + (sizeOfIndent / 2); //Find center of indent
  if (indentLocation >= 8400) indentLocation -= 8400;
}

//Go home, then look for 12 indents
//If any ident is overlapping, start over
void findIndents(void)
{
  //Clear arrays
  for (int x = 0 ; x < 12 ; x++)
  {
    indentNumber[x] = x; //Assign indent number
    indentValues[x] = 0;
    indentWidths[x] = 0;
    indentDepths[x] = 0;
  }

  Serial.println(F("[Location]/[DialValue]/[Width]/[Depth]"));

  for (byte indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
  {
    checkForUserPause(); //Pause if user presses a button

    goHome(); //Recal every loop. Measuring causes havoc with the encoder

    //Look up where we should start looking
    int dialValue;
    if (indentNumber == 0) dialValue = 0;
    else dialValue = convertEncoderToDial(indentValues[indentNumber - 1]) + 10; //Move to the middle of the high part following the last indent
    if (dialValue >= 100) dialValue -= 100;

    setDial(dialValue, false); //Go to the middle of this indent's location

    //messagePause("Next indent");

    measureIndent(indentValues[indentNumber], indentWidths[indentNumber], indentDepths[indentNumber]);

    Serial.print(indentNumber);
    Serial.print(F(": ["));
    Serial.print(indentValues[indentNumber]);
    Serial.print(F("] / ["));
    Serial.print(convertEncoderToDial(indentValues[indentNumber]));
    Serial.print(F("] / ["));
    Serial.print(indentWidths[indentNumber]);
    Serial.print(F("] / ["));
    Serial.print(indentDepths[indentNumber]);
    Serial.print("]");
    Serial.println();
  }

  //Record these values to EEPROM
  for (byte x = 0 ; x < 12 ; x++)
    EEPROM.put(LOCATION_INDENT_DIAL_0 + (x * 2), indentValues[x]); //adr, data

  messagePause("Press key to continue");
}

