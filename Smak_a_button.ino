//---------------------------------------------------------------
//
// Single Player Whack-a-Mole style game 
// designed to use 8 buttons and LED 8x8 matrix, see schematic
//
// this source:  https://github.com/progrmr/ButtonSmack
// derived from:  https://github.com/DIYTechBros/ButtonSmack
//
// 10/21/2021 -- Gary Morris
//
// The game program has 4 states.  Initially it will be in state #1 described below. 
// 
//   1. Game Ready: LEDs will cycle through a fast sequence once per second, 
//                  press any button to start a game (state #2).  
//   2. Game Starting: 3 LEDs will light up, then counts down 3, 2, 1, 
//                  at each count it beeps and turns off 1 LED.  
//                  When it gets to 0, there's a final short beep and game starts (state #3).
//   3. Game Running: LEDs will light up at random, push button for the lit LED, you get a short beep if you hit
//                  it in time and score a point.  Displays score and time clock.  
//                  After 60 seconds, game ends, all LEDs off, goes to state #4.
//   4. Game Over:  Displays final score for 10 seconds, then it goes back to state #1 above.
//
// NOTE2: comment out the "#define DEBUG 1" line below for final deployment after testing is done.
//---------------------------------------------------------------
//#define DEBUG 1
//#define CALIBRATE 1
#define PAROLA

const char* const title = "Lucas";

#define nLEDs (8)
#define kMaxLitLEDs (2)
#define GameTimeLimitMS (60 * 1000UL)      // 60,000 ms == 60 seconds
#define kInitialLEDLitMS (2000UL)
#define kInvalid (-1)
#define kSwitchBounceMS (10)    // source: https://www.eejournal.com/article/ultimate-guide-to-switch-debounce-part-4/

#ifdef PAROLA
#include <stdio.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include "Parola_Fonts_data.h"
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW   // product from Amazon
//#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW    // display from banggood.com
#define MAX_DEVICES 4
#define CLK_PIN   13
#define DATA_PIN  11
#define CS_PIN    10
#define kScoreUpdateIntervalMS (50)
MD_Parola* P = NULL;
#endif

// Arduino pin definitions
#define kButtonPin A0
#define kSpeakerPin A5
const int ledPins[nLEDs] = {2,3,4,5,6,7,8,9};

// input button(s) voltage values
int buttonValues[nLEDs] = {920, 451, 295, 215, 165, 130, 99, 65};
int buttonTolerance[nLEDs] = {0};    // computed

// track actual readings so we can auto-recalibrate button voltage values
uint32_t nButtonValueReadings = 0;    
uint32_t totalButtonValue = 0;

// Game State and scoring data
typedef enum GameStates {gameReady, gameStarting, gameRunning, gameOver, buttonCalibration};
GameStates gameState = gameReady;
uint32_t gameStateMS = 0;     // time that game state changed
uint32_t gameScore = 0;       // hit button while it was lit
uint32_t missedCount = 0;     // hit a button that wasn't lit
uint32_t escapedCount = 0;    // button that was lit but never got hit
uint32_t oopsCount = 0;       // missed a button recently

#ifdef DEBUG
char debugMsg[120];
#endif

// Time tracking for LEDs and buttons and reaction time
uint32_t timeLitLEDMS[nLEDs] = {0};       // time when the LED was turned on
uint32_t maxTimeLitMS = kInitialLEDLitMS; // maximum amount of time to keep LED on (if not hit)

//uint32_t buttonChangeMS[nLEDs] = {0};     // time that buttons last changed up/down state (for debouncing)
//bool buttonIsDown[nLEDs] = {false};       // remember which buttons are down
int buttonDownID = kInvalid;                // current button pressed
int handledButtonID = kInvalid;                // last button processed
uint32_t buttonDownMS = 0;

uint32_t totalReactionTimeMS = 0;         // total time (ms) that it took player to correctly hit a button
uint32_t totalReactionCount = 0;          // total number of times player correctly hit a button

#define kStartActionIntervalMS 500UL
#define kMinActionIntervalMS 100UL
uint32_t actionIntervalMS = kStartActionIntervalMS;      // interval between performing an action  

