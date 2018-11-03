//----------------------------------------------------------------------------
// File:MZmain.cpp
// MZ-80 Emulator m5z80 for M5Stack(with SRAM)
// m5z80:Main Program Module based on mz80rpi
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
#include "m5z80.h"

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
//#include <WiFiMulti.h>

static bool intFlag = false;

static pthread_t scrn_thread_id;
void *scrn_thread(void *arg);
static pthread_t keyin_thread_id;
void *keyin_thread(void *arg);
static pthread_t webserver_thread_id;
void *webserver_thread(void *arg);

#define	FIFO_i_PATH	"/tmp/cmdxfer"
#define	FIFO_o_PATH	"/tmp/stsxfer"
int fifo_i_fd, fifo_o_fd;

SYS_STATUS sysst;
int xferFlag = 0;
int wFifoOpenFlag = 0;

#define SyncTime	17									/* 1/60 sec.(milsecond) */

String romListOptionHTML;
void updateRomList();

String tapeListOptionHTML;
void updateTapeList();

int q_kbd;
typedef struct KBDMSG_t {
	long mtype;
	char len;
	unsigned char msg[80];
} KBDMSG;
const unsigned char ak_tbl[] =
{
	0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x84, 0xff, 0xff,
	0xff, 0x92, 0x80, 0x83, 0x80, 0x90, 0x80, 0x81, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x91, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x73, 0x05, 0x64, 0x74,
	0x14, 0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, 0x80, 0x54, 0x80, 0x25, 0x80, 0x80,
	0x80, 0x40, 0x62, 0x61, 0x41, 0x21, 0x51, 0x42, 0x52, 0x33, 0x43, 0x53, 0x44, 0x63, 0x72, 0x24,
	0x34, 0x20, 0x31, 0x50, 0x22, 0x23, 0x71, 0x30, 0x70, 0x32, 0x60, 0x80, 0x80, 0x80, 0xff, 0xff,
	0xff, 0x40, 0x62, 0x61, 0x41, 0x21, 0x51, 0x42, 0x52, 0x33, 0x43, 0x53, 0x44, 0x63, 0x72, 0x24,
	0x34, 0x20, 0x31, 0x50, 0x22, 0x23, 0x71, 0x30, 0x70, 0x32, 0x60, 0xff, 0xff, 0xff, 0xff, 0xff,
};
const unsigned char ak_tbl_s[] =
{
	0xff, 0xff, 0xff, 0x93, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x92, 0xff, 0x83, 0xff, 0x90, 0xff, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, 0x25, 0x05, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x24, 0xff, 0x20, 0xff, 0x30, 0x33,
	0x23, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x31, 0x32, 0x22, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

#define MAX_PATH 256
char PROGRAM_PATH[MAX_PATH];
char FDROM_PATH[MAX_PATH];
char MONROM_PATH[MAX_PATH];
char CGROM_PATH[MAX_PATH];

extern uint16_t c_bright;

char serialKeyCode;

WebServer webServer(80);

bool webStarted;

bool startWebServer();
bool stopWebServer();
String makePage(String message);
String urlDecode(String input);

void selectTape();
void selectRom();
void sendCommand();
void sendBreakCommand();
void setWiFi();
void deleteWiFi();
void soundSetting();
void doSendCommand(String inputString);

enum{
  WEB_C_NONE = 0,
  WEB_C_SELECT_ROM,
  WEB_C_SELECT_TAPE,
  WEB_C_SEND_COMMAND,
  WEB_C_SEND_BREAK_COMMAND
};

int webCommand;
String webCommandParam;
boolean webCommandBreakFlag;

String inputStringEx;

bool suspendScrnThreadFlag;
bool suspendWebThreadFlag;

MZ_CONFIG mzConfig;

//------------------------------------------------
// Memory Allocation for MZ
//------------------------------------------------
int mz_alloc_mem(void)
{
	int result = 0;

	/* Main Memory */
mem = (UINT8*)ps_malloc(64*1024);
//mem = (UINT8*)ps_malloc((4+6+4+64)*1024);
 
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

	/* PCG-8000 FONT */
	pcg8000_font = (uint8_t*)ps_malloc(PCG8000_SIZE);
	if(pcg8000_font == NULL)
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
	if(pcg8000_font)
	{
		free(pcg8000_font);
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
  File dataFile = SD.open(romPath, FILE_READ);

  int offset = 0;
  if (dataFile) {
    while (dataFile.available()) {
      *(mem + offset) = dataFile.read();
      offset++;
      if(offset % 1000 == 0){
        delay(10);
        Serial.print(".");
        delay(10);
      }
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

}

void fdrom_load(void)
{
	FILE *in;

	if((in = fopen(FDROM_PATH, "r")) != NULL)
	{
		fread(mem + ROM2_START, sizeof(unsigned char), 1024, in);
		fclose(in);
	}
}

//--------------------------------------------------------------
// ＭＺのモニタＲＯＭのセットアップ
//--------------------------------------------------------------
void mz_mon_setup(void)
{
  memset(mem, 0xFF, 64*1024);
  memset(mem+RAM_START, 0, 48*1024);
  memset(mem+VID_START, 0, 1024);

}

//--------------------------------------------------------------
// FIFOの準備
//--------------------------------------------------------------
static int setupXfer(void)
{
  /*
	// 外部プログラムから入力
	mkfifo(FIFO_i_PATH, 0777);
	fifo_i_fd = open(FIFO_i_PATH, O_RDONLY|O_NONBLOCK);
	if(fifo_i_fd == -1)
	{
		perror("FIFO(i) open");
		return -1;
	}

	// 外部プログラムへ出力
	mkfifo(FIFO_o_PATH, 0777);
*/
	return 0;
}

//--------------------------------------------------------------
// 外部プログラムへのステータス送信処理
//--------------------------------------------------------------
static int statusXfer(void)
{
  /*
	char sdata[160];
	int pos = 0;

	if(xferFlag & SYST_CMT)
	{
		sprintf(&sdata[pos], "CP%3d", sysst.tape);
		pos = 6;
		xferFlag &= ~SYST_CMT;
	}
	if(xferFlag & SYST_MOTOR)
	{
		if(pos != 0)
		{
			sdata[pos - 1] = ',';
		}
		sprintf(&sdata[pos], "CM%1d", sysst.motor);
		pos += 4;
		xferFlag &= ~SYST_MOTOR;
	}
	if(xferFlag & SYST_LED)
	{
		if(pos != 0)
		{
			sdata[pos - 1] = ',';
		}
		sprintf(&sdata[pos], "LM%1d", sysst.led);
		pos += 4;
		xferFlag &= ~SYST_LED;
	}
	if(xferFlag & SYST_BOOTUP)
	{
		if(pos != 0)
		{
			sdata[pos - 1] = ',';
		}
		sprintf(&sdata[pos], "BU");
		pos += 3;
		xferFlag &= ~SYST_BOOTUP;
	}
	if(xferFlag & SYST_PCG)
	{
		if(pos != 0)
		{
			sdata[pos - 1] = ',';
		}
		sprintf(&sdata[pos], "GM%1d", hw700.pcg8000_mode);
		pos += 4;
		xferFlag &= ~SYST_PCG;
	}

	if(pos != 0)
	{
		// 初回送信時にオープン
		if(wFifoOpenFlag == 0)
		{
			fifo_o_fd = open(FIFO_o_PATH, O_WRONLY);
			if(fifo_o_fd == -1)
			{
				perror("FIFO(o) open");
				return -1;
			}
			wFifoOpenFlag = 1;
		}

		write(fifo_o_fd, sdata, pos);

		// クローズしない
		//close(fifo_o_fd);
	}
 */
	return 0;
}

//--------------------------------------------------------------
// Webからのコマンド処理
//--------------------------------------------------------------
static void processWebCommand(void)
{
  if(webCommand == WEB_C_NONE){
    return;
  }
  
  switch(webCommand){
    case WEB_C_SELECT_ROM:
    {
      strncpy(mzConfig.romFile, webCommandParam.c_str(), 50);
      mz_exit(0);//モニタROM変更時はリセット
      //monrom_load();
      break;
    }
    case WEB_C_SELECT_TAPE:
    {
      strncpy(mzConfig.tapeFile, webCommandParam.c_str(), 50);
      if(ts700.cmt_tstates == 0)
      {
        suspendScrnThreadFlag = true;
        delay(100);
        String message = "Setting...";
        updateStatusSubArea(message.c_str(), GREEN);
        if(true == set_mztData(webCommandParam)){
          saveConfig();
          Serial.println("saveDone");
          ts700.cmt_play = 0;
          webServer.send(200, "text/html", makePage("Set TAPE[" + webCommandParam + "] "));
        }else{
          webServer.send(200, "text/html", makePage("FAIL:Set TAPE[" + webCommandParam + "] "));
        }
        updateStatusSubArea("",WHITE);
        
        suspendScrnThreadFlag = false;
    } 
      break;
    }
    case WEB_C_SEND_COMMAND:
    {
      inputStringEx = webCommandParam;
      break;
    }
    case WEB_C_SEND_BREAK_COMMAND:{
      webCommandBreakFlag = true;
      break;
    }
  }

  webCommand = WEB_C_NONE;
  webCommandParam = "";
  
  /*
	int num;
	unsigned char ch, cmd[80];
	KBDMSG kbdm;
	static int runmode = 0;
	char str[MAX_PATH];

REPEAT:
	// 予約されたステータス送信処理
	statusXfer();

	// 1行受信
	ch = 0x01;
	num = 1;
	memset(cmd, 0, 80);
	if(read(fifo_i_fd, &cmd[0], 1) <= 0)	// 受信データチェック
	{
		goto EXIT;
	}
	if(cmd[0] == 0)	// 先頭がゼロなら無効行
	{
		goto EXIT;
	}
	while(ch != 0)
	{
		if(read(fifo_i_fd, &ch, 1) == 0)
		{
			usleep(100);
			continue;
		}
		cmd[num++] = ch;
	}
	//printf("recieved:%s\n", cmd);
	num = strlen((char *)cmd);

	switch(cmd[0])
	{
	case 'R':	// Reset MZ
		mz_reset();
		break;
	case 'C':	// Casette Tape
		switch(cmd[1])
		{
		case 'T':	// Set Tape
			if(ts700.cmt_tstates == 0)
			{
				sprintf(str, "%s/%s", PROGRAM_PATH, (char *)&cmd[2]);
				set_mztData(str);
				ts700.cmt_play = 0;
			}
			break;
		case 'P':	// Play Tape
			if(ts700.mzt_settape != 0)
			{
				ts700.cmt_play = 1;
				ts700.mzt_start = ts700.cpu_tstates;
				ts700.cmt_tstates = 1;
				setup_cpuspeed(5);
				sysst.motor = 1;
				xferFlag |= SYST_MOTOR;
			}
			break;
		case 'S':	// Stop Tape
			if(ts700.cmt_tstates != 0)
			{
				ts700.cmt_play = 0;
				ts700.cmt_tstates = 0;
				setup_cpuspeed(1);
				ts700.mzt_elapse += (ts700.cpu_tstates - ts700.mzt_start);
			}
			break;
		case 'E':	// Eject Tape
			if(ts700.cmt_tstates == 0)
			{
				ts700.mzt_settape = 0;
			}
			break;
		default:
			break;
		}
		break;
	case 'P':	// PCG
		if(cmd[1] == '0')	// OFF
		{
			hw700.pcg8000_mode = 0;
		}
		else if(cmd[1] == '1')	// ON
		{
			hw700.pcg8000_mode = 1;
		}
		break;
	case 'K':	// Key in
		kbdm.mtype = 1;
		kbdm.len = num;
		memcpy(kbdm.msg, &cmd[1], num);
		msgsnd(q_kbd, &kbdm, 81, IPC_NOWAIT);
		break;
	case 'S':	// Set/Echo status
		switch(cmd[1])
		{
		case 'R':	// Run
			runmode = 1;
			break;
		case 'S':	// Stop
			runmode = 0;
			break;
		case 'M':	// Monitor ROM
			sprintf(MONROM_PATH, "%s/%s", PROGRAM_PATH, (char *)&cmd[2]);
			monrom_load();
			break;
		case 'F':	// Font ROM
			sprintf(CGROM_PATH, "%s/%s", PROGRAM_PATH, (char *)&cmd[2]);
			font_load(CGROM_PATH);
			break;
		case 'C':	// CRT color
			if(cmd[2] == '0')
			{
				c_bright = WHITE;	// WHITE
			}
			else if(cmd[2] == '1')
			{
				c_bright = GREEN;	// GREEN
			}
			break;
		case 'E':	// Echo
			xferFlag |= SYST_PCG|SYST_LED|SYST_CMT|SYST_MOTOR;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

EXIT:
	if(runmode == 0)
	{
		usleep(3000);
		goto REPEAT;
	}
 */
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

  Serial.println("MZ-80C START");
  M5.Lcd.println("MZ-80C START");

  Serial.println("LOAD CONFIG");
  M5.Lcd.println("Load Config File");
	loadConfig();
 
	//struct sigaction sa;
  //char tmpPathStr[MAX_PATH];

	//sigaction(SIGINT, NULL, &sa);
	//sa.sa_handler = sighandler;
	//sa.sa_flags = SA_NODEFER;
	//sigaction(SIGINT, &sa, NULL);

  suspendScrnThreadFlag = false;

  c_bright = GREEN;
  serialKeyCode = 0;

  updateRomList();
  updateTapeList();

  webStarted = false;
  webCommand = WEB_C_NONE;
  webCommandParam = "";
  webCommandBreakFlag = false;

	//readlink("/proc/self/exe", tmpPathStr, sizeof(tmpPathStr));
	//sprintf(PROGRAM_PATH, "%s", dirname(tmpPathStr));
  
	mz_screen_init();
	mz_alloc_mem();
//	init_defkey();
//	read_defkey();
	makePWM();
 
	// キー1行入力用メッセージキューの準備
	//q_kbd = msgget(QID_KBD, IPC_CREAT);

	// 外部プログラムとの通信の準備
	//setupXfer();

	// ＭＺのモニタのセットアップ
	mz_mon_setup();

	// メインループ実行
	mainloop();

	// 終了
	//mzbeep_clean();
  //end_defkey();
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

	xferFlag |= SYST_BOOTUP;

	// start the CPU emulation

  Serial.println("START READ TAPE!");
  String message = "Tape Setting";
  updateStatusArea(message.c_str());
  delay(100);

  if(strlen(mzConfig.tapeFile) == 0){
    set_mztData(DEFAULT_TAPE_FILE);
  }else{
    set_mztData(mzConfig.tapeFile);
  }
  Serial.println("END READ TAPE!");
  delay(1000);
  updateStatusArea("");

  delay(100);
  /* スレッド　開始 */
  Serial.println("START_THREAD");
  start_thread();

  delay(100);
  
	while(!intByUser())
	{
		processWebCommand();	// Webからのコマンド処理

		timeTmp = millis();
		if (!Z80_Execute()) break;

    M5.update();
    if(M5.BtnA.wasPressed()){
      inputStringEx = "LOAD";
    }

    if(M5.BtnB.wasPressed()){
      if(ts700.mzt_settape != 0 && ts700.cmt_play == 0)
      {
        delay(100);
        Serial.println("TAPE START");
        delay(100);
        ts700.cmt_play = 1;
        ts700.mzt_start = ts700.cpu_tstates;
        ts700.cmt_tstates = 1;
        setup_cpuspeed(5);
        sysst.motor = 1;
        xferFlag |= SYST_MOTOR;
       }else{
          delay(100);
          Serial.println("TAPE CAN'T START");
          delay(100);
       }
    }
    if(M5.BtnC.wasPressed()){ 
      if(webStarted){
        stopWebServer();
      }else{
        startWebServer();
      }
    }
    syncTmp = millis();
    if(synctime - (syncTmp - timeTmp) > 0){
      delay(synctime - (syncTmp - timeTmp));
    }else{
      delay(1);
    }
#if _DEBUG
		if (Z80_Trace)
		{
			usleep(1000000);
		}
#endif
	}
//	
}

//------------------------------------------------------------
// CPU速度を設定 (10-100)
//------------------------------------------------------------
void setup_cpuspeed(int mul) {
	int _iperiod;
	//int a;

	_iperiod = (CPU_SPEED*CpuSpeed*mul)/(100*IFreq);

	//a = (per * 256) / 100;

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
	int st;

	st = pthread_create(&scrn_thread_id, NULL, scrn_thread, NULL);
	if(st != 0)
	{
		perror("update_scrn_thread");
		return;
	}

	pthread_detach(scrn_thread_id);
 
	st = pthread_create(&keyin_thread_id, NULL, keyin_thread, NULL);
	if(st != 0)
	{
		perror("keyin_thread");
		return;
	}
  pthread_detach(keyin_thread_id);
  
  st = pthread_create(&webserver_thread_id, NULL, webserver_thread, NULL);
  
  if(st != 0) {
    perror("webserver_thread");
    return;
  }
  pthread_detach(webserver_thread_id);

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
void * scrn_thread(void *arg)
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
	return NULL;
}

//--------------------------------------------------------------
// キーボード入力スレッド
//--------------------------------------------------------------
void * keyin_thread(void *arg)
{
  char inKeyCode = 0;
 
  while(!intByUser())
  {
    if(webCommandBreakFlag == true){ //SHIFT+BREAK送信
      mz_keydown_sub(0x80);
      delay(60);
      mz_keydown_sub(ak_tbl_s[3]);
      delay(120);
      mz_keyup_sub(ak_tbl_s[3]);
      mz_keyup_sub(0x80);
      delay(60);
      webCommandBreakFlag = false;
    }
    
    if(inputStringEx.length() > 0){
        doSendCommand(inputStringEx);
        inputStringEx = "";
    }
    
    if (Serial.available() > 0) {
      serialKeyCode = Serial.read();
      
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
          webCommandBreakFlag = true; //うまくキーコード処理できなかったのでWebからのBreak送信扱いにします。
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
    delay(10);
  }
	/*
	int fd = 0, fd2, key_active = 0, i;
	struct input_event event;
	KBDMSG kbdm;

	fd2 = open("/dev/input/mice", O_RDONLY);
	if(fd2 != -1)
	{
		ioctl(fd2, EVIOCGRAB, 1);
	}

	while(!intByUser())
	{
		// キー入力
		if(key_active == 0)
		{
			fd = open("/dev/input/event0", O_RDONLY);
			if(fd != -1)
			{
				ioctl(fd, EVIOCGRAB, 1);
				key_active = 1;	// キーデバイス確認、次回はキー入力
			}
		}
		else
		{
			if(read(fd, &event, sizeof(event)) == sizeof(event))
			{
				if(event.type == EV_KEY)
				{
					switch(event.value)
					{
					case 0:
						mz_keyup(event.code);
						break;
					case 1:
						mz_keydown(event.code);
						break;
					default:
						break;
					}
					//printf("code = %d, value = %d\n", event.code, event.value);
				}
			}
			else
			{
				key_active = 0;	// キーデバイス消失、次回はデバイスオープンを再試行
			}
		}

		// 外部プログラムからの1行入力
		if(msgrcv(q_kbd, &kbdm, 81, 0, IPC_NOWAIT) != -1)
		{
			for(i = 0; i < kbdm.len; i++)
			{
				if(ak_tbl[kbdm.msg[i]] == 0xff)
				{
					continue;
				}
				else if(ak_tbl[kbdm.msg[i]] == 0x80)
				{
					mz_keydown_sub(ak_tbl[kbdm.msg[i]]);
					usleep(60000);
					mz_keydown_sub(ak_tbl_s[kbdm.msg[i]]);
					usleep(60000);
					mz_keyup_sub(ak_tbl_s[kbdm.msg[i]]);
					mz_keyup_sub(ak_tbl[kbdm.msg[i]]);
					usleep(60000);
				}
				else
				{
					mz_keydown_sub(ak_tbl[kbdm.msg[i]]);
					usleep(60000);
					mz_keyup_sub(ak_tbl[kbdm.msg[i]]);
					usleep(60000);
				}
			}
		}
		usleep(10000);
	}

	ioctl(fd, EVIOCGRAB, 0);
	close(fd);
	ioctl(fd2, EVIOCGRAB, 0);
	close(fd2);
*/
	return NULL;
}

//--------------------------------------------------------------
// Web処理スレッド 
//--------------------------------------------------------------
void * webserver_thread(void *arg)
{ 
  long synctime = 500;
  long timeTmp;
  long vsyncTmp;

  while(!intByUser())
  {
    timeTmp = millis();
    if(webStarted == true){ // && suspendWebThreadFlag == false){
      webServer.handleClient();
    }else{

    }
    vsyncTmp = millis();
    if(synctime - (vsyncTmp - timeTmp) > 0){
      delay(synctime - (vsyncTmp - timeTmp));
    }else{
      delay(1);
    }
  }
  return NULL;
}

void suspendWebThread(bool flag){
  suspendWebThreadFlag = flag;
  delay(100);
}

//--------------------------------------------------------------
// WebServerLogic
//--------------------------------------------------------------

bool startWebServer(){
  suspendScrnThreadFlag = true;
  delay(100);
  if(strlen(mzConfig.ssid) == 0 || mzConfig.forceAccessPoint == true){
    //アクセスポイントとして動作
    String message = "Starting Access Point...";
    updateStatusArea(message.c_str());
    const IPAddress apIP(192, 168, 4, 1);
    int randNumber = random(99999);
    String adSSIDString = "M5Z-80_" + String(randNumber);
    const char* apSSID = adSSIDString.c_str();
    Serial.print("Access Point Setting");
    Serial.print("SSID:" + String(apSSID));
    Serial.print("IP Address:" + String(apIP));

    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSSID);
    WiFi.mode(WIFI_MODE_AP);
    message = "[SSID:" + String(apSSID) + "] http://" + apIP.toString();
    updateStatusArea(message.c_str());
  }else{
    String message = "Starting WebServer...";
    updateStatusArea(message.c_str());
    WiFi.disconnect();
    delay(100);
    Serial.print("Web Server Setting");
    Serial.print("SSID:" + String(mzConfig.ssid));
   //Serial.print("PASS:" + String(mzConfig.pass));
    Serial.print("WiFi:Start");
    
    WiFi.begin(mzConfig.ssid, mzConfig.pass);
    uint32_t WIFItimeOut = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if( millis() - WIFItimeOut > 20000 ) {
        message = "Connection Fail:" + String(mzConfig.ssid);
        updateStatusArea(message.c_str());
        suspendScrnThreadFlag = false;
        return false;
      }
    }
    Serial.println();
    Serial.println("WiFi connected");
    //M5.Lcd.println("Wi-Fi Connected");
    delay(1000);
    Serial.println(WiFi.localIP());
    
    message = "http://" + WiFi.localIP().toString();
    updateStatusArea(message.c_str());
  }  
  webServer.on("/play", []() {
    if(ts700.mzt_settape != 0)
    {
      ts700.cmt_play = 1;
      ts700.mzt_start = ts700.cpu_tstates;
      ts700.cmt_tstates = 1;
      setup_cpuspeed(5);
      sysst.motor = 1;
      xferFlag |= SYST_MOTOR;
    }
    webServer.send(200, "text/html", makePage("Tape play started"));
  });

  webServer.on("/reset", []() {
    mz_reset();
    webServer.send(200, "text/html", makePage("Reset MZ Done"));
  });

  webServer.on("/selectRom", selectRom);
  webServer.on("/selectTape",selectTape);
  webServer.on("/sendCommand", sendCommand) ;
  webServer.on("/sendBreakCommand", sendBreakCommand) ;
  webServer.on("/setWiFi", setWiFi);
  webServer.on("/deleteWiFi",deleteWiFi);
  webServer.on("/soundSetting",soundSetting);
  webServer.on("/", []() {
    webServer.send(200, "text/html", makePage(""));
  });

  delay(100);
  webServer.begin();
  webStarted = true;
  suspendScrnThreadFlag = false;
  return true;
}

bool stopWebServer(){
  suspendScrnThreadFlag = true;
  delay(100);
  Serial.println("WebServer:Stop");
  webStarted = false;
  webServer.stop();
  //webServer.close();
  Serial.println("WiFi:Disconnect");

  WiFi.disconnect(); 
  updateStatusArea("");
  
  suspendScrnThreadFlag = false;
  
}

void sendCommand(){
  String commandString = urlDecode(webServer.arg("commandString"));
  webCommandParam = commandString;
  webCommand = WEB_C_SEND_COMMAND;
  webServer.send(200, "text/html", makePage("sendCommand[" + commandString + "] Done"));
}

void sendBreakCommand(){
  webCommand = WEB_C_SEND_BREAK_COMMAND;
  webServer.send(200, "text/html", makePage("sendBreakCommand Done"));
}

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
  }
  delay(10);
  //最後に改行コード
  mz_keydown_sub(ak_tbl[13]);
  delay(200);
  mz_keyup_sub(ak_tbl[13]);

}

void selectRom(){
    String romFile =  urlDecode(webServer.arg("romFile"));
    webCommandParam = romFile;
    webCommand = WEB_C_SELECT_ROM;
    webServer.send(200, "text/html", makePage("Set ROM[ "+ romFile +"] Done"));
}

void selectTape(){
    String tapeFile = urlDecode(webServer.arg("tapeFile"));
    webCommandParam = tapeFile;
    webCommand = WEB_C_SELECT_TAPE;
}

void setWiFi(){
  String ssid = urlDecode(webServer.arg("ssid"));
  String pass = urlDecode(webServer.arg("pass"));
  strncpy(mzConfig.ssid, ssid.c_str(), 50);
  strncpy(mzConfig.pass, pass.c_str(), 50);
  
  webServer.send(200, "text/html", makePage("Set SSID[ "+ ssid +"] and Pass Done."));
  delay(100);
  mz_exit(0);//再起動する。設定のセーブはプログラム終了時に行われる
}

void deleteWiFi(){
  strncpy(mzConfig.ssid, "", 50);
  strncpy(mzConfig.pass, "", 50);

  webServer.send(200, "text/html", makePage("Delete SSID and Pass Done."));
  delay(100);
  mz_exit(0);//再起動する。設定のセーブはプログラム終了時に行われる
}

void soundSetting(){
  String enableSound = urlDecode(webServer.arg("enableSound"));
  if(enableSound.toInt()==1){
    mzConfig.enableSound = true;
  }else{
    mzConfig.enableSound = false;
    ledcWriteTone(LEDC_CHANNEL_0, 0);
  }
  String message = String("Set Sound[ ") + (mzConfig.enableSound?String("ON"):String("OFF"))  + String(" ] Done.");
  webServer.send(200, "text/html", makePage(message));
}

String makePage(String message) {
String s = "<!DOCTYPE html><meta charset=\"UTF-8\" /><meta name=\"viewport\" content=\"width=device-width\"><html><head>" 
"<title>MZ-80K/C for M5Stack Setting</title>"
"</head>" 
"<body onLoad=\"document.commandForm.commandString.focus()\">" 
"<button onClick=\"location.href='/'\">Reload</button><hr/>";
if (message.length() > 0){
  s+= "<span style='color:red;'>" + message + "</span><hr/>";
}
s += "<span>SD/MZROM/*.ROM</span>" 
"<form action='/selectRom' method='post'>" 
"<label>ROM: </label><select name='romFile'>";
s += romListOptionHTML;
s += "</select>" 
"<input type='submit' value='SELECT ROM and REBOOT'>" 
"</form>" 
"<hr/>" 
"<span>SD/MZROM/MZTAPE/*</span>" 
"<form action='/selectTape' method='post'>" 
"<label>TAPE: </label><select name='tapeFile'>";
s += tapeListOptionHTML;
s += "</select>"
"<input type='submit' value='SELECT TAPE'>" 
"</form>"
"<br/>" 
"<form action='/play' method='post'>" 
"<input type='submit' value='TAPE PLAY'>" 
"</form>" 
"<hr/>" 
"<form name='commandForm' action='/sendCommand' method='post'>" 
"<input type='text' name='commandString'>" 
"<input type='submit' value='SEND COMMAND'>" 
"</form>" 
"<br/>"
"<form action='/sendBreakCommand' method='post'>" 
"<input type='submit' value='SEND SHIFT+BREAK'>" 
"</form>" 
"<hr/>"
"<div style='display:inline-flex'>"
"<form action='/soundSetting' method='post'>" 
"<input type='hidden' name='enableSound' value='1'>"
"<input type='submit' value='Sound ON'>" 
"</form> "
"<form action='/soundSetting' method='post'>" 
"<input type='hidden' name='enableSound' value='0'>"
"<input type='submit' value='Sound OFF'>" 
"</form></div>" ;

//"<hr/>" 
//"<form action='/crtColorChange' method='post'>" 
//"<input type='submit' value='CRT COLOR CHANGE'>" 
//"</form>" 
//"<hr/>" 
s += "<hr/>";
s += "ROM:" + String(mzConfig.romFile) + "<br/>";
s += "TAPE:" + String(mzConfig.tapeFile) + "<br/>";
s += "SOUND:" + (mzConfig.enableSound?String("ON"):String("OFF")) + "<br/>";
//s += "MZ MODE:" 
//"CRT COLOR:" 
//"Wi-Fi:" 
//"CONNECTED" 
//"SSOD:" 
//"ACCESS POINT MODE" 
//"SSID:" 

s +="<br/><br/><hr/>"
"<form action='/setWiFi' method='post'>" 
"SSID<input type='text' name='ssid'>" 
"<br/>" 
"PASSWORD<input type='text' name='pass'>" 
"<input type='submit' value='SET Wi-Fi SSID Info and REBOOT'>"
"</form>" 
"<hr/>"
"<form action='/deleteWiFi' method='post'>" 
"<input type='submit' value='DELETE Wi-Fi SSID Info and REBOOT'>" 
"</form>" ;

s += "</body>" 
"</html>" ;
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

void updateRomList(){
  File romRoot;
  delay(100);
  romRoot = SD.open(ROM_DIRECTORY);

  romListOptionHTML = "";
  while(1){
    File entry =  romRoot.openNextFile();
    if(!entry){// no more files
        break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
        String fileName = entry.name();
        String romName = fileName.substring(fileName.lastIndexOf("/") + 1);
        String checkName = romName;
        checkName.toUpperCase();
        if(checkName.endsWith(".ROM")){
          romListOptionHTML += "<option value=\"";
          romListOptionHTML += romName;
          romListOptionHTML += "\">";
          romListOptionHTML += romName;
          romListOptionHTML += "</option>";
        }
    }
    
    entry.close();
  }
  romRoot.close();
}
void updateTapeList(){
  File tapeRoot;
  delay(100);
  tapeRoot = SD.open(TAPE_DIRECTORY);

  tapeListOptionHTML = "";
  while(1){
    File entry =  tapeRoot.openNextFile();
    if(!entry){// no more files
        break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
        String fileName = entry.name();
        String tapeName = fileName.substring(fileName.lastIndexOf("/") + 1);
        
        tapeListOptionHTML += "<option value=\"";
        tapeListOptionHTML += tapeName;
        tapeListOptionHTML += "\">";
        tapeListOptionHTML += tapeName;
        tapeListOptionHTML += "</option>";
    }
    entry.close();
  }
  tapeRoot.close();
}
