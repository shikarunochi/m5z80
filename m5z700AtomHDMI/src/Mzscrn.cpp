//----------------------------------------------------------------------------
// File:MZscrn.cpp
// MZ-700 Emulator m5z700 for M5Stack(with SRAM)
// m5z700:VRAM/CRT Emulation Module based on mz80rpi
// modified by @shikarunochi
//
// mz80rpi by Nibble Lab./Oh!Ishi, based on MZ700WIN
// (c) Nibbles Lab./Oh!Ishi 2017
//----------------------------------------------------------------------------
#if defined(_M5STICKCPLUS)
#include <M5GFX.h>
#include <M5StickCPlus.h>
#elif defined(_M5ATOMS3)||defined(_M5ATOMS3LITE)||defined(_TOOTHBRUSH)
#include <M5Unified.h>
#elif defined(_M5STACK)
#include <M5Stack.h>
#elif defined(_M5CARDPUTER)
#include <M5Cardputer.h>
#else
#include <M5Atom.h>  
#endif

#include "FS.h"
#include <SPIFFS.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
//#include <linux/fb.h>
//#include <linux/fs.h>
//#include <sys/mman.h>
//#include <sys/ioctl.h>
#include <stdbool.h>
#include "m5z700.h"

#include "mz700lgfx.h"

//
#include "z80.h"
//
#include "mzmain.h"
#include "MZhw.h"
#include "mzscrn.h"

uint16_t c_bright;
#define c_dark BLACK

//フレームバッファ
//static LGFX_Sprite fb(&m5lcd); // スプライトを使う場合はLGFX_Spriteのインスタンスを作成。
//TFT_eSprite fb = TFT_eSprite(&M5.Lcd);
#ifdef USE_EXT_LCD
#include "mzfont6x8.h"
LGFX_Sprite canvas(&m5lcd);
#endif

#if defined(_M5CARDPUTER)
#include <SD.h>
#endif

#if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5CARDPUTER)
#include "mzfont6x8.h"
M5Canvas canvas(&m5lcd);
#endif
#if defined(_M5STACK)
M5Canvas canvas(&m5lcd);
#endif


int fontOffset = 0;
int fgColor = 0;
int bgColor = 0;
int fgColorIndex = 0;
int bgColorIndex = 0;
int MZ700_COLOR[] = {BLACK, BLUE, RED, MAGENTA, GREEN, CYAN, YELLOW, WHITE};

int lcdMode = 0;
//int lcdRotate = 0; move to m5z700AtomHDMI.ino

boolean needScreenUpdateFlag;
boolean screnUpdateValidFlag;

String statusAreaMessage;

void update_scrn_thread(void *pvParameters);

/*
   表示画面の初期化
*/
int mz_screen_init(void)
{
  needScreenUpdateFlag = false;
  screnUpdateValidFlag = false;

  #ifdef USE_EXT_LCD
  canvas.setColorDepth(8);
  canvas.setTextSize(1);
  canvas.createSprite(40,240); //メモリ足りないので縦40ドット（=5行）に分割して5回に分けて描画する。8x6フォントを90度横向きで描画した後、90度回転させてpushする。
  #endif
  #if defined(_M5STICKCPLUS)||defined(_M5CARDPUTER)
  m5lcd.setRotation(1);
  canvas.setColorDepth(8);
  canvas.setTextSize(1);
  canvas.createSprite(40,240); //メモリ足りないので縦40ドット（=5行）に分割して5回に分けて描画する。8x6フォントを90度横向きで描画した後、90度回転させてpushする。
  #endif
  #if defined(_M5ATOMS3)
  canvas.createSprite(40,240); //メモリ足りないので縦40ドット（=5行）に分割して5回に分けて描画する。8x6フォントを90度横向きで描画した後、90度回転させてpushする。
  #endif
  #if defined(_M5STACK)
  canvas.setColorDepth(8);
  canvas.setTextSize(1);
  canvas.createSprite(320,40); //メモリ足りないので縦40ドット（=5行）に分割して5回に分けて描画する。
  #endif


  statusAreaMessage = "";

  xTaskCreatePinnedToCore(update_scrn_thread, "update_scrn_thread", 4096, NULL, 1, NULL, 0);
  screnUpdateValidFlag = true;
  return 0;
}

/*
   画面関係の終了処理
*/
void mz_screen_finish(void)
{
}