#define kSpeedChangeIntervalMS 1000             // time between speed of play changes 
uint32_t lastActionMS = 0;
uint32_t lastSpeedChangeMS = 0;

void setup()
{
#ifdef DEBUG
  Serial.begin(57600);
  Serial.println("\n\n\nSETUP GAME");
#endif

  randomSeed(generateRandomSeed());
  
  pinMode(kButtonPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(kSpeakerPin, OUTPUT);
  
  for (int i=0; i<nLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    turnOffLED(i);
  }

  calculateButtonTolerance();
  
  uint32_t nowMS = millis();
  gameStateMS = nowMS; 
#ifdef CALIBRATE
  gameState = buttonCalibration;
#else 
  gameState = gameReady;
#endif

  lastSpeedChangeMS = nowMS;
}

static uint32_t nowMS = 0;

void loop()
{
  nowMS = millis();
  uint32_t elapsedInStateMS = nowMS - gameStateMS;
  
  //---------------------------------------------
  // Perform game action based on game state
  //
  if (gameState == gameStarting) {
    //--------------------------------------------
    // NEW GAME STARTING
    //--------------------------------------------
    turnOnLED(0, nowMS);
    turnOnLED(1, nowMS);
    turnOnLED(2, nowMS);
    
    tone(kSpeakerPin, 500, 800);
#ifdef PAROLA
    if (P == NULL) {
      P = new MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
      P->begin(1);         // 1 zones
      P->setZone(0,0,3);
    }
    P->displayZoneText(0, "Ready", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P->displayAnimate();  
#endif
    delay(1000);
    
    turnOffLED(2);
    
    tone(kSpeakerPin, 1000, 700);
#ifdef PAROLA
    P->displayZoneText(0, "Set", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P->displayAnimate();  
#endif
    delay(1000);
    
    turnOffLED(1);
    
    tone(kSpeakerPin, 1500, 500);
#ifdef PAROLA
    P->displayZoneText(0, "Go!", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P->displayAnimate();  
#endif
    delay(900);
    
    tone(kSpeakerPin, 2000, 100);
    delay(100);
    turnOffLED(0);
    tone(kSpeakerPin, 2500, 100);
    
    // reset all the game state variables
    nowMS = millis();
    goNextGameState();
    gameScore = 0;
    oopsCount = 0;
    missedCount = 0;
    escapedCount = 0;
    lastSpeedChangeMS = nowMS;
    lastActionMS = nowMS;
    actionIntervalMS = kStartActionIntervalMS;
    maxTimeLitMS = kInitialLEDLitMS;
    totalReactionTimeMS = 0;
    totalReactionCount = 0;

  } else if (gameState == gameReady) {
     //--------------------------------------------
    // GAME READY
    //---------------------------------------------
#ifdef PAROLA
    if (P == NULL) {
      P = new MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
      P->begin(1);         // 1 zone
      P->setZone(0,0,3);   // zone uses segment 0..3
    }
#endif   

    int flash = 0;
    int maxFlash = nLEDs + 10;   // flash each LEDs then delay 10 times before next flash
    
    while (1) {      
#ifdef PAROLA
      elapsedInStateMS = millis() - gameStateMS;
      static uint32_t prevElapsedSec = 9999;
      uint32_t elapsedSec = elapsedInStateMS / 1000;
      if (prevElapsedSec != elapsedSec) {
        if (elapsedSec % 4 == 0) {
          P->displayZoneText(0, title, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        } else if (elapsedSec % 4 == 2) {
          P->displayZoneText(0, "Go ?", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        }
        P->displayAnimate(); 
        prevElapsedSec = elapsedSec;         
      }
#endif

      if (flash < nLEDs) {
        turnOnLED(flash, 0);
      }
      
      delay(50);
      if (getButtonPressed() != kInvalid) break;
      delay(50);
      if (getButtonPressed() != kInvalid) break;

      if (flash < nLEDs) {
        turnOffLED(flash);
      }
      
      flash = (flash >= maxFlash) ? 0 : flash+1;


    } // end while (1)
    
#ifdef DEBUG
    Serial.println(F("Starting Game"));
#endif
   
    // turn off all LEDs, ready to start game
    for (int i=0; i<nLEDs; i++) {
      turnOffLED(i);
    }

    goNextGameState();

  } else if (gameState == gameOver) {
    //--------------------------------------------
    // GAME OVER
    //--------------------------------------------
#ifdef DEBUG
    sprintf(debugMsg, "*** GAME OVER ***  SCORE: %lu -- MISSED: %lu -- ESCAPED: %lu", gameScore, missedCount, escapedCount);
    Serial.println(debugMsg);
#endif

    if (elapsedInStateMS >= 10000) {
      // we've display the final score for 10 seconds, time to move onto next state
      goNextGameState();
      
    } else {
#ifdef PAROLA
      if (P == NULL) {
        P = new MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
        P->begin(1);         // 2 zones
        P->setZone(0,0,3);
      }
    
      char score1[12];
      sprintf(score1, "%d Hit", gameScore);
    
      P->displayZoneText(0, score1, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P->displayAnimate();
#endif   
      delay(500);
    }

#ifdef CALIBRATE
  } else if (gameState == buttonCalibration) {
    //--------------------------------------------
    // BUTTON CALIBRATION CHECK
    //--------------------------------------------
    static uint32_t lastButtonCalMS = 0;
    static int litLEDId = kInvalid;
    
    uint32_t elapsedButtonCal = nowMS - lastButtonCalMS;
    if (elapsedButtonCal >= 200) {
      int analogVal = analogRead(kButtonPin);
      int buttonId = buttonForAnalogValue(analogVal);
      analogVal = (analogVal / 5) * 5;      // round to nearest 5 count
      lastButtonCalMS = nowMS;

      if (buttonId != litLEDId) {
        if (litLEDId != kInvalid) {
          turnOffLED(litLEDId);       // turn off previously lit LED
        }
        turnOnLED(buttonId, millis());
        litLEDId = buttonId;
      }

#ifdef PAROLA
      if (P == NULL) {
        P = new MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
        P->begin(2);         // 2 zones
        P->setZone(1,0,1);
        P->setZone(0,2,3);
      }
    
      char buttonStr[8];
      char analogStr[8];
      if (buttonId != kInvalid) {
        sprintf(buttonStr, "%d", buttonId);
      } else {
        strcpy(buttonStr, "-");
      }
      sprintf(analogStr, "%d", analogVal);
      
      P->displayZoneText(0, buttonStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P->displayZoneText(1, analogStr, PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P->displayAnimate();
#endif   
    }

#endif

  } else if (gameState == gameRunning) {
    //--------------------------------------------
    // GAME RUNNING
    //--------------------------------------------
    // check to see if it's time to end the game
    if (elapsedInStateMS >= GameTimeLimitMS) {
      // time to end the game
      goNextGameState();
      
      // turn off all LEDs, game over
      for (int i=0; i<nLEDs; i++) {
        turnOffLED(i);
      }

      tone(kSpeakerPin, 300, 2000);

      return;   // --- RETURN --- exit loop(), gameState changed
    }
    
    // check if it's time to speed up the game play
    uint32_t speedChangeElapsedMS = nowMS - lastSpeedChangeMS;
    
    if (speedChangeElapsedMS > kSpeedChangeIntervalMS) {
      // time to speed up the game, with some random variation,
      // so that the timing is not predictable
      int32_t intervalVariation = random(0, actionIntervalMS/5);     // vary interval by 20% (1/5th)
      if (intervalVariation % 2 == 0) {
        actionIntervalMS += intervalVariation;    // add variation
      } else {
        actionIntervalMS -= intervalVariation;    // subtract variation
      }
      
      actionIntervalMS -= actionIntervalMS / 50;                        // shorten the interval by 2% (1/50th)
      actionIntervalMS = max(kMinActionIntervalMS, actionIntervalMS);   // don't go below minimum interval
      actionIntervalMS = min(kStartActionIntervalMS, actionIntervalMS); // don't go above maximum interval

#ifdef DEBUG
      sprintf(debugMsg, "speed change to %lu ms interval", actionIntervalMS);
      Serial.println(debugMsg);
#endif

      if (oopsCount >= 3 && maxTimeLitMS < kInitialLEDLitMS) {
        // increase time that LEDs are lit, 
        // player is hitting button too late
        uint32_t newTimeLitMS = maxTimeLitMS + 200;
#ifdef DEBUG
        sprintf(debugMsg, "increasing LED lit time from %lu to %lu ms", maxTimeLitMS, newTimeLitMS);
        Serial.println(debugMsg);
#endif
        maxTimeLitMS = newTimeLitMS;
        oopsCount = 0;    

      } else if (totalReactionCount >= 3 && oopsCount == 0) {
        // shorten LED on time, change them to
        // halfway between current and reactionTimeMS
        uint32_t reactionTimeMS = (totalReactionTimeMS / totalReactionCount) + 100;  // allow an extra 100ms above reaction time
        uint32_t newTimeLitMS = (reactionTimeMS + maxTimeLitMS) / 2;
        if (newTimeLitMS < maxTimeLitMS && newTimeLitMS >= kSwitchBounceMS*3) {
#ifdef DEBUG
          sprintf(debugMsg, "reducing LED lit time from %lu to %lu ms", maxTimeLitMS, newTimeLitMS);
          Serial.println(debugMsg);
#endif
          maxTimeLitMS = newTimeLitMS;
        }
      }
      
      lastSpeedChangeMS = nowMS;
    }
    
    // check if it's time for more action in the game play
    uint32_t actionElapsedMS = nowMS - lastActionMS;    // elapsed MS since last action
    bool changedLEDs = false;
    
    if (actionElapsedMS >= actionIntervalMS && nLEDsLit() < kMaxLitLEDs) {
      // randomly pick one of the off LEDs to light up next
      int which = pickRandomOffLED();
      
      turnOnLED(which, nowMS);
      changedLEDs = true;
      lastActionMS = nowMS;
    }

    //-------------------------------------------------------------------
    // check for button presses - ignore bounces and duplicates
    //-------------------------------------------------------------------
    const int buttonValue = analogRead(kButtonPin);
    int whichButton = buttonForAnalogValue(buttonValue);
    
    if (whichButton == kInvalid) {
      // all buttons are up, none down
      recalibrateButtonValues();
      
      buttonDownID = kInvalid;
      buttonDownMS = nowMS;
      handledButtonID = kInvalid;
      nButtonValueReadings = 0;
      totalButtonValue = 0;

    } else {
      // a button IS currently down
      if (whichButton == buttonDownID) {
        // same button still being pressed
        const uint32_t elapsedChangeMS = nowMS - buttonDownMS;
        if (elapsedChangeMS <= kSwitchBounceMS) {
          // button down not yet steady, still bouncing, ignore
          whichButton = kInvalid;   // ignore button down, it hasn't been long enough to be valid
#ifdef DEBUG
          sprintf(debugMsg, "--- button %d only down for %lu ms", buttonDownID, elapsedChangeMS);
          Serial.println(debugMsg);
#endif
        } else {
          // button has been down long enough, not a bounce, this is a steady button press
          if (whichButton == handledButtonID) {
            // button has already been handled, don't handle it again, ignore it
            whichButton = kInvalid;
         } else {
            // valid button down, not a bounce
            // remember which button has been handled
            handledButtonID = whichButton; 
          }
        }
        
      } else {
        // different button pressed, restart button tracking
        recalibrateButtonValues();
        
        buttonDownID = whichButton;
        buttonDownMS = nowMS;
        whichButton = kInvalid;     // don't handle it now, we have to wait for debouncing time
        handledButtonID = kInvalid;
        nButtonValueReadings = 0;
        totalButtonValue = 0;
      } // end if isSameButton

      // keep track of the button values that we have seen for the button that is down
      nButtonValueReadings++;
      totalButtonValue += buttonValue;
#ifdef DEBUG
//      int average = totalButtonValue / nButtonValueReadings;
//      sprintf(debugMsg, "--- button %d seen %lux, value %d average: %d (was %d)",
//              buttonDownID, nButtonValueReadings, buttonValue, average, buttonValues[buttonDownID]);
//      Serial.println(debugMsg);
#endif
    } // end button is down

    //------------------------------------------------------
    // if whichButton is still valid at this point, 
    // then we have a new debounced button press to handle
    //------------------------------------------------------

    if (whichButton != kInvalid) {
#ifdef DEBUG
      Serial.println(F("+++ valid button press"));
#endif

      // valid button press, that's a point score if the LED was on
      if (isOnLED(whichButton)) {
        // one of the buttons was just pressed AND it's LED is ON, SCORE!!
        // calculate player's reaction time
        uint32_t reactionTimeMS = nowMS - timeLitLEDMS[whichButton];
        totalReactionTimeMS += reactionTimeMS;
        totalReactionCount++;

        turnOffLED(whichButton);
        gameScore++;
        oopsCount = 0;    // reset oops score
  
        // happy sound beep, two rising high pitched tones
        const int kBeepDuration = 80;
        tone(kSpeakerPin, 1000, kBeepDuration);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(kBeepDuration);
        digitalWrite(LED_BUILTIN, LOW);
        tone(kSpeakerPin, 2000, kBeepDuration);
  
#ifdef DEBUG
       sprintf(debugMsg, "YAY! whacked on LED %d in %lu ms (avg %lu ms), SCORE: %lu", 
                          whichButton, 
                          reactionTimeMS, 
                          totalReactionTimeMS / totalReactionCount, 
                          gameScore);
        Serial.println(debugMsg);
#endif

      } else {
        // Oops, LED was NOT on, player doesn't get any points for this
        // but if this happens a lot, we let the LEDs stay on longer to give player a chance
        oopsCount++;
        missedCount++;
#ifdef DEBUG
        sprintf(debugMsg, "OOPS #%lu: button %d pressed, but LED not lit", oopsCount, whichButton);
        Serial.println(debugMsg);
#endif        
      }
    }
    
    // check to see how long each of the LEDs have been on, 
    // if an LED reaches its time limit, turn it off
    for (int i=0; i<nLEDs; i++) {
      if (timeLitLEDMS[i] > 0) {
        uint32_t elapsedMS = nowMS - timeLitLEDMS[i];
        if (elapsedMS > maxTimeLitMS) {
          // LED was lit, but button never got hit, turn it off
          escapedCount++;
          turnOffLED(i);
          changedLEDs = true;
        }
      }
    }

#ifdef DEBUG
    if (changedLEDs) {
      // print out list of lit LEDs
      strcpy(debugMsg, "current LEDs lit: ");
      for (int i=0; i<nLEDs; i++) {
        if (timeLitLEDMS[i] > 0) {
          char tmp[8];
          sprintf (tmp, "%2d", i);
          strcat(debugMsg, tmp);
        } else {
          strcat(debugMsg, "  ");
        }
      }
      Serial.println(debugMsg);
    }
#endif

#ifdef PAROLA
    if (P == NULL) {
      P = new MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
      P->begin(2);         // 2 zones
      P->setZone(1,0,1);
      P->setZone(0,2,3);
    }
    
    static uint32_t lastScoreUpdateMS = 0;
    uint32_t elapsedSinceScoreUpdateMS = nowMS - lastScoreUpdateMS;
    
    if (elapsedSinceScoreUpdateMS >= kScoreUpdateIntervalMS) {
      char score[8];
      sprintf(score, "%lu", gameScore);

      char timeClock[8];
      uint32_t secsLeftMS = GameTimeLimitMS - elapsedInStateMS;
      unsigned long secsLeftL = lroundf(secsLeftMS / 1000.0);
      sprintf(timeClock, "%lu", secsLeftL);

      P->displayZoneText(0, score, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P->displayZoneText(1, timeClock, PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P->displayAnimate();
      lastScoreUpdateMS = nowMS;
    }
 #endif

  } // end if (gameState == gameRunning)
  
}  // end loop()

//-------------------------------------------------------------------------
// manage transitions throught the various GameStates
//
// enum GameState {gameReady, gameStarting, gameRunning, gameOver, buttonCalibration};
//
void goNextGameState() {
  GameStates newState = gameState;
  
  switch (gameState) {
    case gameReady:     
      newState = gameStarting;
      break;
    case gameStarting:  
      newState = gameRunning;
      break;
    case gameRunning:   
      newState = gameOver;
      break;
    case gameOver:      
      newState = gameReady;
      break;
    default:
      break;
  }
  if (newState != gameState) {
    nowMS = millis();
    gameStateMS = nowMS;
    
    if (P != NULL) {
      free(P);
    }
    P = NULL;
  }
  gameState = newState;
}

// given the analog port reading figures out which button is pressed
// returns the button number from 0 to nLEDs-1,
// if no buttons are pressed returns kInvalid
int getButtonPressed() {
  // check to see which button is pressed (if any)
  int analogValue = analogRead(kButtonPin);

  return buttonForAnalogValue(analogValue);
}

// given the analog port reading figures out which button is pressed
// returns the button number from 0 to N-1,
// if no buttons are pressed returns kInvalid
int buttonForAnalogValue(int analogValue) {
  for (int i=0; i<nLEDs; i++) {
    int tolerance = buttonTolerance[i];
    int minValue = buttonValues[i] - tolerance;
    int maxValue = buttonValues[i] + tolerance;

    bool isPressed = ( analogValue >= minValue and analogValue <= maxValue );
    if (isPressed) {
#ifdef DEBUG
      sprintf(debugMsg, "--- button press %d (value %d, exp %d, allow %d-%d)",
              i, analogValue, buttonValues[i], minValue, maxValue);
      Serial.println(debugMsg);
#endif
      return i;
    }
  }
  return kInvalid;      // no button is pressed  
}

void recalibrateButtonValues() {
  if (nButtonValueReadings >= 3) {
    int averageValue = totalButtonValue / nButtonValueReadings;
    int expectedValue = buttonValues[buttonDownID];
    
    if (abs(averageValue-expectedValue) >= 2) {
      buttonValues[buttonDownID] = (averageValue+expectedValue)/2;
#ifdef DEBUG
      sprintf(debugMsg, "*** recalibrate: button %d seen %lux, averages %d (expected %d), new value %d",
            buttonDownID, nButtonValueReadings, averageValue, expectedValue, buttonValues[buttonDownID]);
      Serial.println(debugMsg);
#endif
      calculateButtonTolerance();
    }
  }
}

void calculateButtonTolerance() {
  for (int i=0; i<nLEDs; i++) {
    // calculate buttonTolerance so we can tell which button was pressed
    if (i < nLEDs-1) {
      buttonTolerance[i] = (buttonValues[i] - buttonValues[i+1]) / 2;
    } else {
      buttonTolerance[i] = buttonTolerance[i-1];
    }    
#ifdef DEBUG
    sprintf(debugMsg, "--- button %d, nominal: %3d, tol: +/-%3d, min: %3d, max: %3d", 
                       i, buttonValues[i], buttonTolerance[i], 
                       buttonValues[i]-buttonTolerance[i],
                       buttonValues[i]+buttonTolerance[i]);
    Serial.println(debugMsg);
#endif
  }
}

unsigned nLEDsLit() {
  unsigned nLit = 0;
  for (int i=0; i<nLEDs; i++) {
    if (timeLitLEDMS[i] > 0) {
      nLit++;
    }
  }
  return nLit;
}

int pickRandomOffLED() {
  // randomly choose an LED from the LEDs that are OFF
  const int nLEDsOff = nLEDs - nLEDsLit();
  if (nLEDsOff == 0) return 0;      // none of them are off !?!
  
  const int pick = random(0,nLEDsOff);       // pick one of the OFF LEDs
  
  int curOffLED = 0;
  for (int i=0; i<nLEDs; i++) {
    if (!isOnLED(i)) {
      if (curOffLED == pick) {
        return i;
      }
      curOffLED++;
    }
  }
}

boolean isOnLED(int which) {
  if (which >= 0 && which < nLEDs) {
    return digitalRead(ledPins[which]) == HIGH;
  } 
  return false;
}

void turnOnLED(int which, uint32_t onTimeMS) {
  if (which >= 0 && which < nLEDs) {
    digitalWrite(ledPins[which], HIGH);
    timeLitLEDMS[which] = onTimeMS;
  }
}

void turnOffLED(int which) {
  if (which >= 0 && which < nLEDs) {
    digitalWrite(ledPins[which], LOW);
    timeLitLEDMS[which] = 0;
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
