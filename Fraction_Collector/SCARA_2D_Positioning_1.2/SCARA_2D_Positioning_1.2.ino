/*Script that drives a 2D SCARA arm to a defined (X,Y) position. 
 * Version 1.2 - fixed bugs and added button input support
 * UPDATE:
 * Can now use MoveUsingSerial(); to move the arm to different X and Y positions using the serial monitor. Can also use MoveUsingButtons(); to
 * drive the arm using 4 directional buttons. This setup uses the Gtronics protoyping board, so all buttons are wired to one analog input pin via
 * different resistors. The sketch decides based on the input voltage which button was pressed.
 * 
 * For using this sketch in other applications:
 * 1) Save the current position using SaveArmPosition()
 * 2) Set the target (x,y) point for the hand using SetTarget(X,Y)
 * 3) Calculate the position of the elbow joint using CalculateElbowPosition()
 * 4) Move the arm using MoveArm()
 * 
 * Any deviation from this sequence will cause errors and unpredictable behaviour (work on this code is still in progress)
 * Also, the SCARA arm operates only in the upper two quadrants meaning the limits for x are: -total arm length <= x <= total arm length
 * The limits for y are 0 <= y <= total arm length. 
 * 
 * Exceeding these limits should give an error
 * 
 * For now, calibration is not included in the script so the user has to manually move the SCARA to the middle position (perpendicular to the base). This point is (0,arm length)
 * 
 * Currently, this sketch is for running a SCARA arm in 2D using 2 NEMA17 motors, 12V, 350 mA, 1.8˚/step and DRV8825 driver boards. The gearing on our arm results in a 90/20 conversion.
 * To adjust this sketch for your needs, you can adjust the SCARA dimensions (length), the degrees/step (degree of joint rotation/step of the motor).
 * Of course pin wiring can also be adjusted. Additionally, speed of the arm can be edited using the stepInterval variables.
 * To increase precision, microstepping can be activated using these driver boards, to reduce the step size. See datasheet for details: https://reprapworld.com/datasheets/datasheet%20drv8825.pdf
 * Our steppers are set such that they both use the same angle/step. If you want to adjust this, you can add another variable or let us know and we can adjust the script to support 
 * this.
 * 
 *  For circuit diagrams see the GitHub repository.
 */

#include <math.h>

//Constants
  const float pi = M_PI;

//Pin for button input
  int analogPinA0 = A0;
  
//Arm constants
  const float lowerArmLength = 125;
  const float upperArmLength = 125;

//Motor constants
  const int stepPinMot1 = 3;
  const int dirPinMot1 = 6;
  const int stepIntervalMot1 =  10;

  const int stepPinMot2 = 5;
  const int dirPinMot2 = 10;
  const int stepIntervalMot2 =  10;
  
  const float degPerStep = 1.8/(90/20);

//Arm positioning variables
  float previousElbowX;
  float previousElbowY;
  float elbowX;
  float elbowY;
  float elbowXmin = -upperArmLength;
  float elbowXmax = upperArmLength;
  float elbowYmin = 0;
  float elbowYmax = upperArmLength;
  
  float previousHandX;
  float previousHandY;
  float handX;
  float handY;
  float handXmin = -(upperArmLength+lowerArmLength);
  float handXmax = upperArmLength+lowerArmLength;
  float handYmin = 0;
  float handYmax = upperArmLength+lowerArmLength;
  
  float shoulderAngle;
  float elbowAngle;

  float targetX;
  float targetY;

  bool positionPossible = true;
  char directionButton;

void setup() {

  //Begin serial communication
  Serial.begin(9600);
  Serial.println("Initialize...");
  delay(3000);

  //Set up Arduino pins
  pinMode(stepPinMot1, OUTPUT);
  pinMode(dirPinMot1,OUTPUT);
  pinMode(stepPinMot2, OUTPUT);
  pinMode(dirPinMot2, OUTPUT);

  pinMode(analogPinA0,INPUT);

  //Calibrate the starting arm position and set HandX and HandY variables as well as ElbowX and ElbowY, also determines limits
  //For now these are hardcoded. Work on a calibration sketch is in progress.
  elbowX = 0;
  elbowY = upperArmLength;
  handX = 0;
  handY = upperArmLength+lowerArmLength; 

  targetX = handX;
  targetY = handY;
}