void set_scren_update_valid_flag(boolean flag){
  screnUpdateValidFlag = flag;
}
/*
   フォントデータを描画用に展開
*/
int font_load(const char *fontfile)
{
  #if defined (USE_EXT_LCD)||defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5CARDPUTER)
    Serial.println("USE INTERNAL FONT DATA");
    return 0;
  #endif
  FILE *fdfont;
  int dcode, line, bit;
  UINT8 lineData;
  uint16_t color;

  String romDir = ROM_DIRECTORY;
  String fontFile = DEFAULT_FONT_FILE;
  #if defined(_M5STACK)||defined(_M5CARDPUTER)
  File dataFile = SD.open(romDir + "/" + fontFile, FILE_READ);
  #else
  File dataFile = SPIFFS.open(romDir + "/" + fontFile, FILE_READ);
  #endif
  if (!dataFile) {
    Serial.println("FONT FILE NOT FOUND");
    perror("Open font file");
    return -1;
  }

  for (dcode = 0; dcode < 256 * 2; dcode++) //拡張フォントも読み込む
  {
    for (line = 0; line < 8; line++)
    {
      if (dataFile.available()) {
        lineData = dataFile.read();
        mz_font[dcode * 8 + line] = lineData;
        if (dcode >= 128 && dcode < 256 ) { //通常フォント後半の場合PCG初期値として設定する
          pcg700_font[(dcode & 0x7F) * 8 + line] = lineData;
          pcg700_font[(dcode) * 8 + line] = lineData;
        }
      }
    }
    delay(10);
  }
  Serial.println("END READ ROM");

  dataFile.close();
  return 0;
}
/**********************/
/* redraw the screen */
/*********************/
void update_scrn(void) {
  needScreenUpdateFlag = true;
}


