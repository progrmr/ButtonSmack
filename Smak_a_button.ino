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
#define DEBUG 1
//#define CALIBRATE 1
#define PAROLA

#define nLEDs (8)
#define kMaxLitLEDs (nLEDs/2)
#define GameTimeLimitMS (60 * 1000UL)      // 60,000 ms == 60 seconds
#define kInitialLEDLitMS (2000UL)
#define kInvalid (-1)
#define kSwitchBounceMS (20)    // source: https://www.eejournal.com/article/ultimate-guide-to-switch-debounce-part-4/

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
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
#endif

// Arduino pin definitions
#define kButtonPin A0
#define kSpeakerPin A5
const int ledPins[nLEDs] = {2,3,4,5,6,7,8,9};

// input button(s) voltage values
uint16_t buttonValues[nLEDs] = {920, 451, 295, 215, 165, 130, 99, 65};
uint16_t buttonTolerance[nLEDs] = {0};    // computed

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

uint32_t buttonChangeMS[nLEDs] = {0};     // time that buttons last changed up/down state (for debouncing)
bool buttonIsDown[nLEDs] = {false};       // remember which buttons are down

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

    // calculate buttonTolerance so we can tell which button was pressed
    if (i < nLEDs-1) {
      buttonTolerance[i] = (buttonValues[i] - buttonValues[i+1]) / 2;
    } else {
      buttonTolerance[i] = buttonTolerance[i-1];
    }    
#ifdef DEBUG
    sprintf(debugMsg, "button %d, nominal: %3d, tol: +/-%3d, min: %3d, max: %3d", 
                       i, buttonValues[i], buttonTolerance[i], 
                       buttonValues[i]-buttonTolerance[i],
                       buttonValues[i]+buttonTolerance[i]);
    Serial.println(debugMsg);
#endif
  }

#ifdef PAROLA
  P.begin(2);         // 1 zone
  P.setZone(1,0,1);
  P.setZone(0,2,3);
  P.displayZoneText(0, "Wak", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(1, "Mol", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
#endif

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
    P.displayZoneText(0, "IN", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayZoneText(1, "3", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();  
#endif
    delay(1000);
    
    turnOffLED(2);
    
    tone(kSpeakerPin, 1000, 700);
#ifdef PAROLA
    P.displayZoneText(1, "2", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();  
#endif
    delay(1000);
    
    turnOffLED(1);
    
    tone(kSpeakerPin, 1500, 500);
#ifdef PAROLA
    P.displayZoneText(1, "1", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();  
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
    P.displayZoneText(0, "GO", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayZoneText(1, "??", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();
#endif   

    int flash = 0;
    int maxFlash = nLEDs + 10;   // flash each LEDs then delay 10 times before next flash
    
    while (1) {
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
    }
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
      char score1[8];
      sprintf(score1, "%d", gameScore);
    
      P.displayZoneText(0, score1, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, "HIT", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayAnimate();
#endif   
      delay(500);
    }

  } else if (gameState == buttonCalibration) {
    //--------------------------------------------
    // BUTTON CALIBRATION CHECK
    //--------------------------------------------
    static uint32_t lastButtonCalMS = 0;
    uint32_t elapsedButtonCal = nowMS - lastButtonCalMS;
    if (elapsedButtonCal >= 250) {
      (void) getButtonPressed();
      lastButtonCalMS = nowMS;
    }

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
        uint32_t reactionTimeMS = totalReactionTimeMS / totalReactionCount;
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

    // check for button presses
    int whichButton = getButtonPressed();

    // check to see if buttons released or pressed, 
    // also check to see if these changes are switch bounces
    for (int i=0; i<nLEDs; i++) {
      // if button change happened too soon, it is switch bounce
      const bool isDown = (i==whichButton);
      const bool buttonChanged = isDown != buttonIsDown[i];
      if (buttonChanged) {
        const uint32_t elapsedChangeMS = nowMS - buttonChangeMS[i];
        if (elapsedChangeMS <= kSwitchBounceMS) { 
          // bounce detected, ignore this change
          if (i==whichButton) {
            whichButton = kInvalid;   // ignore button down, it's a bounce detected
          }
        } else {
          // change is not a switch bounce
          buttonIsDown[i] = isDown;
          buttonChangeMS[i] = nowMS;
        }
      }
      
      if (!buttonChanged && isDown) {
        // button is still down, ignore this button press, it's not new
        whichButton = kInvalid;
#ifdef DEBUG
        //Serial.println(F("*** ignoring button still down"));
#endif
     }
    }

    // check to see if the button was already down,
    // we are only interested in a transition from up to down
    if (whichButton != kInvalid) {
      // button was up last time, now it is down, valid transition
      // we have a valid button press (not a bounce)
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
//        // sad sound beep, two descending low pitched tones
//        const int kBeepDuration = 80;
//        tone(kSpeakerPin, 300, kBeepDuration);
//        delay(kBeepDuration);
//        tone(kSpeakerPin, 200, kBeepDuration);
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
    static uint32_t lastScoreUpdateMS = 0;
    uint32_t elapsedSinceScoreUpdateMS = nowMS - lastScoreUpdateMS;
    
    if (elapsedSinceScoreUpdateMS >= kScoreUpdateIntervalMS) {
      char score[8];
      sprintf(score, "%lu", gameScore);

      char timeClock[8];
      uint32_t secsLeftMS = GameTimeLimitMS - elapsedInStateMS;
      unsigned long secsLeftL = lroundf(secsLeftMS / 1000.0);
      sprintf(timeClock, "%lu", secsLeftL);

      P.displayZoneText(0, score, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, timeClock, PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayAnimate();
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
  }
  gameState = newState;
}

// reads the button port to figure out which button is pressed
// returns the button number from 0 to N-1,
// if no buttons are pressed returns kInvalid
int getButtonPressed() {
  // check to see which button is pressed (if any)
  uint16_t analogValue = analogRead(kButtonPin);

  for (int i=0; i<nLEDs; i++) {
    uint16_t tolerance = buttonTolerance[i];
    uint16_t minValue = buttonValues[i] - tolerance;
    uint16_t maxValue = buttonValues[i] + tolerance;

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