void loop() 
{
  //Moves the arm using input from the serial monitor
  MoveUsingSerial();  
}

  //Saves the current position to the previous position variables
  void SaveArmPosition()
  {
    previousElbowX = elbowX;
    previousElbowY = elbowY;
    previousHandX = handX;
    previousHandY = handY;
  }

  //Sets the target variables, also checks if the target point is inside the defined range of the arm
  void SetTarget(float setX, float setY)
  {
    //Sanity check, can the arm reach, if not, stay in position 
    if(setY >= handYmin && setY <= handYmax && setX >= handXmin && setX <= handXmax)
    {
      targetX = setX;
      targetY = setY;
    }
    else
    {
      Serial.println("Target out of range");
      targetX = handX;
      targetY = handY;
    }
  }

//Calculates the elbow position of the arm for a given target point
void CalculateElbowPosition()
{
  float distance;
  float a;
  float b;
  float h;
  float iX;
  float iY;

  float elbowX1;
  float elbowX2;
  float elbowY1;
  float elbowY2;

  //calculates the distance between the circle midpoints
  distance = sqrt(sq(targetX)+sq(targetY));

  //Error catching
  if(distance > lowerArmLength + upperArmLength)
  {
    Serial.println("Error, position impossible");
    positionPossible = false;
  }
  else if(distance < abs(lowerArmLength - upperArmLength))
  {
    Serial.println("Error, position impossible");
    positionPossible = false;
  }
  else if (distance == 0)
  {
    Serial.println("Error, position impossible");
    positionPossible = false;
  }
  else
  {
    //can move here
    positionPossible = true;
    
    //calculate a and b (distances from the midpoints to the line that goes through both intercept points
    a = (sq(lowerArmLength) - sq(upperArmLength) + sq(distance))/(2*distance);
    b = (sq(upperArmLength) - sq(lowerArmLength) + sq(distance))/(2*distance);

    //calculate h (perpendicular distance from the line between the two midpoints to the intercept points)
    h = sqrt(sq(upperArmLength) - sq(a));

    //calculate the position of point I (point where the line through both intercepts intercepts line through both midpoints
    iY = (a*targetY)/distance;
    iX = (a*targetX)/distance;

    //calculate the position of the elbow
    elbowX1 = iX + ((targetY * h)/distance);
    elbowY1 = iY - ((targetX * h)/distance);
    elbowX2 = iX - ((targetY * h)/distance);
    elbowY2 = iY + ((targetX * h)/distance);

    //Calculate which elbow point causes the least movement of the lower arm within the max range and set the target elbow point accordingly
    if((sqrt(sq(abs(previousElbowX - elbowX1))+sq(abs(previousElbowY - elbowY1))) <= sqrt(sq(abs(previousElbowX - elbowX2))+sq(abs(previousElbowY - elbowY2)))) && 
    elbowXmin <= elbowX1 <= elbowXmax && elbowYmin <= elbowY1 <= elbowYmax)
    {
      elbowX = elbowX1;
      elbowY = elbowY1;
    }
    else
    {
      elbowX = elbowX2;
      elbowY = elbowY2;
    }
  }
}

