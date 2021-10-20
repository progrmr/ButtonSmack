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

#define nLEDs (8)
#define TimeLimitMS (60000)      // 60,000 ms == 60 seconds

uint32_t startMS = 0;

long randNumber;
int step_counter = 0;
int button_values[nLEDs] = {913, 429, 267, 179, 110, 99, 99, 99};    // TODO: last 3 values TBD
int btn_tol = 20;
int analogValue = 0;
int pin_p1 = A0;
int p1_leds[nLEDs] = {2,3,4,5,6,7,8,9};
int p1_score = 0;
int action_speed = 2000;
int action_speed_min = 250;
char message[120];

void setup()
{
  Serial.begin(57600);
  startMS = millis();       // game start time in milliseconds
  
  randomSeed(generateRandomSeed());
  
  pinMode(pin_p1, INPUT);

  for (int i=0; i<nLEDs; i++) {
    pinMode(p1_leds[i], OUTPUT);
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
  
  if (p1_score < 100) {
    
    step_counter++;
    bool step_action = false;
    if (step_counter > action_speed) {
      step_counter = 0;
      step_action = true;  
      action_speed = action_speed - round(action_speed/50);
      if (action_speed < action_speed_min) {
        action_speed = action_speed_min;
      }
      Serial.println(action_speed);
    }
  
    if (step_action) {
      int pin_light = random(0,nLEDs);

      if (digitalRead(p1_leds[pin_light]) == LOW) {
        sprintf(message, "lighting up LED %d", pin_light);
        Serial.println(message);
      } 
      
      digitalWrite(p1_leds[pin_light], HIGH);
    }
  
    analogValue = analogRead(pin_p1);
    for (int i = 0; nLEDs > i; i++) {
      if ( analogValue < button_values[i] + btn_tol and analogValue > button_values[i] - btn_tol ){
        if (digitalRead(p1_leds[i]) == HIGH) {
          digitalWrite(p1_leds[i], LOW);
          p1_score++;
        }
      }
    }
  
 #ifdef PAROLA
    if ( step_counter % 100 == 0){
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
