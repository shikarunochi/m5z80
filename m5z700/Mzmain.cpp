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

WiFiMulti wifiMulti;
    
static bool intFlag = false;

//static pthread_t scrn_thread_id;
void scrn_thread(void *arg);
//static pthread_t keyin_thread_id;
void keyin_thread(void *arg);
//static pthread_t webserver_thread_id;
void webserver_thread(void *arg);
//static pthread_t i2cKeyboard_thread_id;
//void i2cKeyboard_thread(void *arg);

void checkJoyPad();

int buttonApin = 26; //赤ボタン
int buttonBpin = 36; //青ボタン

#define CARDKB_ADDR 0x5F
void checkI2cKeyboard();
//static xQueueHandle keyboard_queue = NULL;

bool joyPadPushed_U;
bool joyPadPushed_D;
bool joyPadPushed_L;
bool joyPadPushed_R;
bool joyPadPushed_A;
bool joyPadPushed_B;
bool joyPadPushed_Press;

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

#define MAX_FILES 255 // this affects memory
String tapeList[MAX_FILES];
int tapeListCount;
void aSortTapeList();

boolean btnBLongPressFlag;

String selectMzt();

void updateTapeList();

int q_kbd;
typedef struct KBDMSG_t {
	long mtype;
	char len;
	unsigned char msg[80];
} KBDMSG;


unsigned char ak_tbl_80c[] =
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
unsigned char ak_tbl_s_80c[] =
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

unsigned char ak_tbl_700[] =
{
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x06, 0x07, 0xff, 0x00, 0xff, 0xff,
  0xff, 0x74, 0x75, 0x73, 0x72, 0x80, 0x80, 0x76, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x64, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x14, 0x13, 0x67, 0x66, 0x61, 0x65, 0x60, 0x70,
  0x63, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x62, 0x01, 0x02, 0x80, 0x05, 0x80, 0x71,
  0x15, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
  0x30, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x17, 0x16, 0x80, 0x80, 0x80, 0x80, 0xff,
  0xff, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
  0x30, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x17, 0x16, 0x80, 0x80, 0x80, 0x80, 0xff
};