//Calculates the angle to move the lower arm to, moves lower arm and calculates the new x,y position of the hand then moves upper arm
void MoveArm()
{
  //Calculate shoulder angle using the angle between two vectors, pointing to the previous elbow position and the current elbow position 
  shoulderAngle = atan2(elbowY,elbowX) - atan2(previousElbowY,previousElbowX);
  
  //Convert angle from radians to degrees
  shoulderAngle = shoulderAngle * (180/M_PI);

  //Calculate number of steps - if try to step behind the arm, reverse direction
  if(shoulderAngle >= 180)
  {
    shoulderAngle = shoulderAngle - 360;
  }
  else if (shoulderAngle <= -180)
  {
    shoulderAngle = shoulderAngle + 360;
  }
  float steps = abs(shoulderAngle/degPerStep);

  //If angle >= 0 move counterclockwise (true) else move clockwise
  if(shoulderAngle >= 0)
  {
    TurnStepper(2,true,steps, stepIntervalMot1);
  }
  else if(shoulderAngle < 0)
  {
    TurnStepper(2,false,steps, stepIntervalMot1);
  }
  
  //Calculate new X,Y position after movement of first motor
  handX = handX + elbowX - previousElbowX;
  handY = handY + elbowY - previousElbowY;

  //Move the elbow point to the origin to simplify the calculations
  float zeroedHandX = handX - elbowX;
  float zeroedHandY = handY - elbowY;
  float zeroedTargetY = targetY - elbowY;
  float zeroedTargetX = targetX - elbowX;

  //Calculate the angle the elbow needs to move by using the angle between the current hand position vector and the target vector
  elbowAngle = atan2(zeroedHandY,zeroedHandX) - atan2(zeroedTargetY,zeroedTargetX);

  //convert angle from radians to degrees
  elbowAngle = elbowAngle * (180/M_PI);

  //calculate number of steps
   steps = abs(elbowAngle/degPerStep);
     
  //If angle >= 0 move clockwise (false) else move counterclockwise
  if(elbowAngle >= 0)
  {
    //Calculate angle between upper arm vector and lower arm vector to avoid moving the lower arm through the upper arm
    float vectorAngle = atan2(zeroedHandY,zeroedHandX)-atan2(-elbowY,-elbowX);
    vectorAngle = vectorAngle * (180/M_PI);

    //Make sure measuring the vector angle in the direction the lower arm will be moving in (i.e. need a value between 0 and 2pi, but atan2 gives a range between -pi and pi)
    if(vectorAngle <0)
    {
      vectorAngle = vectorAngle + 360;
    }

    //Check that the angle the elbow will move by is smaller than the starting angle between the upper and lower arm, if not, turn the other way
    if(elbowAngle < vectorAngle)
    {
    TurnStepper(1,false,steps, stepIntervalMot1);
    }
    else if (elbowAngle >= vectorAngle)
    {
      elbowAngle = elbowAngle - 360;
      steps = abs(elbowAngle/degPerStep);
      TurnStepper(1,true,steps, stepIntervalMot1);
    }
  }
  else if(elbowAngle < 0)
  {
    //Angle between upper arm vector and lower arm vector
    float vectorAngle = atan2(zeroedHandY,zeroedHandX)-atan2(-elbowY,-elbowX);
    vectorAngle = vectorAngle * (180/M_PI);

    //Sanity checks, same as in the previous loop to avoid the lower arm turning into the upper arm
    if(vectorAngle > 0)
    {
      vectorAngle = vectorAngle - 360;
    }
    if (elbowAngle > vectorAngle)
    {
    TurnStepper(1,true,steps, stepIntervalMot1);
    }
    else if (elbowAngle <= vectorAngle)
    {
      elbowAngle = elbowAngle + 360;
      steps = abs(elbowAngle/degPerStep);
      TurnStepper(1,false,steps, stepIntervalMot1);
    }
  }

  //Set hand position to the target position
  handX = targetX;
  handY = targetY;  
}

//Function to turn stepper motor 1 or 2 (motorID) in a defined direction (motorDir) by steps. Interval between steps is interval.
void TurnStepper (int motorID, bool motorDir, int steps, int interval)
{

  int stepPin;
  int dirPin;
  
  if(motorID == 1)
  {
    stepPin = stepPinMot1;
    dirPin = dirPinMot1;
  }
  else if(motorID ==2)
  {
    stepPin = stepPinMot2;
    dirPin = dirPinMot2;
  }
  else
  {
    Serial.println("Motor not found");
  }

  if(motorDir)
  {
    digitalWrite(dirPin,HIGH);
  }
  else
  {
    digitalWrite(dirPin,LOW);
  }
  
  for (int i = 0; i < steps; i++)
  {
    digitalWrite(stepPin,HIGH);
    digitalWrite(stepPin,LOW);
    delay(interval);
  }
}

