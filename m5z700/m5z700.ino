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
  //Cを押下しながら起動された場合は、強制的にアクセスポイントモードにする
  if(digitalRead(BUTTON_C_PIN) == 0) {
    mzConfig.forceAccessPoint = true;
    M5.lcd.println("[C] Pressed : AccessPoint Mode");
    delay(2000);
  }else{
    mzConfig.forceAccessPoint = false;
  }

  mzConfig.enableSound = false; //起動時はサウンドOFF
  
  //Speaker Setup
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(SPEAKER_PIN, LEDC_CHANNEL_0);
  
  mz80c_main();   

  exit(0);  

}

void loop() {
  // put your main code here, to run repeatedly:

}
//Preferencesはファイルで保持。
void saveConfig(){
  Serial.println("saveConfig");

  String confPath = String(ROM_DIRECTORY) + "/m5z700.conf";
  Serial.println(confPath);
  if (SD.exists(confPath)){
    SD.remove(confPath);
  }
  
  File configFile = SD.open(confPath, FILE_WRITE);
 if (configFile) {
    configFile.print("ROMFILE:");
    configFile.println(mzConfig.romFile);
    configFile.print("TAPEFILE:");
    configFile.println(mzConfig.tapeFile);
    configFile.print("SSID:");
    configFile.println(mzConfig.ssid);
    configFile.print("PASS:");
    //Serial.println(mzConfig.pass); 
    char xorData[50]={};
    xorEnc(mzConfig.pass, xorData);
    for(int index = 0;index < strnlen(xorData,50);index++){
      configFile.printf("%x", xorData[index]); 
    }
    configFile.println(); 
    configFile.close();
  }else{
    Serial.println("FAIL:saveConfig");   
  }
  delay(100);

}

void loadConfig(){
  String confPath = String(ROM_DIRECTORY) + "/m5z700.conf";
  File configFile = SD.open(confPath, FILE_READ);
  String lineBuf = "";
  if (configFile) {
    while (configFile.available()) {
      char nextChar = char(configFile.read());
      if(nextChar == '\r' || nextChar == '\n'){
        if(lineBuf.length() == 0){ // バッファまだ0文字の場合、何もせずもう一度処理
          continue;
        }else{
          //１行読み終わり
          if(lineBuf.startsWith("ROMFILE:")){
            strncpy(mzConfig.romFile, lineBuf.substring(8).c_str(), sizeof(mzConfig.romFile));
            Serial.print("ROMFILE:");
            Serial.println(mzConfig.romFile);
          }
          if(lineBuf.startsWith("TAPEFILE:")){
            strncpy(mzConfig.tapeFile, lineBuf.substring(9).c_str(),sizeof(mzConfig.tapeFile));
            Serial.print("TAPEFILE:");
            Serial.println(mzConfig.tapeFile);
          }
          if(lineBuf.startsWith("SSID:")){
            strncpy(mzConfig.ssid, lineBuf.substring(5).c_str(),sizeof(mzConfig.ssid));
            Serial.print("SSID:");
            Serial.println(mzConfig.ssid);
          }
          if(lineBuf.startsWith("PASS:")){
            char xorData[50] = {};
            String hexData = lineBuf.substring(5);
            for(int index = 0;index < hexData.length() / 2;index++){
              String hexValue = hexData.substring(index * 2, index * 2 + 2);
              xorData[index] = strtol(hexValue.c_str(), NULL, 16);
            }
            xorEnc(xorData, mzConfig.pass);
            //Serial.print("PASS:");
            //Serial.println(xorData);
            //Serial.println(mzConfig.pass);
          }
        }
        lineBuf = "";
      }else{
        lineBuf += nextChar; 
      }
    }
    configFile.close();
  }else{
    Serial.println("ConfigFile not found.");
  }
  //preferences に Wi-Fi Configがあれば、そちらで上書きする。
  Preferences preferences;
  preferences.begin("wifi-config");
  String wifi_ssid = preferences.getString("WIFI_SSID");
  if(wifi_ssid.length() > 0){
    strncpy(mzConfig.ssid, wifi_ssid.c_str(),sizeof(mzConfig.ssid));
  }
  String wifi_password = preferences.getString("WIFI_PASSWD");
  if(wifi_password.length() > 0){
    strncpy(mzConfig.pass, wifi_password.c_str(),sizeof(mzConfig.pass));
  }
  preferences.end();

}

void xorEnc(char fromData[], char toData[] ){
  int index = 0;
  for(index = 0;index < strnlen(fromData, 50);index++) {
    toData[index] = fromData[index] ^ xorKey;
  }
  toData[index+1] = '\0';
}