unsigned char ak_tbl_s_700[] =
{
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x76, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xff, 0x60, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0x67, 0x62, 0x14, 0xff,
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
void PCGSetting();
void doSendCommand(String inputString);
int set_mztype(void);

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
  
  //memset(mem, 0xFF, 64*1024);
  memset(mem+VID_START, 0, 4*1024);
  memset(mem+RAM_START, 0, 64*1024);

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
		sprintf(&sdata[pos], "GM%1d", hw700.pcg700_mode);
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
      saveConfig();
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
			hw700.pcg700_mode = 0;
		}
		else if(cmd[1] == '1')	// ON
		{
			hw700.pcg700_mode = 1;
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

  Serial.println("M5Z-700 START");
  M5.Lcd.println("M5Z-700 START");

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

  pinMode(buttonApin, INPUT_PULLUP);
  pinMode(buttonBpin, INPUT_PULLUP);
  joyPadPushed_U = false;
  joyPadPushed_D = false;
  joyPadPushed_L = false;
  joyPadPushed_R = false;
  joyPadPushed_A = false;
  joyPadPushed_B = false;
  joyPadPushed_Press = false;
  
  btnBLongPressFlag = false;

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
	
	saveConfig();
  
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

  if(mzConfig.mzMode == MZMODE_80){
    ak_tbl = ak_tbl_80c;
    ak_tbl_s = ak_tbl_s_80c;
  }else{
    ak_tbl = ak_tbl_700;
    ak_tbl_s = ak_tbl_s_700;
  }
  //keyboard_queue = xQueueCreate(10, sizeof(uint8_t));

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
      Serial.println("buttonA pressed!");
    }

    if(M5.BtnB.wasReleased()){
      if(btnBLongPressFlag != true){
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
      }else{
        btnBLongPressFlag = false;
      }
    }
    if(M5.BtnC.wasPressed()){ 
      if(webStarted){
        stopWebServer();
      }else{
        startWebServer();
      }
    }
    if(M5.BtnB.pressedFor(2000)){
      if(btnBLongPressFlag != true){
        btnBLongPressFlag = true;
        //B長押しで、テープ選択画面
        //選択時は画面描画止める
        suspendScrnThreadFlag = true;
        selectMzt();
        delay(100);
        suspendScrnThreadFlag = false;
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
/*	int st;

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

  st = pthread_create(&i2cKeyboard_thread_id, NULL, i2cKeyboard_thread, NULL);
  
  if(st != 0) {
    perror("i2cKeyboard_thread");
    return;
  }
  pthread_detach(i2cKeyboard_thread_id);
*/
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
   
    xTaskCreatePinnedToCore(
                    webserver_thread,     /* Function to implement the task */
                    "webserver_thread",   /* Name of the task */
                    4096,      /* Stack size in words */
                    NULL,      /* Task input parameter */
                    1,         /* Priority of the task */
                    NULL,      /* Task handle. */
                    0);        /* Core where the task should run */

//    xTaskCreatePinnedToCore(
//                    i2cKeyboard_thread,     /* Function to implement the task */
//                    "i2cKeyboard_thread",   /* Name of the task */
//                    4096,      /* Stack size in words */
//                    NULL,      /* Task input parameter */
//                    1,         /* Priority of the task */
//                    NULL,      /* Task handle. */
//                    0);        /* Core where the task should run */                              
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
    if(webCommandBreakFlag == true){ //SHIFT+BREAK送信
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
      webCommandBreakFlag = false;
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

    checkI2cKeyboard();
    checkJoyPad();
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
	return;// NULL;
}

//--------------------------------------------------------------
// Web処理スレッド 
//--------------------------------------------------------------
void webserver_thread(void *arg)
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
  return;// NULL;
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
    String adSSIDString = "M5Z700_" + String(randNumber);
    const char* apSSID = adSSIDString.c_str();
    Serial.print("Access Point Setting");
    Serial.print("SSID:" + String(apSSID));
    Serial.print("IP Address:" + String(apIP));

    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect(true);
    delay(1000);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSSID);
    WiFi.mode(WIFI_MODE_AP);
    message = "[SSID:" + String(apSSID) + "] http://" + apIP.toString();
    updateStatusArea(message.c_str());
  }else{
    String message = "Starting WebServer...";
    updateStatusArea(message.c_str());
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(mzConfig.ssid, mzConfig.pass);
    Serial.println("Connecting Wifi...");
    Serial.print("Web Server Setting");
    Serial.print("SSID:" + String(mzConfig.ssid));
    if(wifiMulti.run() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }else{
      message = "Connection Fail:" + String(mzConfig.ssid);
      updateStatusArea(message.c_str());
      suspendScrnThreadFlag = false;
      return false;
    }
    
//    WiFi.disconnect();
//    delay(100);
//    Serial.print("Web Server Setting");
//    Serial.print("SSID:" + String(mzConfig.ssid));
//   //Serial.print("PASS:" + String(mzConfig.pass));
//    Serial.print("WiFi:Start");
//    
//    WiFi.begin(mzConfig.ssid, mzConfig.pass);
//    uint32_t WIFItimeOut = millis();
//    while (WiFi.status() != WL_CONNECTED) {
//      delay(500);
//      Serial.print(".");
//      if( millis() - WIFItimeOut > 20000 ) {
//        message = "Connection Fail:" + String(mzConfig.ssid);
//        updateStatusArea(message.c_str());
//        suspendScrnThreadFlag = false;
//        return false;
//      }
//    }
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
  webServer.on("/PCGSetting",PCGSetting);
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

  WiFi.disconnect(true); 
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
    delay(200);
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

void PCGSetting(){
  String enablePCG = urlDecode(webServer.arg("enablePCG"));
  if(enablePCG.toInt()==1){
        hw700.pcg700_mode = 1;
  }else{
      hw700.pcg700_mode = 0;
  }
  String message = String("Set PCG[ ") + ((hw700.pcg700_mode == 1)?String("ON"):String("OFF"))  + String(" ] Done.");
  webServer.send(200, "text/html", makePage(message));
}

String makePage(String message) {
String s = "<!DOCTYPE html><meta charset=\"UTF-8\" /><meta name=\"viewport\" content=\"width=device-width\"><html><head>" 
"<title>MZ-700 for M5Stack Setting</title>"
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
"</form></div>" 
"<hr/>"
"<div style='display:inline-flex'>"
"<form action='/PCGSetting' method='post'>" 
"<input type='hidden' name='enablePCG' value='1'>"
"<input type='submit' value='PCG ON'>" 
"</form> "
"<form action='/PCGSetting' method='post'>" 
"<input type='hidden' name='enablePCG' value='0'>"
"<input type='submit' value='PCG OFF'>" 
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
s += "PCG:" + ((hw700.pcg700_mode == 1)?String("ON"):String("OFF"))  + "<br/>";
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
  tapeListCount = 0;
  
  while(1){
    File entry =  tapeRoot.openNextFile();
    if(!entry){// no more files
        break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
        String fileName = entry.name();
        String tapeName = fileName.substring(fileName.lastIndexOf("/") + 1);
        tapeList[tapeListCount] = tapeName;
        tapeListCount++;
    }
    entry.close();
  }
  tapeRoot.close();
  
  aSortTapeList();

  for(int index = 0;index < tapeListCount;index++){
    tapeListOptionHTML += "<option value=\"";
    tapeListOptionHTML += tapeList[index];
    tapeListOptionHTML += "\">";
    tapeListOptionHTML += tapeList[index];
    tapeListOptionHTML += "</option>";
  }
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
      webCommandBreakFlag = true; //うまくキーコード処理できなかったのでWebからのBreak送信扱いにします。
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
  delay(10);
  M5.Lcd.fillScreen(TFT_BLACK);
  delay(10);
  M5.Lcd.setTextSize(2);
  String curTapeFile  = String(mzConfig.tapeFile);
  int selectIndex = 0;
  for(int index = 0;index < tapeListCount;index++){
    if(tapeList[index].compareTo(curTapeFile)==0){
      selectIndex = index;
      break;
    }
  }
  
  int startIndex = 0;
  int endIndex = startIndex + 10;
  if(endIndex > tapeListCount){
    endIndex = tapeListCount;
  }
  Serial.print("start:");
  Serial.println(startIndex);
  Serial.println("end:");
  Serial.println(endIndex);
  boolean needRedraw = true;
  while(true){

    if(needRedraw == true){
      M5.Lcd.fillScreen(0);
      M5.Lcd.setCursor(0,0);
      startIndex = selectIndex - 5;
      if(startIndex < 0){
        startIndex = 0;
      }

      endIndex = startIndex + 13;
      if(endIndex > tapeListCount -1){
        endIndex = tapeListCount -1;
      }

      for(int index = startIndex;index < endIndex;index++){
          if(index == selectIndex){
             M5.Lcd.setTextColor(TFT_GREEN);
          }else{
            M5.Lcd.setTextColor(TFT_WHITE);
          }
          M5.Lcd.println(tapeList[index]);
      }
      M5.Lcd.setTextColor(TFT_WHITE);

      M5.Lcd.drawRect(0, 240 - 19, 100 , 18, TFT_WHITE);
      M5.Lcd.setCursor(35, 240 - 17);
      M5.Lcd.print("U P");
      M5.Lcd.drawRect(110, 240 - 19, 100 , 18, TFT_WHITE);
      M5.Lcd.setCursor(125, 240 - 17);
      M5.Lcd.print("SELECT");
      M5.Lcd.drawRect(220, 240 - 19, 100 , 18, TFT_WHITE);
      M5.Lcd.setCursor(245, 240 - 17);
      M5.Lcd.print("DOWN");
      needRedraw = false;
    }
    M5.update();
    if(M5.BtnA.wasPressed()){
      selectIndex--;
      if(selectIndex < 0){
        selectIndex = 0;
      }
      needRedraw = true;
    }

    if(M5.BtnC.wasPressed()){
      selectIndex++;
      if(selectIndex > tapeListCount - 1){
        selectIndex = tapeListCount -1;
      }
      needRedraw = true;
    }
    
    if(M5.BtnB.wasPressed()){
      Serial.print("select:");
      Serial.println(tapeList[selectIndex]);
      delay(10);
      M5.Lcd.fillScreen(TFT_BLACK);

      strncpy(mzConfig.tapeFile, tapeList[selectIndex].c_str(), 50);
      set_mztData(mzConfig.tapeFile);
      saveConfig();
      Serial.println("saveDone");
      ts700.cmt_play = 0;
         
      M5.Lcd.setCursor(0,0);
      M5.Lcd.print("Set:");
      M5.Lcd.print(tapeList[selectIndex]);
      delay(2000);
      M5.Lcd.fillScreen(TFT_BLACK);
      delay(10);
      return tapeList[selectIndex];
    }    
    delay(100);
  }
}


/*
void i2cKeyboard_thread(void *arg){
  while(!intByUser())
  {
    if(Wire.requestFrom(CARDKB_ADDR, 1)){  // request 1 byte from keyboard
      while (Wire.available()) {
        char i2cKeyCode = Wire.read();                  // receive a byte as 
        if(i2cKeyCode != 0) {
          xQueueSendFromISR(keyboard_queue, &i2cKeyCode, NULL);
          //Serial.println(i2cKeyCode, HEX);
        }
        break;
      }
    }
    checkJoyPad();
    delay(5);
    
  }
}
*/


/* bubble sort filenames */
//https://github.com/tobozo/M5Stack-SD-Updater/blob/master/examples/M5Stack-SD-Menu/M5Stack-SD-Menu.ino
void aSortTapeList() {
  
  bool swapped;
  String temp;
  String name1, name2;
  do {
    swapped = false;
    for(uint16_t i=0; i<tapeListCount-1; i++ ) {
      name1 = tapeList[i];
      name1.toUpperCase();
      name2 = tapeList[i+1];
      name2.toUpperCase();
      if (name1.compareTo(name2) > 0) {
        temp = tapeList[i];
        tapeList[i] = tapeList[i+1];
        tapeList[i+1] = temp;
        swapped = true;
      }
    }
  } while (swapped);
}
