//----------------------------------------------------------------------------
// File:m5z700AtomHDMI.ino
// MZ-700 Emulator m5z700Atom for M5SAtom(with AtomDisplay and BluetoothKeyboar)
// based on mz80rpi
// modified by @shikarunochi
//
// mz80rpi by Nibble Lab./Oh!Ishi, based on MZ700WIN
// (c) Nibbles Lab./Oh!Ishi 2017
//
// 'mz700win' by Takeshi Maruyama, based on Russell Marks's 'mz700em'.
// Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
//----------------------------------------------------------------------------

#include <M5Atom.h>  
#include "FS.h"
#include <SPIFFS.h>
#include "m5z700.h"
#include "mzmain.h"
//#include <M5StackUpdater.h>     // https://github.com/tobozo/M5Stack-SD-Updater/
#include <Preferences.h>
#include "mz700lgfx.h"

M5AtomDisplay m5lcd(320,200);                 // LGFXのインスタンスを作成。

int xorKey = 0x80;

void setup() {
  M5.begin(true,false,true); 
  Serial.println("MAIN_START");
  
  Wire.begin(26,32,100000);

  //if(digitalRead(BUTTON_A_PIN) == 0) {
  //   Serial.println("Will Load menu binary");
  //   updateFromFS(SD);
  //   ESP.restart();
  //}
  if (!SPIFFS.begin()) { 
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  mzConfig.enableSound = false; //起動時はサウンドOFF
  m5lcd.init();

  //Speaker Setup
  //ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  //ledcAttachPin(SPEAKER_PIN, LEDC_CHANNEL_0);
  
  mz80c_main();   
  exit(0);  
}

void loop() {

}
