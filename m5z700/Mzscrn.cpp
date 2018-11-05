//----------------------------------------------------------------------------
// File:MZscrn.cpp
// MZ-700 Emulator m5z700 for M5Stack(with SRAM)
// m5z700:VRAM/CRT Emulation Module based on mz80rpi
// modified by @shikarunochi
//
// mz80rpi by Nibble Lab./Oh!Ishi, based on MZ700WIN
// (c) Nibbles Lab./Oh!Ishi 2017
//----------------------------------------------------------------------------
#include <M5Stack.h>  
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
//
#include "z80.h"
//
#include "mzmain.h"
#include "MZhw.h"
#include "mzscrn.h"

// フレームバッファ
//#define FB_NAME "/dev/fb0"
//static char *fbptr;
//static int fd_fb;
//static int ssize;
//#define YOFFSET (640*30)
//int bpp, pitch;

uint16_t c_bright;
#define c_dark BLACK

//フレームバッファ
TFT_eSprite fb = TFT_eSprite(&M5.Lcd);
TFT_eSprite statusArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite statusSubArea = TFT_eSprite(&M5.Lcd);

int fontOffset = 0;
int fgColor = 0;
int bgColor = 0;
int fgColorIndex = 0;
int bgColorIndex = 0;
int MZ700_COLOR[] = {BLACK,BLUE,RED,MAGENTA,GREEN,CYAN,YELLOW,WHITE};

/*
 * 表示画面の初期化
 */
int mz_screen_init(void)
{
  fb.setColorDepth(8);
  fb.createSprite(320, 200);
  fb.fillSprite(TFT_BLACK);
  fb.pushSprite(0, 0);

  statusArea.setColorDepth(8);
  statusArea.createSprite(260,12);
  statusArea.fillSprite(TFT_BLACK);

  statusSubArea.setColorDepth(8);
  statusSubArea.createSprite(60,12);
  statusSubArea.fillSprite(TFT_BLACK);

//	FILE *fd;
//	struct fb_var_screeninfo vinfo;
//	struct fb_fix_screeninfo finfo;

//	// フレームバッファのオープン
//	fd_fb = open(FB_NAME, O_RDWR);
//	if (!fd_fb)
//	{
//		perror("Open Framebuffer device");
//		return -1;
//	}

//	// スクリーン情報取得
//	if (ioctl(fd_fb, FBIOGET_FSCREENINFO, &finfo))
//	{
//		perror("FB fixed info");
//		return -1;
//	}
//	if (ioctl(fd_fb, FBIOGET_VSCREENINFO, &vinfo))
//	{
//		perror("FB variable info");
//		return -1;
//	}
//
//	bpp = vinfo.bits_per_pixel;
//	pitch = finfo.line_length;
//	printf("X = %d, Y = %d, %d(bpp)\n", vinfo.xres, vinfo.yres, bpp);
//	ssize = vinfo.xres * vinfo.yres * bpp / 8;

//	// メモリへマッピング
//	fbptr = (char *)mmap(0, ssize, PROT_READ|PROT_WRITE, MAP_SHARED, fd_fb, 0);
//	if ((int)fbptr == -1)
//	{
//		perror("Mapping FB");
//		return -1;
//	}

//	// 画面クリア
//	for(int ptr = 0; ptr < ssize; ptr++)
//	{
//		*(fbptr + ptr) = 0;
//	}

//	// カーソル点滅抑止
//	system("setterm -cursor off > /dev/tty0");
//
	return 0;
}

/*
 * 画面関係の終了処理
 */
void mz_screen_finish(void)
{
	//munmap(fbptr, ssize);
	//close(fd_fb);
}

/*
 * フォントデータを描画用に展開
 */
int font_load(const char *fontfile)
{
	FILE *fdfont;
	int dcode, line, bit;
	UINT8 lineData;
	uint16_t color;

  String romDir = ROM_DIRECTORY;
  String fontFile = DEFAULT_FONT_FILE;
  File dataFile = SD.open(romDir + "/" + fontFile, FILE_READ);
  if(!dataFile){
   Serial.println("FONT FILE NOT FOUNT");
    perror("Open font file");
    return -1;
  }

	for(dcode = 0; dcode < 256 * 2; dcode++) //拡張フォントも読み込む
	{
		for(line = 0; line < 8; line++)
		{
      if(dataFile.available()){
  			lineData = dataFile.read();
    		mz_font[dcode * 8 + line] = lineData;
  	  	if(dcode >=128 && dcode < 256 ){ //通常フォント後半の場合PCG初期値として設定する
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
void update_scrn(void)
{
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
	hw700.retrace=1;
	
	hw700.cursor_cou++;
	if(hw700.cursor_cou>=40)													/* for japan.1/60 */
	{
		hw700.cursor_cou=0;
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
  for(int cy = 0; cy < 25; cy++){
    for(int cx = 0; cx < 40; cx++){
      ch = mem[VID_START + cx + cy * 40];
      if(mzConfig.mzMode == MZMODE_700){
        chAttr = mem[VID_START + cx + cy * 40 + 0x800];
        if((chAttr & 0x80) == 0x80){ //MZ-700キャラクタセット
          fontOffset = 0x100;
        }else{
          fontOffset = 0;
        }
        //Color
        fgColorIndex = ((chAttr >> 4)& 0x07) ;
        bgColorIndex = (chAttr & 0x07);

        if(fgColorIndex <= 7){
          fgColor = MZ700_COLOR[fgColorIndex];
        }else{
          fgColor = MZ700_COLOR[7];
          Serial.print("fgColorIndexError:");
          Serial.print(fgColorIndex);
        }
        if(bgColorIndex <=7){
          bgColor = MZ700_COLOR[bgColorIndex];
        }else{
          fgColor = MZ700_COLOR[0];
          Serial.print("bgColorIndexError:");
          Serial.print(bgColorIndex);
        }
      }else{
        fgColor = c_bright;
        bgColor = c_dark;
      }
      
      //fb.drawBitmap(cx * 8, cy * 8, (const uint8_t *)((hw700.pcg700_mode == 0) ? mz_font[ch * 8] : pcg700_font[ch * 8]), 8, 8, fgColor);
      fb.fillRect(cx * 8, cy * 8, 8, 8, bgColor);
      if(hw700.pcg700_mode == 0 || !(ch & 0x80)){
        fontPtr = &mz_font[(ch + fontOffset) * 8];
      }else{
        if((chAttr & 0x80) != 0x80){
          fontPtr = &pcg700_font[(ch & 0x7F) * 8];
        }else{
          fontPtr = &pcg700_font[(ch) * 8];
        }
      }
      fb.drawBitmap(cx * 8, cy * 8, fontPtr, 8, 8, fgColor);
    }
  }
  fb.pushSprite(0, 0);    
       
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
}

void updateStatusArea(const char* message){
  statusArea.fillSprite(TFT_BLACK);
  statusArea.setCursor(0,0);
  statusArea.print(message);
  statusArea.pushSprite(0, 228);
}

void updateStatusSubArea(const char* message, int color = WHITE){
  statusSubArea.fillSprite(TFT_BLACK);
  statusSubArea.setTextColor(color);
  statusSubArea.setCursor(0,0);
  statusSubArea.print(message);
  statusSubArea.pushSprite(260, 228);
}
