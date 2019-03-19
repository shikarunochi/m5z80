//----------------------------------------------------------------------------
// File:MZmain.cpp
// MZ-700 Emulator m5z700 for M5Stack(with SRAM)
// m5z700:Main Program Module based on mz80rpi
// modified by @shikarunochi
//
// mz80rpi by Nibble Lab./Oh!Ishi, based on MZ700WIN
// (c) Nibbles Lab./Oh!Ishi 2017
//
// 'mz700win' by Takeshi Maruyama, based on Russell Marks's 'mz700em'.
// Z80 emulation from 'Z80em' Copyright (C) Marcel de Kogel 1996,1997
//----------------------------------------------------------------------------

#include <M5Stack.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <linux/input.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
//#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>
//#include <sys/ipc.h>
//#include <sys/msg.h>
#include <libgen.h>
#include "m5z700.h"

extern "C" {
#include "z80.h"
#include "Z80Codes.h"
}

#include "defkey.h"

#include "mzmain.h"
#include "MZhw.h"
#include "mzscrn.h"
//#include "mzbeep.h"
#include <WebServer.h>
#include <WiFiMulti.h>

static bool intFlag = false;

void scrn_thread(void *arg);
void keyin_thread(void *arg);

void systemMenu();

void checkJoyPad();

int buttonApin = 26; //赤ボタン
int buttonBpin = 36; //青ボタン

#define CARDKB_ADDR 0x5F
void checkI2cKeyboard();

void sortList(String fileList[], int fileListCount);

bool joyPadPushed_U;
bool joyPadPushed_D;
bool joyPadPushed_L;
bool joyPadPushed_R;
bool joyPadPushed_A;
bool joyPadPushed_B;
bool joyPadPushed_Press;
bool enableJoyPadButton;

SYS_STATUS sysst;
int xferFlag = 0;

#define SyncTime	17									/* 1/60 sec.(milsecond) */

#define MAX_FILES 255 // this affects memory

String selectMzt();

int q_kbd;
typedef struct KBDMSG_t {
	long mtype;
	char len;
	unsigned char msg[80];
} KBDMSG;

