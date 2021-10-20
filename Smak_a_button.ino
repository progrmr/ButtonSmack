// IMPORTANT!
// DOWNLOAD the MD_Parola library
// https://github.com/MajicDesigns/MD_Parola

#define kInvalid (-1)
#define nLEDs (8)
#define kMaxLitLEDs (nLEDs/2)
#define GameTimeLimitMS (120 * 1000UL)      // 60,000 ms == 60 seconds
#define kButtonPin A0
#define kSpeedChangeIntervalMS 1000             // time between speed of play changes 

enum GameState {gameNotStarted, gameRunning, gameOver};
GameState gameState = gameNotStarted;
uint32_t gameStateMS = 0;     // time that game state changed
uint32_t gameScore = 0;

char debugMsg[120];

int ledPins[nLEDs] = {2,3,4,5,6,7,8,9};
uint32_t timeLitLEDMS[nLEDs] = {0};
uint32_t buttonDownMS[nLEDs] = {0};
uint32_t maxTimeLitMS = 2000;           // start out at 2000 ms (2 sec) max time LED is lit

uint32_t totalResponseTimeMS = 0;
uint32_t totalResponseCount = 0;

int button_values[nLEDs] = {920, 451, 295, 215, 165, 130, 99, 65};
int button_tolerance[nLEDs] = {0};    // computed

#define kStartActionInterval 500UL
#define kMinActionInterval 100UL
uint32_t actionIntervalMS = kStartActionInterval;      // interval between performing an action  
uint32_t lastActionMS = 0;
uint32_t lastSpeedChangeMS = 0;

void setup()
{
  Serial.begin(57600);
  Serial.println("\n\n\nSETUP GAME");
  
  randomSeed(generateRandomSeed());
  
  pinMode(kButtonPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i=0; i<nLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    turnOffLED(i);

    // calculate button_tolerance so we can tell which button was pressed
    if (i < nLEDs-1) {
      button_tolerance[i] = (button_values[i] - button_values[i+1]) / 2;
    } else {
      button_tolerance[i] = button_tolerance[i-1];
    }    
    sprintf(debugMsg, "button %d, nominal: %3d, tol: +/-%3d, min: %3d, max: %3d", 
                       i, button_values[i], button_tolerance[i], 
                       button_values[i]-button_tolerance[i],
                       button_values[i]+button_tolerance[i]);
    Serial.println(debugMsg);
  }

  gameStateMS = millis(); 
  gameState = gameRunning;
  
  lastSpeedChangeMS = millis();
}

void loop()
{
  uint32_t nowMS = millis();

  uint32_t gameTimeElapsedMS = nowMS - gameStateMS;

  if (gameTimeElapsedMS >= GameTimeLimitMS) {
    // time to end the game
    gameState = gameOver;
    gameStateMS = nowMS;
  }

  if (gameState == gameOver) {
    // turn off all LEDs, game over
    for (int i=0; i<nLEDs; i++) {
      turnOffLED(i);
    }
    
    Serial.println(F("TIME IS UP -- GAME OVER"));
    delay(1000);
    
  } else if (gameState == gameRunning) {

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
      
      actionIntervalMS -= actionIntervalMS / 50;                      // shorten the interval by 2% (1/50th)
      actionIntervalMS = max(kMinActionInterval, actionIntervalMS);   // don't go below minimum interval
      actionIntervalMS = min(kStartActionInterval, actionIntervalMS); // don't go above maximum interval
      
      sprintf(debugMsg, "speed change to %ld ms interval", actionIntervalMS);
      Serial.println(debugMsg);

      // also reduce the length of time that the LEDs are lit,
      // if we have enough reaction time data
      if (totalResponseCount >= 3) {
        uint32_t reactionTimeMS = totalResponseTimeMS / totalResponseCount;
        // change the LED lit time to halfway between current and reactionTimeMS
        uint32_t newLitLEDTimeMS = (reactionTimeMS + maxTimeLitMS) / 2;
        sprintf(debugMsg, "reducing LED lit time from %ld ms to %ld ms", maxTimeLitMS, newLitLEDTimeMS);
        Serial.println(debugMsg);
        
        maxTimeLitMS = newLitLEDTimeMS;
      }
      
      lastSpeedChangeMS = nowMS;
    }
    
    // check if it's time for more action in the game play
    uint32_t actionElapsedMS = nowMS - lastActionMS;    // elapsed MS since last action
    
    if (actionElapsedMS >= actionIntervalMS && nLEDsLit() < kMaxLitLEDs) {
      // pick another LED to light up
      int which = random(0,nLEDs);

      if (isOnLED(which)) {
        // already ON, find the next one that is OFF
        which = findOffLED(which);
      }
      
      sprintf(debugMsg, "lighting up LED %d", which);
      Serial.println(debugMsg);
      turnOnLED(which, nowMS);
          
      lastActionMS = nowMS;
    }

    // check for button presses
    int whichButton = getButtonPressed();
  
    // reset button down time for all NOT pressed buttons, reset it to 0, 
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
  
    // check to see if the button pressed has its LED lit,
    // if not, then it doesn't count for anything, bad button press
    if (whichButton != kInvalid && !isOnLED(whichButton)) {
      sprintf(debugMsg, "OOPS: button %d pressed, but LED not lit", whichButton);
      Serial.println(debugMsg);
      whichButton = kInvalid;
    }
    
    if (whichButton != kInvalid) {
      // one of the buttons was just pressed AND it's LED is ON, SCORE!!
      // calculate player's response time
      uint32_t responseTimeMS = nowMS - timeLitLEDMS[whichButton];
      totalResponseTimeMS += responseTimeMS;
      totalResponseCount++;
        
      turnOffLED(whichButton);
      gameScore++;
      
      sprintf(debugMsg, "YAY! whacked on LED %d in %ld ms (avg %ld ms), SCORE: %d", 
                        whichButton, 
                        responseTimeMS, 
                        totalResponseTimeMS / totalResponseCount, 
                        gameScore);
      Serial.println(debugMsg);
    }

    // check to see how long each of the LEDs have been on, 
    // if they reach their on time limit, turn them off
    for (int i=0; i<nLEDs; i++) {
      if (timeLitLEDMS[i] > 0) {
        uint32_t elapsedMS = nowMS - timeLitLEDMS[i];
        if (elapsedMS > maxTimeLitMS) {
          //sprintf(debugMsg, "turning off LED %d after %d ms", i, elapsedMS);
          //Serial.println(debugMsg);
          turnOffLED(i);
        }
      }
    }
  } // end if (gameState == gameRunning)
  
}  // end loop()


// reads the button port to figure out which button is pressed
// returns the button number from 0 to N-1,
// if no buttons are pressed returns kInvalid
int getButtonPressed() {
  // check to see which button is pressed (if any)
  int analogValue = analogRead(kButtonPin);

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

unsigned nLEDsLit() {
  unsigned nLit = 0;
  for (int i=0; i<nLEDs; i++) {
    if (timeLitLEDMS[i] > 0) {
      nLit++;
    }
  }
  return nLit;
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
