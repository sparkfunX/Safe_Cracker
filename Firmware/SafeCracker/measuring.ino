/*
  These functions are used to measure the internal indents on disc C
*/

byte indentNumber[12];
int indentWidths[12];
int indentDepths[12];

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

      measureIndent(indentWidths[indentNumber], indentDepths[indentNumber]);

      Serial.print(F("Test "));
      Serial.print(testNumber + 1);
      Serial.print(" of ");
      Serial.print(numberOfTests);
      Serial.println(F(". [Width]/[Depth]"));

      for (int x = 0 ; x < 12 ; x++)
      {
        Serial.print(x);
        Serial.print(F(": ["));
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
    Serial.print(indentWidths[x]);
    Serial.print(F("] / ["));
    Serial.print(indentDepths[x]);
    Serial.print("]");
    Serial.println();
  }

  messagePause("");
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
void measureIndent(int &indentWidth, int &indentDepth)
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

  //if (steps > 84 * 40)
  //messagePause("Check dial and press a key");

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

  //if (steps > 84 * 40)
  //messagePause("Check dial and press a key");

  handleServo.write(servoRestingPosition); //Release servo
  delay(200); //Allow servo to release

  int sizeOfIndent;
  if (edgeFar > edgeNear) sizeOfIndent = 8400 - edgeFar + edgeNear;
  else sizeOfIndent = edgeNear - edgeFar;

  /*
     Serial.print("edgeNear: ");
    Serial.print(edgeNear);
    Serial.print(" / ");
    Serial.println(convertEncoderToDial(edgeNear));
    Serial.print("edgeFar: ");
    Serial.print(edgeFar);
    Serial.print(" / ");
    Serial.println(convertEncoderToDial(edgeFar));

    //Display where the center of this indent is on the dial
    int centerOfIndent = sizeOfIndent / 2 + edgeFar;
    Serial.print("centerOfIndent: ");
    Serial.print(centerOfIndent);
    Serial.print(" / ");
    Serial.println(convertEncoderToDial(centerOfIndent));
  */

  indentDepth += handlePosition; //Record handle depth
  indentWidth += sizeOfIndent; //Record this value to what the user passed us
}

