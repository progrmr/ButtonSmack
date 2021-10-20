// IMPORTANT!
// DOWNLOAD the MD_Parola library
// https://github.com/MajicDesigns/MD_Parola

#undef PAROLA

#ifdef PAROLA
#include <stdio.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include "Parola_Fonts_data.h"
#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW
#define MAX_DEVICES 8
#define CLK_PIN   13
#define DATA_PIN  11
#define CS_PIN    10
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
#endif

#define kInvalid (-1)
#define nLEDs (8)
#define TimeLimitMS (600000)      // 60,000 ms == 60 seconds

uint32_t gameStartMS = 0;
bool gameOver = false;

char debugMsg[120];
const int buttonPin = A0;
int ledPins[nLEDs] = {2,3,4,5,6,7,8,9};
uint32_t timeLitLED[nLEDs] = {0};
uint32_t buttonDownMS[nLEDs] = {0};
uint32_t maxTimeLitMS = 2000;           // start out at 2000 ms (2 sec) max time LED is lit

uint32_t totalResponseTimeMS = 0;
uint32_t totalResponseCount = 0;

uint32_t loopCount = 0;

long randNumber;
int button_values[nLEDs] = {920, 451, 295, 215, 165, 130, 99, 65};    // TODO: last 3 values TBD
int button_tolerance[nLEDs] = {0};    // computed
int p1_score = 0;
int action_speed = 2000;
int action_speed_min = 250;

