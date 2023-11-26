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
#elif defined(_M5ATOMS3)||defined(_M5ATOMS3LITE)
#include <M5Unified.h>
#include<Wire.h>
#elif defined(_M5STACK)
#include <M5Stack.h>
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/
#elif defined(_M5CARDPUTER)
#include <SD.h>
#include "M5Cardputer.h"
#include <SPI.h>
#include <M5StackUpdater.h>	
SPIClass SPI2;
#define KEY_FN         0xff
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
#elif defined(_M5ATOMS3)
M5GFX m5lcd;
#elif defined(_M5STACK)
M5GFX m5lcd;
#elif defined(_M5STAMP)
Button extBtn = Button(18, true, 10);
LGFX m5lcd;
#elif  defined(_M5CARDPUTER)
#include <Wire.h>

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
  m5lcd.init();
  #elif defined(_M5ATOMS3)
  auto cfg = M5.config();
  cfg.internal_imu=false;
  M5.begin(cfg);
  //Wire.begin(2,1,100000UL);
  Wire.begin(2,1,400000UL);
  //Wire.begin(2,1,40000000UL);
  
  m5lcd = M5.Display;
  m5lcd.setRotation(0);
  #elif defined(_M5STACK)
  M5.begin(false,true,true,true); 
  m5lcd.begin();
  if(digitalRead(BUTTON_A_PIN) == 0) {
     Serial.println("Will Load menu binary");
     updateFromFS(SD);
     ESP.restart();
  }
  #elif defined(_M5ATOMS3LITE)
  auto cfg = M5.config();
  cfg.internal_imu=false;
  M5.begin(cfg);
  Wire.begin(2,1,100000UL);
  m5lcd.init();
  #elif defined(_M5STAMP)
  M5.begin(true,false,true); 
  Wire.begin(32,33);
  m5lcd.init();
  extBtn.read(); //extBtn 状態を一度クリアする
  #elif defined(_M5CARDPUTER)
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  SPI2.begin(
     M5.getPin(m5::pin_name_t::sd_spi_sclk),
     M5.getPin(m5::pin_name_t::sd_spi_miso),
     M5.getPin(m5::pin_name_t::sd_spi_mosi),
     M5.getPin(m5::pin_name_t::sd_spi_ss)
   );
  while (false == SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss),SPI2)) {
    delay(500);
  }
  
  M5Cardputer.update();
    
  if (M5Cardputer.Keyboard.isKeyPressed('a')) {
      Serial.println("Will Load menu binary");
      updateFromFS(SD,"/menu.bin");
      ESP.restart();
  }

  //Wire.begin(2,1,400000UL);  
  m5lcd = M5Cardputer.Display;
  m5lcd.init();
  m5lcd.setRotation(1);
  M5Cardputer.Speaker.begin();
  USBSerial.begin(11520);
  #else
  //M5AtomLite
  M5.begin(true,false,true); 
  #ifndef USE_SPEAKER_G26
  Wire.begin(26,32);
  #endif
  m5lcd.init();
  #endif
  
  Serial.println("MAIN_START");
  
  #if !defined(_M5STICKCPLUS) && !defined(_M5STAMP)
  if(digitalRead(39) == 0) { //M5Atom Button
     Serial.println("RoteteMode:for extCRT");
     strcpy(mzConfig.romFile, "1Z009.ROM"); //MZ-700 MODE
     lcdRotate = 1;
     //m5lcd.setRotation(2);
  }
  #endif

  #if defined(_M5STAMP)
  if(digitalRead(18) == 0) { //
     strcpy(mzConfig.romFile, "1Z009.ROM"); //MZ-700 MODE
  }

  #endif
  #if !defined(_M5STACK)&& !defined(_M5CARDPUTER)
  if (!SPIFFS.begin()) { 
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  #endif

  mzConfig.enableSound = false; //起動時はサウンドOFF


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
