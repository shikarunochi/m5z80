//----------------------------------------------------------------------------
// File:m5z700.ino
// MZ-700 Emulator m5z700 for M5Stack(with SRAM)
// based on mz80rpi
// modified by @shikarunochi
//
// mz80rpi by Nibble Lab./Oh!Ishi, based on MZ700WIN
// (c) Nibbles Lab./Oh!Ishi 2017
//
// 'mz700win' by Takeshi Maruyama, based on Russell Marks's 'mz700em'.
// Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
//----------------------------------------------------------------------------

#include <M5Stack.h>  
#include "m5z700.h"
#include "mzmain.h"
#include <M5StackUpdater.h>     // https://github.com/tobozo/M5Stack-SD-Updater/
#include <Preferences.h>

int xorKey = 0x80;

void setup() {
  M5.begin(); //Serial,SD はこの中でbeginされている
  Serial.println("MAIN_START");

  Wire.begin();
  if(digitalRead(BUTTON_A_PIN) == 0) {
     Serial.println("Will Load menu binary");
     updateFromFS(SD);
     ESP.restart();
  }
 
  mzConfig.enableSound = false; //起動時はサウンドOFF
  
  //Speaker Setup
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(SPEAKER_PIN, LEDC_CHANNEL_0);
  
  mz80c_main();   

  exit(0);  

}

void loop() {

}