void setup()
{
  Serial.begin(57600);
  Serial.println("\n\n\nSTARTING GAME");
  
  gameStartMS = millis();       // game start time in milliseconds
  
  randomSeed(generateRandomSeed());
  
  pinMode(buttonPin, INPUT);

  for (int i=0; i<nLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    turnOffLED(i);

    if (i < nLEDs-1) {
      button_tolerance[i] = (button_values[i] - button_values[i+1]) / 2;
    } else {
      button_tolerance[i] = button_tolerance[i-1];
    }    
    sprintf(debugMsg, "button tolerance[%d] = %d", i, button_tolerance[i]);
    Serial.println(debugMsg);
  }

#ifdef PAROLA
  P.begin(2);
  P.setZone(0,0,3);
  P.setZone(1,4,7);
  P.displayZoneText(0, "abc", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.setZoneEffect(0, true, PA_FLIP_UD);
  P.displayZoneText(1, "abcd", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.setZoneEffect(1, true, PA_FLIP_UD);
  P.displayAnimate();
#endif
}

void loop()
{
  uint32_t nowMS = millis();
  
  if (p1_score < 100) {
    
    loopCount++;
    bool step_action = false;
    
    if (loopCount > action_speed) {
      loopCount = 0;
      step_action = true;  
      //action_speed = action_speed - round(action_speed/50);
      action_speed = max(action_speed, action_speed_min);
      Serial.println(action_speed);

      // check game time limit
      uint32_t gameTimeMS = nowMS - gameStartMS;
      if (gameTimeMS >= TimeLimitMS) {
        gameOver = true;

        // turn off all LEDs, game over
        for (int i=0; i<nLEDs; i++) {
          turnOffLED(i);
        }
        
        Serial.println(F("TIME'S UP -- GAME OVER"));
        delay(100);
      }

      if (!gameOver) {
        // pick another LED to light up
        int which = random(0,nLEDs);

        if (isOnLED(which)) {
          // already ON, find the next one that is OFF
          which = findOffLED(which);
        }
        
        if (!isOnLED(which)) {
          sprintf(debugMsg, "lighting up LED %d", which);
          Serial.println(debugMsg);
          turnOnLED(which);
        }
      }
    }

    // check for button presses
    int whichButton = buttonPressed();

    // mark button down time for all not pressed buttons to 0, 
    // since they are not being pressed
    for (int i=0; i<nLEDs; i++) {
      if (i != whichButton) {
        buttonDownMS[i] = 0;      // button i is not pressed
      }
    }

    // check to see if the button was already down,
    // we are only interested in a transition from up to down
    if (whichButton != kInvalid) {
      if (buttonDownMS[whichButton] != 0) {
        // button was already pressed, so ignore that it is still pressed
        whichButton = kInvalid;
      } else {
        // button was not pressed, now it is pressed, valid transition
        // record what time it was pressed
        buttonDownMS[whichButton] = nowMS;
      }
    }

    // check to see if the button pressed also had its LED lit,
    // if not, then it doesn't count for anything, bad button press
    if (whichButton != kInvalid && !isOnLED(whichButton)) {
      whichButton = kInvalid;
    }
    
    if (whichButton != kInvalid) {
      // one of the buttons was just pressed AND it's LED is ON, SCORE!!
      uint32_t responseMS = nowMS - timeLitLED[whichButton];
      totalResponseTimeMS += responseMS;
      totalResponseCount++;
        
      turnOffLED(whichButton);
      p1_score++;
        
      sprintf(debugMsg, "YAY! whacked on LED %d in %ld millisec, SCORE: %d", whichButton, responseMS, p1_score);
      Serial.println(debugMsg);
    }

    // check to see if any LEDs have been on long enough, if so, turn them off
    for (int i=0; i<nLEDs; i++) {
      if (timeLitLED[i] > 0) {
        uint32_t elapsedMS = nowMS - timeLitLED[i];
        if (elapsedMS > maxTimeLitMS) {
          turnOffLED(i);
        }
      }
    }
  
 #ifdef PAROLA
    if ( loopCount % 100 == 0){
      char Score1[80];
      sprintf(Score1, "%d", p1_score);
      char *chrdisp[] = {Score1};
  
      char Score2[80];
      sprintf(Score2, "%d", p2_score);
      char *chrdisp2[] = {Score2};
  
      P.displayZoneText(0, chrdisp[0], PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, chrdisp2[0], PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayAnimate();
    }
 #endif
  } else {
 #ifdef PAROLA
    if (p1_score > p2_score) {
      P.displayZoneText(0, "Winner", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, "Looser", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    } else {
      P.displayZoneText(0, "Looser", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, "Winner", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    }
    P.displayAnimate();  
    delay(500);
      char Score1[80];
      sprintf(Score1, "%d", p1_score);
      char *chrdisp[] = {Score1};
  
      char Score2[80];
      sprintf(Score2, "%d", p2_score);
      char *chrdisp2[] = {Score2};
    P.displayZoneText(0, chrdisp[0], PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayZoneText(1, chrdisp2[0], PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);    
    P.displayAnimate();
    delay(500); 
#endif       
  }
}

// reads the button port to figure out which button is pressed
// returns the button number from 0 to N-1,
// if no buttons are pressed returns kInvalid
int buttonPressed() {
  // check to see which button is pressed (if any)
  int analogValue = analogRead(buttonPin);

  for (int i=0; i<nLEDs; i++) {
    int tolerance = button_tolerance[i];
    int minValue = button_values[i] - tolerance;
    int maxValue = button_values[i] + tolerance;

    bool isPressed = ( analogValue > minValue and analogValue < maxValue );
    if (isPressed) {
      return i;
    }
  }

  return kInvalid;      // no button is pressed
}

int findOffLED(int which) {
  // given a LED that is on, find the next one that is OFF
  int next = which;
  while (1) {
    next = (next == nLEDs-1) ? 0 : next+1;
    if (!isOnLED(next)) {
      return next;
    }
    if (next == which) {
      return which;       /// count not find an LED that was off
    }
  }
}

boolean isOnLED(int which) {
  if (which >= 0 && which < nLEDs) {
    return digitalRead(ledPins[which]) == HIGH;
  } 
  return false;
}

void turnOnLED(int which) {
  if (which >= 0 && which < nLEDs) {
    digitalWrite(ledPins[which], HIGH);
    timeLitLED[which] = millis();
  }
}

void turnOffLED(int which) {
  if (which >= 0 && which < nLEDs) {
    digitalWrite(ledPins[which], LOW);
    timeLitLED[which] = 0;
  }
}

//
// generateRandomSeed() - generates a 32 bit random seed value
// source:  https://rheingoldheavy.com/better-arduino-random-values/
//
uint32_t generateRandomSeed()
{
  const int seedPin = A6;           // unused analog input pin on Arduino Uno
  
  uint8_t  seedBitValue  = 0;
  uint8_t  seedByteValue = 0;
  uint32_t seedWordValue = 0;
 
  for (uint8_t wordShift = 0; wordShift < 4; wordShift++)     // 4 bytes in a 32 bit word
  {
    for (uint8_t byteShift = 0; byteShift < 8; byteShift++)   // 8 bits in a byte
    {
      for (uint8_t bitSum = 0; bitSum <= 8; bitSum++)         // 8 samples of analog pin
      {
        seedBitValue = seedBitValue + (analogRead(seedPin) & 0x01);                // Flip the coin eight times, adding the results together
      }
      delay(1);                                                                    // Delay a single millisecond to allow the pin to fluctuate
      seedByteValue = seedByteValue | ((seedBitValue & 0x01) << byteShift);        // Build a stack of eight flipped coins
      seedBitValue = 0;                                                            // Clear out the previous coin value
    }
    seedWordValue = seedWordValue | (uint32_t)seedByteValue << (8 * wordShift);    // Build a stack of four sets of 8 coins (shifting right creates a larger number so cast to 32bit)
    seedByteValue = 0;                                                             // Clear out the previous stack value
  }
  return (seedWordValue);
}