//Prints out the current arm positions - useful for troubleshooting
void PrintCurrentArmPosition()
{
  Serial.print("Hand X: ");
  Serial.print(handX);
  Serial.println();
  Serial.print("Hand Y: ");
  Serial.print(handY);
  Serial.println();
    Serial.print("Elbow X: ");
  Serial.print(elbowX);
  Serial.println();
  Serial.print("Elbow Y: ");
  Serial.print(elbowY);
  Serial.println();
}

//Function to receive a number input from the Serial port largely from https://startingelectronics.org/software/arduino/learn-to-program-course/19-serial-input/ - want to 
//function has been adjusted to handle floats instead of int and to also allow negative numbers. Need to add error handling for multiple - or multiple .
float ReceiveInputFromSerial()
{
char rx_byte = 0;
String rx_str = "";
bool not_number = false;
float result;
bool isNegative = false;
 
while(!not_number)
{
  if (Serial.available()) 
  {    
    // is a character available?
    rx_byte = Serial.read();       // get the character
    
    if ((rx_byte >= '0') && (rx_byte <= '9')) 
    {
      rx_str += rx_byte;
    }
    else if (rx_byte == '-' && rx_str.length() == 0)
    {
      isNegative = true;
    }
    else if(rx_byte == '.')
    {
      rx_str += rx_byte;
    }
    else if (rx_byte == '\n') 
    {
      // end of string
      if (not_number) 
      {
        Serial.println("Not a number");
      }
      else 
      {
        result = rx_str.toFloat();

        //Check if the number is negative
        if(isNegative)
        {
          result = -result;
          return result;
        }
        else
        {
          return result; 
        }
      }
      not_number = false;         // reset flag
      rx_str = "";                // clear the string for reuse
    }
    else 
    {
      // non-number character received
      not_number = true;    // flag a non-number
    }
  }
}

}

//Function that reads input from buttons in a Gtronics prototyping shield and returns the button pressed
char readShieldButtons()
{
  char buttonPressed;
  int buttonValue;

  buttonValue = analogRead(analogPinA0);

  if(buttonValue >= 1000)
  {
    buttonPressed = '-';
  }
  else if (buttonValue < 1000 && buttonValue >= 700)
  {
    buttonPressed = 'A';
  }
  else if (buttonValue < 700 && buttonValue >= 450)
  {
    buttonPressed = 'B';
  }
  else if (buttonValue < 450 && buttonValue >= 300)
  {
    buttonPressed = 'C';
  }
  else if (buttonValue < 300 && buttonValue >= 100)
  {
    buttonPressed = 'D';
  }
  else
  {
    buttonPressed = 'E';
  }
  
  return buttonPressed;
}

//Moves the arm using Serial coordinate input
void MoveUsingSerial()
{
    //Set the current elbow and hand positions equal to the previous positions to save the current position
  SaveArmPosition();

  //Enter coordinates for the arm to move to
  Serial.println("Enter an X coordinate");
  targetX = ReceiveInputFromSerial();
  Serial.println(targetX);
  Serial.println("Enter a Y coordinate");
  targetY = ReceiveInputFromSerial();
  Serial.println(targetY);

  //Set these as target coordinates - this step is important as this function performs the sanity check (will probably change this in the future)
  SetTarget(targetX,targetY);
  CalculateElbowPosition();
  
  if(positionPossible)
  {
    MoveArm();
  }
}

//Moves the arm using button input
void MoveUsingButtons()
{
    SaveArmPosition();
  directionButton = readShieldButtons();
  
    if(directionButton == 'D')
  {
    targetY += 1;
  }
  else if (directionButton == 'C')
  {
    targetY -= 1;
  }
  else if (directionButton == 'B')
  {
    targetX -= 1;
  }
  else if (directionButton == 'E')
  {
    targetX += 1;
  }
  SetTarget(targetX, targetY);
  CalculateElbowPosition();

  if(positionPossible)
  {
    MoveArm();
  }

  targetY = handY;
  targetX = handX;  
}

