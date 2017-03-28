/*
  Functions to run the big seven segment display
*/

//Sends 6 ' ' characters and erases anything on the display
void clearDisplay()
{
  for (byte x = 0 ; x < 6 ; x++)
  {
    postNumber(' ', false, false);
  }

  //Latch the current segment data
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Takes a value and displays 6 numbers.
//Inverts every other number
void showNumber(long number)
{
  //Chop the value into the array
  byte values[6];
  for (byte x = 0 ; x < 6 ; x++)
  {
    values[5 - x] = number % 10;
    number /= 10;
  }

  //Push the values out to the display
  for (byte x = 0 ; x < 6 ; x++)
  {
    if (x % 2 == 0) //Rotate number
      postNumber(values[x], false, true); //Don't write a decimal, rotate this digit
    else
      postNumber(values[x], false, false); //Don't write a decimal, not rotated
  }

  //Latch the current segment data - this causes display to show what's been clocked in
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given hours, minutes, seconds, display time with colons
//Inverts every other number
void showTime(long milliseconds)
{
  long seconds = milliseconds / 1000;
  long minutes = seconds / 60;
  seconds %= 60;
  long hours = minutes / 60;
  minutes %= 60;

  //Chop the value into the array
  byte values[6];
  values[0] = hours / 10;
  values[1] = hours % 10;
  values[2] = minutes / 10;
  values[3] = minutes % 10;
  values[4] = seconds / 10;
  values[5] = seconds % 10;

  //Push the values out to the display
  for (byte x = 0 ; x < 6 ; x++)
  {
    if (x % 2 == 0) //Rotate number
    {
      //Turn on decimal when x is 1 to 4
      if (x > 0 && x < 5) postNumber(values[x], true, true); //Write a decimal, rotate this digit
      else postNumber(values[x], false, true); //Don't write a decimal, rotate this digit
    }
    else
    {
      //Turn on decimal when x is 1 to 4
      if (x > 0 && x < 5) postNumber(values[x], true, false); //Write a decimal, not rotated
      else postNumber(values[x], false, false); //Don't write a decimal, not rotated
    }
  }

  //Latch the current segment data - this causes display to show what's been clocked in
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given the disc values, display them with separating colons
//Inverts every other number
void showCombination(byte discA, byte discB, byte discC)
{
  //Chop the value into the array
  byte values[6];
  values[0] = discA / 10;
  values[1] = discA % 10;
  values[2] = discB / 10;
  values[3] = discB % 10;
  values[4] = discC / 10;
  values[5] = discC % 10;

  //Push the values out to the display
  for (byte x = 0 ; x < 6 ; x++)
  {
    if (x % 2 == 0) //Rotate number
    {
      //Turn on decimal on 1 and 3
      if (x == 1 || x == 3) postNumber(values[x], true, true); //Write a decimal, rotate this digit
      else postNumber(values[x], false, true); //Don't write a decimal, rotate this digit
    }
    else
    {
      //Turn on decimal on 1 and 3
      if (x == 1 || x == 3) postNumber(values[x], true, false); //Write a decimal, not rotated
      else postNumber(values[x], false, false); //Don't write a decimal, not rotated
    }
  }

  //Latch the current segment data - this causes display to show what's been clocked in
  digitalWrite(displayLatch, LOW);
  digitalWrite(displayLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given a number, or '-', or ' ', shifts it out to the display
//Inverting swaps all the segments around
void postNumber(byte number, boolean decimal, boolean inverted)
{
  //    -  A
  //   / / F/B
  //    -  G
  //   / / E/C
  //    -. D/DP

#define a  1<<0
#define b  1<<6
#define c  1<<5
#define d  1<<4
#define e  1<<3
#define f  1<<1
#define g  1<<2
#define dp 1<<7

  byte segments;

  if (inverted == false)
  {
    switch (number)
    {
      case 1: segments = b | c; break;
      case 2: segments = a | b | d | e | g; break;
      case 3: segments = a | b | c | d | g; break;
      case 4: segments = f | g | b | c; break;
      case 5: segments = a | f | g | c | d; break;
      case 6: segments = a | f | g | e | c | d; break;
      case 7: segments = a | b | c; break;
      case 8: segments = a | b | c | d | e | f | g; break;
      case 9: segments = a | b | c | d | f | g; break;
      case 0: segments = a | b | c | d | e | f; break;
      case ' ': segments = 0; break;
      case '-': segments = g; break;
    }
  }
  else
  {
    switch (number)
    {
      //   .-  DP/D
      //   / / C/E
      //    -  G
      //   / / B/F
      //    -  A

      case 1: segments = e | f; break;
      case 2: segments = d | b | g | e | a; break;
      case 3: segments = d | f | g | e | a; break;
      case 4: segments = c | e | f | g; break;
      case 5: segments = d | c | g | f | a; break;
      case 6: segments = d | c | g | b | f | a; break;
      case 7: segments = d | e | f; break;
      case 8: segments = a | b | c | d | e | f | g; break;
      case 9: segments = a | c | d | e | f | g; break;
      case 0: segments = a | b | c | d | e | f; break;
      case ' ': segments = 0; break;
      case '-': segments = g; break;
    }

  }

  if (decimal) segments |= dp;

  //Clock these bits out to the drivers
  for (byte x = 0 ; x < 8 ; x++)
  {
    digitalWrite(displayClock, LOW);
    digitalWrite(displayData, segments & 1 << (7 - x));
    digitalWrite(displayClock, HIGH); //Data transfers to the register on the rising edge of Clock
  }
}