void update_scrn_thread(void *pvParameters)
{
  while (1) {
    if (needScreenUpdateFlag == true && screnUpdateValidFlag == true) {
      //m5lcd.startWrite();
      needScreenUpdateFlag = false;
      UINT8 ch;
      UINT8 chAttr;
      UINT8 lineData;
      uint8_t *fontPtr;
      //	struct timespec h_start, h_end, h_split;
      //uint32_t ptr = 0;//YOFFSET;
      int ptr = 0;//YOFFSET;
      //	int32_t hwait;
      //	int32_t htime = 62000;	// 64us
      uint16_t blankdat[8] = {0, 0, 0, 0, 0, 0, 0, 0};
      int color;
      hw700.retrace = 1;

      hw700.cursor_cou++;
      if (hw700.cursor_cou >= 40)													/* for japan.1/60 */
      {
        hw700.cursor_cou = 0;
      }
      /*
        int screenY = 0;
        for(int cy = 0; cy < 1000; cy+=40)
        {
      	for(int cl = 0; cl < 8; cl++)
      	{
        //			clock_gettime(CLOCK_MONOTONIC_RAW, &h_start);
      		for(int cx = 0; cx < 40; cx++)
      		{
      			ch = mem[VID_START + cx + cy];
      			if(hw700.vgate == 0)
      			{
      			//	//memcpy(fbptr + ptr, blankdat, sizeof(uint16_t) * 8);
              // //fb.drawBitmap(0,ptr,8,8, 0);
      			}
      			else
      			{
      				//memcpy(fbptr + ptr, (hw700.pcg700_mode == 0) ? mz_font[ch * 8 + cl] : pcg700_font[ch * 8 + cl], sizeof(uint16_t) * 8);
               lineData = (hw700.pcg700_mode == 0) ? mz_font[ch * 8 + cl] : pcg700_font[ch * 8 + cl];
               //for(int bit = 0;bit < 8;bit++){
               //   color = (lineData & 0x80) == 0 ? c_dark : c_bright;
               //   fb.drawPixel(cx * 8 + bit, screenY * 8 + cl,color);
               //   lineData <<= 1;
               //}
      			}
      		}
        }
      */
      fontOffset = 0;
      fgColor = 0;
      bgColor = 0;
      fgColorIndex = 0;
      bgColorIndex = 0;
      #if defined (USE_EXT_LCD)||defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5STACK)||defined(_M5CARDPUTER)
      int drawIndex = 0;
      #endif
      m5lcd.startWrite();
      for (int cy = 0; cy < 25; cy++) {
        for (int cx = 0; cx < 40; cx++) {
          ch = mem[VID_START + cx + cy * 40];
          if (mzConfig.mzMode == MZMODE_700) {
            chAttr = mem[VID_START + cx + cy * 40 + 0x800];
            if ((chAttr & 0x80) == 0x80) { //MZ-700キャラクタセット
              fontOffset = 0x100;
            } else {
              fontOffset = 0;
            }
            //Color
            fgColorIndex = ((chAttr >> 4) & 0x07) ;
            bgColorIndex = (chAttr & 0x07);

            if (fgColorIndex <= 7) {
              fgColor = MZ700_COLOR[fgColorIndex];
            } else {
              fgColor = MZ700_COLOR[7];
              Serial.print("fgColorIndexError:");
              Serial.print(fgColorIndex);
            }
            if (bgColorIndex <= 7) {
              bgColor = MZ700_COLOR[bgColorIndex];
            } else {
              fgColor = MZ700_COLOR[0];
              Serial.print("bgColorIndexError:");
              Serial.print(bgColorIndex);
            }
          } else {
            fgColor = c_bright;
            bgColor = c_dark;
          }
          //fb.drawBitmap(cx * 8, cy * 8, (const uint8_t *)((hw700.pcg700_mode == 0) ? mz_font[ch * 8] : pcg700_font[ch * 8]), 8, 8, fgColor);
          #if defined (USE_EXT_LCD)||defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5CARDPUTER)
           canvas.fillRect((cy * 8) % 40, cx * 6, 8, 6, bgColor);
           fontPtr = &mz_font_6x8[(ch + fontOffset) * 6]; 
          #else
            canvas.fillRect(cx * 8, (cy * 8) % 40, 8, 8, bgColor);
          if (hw700.pcg700_mode == 0 || !(ch & 0x80)) {
              fontPtr = &mz_font[(ch + fontOffset) * 8];
          } else {
            if ((chAttr & 0x80) != 0x80) {
              fontPtr = &pcg700_font[(ch & 0x7F) * 8];
            } else {
              fontPtr = &pcg700_font[(ch) * 8];
            }
          }
          #endif
          #if defined (USE_EXT_LCD)||defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5CARDPUTER)
            canvas.drawBitmap((cy * 8) % 40, cx * 6, fontPtr, 8, 6, fgColor);
          #else
           canvas.drawBitmap(cx * 8, (cy * 8) %40 , fontPtr, 8, 8, fgColor);
          #endif
        }
        #if defined (USE_EXT_LCD) 
        #if defined(USE_ST7735S) //USE_ST7735S 160x128液晶
                if((cy+1)%5 == 0){
          //6x8ドットフォント使用＆縦横方向60%縮小
          //真ん中のドットが標準位置になるため、わざと奇数ドットになるように縮小する
          //canvas.pushRotateZoomWithAA(120*0.55 + 10, (40 * drawIndex)*0.55+ 20,90,0.5583,-0.575); 
          //canvas.pushRotateZoomWithAA(120*0.55, (40 * drawIndex)*0.55+ 20,90,0.55,-0.60); 

          //pushAffineWithAAを使えば、中心位置指定ではないのでキレイに表示できる。
          float matrix[6] = {0,   // 横倍
                     0.60,  // 横傾き
                    0, // X座標
                     0.55,   // 縦傾き
                    0,   // 縦倍
                     (40 * drawIndex)*0.55 + 7  // Y座標
                    };
          canvas.pushAffineWithAA(matrix);

          drawIndex = drawIndex + 1;
        }
        #elif defined(USE_GC9107)
        if((cy+1)%5 == 0){ //128x128
          //6x8ドットフォント使用＆縦横方向60%縮小
          //pushAffineWithAAを使えば、中心位置指定ではないのでキレイに表示できる。
          float matrix[6] = {0,   // 横倍
                     0.55,  // 横傾き
                    0, // X座標
                     0.50,   // 縦傾き
                    0,   // 縦倍
                     (40 * drawIndex)*0.50 + 7  // Y座標
                    };
          canvas.pushAffineWithAA(matrix);
          drawIndex = drawIndex + 1;
        }
        #else //外部液晶＆240x240の場合、縮小せずに表示
        if((cy+1)%5 == 0){
          if(lcdRotate == 0){
            canvas.pushRotateZoom(120, 40 * drawIndex + 20,90,1,-1); 
          }else{
            //canvas.pushRotateZoomWithAA(120, (40 * drawIndex + 40)*0.85,90,0.85,-1); //表示位置の 微調整は、各自のLCDに合わせてください
            canvas.pushRotateZoom(120, 40 * drawIndex + 20,90,1,-1); 
          }
          drawIndex = drawIndex + 1;
        }
        #endif
        #elif defined(_M5STICKCPLUS)||defined(_M5CARDPUTER)
        if((cy+1)%5 == 0){
          //6x8ドットフォント使用＆縦方向70%縮小
          //canvas.pushRotateZoomWithAA(120, (40 * drawIndex + 20)*0.7,90,0.7,-1); 
          canvas.pushRotateZoomWithAA(120, (40 * drawIndex + 20)*0.67,90,0.67,-1); 
          //canvas.pushRotateZoom(120, (40 * drawIndex + 20)*0.67,90,0.67,-1); 
          drawIndex = drawIndex + 1;
        }
        #elif defined(_M5ATOMS3)
        if((cy+1)%5 == 0){
          //6x8ドットフォント使用＆縦横方向60%縮小
          //真ん中のドットが標準位置になるため、わざと奇数ドットになるように縮小する
          //canvas.pushRotateZoomWithAA(120*0.55, (40 * drawIndex)*0.55+ 20,90,0.55,-0.55); 
          //canvas.pushRotateZoomWithAA(120*0.55, (40 * drawIndex)*0.55+ 20,90,0.5583,-0.575); 
          //canvas.pushRotateZoom(120*0.55, (40 * drawIndex)*0.55+ 20,90,0.55,-0.55); 

          //pushAffineWithAAを使えば、中心位置指定ではないのでキレイに表示できる。
          float matrix[6] = {0,   // 横倍
                     0.55,  // 横傾き
                    0, // X座標
                     0.55,   // 縦傾き
                    0,   // 縦倍
                     (40 * drawIndex)*0.55 + 7  // Y座標
                    };
          canvas.pushAffineWithAA(matrix);
          drawIndex = drawIndex + 1;
        }
        #elif defined(_M5STACK)
          if((cy+1)%5 == 0){
          canvas.pushSprite(0, 40 * drawIndex); 
          drawIndex = drawIndex + 1;
        }
        #endif
      }
      if(statusAreaMessage.equals("")==false){
        #ifdef USE_EXT_LCD
          m5lcd.setTextColor(TFT_WHITE);
          m5lcd.setTextSize(1);
          m5lcd.fillRect(0, 190, 240, 90, TFT_BLACK);
          m5lcd.setCursor(0, 190);
          m5lcd.print(statusAreaMessage);
        #elif defined(_M5STICKCPLUS)||defined(_M5CARDPUTER)
          m5lcd.setTextColor(TFT_WHITE);
          m5lcd.setTextSize(1);
          m5lcd.fillRect(0, 122, 240, 10, TFT_BLACK);
          m5lcd.setCursor(0, 122);
          m5lcd.print(statusAreaMessage);
        #else
          m5lcd.setTextColor(TFT_WHITE);
          m5lcd.setTextSize(1);
          m5lcd.fillRect(0, 190, 320, 10, TFT_BLACK);
          m5lcd.setCursor(0, 190);
          m5lcd.print(statusAreaMessage);
        #endif
      }
      m5lcd.endWrite();

      //			clock_gettime(CLOCK_MONOTONIC_RAW, &h_split);
      //			do
      //			{
      //				clock_gettime(CLOCK_MONOTONIC_RAW, &h_end);
      //				if(h_end.tv_nsec < h_start.tv_nsec)
      //				{
      //					hwait = htime - (1000000000 + h_end.tv_nsec - h_start.tv_nsec);
      //				}
      //				else
      //				{
      //					hwait = htime - (h_end.tv_nsec - h_start.tv_nsec);
      //				}
      //			}
      //			while(hwait > 0);
      //		//}
      //    screenY += 1;
      //	}

      //	printf("H-start = %ld, H-end = %ld\n", h_start.tv_nsec, h_end.tv_nsec);
      //	printf("H-disp  = %ld\n", h_split.tv_nsec);
      //	printf("V-start = %ld, H-end = %ld\n", v_start.tv_nsec, v_end.tv_nsec);
      //	printf("V-blank = %ld\n", v_wait.tv_nsec);
      //m5lcd.endWrite();
    }
    delay(10);
  }
}

void updateStatusArea(const char* message) {
  statusAreaMessage = String(message);
}
