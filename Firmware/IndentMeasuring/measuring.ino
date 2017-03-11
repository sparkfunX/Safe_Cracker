//Measure the 12 indents in disc A
void measureDiscA()
{
  int indentWidths[12];
  for(int x = 0 ; x < 12 ; x++) indentWidths[x] = 0; //Clear array

  for(int x = 0 ; x < 10 ; x++)
  {
    goHome(); //Detect magnet and center the dial
  
    for (int indentNumber = 0 ; indentNumber < 12 ; indentNumber++)
    {
      int dialValue = lookupIndentValues(indentNumber); //Get this indent's dial value
      setDial(dialValue, false); //Go to the middle of this indent's location
  
      indentWidths[indentNumber] += measureIndent();
  
      Serial.println("Widths: ");
      for (int x = 0 ; x < indentNumber + 1 ; x++)
      {
        Serial.print("[");
        Serial.print(x);
        Serial.print("] = ");
        Serial.print(indentWidths[x]);
        Serial.println();
      }
    }
  }
  
  messagePause("Done measuring");
}

//From our current position, begin looking for and take internal measurement of the indents
//on the blocking disc A
//Caller has set the dial to wherever they want to start measuring from
int measureIndent(void)
{
  int idleSpinCurrent = 400; //Found by running motor without servo applied

  byte searchSlow = 50; //We don't want 255 fast, just a jaunt speed

  int edgeFar = 0;
  int edgeNear = 0;

  //Apply pressure to handle
  handleServo.write(servoPressurePosition);
  delay(500); //Wait for servo to move

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
    if (millis() - timeSinceLastMovement > 25) break;

    delay(1);
  }
  setMotorSpeed(0); //Stop!

  delay(100); //Alow motor to stop spinning

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
    if (millis() - timeSinceLastMovement > 25) break;

    delay(1);
  }
  setMotorSpeed(0); //Stop!

  delay(100); //Alow motor to stop spinning

  edgeNear = steps; //Take measurement

  handleServo.write(servoRestingPosition); //Release servo
  delay(500); //Allow servo to release

  int sizeOfIndent;
  if (edgeFar > edgeNear) sizeOfIndent = 8400 - edgeFar + edgeNear;
  else sizeOfIndent = edgeNear - edgeFar;
  return (sizeOfIndent);
}