//MZ-80K/C キーマトリクス参考：http://www43.tok2.com/home/cmpslv/Mz80k/EnrMzk.htm
unsigned char ak_tbl_80c[] =
 {
	0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0x65, 0xff, 0x80, 0xff, 0x84, 0xff, 0xff,
	0xff, 0x92, 0x80, 0x83, 0x80, 0x90, 0x80, 0x81, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

// " "    !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /    
	0x91, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x73, 0x05, 0x64, 0x74,
//  0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
	0x14, 0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, 0x80, 0x54, 0x80, 0x25, 0x80, 0x80,
//  @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
//  P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff,
//  `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
	0xff, 0x40, 0x62, 0x61, 0x41, 0x21, 0x51, 0x42, 0x52, 0x33, 0x43, 0x53, 0x44, 0x63, 0x72, 0x24,
//  p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~
	0x34, 0x20, 0x31, 0x50, 0x22, 0x23, 0x71, 0x30, 0x70, 0x32, 0x60, 0xff, 0xff, 0xff, 0xff, 0xff,
};
unsigned char ak_tbl_s_80c[] =
{
	0xff, 0xff, 0xff, 0x93, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x65, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x92, 0xff, 0x83, 0xff, 0x90, 0xff, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, 0x25, 0x05, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x24, 0xff, 0x20, 0xff, 0x30, 0x33,
	0x23, 0x40, 0x62, 0x61, 0x41, 0x21, 0x51, 0x42, 0x52, 0x33, 0x43, 0x53, 0x44, 0x63, 0x72, 0x24,
	0x34, 0x20, 0x31, 0x50, 0x22, 0x23, 0x71, 0x30, 0x70, 0x32, 0x60, 0x31, 0x32, 0x22, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

//MZ-700 キーマトリクス参考：http://www.maroon.dti.ne.jp/youkan/mz700/mzioframe.html
unsigned char ak_tbl_700[] =
{
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x06, 0x07, 0xff, 0x00, 0xff, 0xff,
  0xff, 0x74, 0x75, 0x73, 0x72, 0x80, 0x80, 0x76, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x64, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x14, 0x13, 0x67, 0x66, 0x61, 0x65, 0x60, 0x70,
  0x63, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x62, 0x01, 0x02, 0x80, 0x05, 0x80, 0x71,
  0x15, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff,
  0xff, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
  0x30, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x17, 0x16, 0x80, 0x80, 0x80, 0x80, 0xff
};

unsigned char ak_tbl_s_700[] =
{
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x76, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xff, 0x60, 0xff,
  0xff, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
  0x30, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x17, 0x16, 0x50, 0x67, 0x62, 0x14, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x13, 0x71, 0x66, 0xff
};

unsigned char *ak_tbl;
unsigned char *ak_tbl_s;

#define MAX_PATH 256
char PROGRAM_PATH[MAX_PATH];
char FDROM_PATH[MAX_PATH];
char MONROM_PATH[MAX_PATH];
char CGROM_PATH[MAX_PATH];

extern uint16_t c_bright;

char serialKeyCode;

void selectTape();
void selectRom();
void sendCommand();
void sendBreakCommand();
void setWiFi();
void deleteWiFi();
void soundSetting();
void PCGSetting();
void doSendCommand(String inputString);
int set_mztype(void);

boolean sendBreakFlag;

String inputStringEx;

bool suspendScrnThreadFlag;

MZ_CONFIG mzConfig;

//------------------------------------------------
// Memory Allocation for MZ
//------------------------------------------------
int mz_alloc_mem(void)
{
	int result = 0;

	/* Main Memory */
//mem = (UINT8*)ps_malloc(64*1024);
mem = (UINT8*)ps_malloc((4+6+4+64)*1024);
 
	if(mem == NULL)
	{
		return -1;
	}

	/* Junk(Dummy) Memory */
	junk = (UINT8*)ps_malloc(4096);
	if(junk == NULL)
	{
		return -1;
	}

	/* MZT file Memory */
	//mzt_buf = (UINT32*)ps_malloc(4*64*1024);
  mzt_buf = (UINT32*)ps_malloc(16*64*1024);
	if(mzt_buf == NULL)
	{
		return -1;
	}

	/* ROM FONT */
	mz_font = (uint8_t*)ps_malloc(ROMFONT_SIZE);
	if(mz_font == NULL)
	{
		result = -1;
	}

	/* PCG-700 FONT */
	pcg700_font = (uint8_t*)ps_malloc(PCG700_SIZE);
	if(pcg700_font == NULL)
	{
		result = -1;
	}

	return result;
}

//------------------------------------------------
// Release Memory for MZ
//------------------------------------------------
void mz_free_mem(void)
{
	if(pcg700_font)
	{
		free(pcg700_font);
	}

	if(mz_font)
	{
		free(mz_font);
	}

	if(mzt_buf)
	{
		free(mzt_buf);
	}

	if(junk)
	{
		free(junk);
	}

	if(mem)
	{
		free(mem);
	}
}

//--------------------------------------------------------------
// ＲＯＭモニタを読み込む
//--------------------------------------------------------------
void monrom_load(void)
{
  Serial.println("mzConfig.romFile");
  Serial.println(mzConfig.romFile);
  String romFile = mzConfig.romFile;
  if(romFile.length() == 0){
    romFile = DEFAULT_ROM_FILE;
  }
  String romPath = String(ROM_DIRECTORY) + "/" + romFile;
  Serial.println("ROM PATH:" + romPath);

  if(SD.exists(romPath)==false){
    M5.Lcd.println("ROM FILE NOT EXIST");
    M5.Lcd.printf("[%s]", romPath);
  }

  File dataFile = SD.open(romPath, FILE_READ);

  int offset = 0;
  if (dataFile) {
    while (dataFile.available()) {
      *(mem + offset) = dataFile.read();
      offset++;
    }
    dataFile.close();
    M5.Lcd.print("ROM FILE:");
    M5.Lcd.println(romPath);
    Serial.println(":END READ ROM:" + romPath);
  }else{
    M5.Lcd.print("ROM FILE FAIL:");
    M5.Lcd.println(romPath);
    Serial.println("FAIL END READ ROM:" + romPath);
  }
  mzConfig.mzMode = set_mztype();
  Serial.print("MZ TYPE=");
  Serial.println(mzConfig.mzMode);
  
}

int set_mztype(void){
  if (!strncmp((const char*)(mem + 0x6F3),"1Z-009",5))
  {
    return MZMODE_700;
  }
  else
  if (!strncmp((const char*)(mem + 0x6F3),"1Z-013",5))
  {
    return MZMODE_700;
  }
  else
  if (!strncmp((const char*)(mem + 0x142),"MZ\x90\x37\x30\x30",6))
  {
    return MZMODE_700;
  }
  return MZMODE_80;
}

//--------------------------------------------------------------
// ＭＺのモニタＲＯＭのセットアップ
//--------------------------------------------------------------
void mz_mon_setup(void)
{
  
  //memset(mem, 0xFF, 64*1024);
  memset(mem+VID_START, 0, 4*1024);
  memset(mem+RAM_START, 0, 64*1024);

}

//--------------------------------------------------------------
// シグナル関連
//--------------------------------------------------------------
void sighandler(int act)
{
	intFlag = true;
}

bool intByUser(void)
{
	return intFlag;
}

void mz_exit(int arg)
{
	sighandler(0);
}

//--------------------------------------------------------------
// メイン部
//--------------------------------------------------------------
int mz80c_main()
{
  delay(500);

  Serial.println("M5Z-700 START");
  M5.Lcd.println("M5Z-700 START");

  suspendScrnThreadFlag = false;

  c_bright = GREEN;
  serialKeyCode = 0;

  sendBreakFlag = false;

  pinMode(buttonApin, INPUT_PULLUP);
  pinMode(buttonBpin, INPUT_PULLUP);
  joyPadPushed_U = false;
  joyPadPushed_D = false;
  joyPadPushed_L = false;
  joyPadPushed_R = false;
  joyPadPushed_A = false;
  joyPadPushed_B = false;
  joyPadPushed_Press = false;
  enableJoyPadButton = false;
 
  mz_screen_init();
	mz_alloc_mem();

	makePWM();
 
	// ＭＺのモニタのセットアップ
	mz_mon_setup();

	// メインループ実行
	mainloop();

  delay(1000);
 
	mz_free_mem();
	mz_screen_finish();

	return 0;
}

////-------------------------------------------------------------
////  mz700win MAINLOOP
////-------------------------------------------------------------
void mainloop(void)
{
	long synctime = SyncTime;
	long timeTmp;
  long syncTmp;

	//mzbeep_init(44100);
  monrom_load();
  font_load("");
  mz_reset();

	setup_cpuspeed(1);
	Z80_IRQ = 0;
//	Z80_Trap = 0x0556;

// start the CPU emulation

  Serial.println("START READ TAPE!");
  String message = "Tape Setting";
  updateStatusArea(message.c_str());

  if(strlen(mzConfig.tapeFile) == 0){
    set_mztData(DEFAULT_TAPE_FILE);
  }else{
    set_mztData(mzConfig.tapeFile);
  }
  Serial.println("END READ TAPE!");

  updateStatusArea("");

  if(mzConfig.mzMode == MZMODE_80){
    ak_tbl = ak_tbl_80c;
    ak_tbl_s = ak_tbl_s_80c;
  }else{
    ak_tbl = ak_tbl_700;
    ak_tbl_s = ak_tbl_s_700;
  }
  
  /* スレッド　開始 */
  Serial.println("START_THREAD");
  start_thread();

  delay(100);

  while(!intByUser())
	{
		timeTmp = millis();
		if (!Z80_Execute()) break;

    M5.update();
    if(M5.BtnA.wasPressed()){
      if(ts700.mzt_settape != 0 && ts700.cmt_play == 0)
        {
          ts700.cmt_play = 1;
          ts700.mzt_start = ts700.cpu_tstates;
          ts700.cmt_tstates = 1;
          setup_cpuspeed(5);
          sysst.motor = 1;
          xferFlag |= SYST_MOTOR;
         }else{
            String message = "TAPE CAN'T START";
            updateStatusArea(message.c_str());
            delay(2000);
            Serial.println("TAPE CAN'T START");
         }
    }

    if(M5.BtnB.wasReleased()){
        suspendScrnThreadFlag = true;
        delay(100);
        selectMzt();
        delay(100);
        suspendScrnThreadFlag = false;
    }
    if(M5.BtnC.wasPressed()){ 
      //メニュー表示
      suspendScrnThreadFlag = true;
      delay(100);
      systemMenu();
      delay(100);
      suspendScrnThreadFlag = false;
    }
    syncTmp = millis();
    if(synctime - (syncTmp - timeTmp) > 0){
      delay(synctime - (syncTmp - timeTmp));
    }else{
      delay(1);
    }
	}
}

//------------------------------------------------------------
// CPU速度を設定 (10-100)
//------------------------------------------------------------
void setup_cpuspeed(int mul) {
	int _iperiod;

	_iperiod = (CPU_SPEED*CpuSpeed*mul)/(100*IFreq);

	_iperiod *= 256;
	_iperiod >>= 8;

	Z80_IPeriod = _iperiod;
	Z80_ICount = _iperiod;

}

//--------------------------------------------------------------
// スレッドの準備
//--------------------------------------------------------------
int create_thread(void)
{
	return 0;
}

//--------------------------------------------------------------
// スレッドの開始
//--------------------------------------------------------------
void start_thread(void)
{
    xTaskCreatePinnedToCore(
                    scrn_thread,     /* Function to implement the task */
                    "scrn_thread",   /* Name of the task */
                    4096,      /* Stack size in words */
                    NULL,      /* Task input parameter */
                    1,         /* Priority of the task */
                    NULL,      /* Task handle. */
                    1);        /* Core where the task should run */
    
    xTaskCreatePinnedToCore(
                    keyin_thread,     /* Function to implement the task */
                    "keyin_thread",   /* Name of the task */
                    4096,      /* Stack size in words */
                    NULL,      /* Task input parameter */
                    1,         /* Priority of the task */
                    NULL,      /* Task handle. */
                    0);        /* Core where the task should run */        
   
}

//--------------------------------------------------------------
// スレッドの後始末
//--------------------------------------------------------------
int end_thread(void)
{
	return 0;
}

void suspendScrnThread(bool flag){
  suspendScrnThreadFlag = flag;
  delay(100);
}

//--------------------------------------------------------------
// 画面描画スレッド 
//--------------------------------------------------------------
void scrn_thread(void *arg)
{
  //long synctime = 17;// 60fps
  long synctime = 50;// 20fps
  long timeTmp;
  long vsyncTmp;

	while(!intByUser())
	{
    if(suspendScrnThreadFlag == false){
  		// 画面更新処理
  		hw700.retrace = 1;											/* retrace = 0 : in v-blnk */
  		vblnk_start();
      timeTmp = millis();
  		
  		update_scrn();												/* 画面描画 */
      vsyncTmp = millis();
      
      if(synctime - (vsyncTmp - timeTmp) > 0){
        delay(synctime - (vsyncTmp - timeTmp));
      }else{
        delay(1);
      }
    }else{
       delay(synctime);
    }
	}
	return;// NULL;
}

//--------------------------------------------------------------
// キーボード入力スレッド
//--------------------------------------------------------------
void keyin_thread(void *arg)
{
  char inKeyCode = 0;
 
  while(!intByUser())
  {
    if(suspendScrnThreadFlag == false){

      if(sendBreakFlag == true){ //SHIFT+BREAK送信
        mz_keydown_sub(0x80);
        delay(60);
        if(mzConfig.mzMode == MZMODE_700){
          mz_keydown_sub(0x87);
          delay(120);
          mz_keyup_sub(0x87);
        }else{
          mz_keydown_sub(ak_tbl_s[3]);
          delay(120);
          mz_keyup_sub(ak_tbl_s[3]);
        }
        
        mz_keyup_sub(0x80);
        delay(60);
        sendBreakFlag = false;
      }
      
      if(inputStringEx.length() > 0){
        doSendCommand(inputStringEx);
        inputStringEx = "";
      }
      
      if (Serial.available() > 0) {
        serialKeyCode = Serial.read();
        Serial.println(serialKeyCode);
        //Special Key
        if( serialKeyCode == 27 ){ //ESC
          serialKeyCode = Serial.read();
          if(serialKeyCode == 91){
            serialKeyCode = Serial.read();
            switch(serialKeyCode){
              case 65:
                serialKeyCode = 0x12;  //UP
                break;
              case 66:
                serialKeyCode = 0x11;  //DOWN
                break;
              case 67:
                serialKeyCode = 0x13;  //RIGHT
                break;
              case 68:
                serialKeyCode = 0x14;  //LEFT
                break;
              case 49:
                serialKeyCode = 0x15;  //HOME
                break;
              case 52:
                serialKeyCode = 0x16;  //END -> CLR
                break;
              case 50:
                serialKeyCode = 0x18;  //INST
                break;
              default:
                serialKeyCode = 0;
            }
          }else if(serialKeyCode == 255)
          {
            //Serial.println("ESC");
            //serialKeyCode = 0x03;  //ESC -> SHIFT + BREAK 
            sendBreakFlag = true; //うまくキーコード処理できなかったのでWebからのBreak送信扱いにします。
            serialKeyCode = 0;
          }
        }
        if(serialKeyCode == 127){ //BackSpace
          serialKeyCode = 0x17;
        }
        while(Serial.available() > 0 && Serial.read() != -1);
      }
      
      inKeyCode = serialKeyCode;
      serialKeyCode = 0;
      if (inKeyCode != 0) {
        if(ak_tbl[inKeyCode] == 0xff)
        {
          //何もしない
        }
        else if(ak_tbl[inKeyCode] == 0x80)
        {
              mz_keydown_sub(ak_tbl[inKeyCode]);
              delay(60);
              mz_keydown_sub(ak_tbl_s[inKeyCode]);
              delay(60);
              mz_keyup_sub(ak_tbl_s[inKeyCode]);
              mz_keyup_sub(ak_tbl[inKeyCode]);
              delay(60);
        }
        else
        {
              mz_keydown_sub(ak_tbl[inKeyCode]);
              delay(60);
              mz_keyup_sub(ak_tbl[inKeyCode]);
              delay(60);
        }
      
      }
      checkI2cKeyboard();
      checkJoyPad();
    }
    delay(10);
  }
	return;
}

//--------------------------------------------------------------
// 自動文字列入力
//--------------------------------------------------------------
void doSendCommand(String inputString){
  for(int i = 0;i < inputString.length();i++){
    char inKeyCode = inputString.charAt(i);
    if (inKeyCode != 0) {
      if(ak_tbl[inKeyCode] == 0xff)
      {
        //何もしない
        delay(100);
      }
      else if(ak_tbl[inKeyCode] == 0x80)
      {
        mz_keydown_sub(ak_tbl[inKeyCode]);
        delay(200);
        mz_keydown_sub(ak_tbl_s[inKeyCode]);
        delay(200);
        mz_keyup_sub(ak_tbl_s[inKeyCode]);
        mz_keyup_sub(ak_tbl[inKeyCode]);
        delay(200);
      }
      else
      {
        mz_keydown_sub(ak_tbl[inKeyCode]);
        delay(200);
        mz_keyup_sub(ak_tbl[inKeyCode]);
        delay(200);
      }
    }
    delay(200);
  }
  delay(10);
  //最後に改行コード
  mz_keydown_sub(ak_tbl[13]);
  delay(200);
  mz_keyup_sub(ak_tbl[13]);
}


//--------------------------------------------------------------
// JoyPadLogic
//--------------------------------------------------------------

void checkJoyPad(){
  if(mzConfig.mzMode != MZMODE_700){
    return;
  }
  
  int joyX,joyY,joyPress,buttonA,buttonB;
  joyX = 0;

  if(Wire.requestFrom(0x52,3) >= 3){
      if(Wire.available()){joyX = Wire.read();}//X（ジョイパッド的には Y）
      if(Wire.available()){joyY = Wire.read();}//Y（ジョイパッド的には X）
      if(Wire.available()){joyPress = Wire.read();}//Press
  }
  

  if(joyX == 0){
    return; //接続されていない
  }
  
  if (digitalRead(buttonApin) == LOW)
  {
    buttonA = 1;
  }else{
    buttonA = 0;
  }
  if (digitalRead(buttonBpin) == LOW)
  {
    buttonB = 1;
  }else{
    buttonB = 0;
  }
  
  if(enableJoyPadButton == false){
    if(buttonA == 1){ //ボタン接続されていなければボタンAが0、Bが1のままなので、1度Aが1になれば、以降はボタン接続アリとする。
      enableJoyPadButton = true;
    }else{
      //接続されていない場合は押されてない扱いとする。
      buttonA = 0;
      buttonB = 0;
    }
  }

  if(joyPadPushed_U == false && joyX < 80){
    //カーソル上を押す
    mz_keydown_sub(0x75);
    joyPadPushed_U = true;
    delay(60);
  }else if(joyPadPushed_U == true && joyX > 80){
    //カーソル上を戻す
    mz_keyup_sub(0x75);
    joyPadPushed_U = false;
  }
  if(joyPadPushed_D == false && joyX > 160){
    //カーソル下を押す
    mz_keydown_sub(0x74);
    joyPadPushed_D  =true;
    delay(60);
  }else if(joyPadPushed_D == true && joyX < 160){
    //カーソル下を戻す
    mz_keyup_sub(0x74);
    joyPadPushed_D = false;
  }
  if(joyPadPushed_L == false && joyY < 80){
    //カーソル左を押す
    mz_keydown_sub(0x72);
    joyPadPushed_L = true;
    delay(60);
  }else if(joyPadPushed_L == true && joyY > 80){
    //カーソル左を戻す
    mz_keyup_sub(0x72);
    joyPadPushed_L = false;
  }
  if(joyPadPushed_R == false && joyY > 160){
    //カーソル右を押す
    mz_keydown_sub(0x73);
    joyPadPushed_R = true;
    delay(60);
  }else if(joyPadPushed_R == true && joyY < 160){
    //カーソル右を戻す
    mz_keyup_sub(0x73);
    joyPadPushed_R = false;
  }
  if(joyPadPushed_A == false && buttonA == 1){
    //CTRL & SPACE & 1 押下して戻す
    mz_keydown_sub(0x64);
    delay(60);
    mz_keyup_sub(0x64);
    delay(10);
    mz_keydown_sub(0x86);
    delay(60);
    mz_keyup_sub(0x86);
    delay(10);
    mz_keydown_sub(0x57);
    delay(60);
    mz_keyup_sub(0x57);
    //joyPadPushed_A = true;
  }else if(joyPadPushed_R == true && buttonA == 0){
    //スペースを戻す
    //mz_keyup_sub(0x64);
    //delay(10);
    //mz_keyup_sub(0x86);
    joyPadPushed_A = false;
  }
  if(joyPadPushed_B == false && buttonB == 1){
    //SHIFT & 3 押下して戻す
    mz_keydown_sub(0x80);
    delay(60);
    mz_keyup_sub(0x80);
    delay(10);
    mz_keydown_sub(0x55);
    delay(60);
    mz_keyup_sub(0x55);
    //joyPadPushed_B = true;
  }else if(joyPadPushed_B == true && buttonB == 0){
    //SHIFTを戻す
    //mz_keyup_sub(0x80);
    //joyPadPushed_B = false;
  }
  if(joyPadPushed_Press == false && joyPress == 1){
    //CR&S押下
    mz_keydown_sub(0x00);
    delay(60);
    mz_keyup_sub(0x00);
    delay(10);
    mz_keydown_sub(0x25);
    delay(60);
    mz_keyup_sub(0x25);
    joyPadPushed_Press = true;

  }else if(joyPadPushed_Press == true && joyPress == 0){
    //CRを戻す
    mz_keyup_sub(0x00);
    delay(10);
    mz_keyup_sub(0x25);
    joyPadPushed_Press = false;
  }
}
//--------------------------------------------------------------
// I2C Keyboard Logic
//--------------------------------------------------------------
void checkI2cKeyboard(){
uint8_t i2cKeyCode = 0;  
  if(Wire.requestFrom(CARDKB_ADDR, 1)){  // request 1 byte from keyboard
    while (Wire.available()) {
      i2cKeyCode = Wire.read();                  // receive a byte as 
      break;
    }
  }
  if(i2cKeyCode == 0){
    return;
  }

  Serial.println(i2cKeyCode, HEX);

  //特殊キー
  switch(i2cKeyCode){
    case 0xB5:
      i2cKeyCode = 0x12;  //UP
      break;
    case 0xB6:
      i2cKeyCode = 0x11;  //DOWN
      break;
    case 0xB7:
      i2cKeyCode = 0x13;  //RIGHT
      break;
    case 0xB4:
      i2cKeyCode = 0x14;  //LEFT
      break;
    case 0x99: //Fn + UP
      i2cKeyCode = 0x15;  //HOME
      break;
    case 0xA4: //Fn + Down
      i2cKeyCode = 0x16;  //END -> CLR
      break;
    case 0xA5:  //Fn + Right
      i2cKeyCode = 0x18;  //INST
      break;
    case 0x08: //BS
      i2cKeyCode = 0x17;
      break;
    case 0x8C: //Fn + Tab
      i2cKeyCode = 0x0A; //記号モード 0x0A に記号ボタンを割り当てている。（TABには英数ボタン割り当て。）
      break;
    case 0x8B: //Fn + BS
      i2cKeyCode = 0x0B; //かなモード 0x0B にかなボタンを割り当てている。
      break;
    case 0x1B: //ESC
      sendBreakFlag = true; //うまくキーコード処理できなかったのでWebからのBreak送信扱いにします。
      return;              
    default:
      break;
  }

  if(ak_tbl[i2cKeyCode] == 0xff)
  {
        //何もしない
  }
  else if(ak_tbl[i2cKeyCode] == 0x80)
  {
    mz_keydown_sub(ak_tbl[i2cKeyCode]);
    delay(60);
    mz_keydown_sub(ak_tbl_s[i2cKeyCode]);
    delay(60);
    mz_keyup_sub(ak_tbl_s[i2cKeyCode]);
    mz_keyup_sub(ak_tbl[i2cKeyCode]);
    delay(60);
  }
  else
  {
    mz_keydown_sub(ak_tbl[i2cKeyCode]);
    delay(60);
    mz_keyup_sub(ak_tbl[i2cKeyCode]);
    delay(60);
  } 
  
}

String selectMzt(){
  File fileRoot;
  String fileList[MAX_FILES];

  M5.Lcd.fillScreen(TFT_BLACK);
	M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
    
  String fileDir = TAPE_DIRECTORY;
  if (fileDir.endsWith("/") == true)
  {
    fileDir = fileDir.substring(0, fileDir.length() - 1);
  }

	if(SD.exists(fileDir)==false){
		M5.Lcd.println("DIR NOT EXIST");
		M5.Lcd.printf("[SD:%s/]",fileDir.c_str());
		delay(5000);
		M5.Lcd.fillScreen(TFT_BLACK);
		return "";
	}
    fileRoot = SD.open(fileDir);
    int fileListCount = 0;
    
    while (1)
    {
        File entry = fileRoot.openNextFile();
        if (!entry)
        { // no more files
            break;
        }
        //ファイルのみ取得
        if (!entry.isDirectory())
        {
            String fullFileName = entry.name();
            String fileName = fullFileName.substring(fullFileName.lastIndexOf("/") + 1);
            fileList[fileListCount] = fileName;
            fileListCount++;
            //Serial.println(fileName);
        }
        entry.close();
    }
    fileRoot.close();

    int startIndex = 0;
    int endIndex = startIndex + 10;
    if (endIndex > fileListCount)
    {
        endIndex = fileListCount;
    }

    sortList(fileList, fileListCount);

    boolean needRedraw = true;
    int selectIndex = 0;
	  int preStartIndex = 0;
    bool isLongPress = false;
    while (true)
    {
        if (needRedraw == true)
        {
            M5.Lcd.setCursor(0, 0);
            startIndex = selectIndex - 5;
            if (startIndex < 0)
            {
                startIndex = 0;
            }
            endIndex = startIndex + 12;
            if (endIndex > fileListCount)
            {
                endIndex = fileListCount;
                startIndex = endIndex - 12;
                if(startIndex < 0){
                    startIndex = 0;
                }
            }
			if(preStartIndex != startIndex){
            	M5.Lcd.fillRect(0,0,320,220,0);
				preStartIndex = startIndex;				
			}
            for (int index = startIndex; index < endIndex + 1; index++)
            {
                if (index == selectIndex)
                {
                    M5.Lcd.setTextColor(TFT_GREEN);
                }
                else
                {
                    M5.Lcd.setTextColor(TFT_WHITE);
                }
                if (index == 0)
                {
                    M5.Lcd.println("[BACK]");
                }
                else
                {
                    M5.Lcd.println(fileList[index - 1].substring(0,26));
                }
            }
            M5.Lcd.setTextColor(TFT_WHITE);

            M5.Lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
            M5.Lcd.drawCentreString("U P", 53, 240 - 17, 1);
            M5.Lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
            M5.Lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
            M5.Lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
            M5.Lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
            needRedraw = false;
        }
        M5.update();
        if (M5.BtnA.pressedFor(500)){
            isLongPress = true;
            selectIndex--;
            if (selectIndex < 0)
            {
                selectIndex = fileListCount;
            }
            needRedraw = true;
            delay(100);
        }
        if (M5.BtnA.wasReleased())
        {
            if(isLongPress == true)
            {
                isLongPress = false;
            }else{
                selectIndex--;
                if (selectIndex < 0)
                {
                    selectIndex = fileListCount;
                }
                needRedraw = true;
            }
        }
        if (M5.BtnC.pressedFor(500)){
            isLongPress = true;
            selectIndex++;
            if (selectIndex > fileListCount)
            {
                selectIndex = 0;
            }
            needRedraw = true;
            delay(100);
        }
        if (M5.BtnC.wasReleased())
        {
            if(isLongPress == true)
            {
                isLongPress = false;
            }else{
                selectIndex++;
                if (selectIndex > fileListCount)
                {
                    selectIndex = 0;
                }
                needRedraw = true;
            }
        }

        if (M5.BtnB.wasReleased())
        {
            if (selectIndex == 0)
            {
                //何もせず戻る
                M5.Lcd.fillScreen(TFT_BLACK);
                delay(10);
                return "";
            }
            else
            {
               Serial.print("select:");
               Serial.println(fileList[selectIndex-1]);
               delay(10);
               M5.Lcd.fillScreen(TFT_BLACK);

               strncpy(mzConfig.tapeFile, fileList[selectIndex-1].c_str(), 50);
               set_mztData(mzConfig.tapeFile);
               ts700.cmt_play = 0;
         
               M5.Lcd.setCursor(0,0);
               M5.Lcd.print("Set:");
               M5.Lcd.print(fileList[selectIndex -1]);
               delay(2000);
               M5.Lcd.fillScreen(TFT_BLACK);
               delay(10);
 				       return fileList[selectIndex - 1];
            }
        }
        delay(100);
    }
}    
 
#define MENU_ITEM_COUNT 8
#define PCG_INDEX 5
#define SOUNT_INDEX 6
void systemMenu()
{
  static String menuItem[] =
  {
   "[BACK]",
   "RESET:NEW MONITOR7",
   "RESET:NEW MONITOR",
   "RESET:MZ-1Z009",
   "RESET:SP-1002",
   "PCG",
   "SOUND",
   "INPUT \"LOAD(CR)\"",
   ""};

  delay(10);
  M5.Lcd.fillScreen(TFT_BLACK);
  delay(10);
  M5.Lcd.setTextSize(2);
  bool needRedraw = true;

  int menuItemCount = 0;
  while(menuItem[menuItemCount] != ""){
    menuItemCount++;
  }

  int selectIndex = 0;
  delay(100);
  M5.update();

  while (true)
  {
    if (needRedraw == true)
    {
      M5.Lcd.setCursor(0, 0);
      for (int index = 0; index < menuItemCount; index++)
      {
        if (index == selectIndex)
        {
          M5.Lcd.setTextColor(TFT_GREEN);
        }
        else
        {
          M5.Lcd.setTextColor(TFT_WHITE);
        }
        String curItem = menuItem[index];
        if(index == PCG_INDEX){
          curItem = curItem + ((hw700.pcg700_mode == 1)?String(": ON"):String(": OFF"));
        }
        if(index == SOUNT_INDEX){
          curItem = curItem + (mzConfig.enableSound?String(": ON"):String(": OFF"));
        }
          M5.Lcd.println(curItem);
        }
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
        M5.Lcd.drawCentreString("U P", 53, 240 - 17, 1);
        M5.Lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
        M5.Lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
        M5.Lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
        M5.Lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
        needRedraw = false;
      }
      M5.update();
      if (M5.BtnA.wasReleased())
      {
        selectIndex--;
        if (selectIndex < 0)
        {
          selectIndex = menuItemCount -1;
        }
        needRedraw = true;
      }

      if (M5.BtnC.wasReleased())
      {
        selectIndex++;
        if (selectIndex >= menuItemCount)
        {
          selectIndex = 0;
        }
        needRedraw = true;
      }

      if (M5.BtnB.wasReleased())
      {
        if (selectIndex == 0)
        {
          M5.Lcd.fillScreen(TFT_BLACK);
          delay(10);
          return;
        }
        switch (selectIndex)
        {
          case 1:
            strcpy(mzConfig.romFile, "NEWMON7.ROM");
            break;
          case 2:
            strcpy(mzConfig.romFile, "NEWMON.ROM");
            break;
          case 3:
            strcpy(mzConfig.romFile, "1Z009.ROM");
            break;
          case 4:
            strcpy(mzConfig.romFile, "SP-1002.ROM");
            break;
          case PCG_INDEX:
            hw700.pcg700_mode = (hw700.pcg700_mode == 1)?0:1;
            break;
          case SOUNT_INDEX:
            mzConfig.enableSound = mzConfig.enableSound?false:true;
            break;
          case 7:
                inputStringEx = "load";
                M5.Lcd.fillScreen(TFT_BLACK);
                delay(10);
                return;

          default:
                M5.Lcd.fillScreen(TFT_BLACK);
                delay(10);
                return;
        }
        if(selectIndex >=1 && selectIndex <= 4){
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setCursor(0, 0);
            M5.Lcd.print("Reset MZ:");
            M5.Lcd.println(mzConfig.romFile);
            delay(2000);
            monrom_load();
            mz_reset();
            return;
        }
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setCursor(0, 0);
        needRedraw = true;
      }
      delay(100);
    }
}

//https://github.com/tobozo/M5Stack-SD-Updater/blob/master/examples/M5Stack-SD-Menu/M5Stack-SD-Menu.ino
void sortList(String fileList[], int fileListCount) { 
  bool swapped;
  String temp;
  String name1, name2;
  do {
    swapped = false;
    for(int i = 0; i < fileListCount-1; i++ ) {
      name1 = fileList[i];
      name1.toUpperCase();
      name2 = fileList[i+1];
      name2.toUpperCase();
      if (name1.compareTo(name2) > 0) {
        temp = fileList[i];
        fileList[i] = fileList[i+1];
        fileList[i+1] = temp;
        swapped = true;
      }
    }
  } while (swapped);
}
