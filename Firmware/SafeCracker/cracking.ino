/*
  These are the actual high level cracking functions

  nextCombination() - Every time this is called, we attempt the current combo and then advance to next

*/
int combinationsAttempted = 0;

//Given the current state of discs, advance to the next combo
void nextCombination()
{
  combinationsAttempted++; //Increase the overall count

  discCAttempts++; //There are as many as 12 indents to try.

  if (discCAttempts >= maxCAttempts) //Idents are exhausted, time to adjust discB
  {
    discCAttempts = 0; //Reset count

    //The interference width of B to A is 11, meaning, if discB is at 40 and discA 
    //is at 30, if you move discB to 37 it will cause discA to move by 4 (to 26).
    boolean crossesA = checkCrossing(discB, -11, discA); //Check to see if the next B will cross A
    if (crossesA == true) //disc B is exhausted, time to adjust discA
    {
      discA += 3; //Disc A changes by 3
      if (discA > 99) //This is the end case
      {
        Serial.println("Damn. We exhausted the combination domain. Try changing the center values.");
        return;
      }
      else //Adjust discA, discB, discC
      {
        discB = discA - 3; //Reset discB
        if (discB < 0) discB += 100;

        discC = getNextIndent(discB); //Get the first indent after B

        Serial.println("Resetting dial...");

        findFlag(); //Re-home the dial between large finds

        //With this new A value, reset all discs
        //resetDiscsWithCurrentCombo(true); //Do pauses to verify positions
        resetDiscsWithCurrentCombo(false);
        discCAttempts = 0; //Reset count

      }
    }
    else //Adjust discB and discC
    {
      //Serial.println("B: Adjust");

      //Adjust discB to this new value
      turnCW();

      discB -= 3; //Disc B changes by 3
      if (discB < 0) discB += 100;

      int discBIsAt = setDial(discB, false);
      //Serial.print("DiscB is at: ");
      //Serial.println(discBIsAt);
      //messagePause("Check dial position");

      //You can't have a combo that is X-45-46: too close.
      //There is a cross over point that comes when discB combo crosses
      //Disc C. When DiscB is within 3 of discC, moving disc C will disrupt
      //Disc B. Therefore, when you're this close skip the next B
      boolean crossesB = checkCrossing(discC, 3, discB); //Check to see if the next B will cross A
      if (crossesB == true) //We need to skip this test
      {
        //Do nothing
        //messagePause("skipping this discC");
      }
      else
      {
        discC = getNextIndent(discB); //Get the first indent after B

        turnCCW();
        int discCIsAt = setDial(discC, false);
        //Serial.print("DiscC is at: ");
        //Serial.println(discCIsAt);
        //messagePause("Check dial position");
      }



    }

  }
  else //Adjust discC
  {
    //Serial.println("C: Adjust");

    turnCCW();

    discC = getNextIndent(discC); //Get next discC position

    //Adjust discC to this new value
    int discCIsAt = setDial(discC, false);
    //Serial.print("DiscC is at: ");
    //Serial.println(discCIsAt);
  }

  showCombination(discA, discB, discC); //Update display

  Serial.print("Time, ");
  Serial.print(millis()); //Show timestamp

  Serial.print(", Combination, ");
  Serial.print(discA);
  Serial.print("/");
  Serial.print(discB);
  Serial.print("/");
  Serial.print(discC);

  //Try the handle
  if (tryHandle() == true)
  {
    Serial.print(", Handle position, ");
    Serial.print(handlePosition);

    Serial.println();
    Serial.println("Door is open!!!");
    announceSuccess();
    disableMotor(); //Power down motor

    Serial.println(F("Pausing. Press key to release handle."));
    while (!Serial.available());
    Serial.read();

    //Return to resting position
    handleServo.write(servoRestingPosition);
    delay(timeServoRelease); //Allow servo to release. 200 was too short on new safe

    while (1); //Freeze!
  }

  Serial.print(", Handle position, ");
  Serial.print(handlePosition);

  Serial.print(", attempted, ");
  Serial.print(combinationsAttempted);

  float secondsPerTest = (float)(millis() - startTime) / 1000.0 / combinationsAttempted;
  Serial.print(", seconds per attempt, ");
  Serial.print(secondsPerTest);

  Serial.println();
}

//Given a disc position, and a change amount (negative is allowed)
//Do we cross over the check spot?
//The logic as it sits will have checkCrossing(10, -3, 7) return false, meaning
//the cracker will try a combination with two same numbers (ie: 7/7/0)
boolean checkCrossing(int currentSpot, int changeAmount, int checkSpot)
{
  //Look at each step as we make a theoretical move from current to the check location
  for (int x = 0 ; x < abs(changeAmount) ; x++)
  {
    if (currentSpot == checkSpot) return (true); //If we make this move it will disrupt the disc with checkSpot

    if (changeAmount < 0) currentSpot--;
    else currentSpot++;

    if (currentSpot > 99) currentSpot = 0;
    if (currentSpot < 0) currentSpot = 99;
  }

  return (false);

}

