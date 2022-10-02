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
#if defined(_M5STICKCPLUS)
#include <M5GFX.h>
#include <M5StickCPlus.h>
#else
#include <M5Atom.h>  
#endif

#include "FS.h"
#include <SPIFFS.h>
#include "m5z700.h"
#include "mzmain.h"
//#include <M5StackUpdater.h>     // https://github.com/tobozo/M5Stack-SD-Updater/
#include <Preferences.h>
#include "mz700lgfx.h"

#if defined(_M5STICKCPLUS)
M5GFX m5lcd;

#else
#ifdef USE_EXT_LCD
LGFX m5lcd;
#else
M5AtomDisplay m5lcd(320,200);                 // LGFXのインスタンスを作成。
#endif
#endif
int xorKey = 0x80;

int lcdRotate = 0;

void setup() {
  #if defined(_M5STICKCPLUS)
  M5.begin(false,true,true); 
  Wire.begin(32,33);
  #else
  //M5AtomLite
  M5.begin(true,false,true); 
  #ifndef USE_SPEAKER_G26
  Wire.begin(26,32,100000);
  #endif

  #endif
  
  Serial.println("MAIN_START");
  #if !defined(_M5STICKCPLUS)
  if(digitalRead(39) == 0) { //M5Atom Button
     Serial.println("RoteteMode:for extCRT");
     //strcpy(mzConfig.romFile, "1Z009.ROM"); MZ-700 MODE
     lcdRotate = 1;
     m5lcd.setRotation(3);
  }
  #endif
  if (!SPIFFS.begin()) { 
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  mzConfig.enableSound = false; //起動時はサウンドOFF
  m5lcd.init();

  //Speaker Setup
#if defined(USE_SPEAKER_G25)||defined(USE_SPEAKER_G26)||defined(_M5STICKCPLUS)
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(SPEAKER_PIN, LEDC_CHANNEL_0);
#endif  
  mz80c_main();   
  exit(0);  
}

void loop() {

}
