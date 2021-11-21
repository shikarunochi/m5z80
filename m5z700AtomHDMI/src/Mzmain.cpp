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

#include <M5Atom.h>
#include "FS.h"
#include <SPIFFS.h>
 
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
#include "mz700lgfx.h"

#include "hid_server/hid_server.h"
#include "hid_server/hci_server.h"

#define ROM_START  0
#define L_TMPEX  0x0FFE

//M5AtomDisplay m5lcd;                 // LGFXのインスタンスを作成。
//lgfx::LGFX_SPI<EXLCD_LGFX_Config> exLcd;
//lgfx::Panel_ST7789 panel;

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

void scrn_draw();
void keyCheck();

void systemMenu();

void checkJoyPad();

//int buttonApin = 26; //赤ボタン
//int buttonBpin = 36; //青ボタン

#define CARDKB_ADDR 0x5F
int checkI2cKeyboard();
int checkSerialKey();
int keyCheckCount;
int preKeyCode;
bool inputStringMode;

void sortList(String fileList[], int fileListCount);
static void keyboard(const uint8_t* d, int len);
void gui_hid(const uint8_t* hid, int len);  // Parse HID event

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

CRGB dispColor(uint8_t g, uint8_t r, uint8_t b) {
  return (CRGB)((g << 16) | (r << 8) | b);
}

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

unsigned char hid_to_mz700[] ={
0xff,//0	0x00	Reserved (no event indicated)
0xff,//1	0x01	Keyboard ErrorRollOver?
0xff,//2	0x02	Keyboard POSTFail
0xff,//3	0x03	Keyboard ErrorUndefined?
0x47,//4	0x04	Keyboard a and A
0x46,//5	0x05	Keyboard b and B
0x45,//6	0x06	Keyboard c and C
0x44,//7	0x07	Keyboard d and D
0x43,//8	0x08	Keyboard e and E
0x42,//9	0x09	Keyboard f and F
0x41,//10	0x0A	Keyboard g and G
0x40,//11	0x0B	Keyboard h and H
0x37,//12	0x0C	Keyboard i and I
0x36,//13	0x0D	Keyboard j and J
0x35,//14	0x0E	Keyboard k and K
0x34,//15	0x0F	Keyboard l and L
0x33,//16	0x10	Keyboard m and M
0x32,//17	0x11	Keyboard n and N
0x31,//18	0x12	Keyboard o and O
0x30,//19	0x13	Keyboard p and P
0x27,//20	0x14	Keyboard q and Q
0x26,//21	0x15	Keyboard r and R
0x25,//22	0x16	Keyboard s and S
0x24,//23	0x17	Keyboard t and T
0x23,//24	0x18	Keyboard u and U
0x22,//25	0x19	Keyboard v and V
0x21,//26	0x1A	Keyboard w and W
0x20,//27	0x1B	Keyboard x and X
0x17,//28	0x1C	Keyboard y and Y
0x16,//29	0x1D	Keyboard z and Z
0x57,//30	0x1E	Keyboard 1 and !
0x56,//31	0x1F	Keyboard 2 and @
0x55,//32	0x20	Keyboard 3 and #
0x54,//33	0x21	Keyboard 4 and $
0x53,//34	0x22	Keyboard 5 and %
0x52,//35	0x23	Keyboard 6 and ^
0x51,//36	0x24	Keyboard 7 and &
0x50,//37	0x25	Keyboard 8 and *
0x62,//38	0x26	Keyboard 9 and (
0x63,//39	0x27	Keyboard 0 and )
0x00,//40	0x28	Keyboard Return (ENTER)
0x87,//41	0x29	Keyboard ESCAPE
0x76,//42	0x2A	Keyboard DELETE (Backspace)
0x06,//43	0x2B	Keyboard Tab
0x64,//44	0x2C	Keyboard Spacebar
0x65,//45	0x2D	Keyboard - and (underscore)
0x66,//46	0x2E	Keyboard = and +
0x15,//47	0x2F	Keyboard [ and {
0x14,//48	0x30	Keyboard ] and }
0x63,//49	0x31	Keyboard \ and ｜
0x13,//50	0x32	Keyboard Non-US # and ~
0x02,//51	0x33	Keyboard ; and :
0x01,//52	0x34	Keyboard ' and "
0x07,//53	0x35	Keyboard Grave Accent and Tilde
0x61,//54	0x36	Keyboard, and <
0x60,//55	0x37	Keyboard . and >
0x71,//56	0x38	Keyboard / and ?
0x04,//57	0x39	Keyboard Caps Lock
0x97,//58	0x3A	Keyboard F1
0x96,//59	0x3B	Keyboard F2
0x95,//60	0x3C	Keyboard F3
0x94,//61	0x3D	Keyboard F4
0x93,//62	0x3E	Keyboard F5
0xff,//63	0x3F	Keyboard F6
0xff,//64	0x40	Keyboard F7
0xff,//65	0x41	Keyboard F8
0xff,//66	0x42	Keyboard F9
0xff,//67	0x43	Keyboard F10
0xff,//68	0x44	Keyboard F11
0xff,//69	0x45	Keyboard F12
0xff,//70	0x46	Keyboard PrintScreen
0xff,//71	0x47	Keyboard Scroll Lock
0xff,//72	0x48	Keyboard Pause
0xff,//73	0x49	Keyboard Insert
0xff,//74	0x4A	Keyboard Home
0xff,//75	0x4B	Keyboard PageUp
0xff,//76	0x4C	Keyboard Delete Forward
0xff,//77	0x4D	Keyboard End
0xff,//78	0x4E	Keyboard PageDown
0x73,//79	0x4F	Keyboard RightArrow
0x72,//80	0x50	Keyboard LeftArrow
0x74,//81	0x51	Keyboard DownArrow
0x75,//82	0x52	Keyboard UpArrow
0xff,//83	0x53	Keypad Num Lock and Clear
0xff,//84	0x54	Keypad /
0xff,//85	0x55	Keypad *
0xff,//86	0x56	Keypad -
0xff,//87	0x57	Keypad +
0xff,//88	0x58	Keypad ENTER
0xff,//89	0x59	Keypad 1 and End
0xff,//90	0x5A	Keypad 2 and Down Arrow
0xff,//91	0x5B	Keypad 3 and PageDn?
0xff,//92	0x5C	Keypad 4 and Left Arrow
0xff,//93	0x5D	Keypad 5
0xff,//94	0x5E	Keypad 6 and Right Arrow
0xff,//95	0x5F	Keypad 7 and Home
0xff,//96	0x60	Keypad 8 and Up Arrow
0xff,//97	0x61	Keypad 9 and PageUp?
0xff,//98	0x62	Keypad 0 and Insert
0xff,//99	0x63	Keypad . and Delete
0x70,//100	0x64	Keyboard Non-US \ and ｜
0xff,//101	0x65	Keyboard Application
0xff,//102	0x66	Keyboard Power
0xff,//103	0x67	Keypad =
0xff,//104	0x68	Keyboard F13
0xff,//105	0x69	Keyboard F14
0xff,//106	0x6A	Keyboard F15
0xff,//107	0x6B	Keyboard F16
0xff,//108	0x6C	Keyboard F17
0xff,//109	0x6D	Keyboard F18
0xff,//110	0x6E	Keyboard F19
0xff,//111	0x6F	Keyboard F20
0xff,//112	0x70	Keyboard F21
0xff,//113	0x71	Keyboard F22
0xff,//114	0x72	Keyboard F23
0xff,//115	0x73	Keyboard F24
0xff,//116	0x74	Keyboard Execute
0xff,//117	0x75	Keyboard Help
0xff,//118	0x76	Keyboard Menu
0xff,//119	0x77	Keyboard Select
0xff,//120	0x78	Keyboard Stop
0xff,//121	0x79	Keyboard Again
0xff,//122	0x7A	Keyboard Undo
0xff,//123	0x7B	Keyboard Cut
0xff,//124	0x7C	Keyboard Copy
0xff,//125	0x7D	Keyboard Paste
0xff,//126	0x7E	Keyboard Find
0xff,//127	0x7F	Keyboard Mute
0xff,//128	0x80	Keyboard Volume Up
0xff,//129	0x81	Keyboard Volume Down
0xff,//130	0x82	Keyboard Locking Caps Lock
0xff,//131	0x83	Keyboard Locking Num Lock
0xff,//132	0x84	Keyboard Locking Scroll Lock
0xff,//133	0x85	Keypad Comma
0xff,//134	0x86	Keypad Equal Sign
0x05,//135	0x87	Keyboard International1
0xff,//136	0x88	Keyboard International2
0x67,//137	0x89	Keyboard International3
0xff,//138	0x8A	Keyboard International4
0xff,//139	0x8B	Keyboard International5
0xff,//140	0x8C	Keyboard International6
0xff,//141	0x8D	Keyboard International7
0xff,//142	0x8E	Keyboard International8
0xff,//143	0x8F	Keyboard International9
0xff,//144	0x90	Keyboard LANG1
0xff,//145	0x91	Keyboard LANG2
0xff,//146	0x92	Keyboard LANG3
0xff,//147	0x93	Keyboard LANG4
0xff,//148	0x94	Keyboard LANG5
0xff,//149	0x95	Keyboard LANG6
0xff,//150	0x96	Keyboard LANG7
0xff,//151	0x97	Keyboard LANG8
0xff,//152	0x98	Keyboard LANG9
0xff,//153	0x99	Keyboard Alternate Erase
0xff,//154	0x9A	Keyboard SysReq?/Attention
0xff,//155	0x9B	Keyboard Cancel
0xff,//156	0x9C	Keyboard Clear
0xff,//157	0x9D	Keyboard Prior
0xff,//158	0x9E	Keyboard Return
0xff,//159	0x9F	Keyboard Separator
0xff,//160	0xA0	Keyboard Out
0xff,//161	0xA1	Keyboard Oper
0xff,//162	0xA2	Keyboard Clear/Again
0xff,//163	0xA3	Keyboard CrSel?/Props
0xff,//164	0xA4	Keyboard ExSel?
0xff,//224	0xE0	Keyboard LeftControl?
0xff,//225	0xE1	Keyboard LeftShift?
0xff,//226	0xE2	Keyboard LeftAlt?
0xff,//227	0xE3	Keyboard Left GUI
0xff,//228	0xE4	Keyboard RightControl?
0xff,//229	0xE5	Keyboard RightShift?
0xff,//230	0xE6	Keyboard RightAlt?
0xff,//231	0xE7	Keyboard Right GUI
0xff//232-65535	0xE8-0xFFFF	Reserved
};


unsigned char hid_to_mz80c[] ={
0xff,//0	0x00	Reserved (no event indicated)
0xff,//1	0x01	Keyboard ErrorRollOver?
0xff,//2	0x02	Keyboard POSTFail
0xff,//3	0x03	Keyboard ErrorUndefined?
0x40,//4	0x04	Keyboard a and A
0x62,//5	0x05	Keyboard b and B
0x61,//6	0x06	Keyboard c and C
0x41,//7	0x07	Keyboard d and D
0x21,//8	0x08	Keyboard e and E
0x51,//9	0x09	Keyboard f and F
0x42,//10	0x0A	Keyboard g and G
0x52,//11	0x0B	Keyboard h and H
0x33,//12	0x0C	Keyboard i and I
0x43,//13	0x0D	Keyboard j and J
0x53,//14	0x0E	Keyboard k and K
0x44,//15	0x0F	Keyboard l and L
0x63,//16	0x10	Keyboard m and M
0x72,//17	0x11	Keyboard n and N
0x24,//18	0x12	Keyboard o and O
0x34,//19	0x13	Keyboard p and P
0x20,//20	0x14	Keyboard q and Q
0x31,//21	0x15	Keyboard r and R
0x50,//22	0x16	Keyboard s and S
0x22,//23	0x17	Keyboard t and T
0x23,//24	0x18	Keyboard u and U
0x71,//25	0x19	Keyboard v and V
0x30,//26	0x1A	Keyboard w and W
0x70,//27	0x1B	Keyboard x and X
0x32,//28	0x1C	Keyboard y and Y
0x60,//29	0x1D	Keyboard z and Z
0x00,//30	0x1E	Keyboard 1 and !
0x10,//31	0x1F	Keyboard 2 and @
0x01,//32	0x20	Keyboard 3 and #
0x11,//33	0x21	Keyboard 4 and $
0x02,//34	0x22	Keyboard 5 and %
0x12,//35	0x23	Keyboard 6 and ^
0x03,//36	0x24	Keyboard 7 and &
0x13,//37	0x25	Keyboard 8 and *
0x04,//38	0x26	Keyboard 9 and (
0x14,//39	0x27	Keyboard 0 and )
0x84,//40	0x28	Keyboard Return (ENTER)
0x93,//41	0x29	Keyboard ESCAPE
0x81,//42	0x2A	Keyboard DELETE (Backspace)
0x93,//43	0x2B	Keyboard Tab
0x91,//44	0x2C	Keyboard Spacebar
0x05,//45	0x2D	Keyboard - and (underscore)
0x25,//46	0x2E	Keyboard = and +
0xff,//47	0x2F	Keyboard [ and {
0xff,//48	0x30	Keyboard ] and }
0xff,//49	0x31	Keyboard \ and ｜
0xff,//50	0x32	Keyboard Non-US # and ~
0x54,//51	0x33	Keyboard ; and :
0xff,//52	0x34	Keyboard ' and "
0xff,//53	0x35	Keyboard Grave Accent and Tilde
0x64,//54	0x36	Keyboard, and <
0x64,//55	0x37	Keyboard . and >
0xff,//56	0x38	Keyboard / and ?
0x65,//57	0x39	Keyboard Caps Lock
0xff,//58	0x3A	Keyboard F1
0xff,//59	0x3B	Keyboard F2
0xff,//60	0x3C	Keyboard F3
0xff,//61	0x3D	Keyboard F4
0xff,//62	0x3E	Keyboard F5
0xff,//63	0x3F	Keyboard F6
0xff,//64	0x40	Keyboard F7
0xff,//65	0x41	Keyboard F8
0xff,//66	0x42	Keyboard F9
0xff,//67	0x43	Keyboard F10
0xff,//68	0x44	Keyboard F11
0xff,//69	0x45	Keyboard F12
0xff,//70	0x46	Keyboard PrintScreen
0xff,//71	0x47	Keyboard Scroll Lock
0xff,//72	0x48	Keyboard Pause
0xff,//73	0x49	Keyboard Insert
0x90,//74	0x4A	Keyboard Home
0xff,//75	0x4B	Keyboard PageUp
0xff,//76	0x4C	Keyboard Delete Forward
0xff,//77	0x4D	Keyboard End
0xff,//78	0x4E	Keyboard PageDown
0x83,//79	0x4F	Keyboard RightArrow
0xff,//80	0x50	Keyboard LeftArrow
0x92,//81	0x51	Keyboard DownArrow
0xff,//82	0x52	Keyboard UpArrow
0xff,//83	0x53	Keypad Num Lock and Clear
0xff,//84	0x54	Keypad /
0xff,//85	0x55	Keypad *
0xff,//86	0x56	Keypad -
0xff,//87	0x57	Keypad +
0xff,//88	0x58	Keypad ENTER
0xff,//89	0x59	Keypad 1 and End
0xff,//90	0x5A	Keypad 2 and Down Arrow
0xff,//91	0x5B	Keypad 3 and PageDn?
0xff,//92	0x5C	Keypad 4 and Left Arrow
0xff,//93	0x5D	Keypad 5
0xff,//94	0x5E	Keypad 6 and Right Arrow
0xff,//95	0x5F	Keypad 7 and Home
0xff,//96	0x60	Keypad 8 and Up Arrow
0xff,//97	0x61	Keypad 9 and PageUp?
0xff,//98	0x62	Keypad 0 and Insert
0xff,//99	0x63	Keypad . and Delete
0xff,//100	0x64	Keyboard Non-US \ and ｜
0xff,//101	0x65	Keyboard Application
0xff,//102	0x66	Keyboard Power
0xff,//103	0x67	Keypad =
0xff,//104	0x68	Keyboard F13
0xff,//105	0x69	Keyboard F14
0xff,//106	0x6A	Keyboard F15
0xff,//107	0x6B	Keyboard F16
0xff,//108	0x6C	Keyboard F17
0xff,//109	0x6D	Keyboard F18
0xff,//110	0x6E	Keyboard F19
0xff,//111	0x6F	Keyboard F20
0xff,//112	0x70	Keyboard F21
0xff,//113	0x71	Keyboard F22
0xff,//114	0x72	Keyboard F23
0xff,//115	0x73	Keyboard F24
0xff,//116	0x74	Keyboard Execute
0xff,//117	0x75	Keyboard Help
0xff,//118	0x76	Keyboard Menu
0xff,//119	0x77	Keyboard Select
0xff,//120	0x78	Keyboard Stop
0xff,//121	0x79	Keyboard Again
0xff,//122	0x7A	Keyboard Undo
0xff,//123	0x7B	Keyboard Cut
0xff,//124	0x7C	Keyboard Copy
0xff,//125	0x7D	Keyboard Paste
0xff,//126	0x7E	Keyboard Find
0xff,//127	0x7F	Keyboard Mute
0xff,//128	0x80	Keyboard Volume Up
0xff,//129	0x81	Keyboard Volume Down
0xff,//130	0x82	Keyboard Locking Caps Lock
0xff,//131	0x83	Keyboard Locking Num Lock
0xff,//132	0x84	Keyboard Locking Scroll Lock
0xff,//133	0x85	Keypad Comma
0xff,//134	0x86	Keypad Equal Sign
0xff,//135	0x87	Keyboard International1
0xff,//136	0x88	Keyboard International2
0xff,//137	0x89	Keyboard International3
0xff,//138	0x8A	Keyboard International4
0xff,//139	0x8B	Keyboard International5
0xff,//140	0x8C	Keyboard International6
0xff,//141	0x8D	Keyboard International7
0xff,//142	0x8E	Keyboard International8
0xff,//143	0x8F	Keyboard International9
0xff,//144	0x90	Keyboard LANG1
0xff,//145	0x91	Keyboard LANG2
0xff,//146	0x92	Keyboard LANG3
0xff,//147	0x93	Keyboard LANG4
0xff,//148	0x94	Keyboard LANG5
0xff,//149	0x95	Keyboard LANG6
0xff,//150	0x96	Keyboard LANG7
0xff,//151	0x97	Keyboard LANG8
0xff,//152	0x98	Keyboard LANG9
0xff,//153	0x99	Keyboard Alternate Erase
0xff,//154	0x9A	Keyboard SysReq?/Attention
0xff,//155	0x9B	Keyboard Cancel
0xff,//156	0x9C	Keyboard Clear
0xff,//157	0x9D	Keyboard Prior
0xff,//158	0x9E	Keyboard Return
0xff,//159	0x9F	Keyboard Separator
0xff,//160	0xA0	Keyboard Out
0xff,//161	0xA1	Keyboard Oper
0xff,//162	0xA2	Keyboard Clear/Again
0xff,//163	0xA3	Keyboard CrSel?/Props
0xff,//164	0xA4	Keyboard ExSel?
0xff,//224	0xE0	Keyboard LeftControl?
0xff,//225	0xE1	Keyboard LeftShift?
0xff,//226	0xE2	Keyboard LeftAlt?
0xff,//227	0xE3	Keyboard Left GUI
0xff,//228	0xE4	Keyboard RightControl?
0xff,//229	0xE5	Keyboard RightShift?
0xff,//230	0xE6	Keyboard RightAlt?
0xff,//231	0xE7	Keyboard Right GUI
0xff//232-65535	0xE8-0xFFFF	Reserved
};

#define SHIFT_KEY_700 0x80
#define SHIFT_KEY_80C 0x85
unsigned char *ak_tbl;
unsigned char *ak_tbl_s;
unsigned char *hid_to_emu;
unsigned char shift_code;

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
//void setWiFi();
//void deleteWiFi();
void soundSetting();
void PCGSetting();
void doSendCommand(String inputString);
int set_mztype(void);

boolean sendBreakFlag;

String inputStringEx;

bool suspendScrnThreadFlag;

MZ_CONFIG mzConfig;

bool btKeyboardConnect = false;

//------------------------------------------------
// Memory Allocation for MZ
//------------------------------------------------
int mz_alloc_mem(void)
{
  int result = 0;

  /* Main Memory */
  //mem = (UINT8*)ps_malloc(64*1024);
  //mem = (UINT8*)ps_malloc((4 + 6 + 4 + 64) * 1024);
  mem = (UINT8*)malloc((4 + 6 + 4 + 64) * 1024);

  if (mem == NULL)
  {
    return -1;
  }

  /* Junk(Dummy) Memory */
  //junk = (UINT8*)ps_malloc(4096);
  junk = (UINT8*)malloc(4096);
  if (junk == NULL)
  {
    return -1;
  }

  /* MZT file Memory */
  //mzt_buf = (UINT32*)ps_malloc(4*64*1024);
  //mzt_buf = (UINT32*)ps_malloc(16 * 64 * 1024);
  //mzt_buf = (UINT32*)malloc(16 * 64 * 1024);
  //if (mzt_buf == NULL)
  //{
  //    return -1;
  //  }

  /* ROM FONT */
  //mz_font = (uint8_t*)ps_malloc(ROMFONT_SIZE);
  mz_font = (uint8_t*)malloc(ROMFONT_SIZE);
  if (mz_font == NULL)
  {
    result = -1;
  }

  /* PCG-700 FONT */
  //pcg700_font = (uint8_t*)ps_malloc(PCG700_SIZE);
  pcg700_font = (uint8_t*)malloc(PCG700_SIZE);
  if (pcg700_font == NULL)
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
  if (pcg700_font)
  {
    free(pcg700_font);
  }

  if (mz_font)
  {
    free(mz_font);
  }

  if (mzt_buf)
  {
    free(mzt_buf);
  }

  if (junk)
  {
    free(junk);
  }

  if (mem)
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
  if (romFile.length() == 0) {
    romFile = DEFAULT_ROM_FILE;
  }
  String romPath = String(ROM_DIRECTORY) + "/" + romFile;
  Serial.println("ROM PATH:" + romPath);

  if (SPIFFS.exists(romPath) == false) {
    m5lcd.println("ROM FILE NOT EXIST");
    m5lcd.printf("[%s]", romPath);
  }

  File dataFile = SPIFFS.open(romPath, FILE_READ);

  int offset = 0;
  if (dataFile) {
    while (dataFile.available()) {
      *(mem + offset) = dataFile.read();
      offset++;
    }
    dataFile.close();
    m5lcd.print("ROM FILE:");
    m5lcd.println(romPath);
    Serial.println(":END READ ROM:" + romPath);
  } else {
    m5lcd.print("ROM FILE FAIL:");
    m5lcd.println(romPath);
    Serial.println("FAIL END READ ROM:" + romPath);
  }
  mzConfig.mzMode = set_mztype();
  Serial.print("MZ TYPE=");
  Serial.println(mzConfig.mzMode);

  if (mzConfig.mzMode == MZMODE_80) {
    ak_tbl = ak_tbl_80c;
    ak_tbl_s = ak_tbl_s_80c;
    hid_to_emu = hid_to_mz80c;
    shift_code = SHIFT_KEY_80C;
  } else {
    ak_tbl = ak_tbl_700;
    ak_tbl_s = ak_tbl_s_700;
    hid_to_emu = hid_to_mz700;
    shift_code = SHIFT_KEY_700;
  }

}

int set_mztype(void) {
  if (!strncmp((const char*)(mem + 0x6F3), "1Z-009", 5))
  {
    return MZMODE_700;
  }
  else if (!strncmp((const char*)(mem + 0x6F3), "1Z-013", 5))
  {
    return MZMODE_700;
  }
  else if (!strncmp((const char*)(mem + 0x142), "MZ\x90\x37\x30\x30", 6))
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
  memset(mem + VID_START, 0, 4 * 1024);
  memset(mem + RAM_START, 0, 64 * 1024);

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
  m5lcd.println("M5Z-700 START");

  hid_init("emu32"); 
  m5lcd.println("HID START");
  btKeyboardConnect = false;
  M5.dis.drawpix(0, dispColor(50,0,0));
  
  suspendScrnThreadFlag = false;

  c_bright = GREEN;
  serialKeyCode = 0;

  sendBreakFlag = false;

  //pinMode(buttonApin, INPUT_PULLUP);
  //pinMode(buttonBpin, INPUT_PULLUP);
  joyPadPushed_U = false;
  joyPadPushed_D = false;
  joyPadPushed_L = false;
  joyPadPushed_R = false;
  joyPadPushed_A = false;
  joyPadPushed_B = false;
  joyPadPushed_Press = false;
  enableJoyPadButton = false;

  keyCheckCount = 0;
  preKeyCode = -1;
  inputStringMode = false;

  Serial.println("Screen init:Start");
  mz_screen_init();
  Serial.println("Screen init:OK");
  mz_alloc_mem();
  Serial.println("Alloc mem:OK");

  makePWM();
  Serial.println("MakePWM:OK");
  // ＭＺのモニタのセットアップ
  mz_mon_setup();
  Serial.println("Monitor:OK");
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
 Serial.println("mz_reset:OK");
  setup_cpuspeed(1);
   Serial.println("setup_cpuspeed:OK");
  Z80_IRQ = 0;
  //	Z80_Trap = 0x0556;

  // start the CPU emulation

  //Serial.println("START READ TAPE!");
  //String message = "Tape Setting";
  //updateStatusArea(message.c_str());

  //if (strlen(mzConfig.tapeFile) == 0) {
  //  set_mztData(DEFAULT_TAPE_FILE);
  //} else {
  //  set_mztData(mzConfig.tapeFile);
  //}
  //Serial.println("END READ TAPE!");

  //updateStatusArea("");
  
  /* スレッド　開始 */
  //Serial.println("START_THREAD");
  //start_thread();
  
  m5lcd.fillScreen(TFT_BLACK);
  
  delay(100);

  while (!intByUser())
  {
    timeTmp = millis();
    if (!Z80_Execute()) break;

    scrn_draw(); 
    keyCheck();
    hid_update();
    uint8_t buf[64];
    int n = hid_get(buf,sizeof(buf));    // called from emulation loop
    if(n != -1){ //Bluetoothキーボード接続完了…と思われる。
      if(btKeyboardConnect == false){
        btKeyboardConnect = true;
        //接続したらLEDを緑に。
        M5.dis.drawpix(0, dispColor(0,50,0));
        updateStatusArea("");
      }
    }
    if (n > 0){
          gui_hid(buf,n);
    }
    M5.update();

    //if(M5.Btn.wasPressed()){
    //  setMztToMemory("S-BASIC.MZT");
    //}
    //if (M5.BtnA.wasPressed()) {
    //  if (ts700.mzt_settape != 0 && ts700.cmt_play == 0)
    //  {
    //    ts700.cmt_play = 1;
    //    ts700.mzt_start = ts700.cpu_tstates;
    //    ts700.cmt_tstates = 1;
    //    setup_cpuspeed(5);
    //    sysst.motor = 1;
    //    xferFlag |= SYST_MOTOR;
    //  } else {
    //    String message = "TAPE CAN'T START";
    //    updateStatusArea(message.c_str());
    //   delay(2000);
    //    Serial.println("TAPE CAN'T START");
    //  }
    //}
    if (M5.Btn.wasReleasefor(1000)) {
      //メニュー表示
      set_scren_update_valid_flag(false);
      suspendScrnThreadFlag = true;
      delay(100);
      systemMenu();
      delay(100);
      m5lcd.fillScreen(TFT_BLACK);
      suspendScrnThreadFlag = false;
      set_scren_update_valid_flag(true);
    }else if (M5.Btn.wasReleased()) {
      set_scren_update_valid_flag(false);
      suspendScrnThreadFlag = true;
      delay(100);
      selectMzt();
      delay(100);
      m5lcd.fillScreen(TFT_BLACK);
      suspendScrnThreadFlag = false;
      set_scren_update_valid_flag(true);
    }

      
    syncTmp = millis();
    if (synctime - (syncTmp - timeTmp) > 0) {
      delay(synctime - (syncTmp - timeTmp));
    } else {
      delay(1);
    }
  }

}

//------------------------------------------------------------
// CPU速度を設定 (10-100)
//------------------------------------------------------------
void setup_cpuspeed(int mul) {
  int _iperiod;

  _iperiod = (CPU_SPEED * CpuSpeed * mul) / (100 * IFreq);

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
}

//--------------------------------------------------------------
// スレッドの後始末
//--------------------------------------------------------------
int end_thread(void)
{
  return 0;
}

void suspendScrnThread(bool flag) {
  suspendScrnThreadFlag = flag;
  delay(100);
}

//--------------------------------------------------------------
// 画面描画
//--------------------------------------------------------------
void scrn_draw()
{
  long synctime = 50;// 20fps
  long timeTmp;
  long vsyncTmp;
  if (suspendScrnThreadFlag == false) {
    // 画面更新処理
    hw700.retrace = 1;											/* retrace = 0 : in v-blnk */
    vblnk_start(); 
    timeTmp = millis();

    //前回から時間が経っていれば描画
    if (synctime - (vsyncTmp - timeTmp) > 0) {
      update_scrn();												/* 画面描画 */
      vsyncTmp = millis();
    }
  }
  return;
}

//--------------------------------------------------------------
// キーボード入力チェック
//--------------------------------------------------------------
void keyCheck() {
  //入力がある場合、5回は入力のまま。
  if (preKeyCode != -1) {
    keyCheckCount++;
    if (keyCheckCount < 5) {
      return;
    }
    mz_keyup_sub(preKeyCode);
    mz_keyup_sub(shift_code);
    preKeyCode = -1;
    keyCheckCount = 0;
  }
  if (sendBreakFlag == true) { //SHIFT+BREAK送信
    mz_keydown_sub(shift_code);
    if (mzConfig.mzMode == MZMODE_700) {
      mz_keydown_sub(0x87);
      preKeyCode = 0x87;
    } else {
      mz_keydown_sub(ak_tbl_s[3]);
      preKeyCode = ak_tbl_s[3];
    }
    sendBreakFlag = false;
    return;
  }

  char inKeyCode = 0;
  checkJoyPad();

  //特別扱いのキー（ジョイパッドボタン用）
  bool ctrlFlag = false;
  bool shiftFlag = false;

  //inputStringExが入っている場合は、１文字ずつ入力処理
  if (inputStringEx.length() > 0) {
    inKeyCode = inputStringEx.charAt(0);
    if (inKeyCode == 'C') { //CTRL
      ctrlFlag = true;
    }
    if (inKeyCode == 'S') { //SHIFT
      shiftFlag = true;
    }
    inputStringEx = inputStringEx.substring(1);
  } else {
    if (inputStringMode == true) {
      inKeyCode = 0x0D;
      inputStringMode = false;
    }
  }

  if (ctrlFlag == false && shiftFlag == false) {
    //キー入力
    if (inKeyCode == 0) {
      inKeyCode = checkSerialKey();
    }
    if (inKeyCode == 0) {
      inKeyCode = checkI2cKeyboard();
    }
    if (inKeyCode == 0) {
      return;
    }
    if (ak_tbl[inKeyCode] == 0xff)
    {
      //何もしない
    }
    else if (ak_tbl[inKeyCode] == shift_code)
    {
      mz_keydown_sub(shift_code);
      mz_keydown_sub(ak_tbl_s[inKeyCode]);
    }
    else
    {
      mz_keydown_sub(ak_tbl[inKeyCode]);
    }
    preKeyCode = ak_tbl[inKeyCode];
  } else {
    if (ctrlFlag == true) {
      mz_keydown_sub(0x86);
      preKeyCode = 0x86;
      ctrlFlag = false;
    }
    if (shiftFlag == true) {
      mz_keydown_sub(0x80);
      preKeyCode = 0x80;
      shiftFlag = false;

    }

  }
}

//--------------------------------------------------------------
// シリアル入力
//--------------------------------------------------------------
int checkSerialKey()
{
  if (Serial.available() > 0) {
    serialKeyCode = Serial.read();
    if ( serialKeyCode == 27 ) { //ESC
      serialKeyCode = Serial.read();
      if (serialKeyCode == 91) {
        serialKeyCode = Serial.read();
        switch (serialKeyCode) {
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
      } else if (serialKeyCode == 255)
      {
        //Serial.println("ESC");
        //serialKeyCode = 0x03;  //ESC -> SHIFT + BREAK
        sendBreakFlag = true; //うまくキーコード処理できなかったのでWebからのBreak送信扱いにします。
        serialKeyCode = 0;
      }
    }
    if (serialKeyCode == 127) { //BackSpace
      serialKeyCode = 0x17;
    }
    while (Serial.available() > 0 && Serial.read() != -1);
  }
  return serialKeyCode;
}

//--------------------------------------------------------------
// JoyPadLogic
//--------------------------------------------------------------

void checkJoyPad() {
  if (mzConfig.mzMode != MZMODE_700) {
    return;
  }

  int joyX, joyY, joyPress, buttonA, buttonB;
  joyX = 0;

  if (Wire.requestFrom(0x52, 3) >= 3) {
    if (Wire.available()) {
      joyX = Wire.read(); //X（ジョイパッド的には Y）
    }
    if (Wire.available()) {
      joyY = Wire.read(); //Y（ジョイパッド的には X）
    }
    if (Wire.available()) {
      joyPress = Wire.read(); //Press
    }
  }


  if (joyX == 0) {
    return; //接続されていない
  }

/*
  if (digitalRead(buttonApin) == LOW)
  {
    buttonA = 1;
  } else {
    buttonA = 0;
  }
  if (digitalRead(buttonBpin) == LOW)
  {
    buttonB = 1;
  } else {
    buttonB = 0;
  }

  if (enableJoyPadButton == false) {
    if (buttonA == 1) { //ボタン接続されていなければボタンAが0、Bが1のままなので、1度Aが1になれば、以降はボタン接続アリとする。
      enableJoyPadButton = true;
    } else {
      //接続されていない場合は押されてない扱いとする。
      buttonA = 0;
      buttonB = 0;
    }
  }
*/
  if (joyPadPushed_U == false && joyX < 80) {
    //カーソル上を押す
    mz_keydown_sub(0x75);
    joyPadPushed_U = true;
  } else if (joyPadPushed_U == true && joyX > 80) {
    //カーソル上を戻す
    mz_keyup_sub(0x75);
    joyPadPushed_U = false;
  }
  if (joyPadPushed_D == false && joyX > 160) {
    //カーソル下を押す
    mz_keydown_sub(0x74);
    joyPadPushed_D  = true;
  } else if (joyPadPushed_D == true && joyX < 160) {
    //カーソル下を戻す
    mz_keyup_sub(0x74);
    joyPadPushed_D = false;
  }
  if (joyPadPushed_L == false && joyY < 80) {
    //カーソル左を押す
    mz_keydown_sub(0x72);
    joyPadPushed_L = true;
  } else if (joyPadPushed_L == true && joyY > 80) {
    //カーソル左を戻す
    mz_keyup_sub(0x72);
    joyPadPushed_L = false;
  }
  if (joyPadPushed_R == false && joyY > 160) {
    //カーソル右を押す
    mz_keydown_sub(0x73);
    joyPadPushed_R = true;
  } else if (joyPadPushed_R == true && joyY < 160) {
    //カーソル右を戻す
    mz_keyup_sub(0x73);
    joyPadPushed_R = false;
  }
  if (joyPadPushed_A == false && buttonA == 1) {
    //CTRL & SPACE & 1 順に押下
    if (inputStringEx.length() == 0) {
      inputStringEx = " 1C"; //C=CTRL
    }
    joyPadPushed_A = true;
  } else if (joyPadPushed_A == true && buttonA == 0) {
    joyPadPushed_A = false;
  }
  if (joyPadPushed_B == false && buttonB == 1) {
    if (inputStringEx.length() == 0) {
      //SHIFT & 3 順に押下
      inputStringEx = "3S"; //S=SHIFT
    }
  } else if (joyPadPushed_B == true && buttonB == 0) {
    joyPadPushed_B = false;
  }
  if (joyPadPushed_Press == false && joyPress == 1) {
    //CR&S押下
    if (inputStringEx.length() == 0) {
      inputStringEx = "s";
      inputStringMode = true; //このフラグによりCR が押下される
    }
    joyPadPushed_Press = true;

  } else if (joyPadPushed_Press == true && joyPress == 0) {
    joyPadPushed_Press = false;
  }
}
//--------------------------------------------------------------
// I2C Keyboard Logic
//--------------------------------------------------------------
int checkI2cKeyboard() {
  uint8_t i2cKeyCode = 0;
  if (Wire.requestFrom(CARDKB_ADDR, 1)) { // request 1 byte from keyboard
    while (Wire.available()) {
      i2cKeyCode = Wire.read();                  // receive a byte as
      break;
    }
  }
  if (i2cKeyCode == 0) {
    return 0;
  }

  //Serial.println(i2cKeyCode, HEX);

  //特殊キー
  switch (i2cKeyCode) {
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
      return 0;
    default:
      break;
  }
  return i2cKeyCode;

}

String selectMzt() {
  File fileRoot;
  String fileList[MAX_FILES];

  m5lcd.fillScreen(TFT_BLACK);
  m5lcd.setCursor(0, 0);
  m5lcd.setTextSize(1);

  String fileDir = "/"; 
  /*
  if (fileDir.endsWith("/") == true)
  {
    fileDir = fileDir.substring(0, fileDir.length() - 1);
  }

  if (SPIFFS.exists(fileDir) == false) {
    m5lcd.println("DIR NOT EXIST");
    m5lcd.printf("[SPIFFS:%s/]", fileDir.c_str());
    delay(5000);
    m5lcd.fillScreen(TFT_BLACK);
    return "";
  }
  */
  fileRoot = SPIFFS.open(fileDir);
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
      String ext = fileName.substring(fileName.lastIndexOf(".") + 1);
      ext.toUpperCase();
      if(ext.equals("MZT")==false && ext.equals("MZF")==false && ext.equals("M12")==false){
        continue;
      }

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
  bool isBottonLongPress = false;
  while (true)
  {
    if (needRedraw == true)
    {
      m5lcd.setCursor(0, 0);
      startIndex = selectIndex - 5;
      if (startIndex < 0)
      {
        startIndex = 0;
      }
      endIndex = startIndex + 24;
      if (endIndex > fileListCount)
      {
        endIndex = fileListCount;
        //startIndex = endIndex - 12;
        startIndex = endIndex - 24;
        if (startIndex < 0) {
          startIndex = 0;
        }
      }
      if (preStartIndex != startIndex) {
        m5lcd.fillRect(0, 0, 320, 220, 0);
        preStartIndex = startIndex;
      }
      for (int index = startIndex; index < endIndex + 1; index++)
      {
        if (index == selectIndex)
        {
          if (isBottonLongPress == true && index != 0) {
            m5lcd.setTextColor(TFT_RED);
          } else {
            m5lcd.setTextColor(TFT_GREEN);
          }
        }
        else
        {
          m5lcd.setTextColor(TFT_WHITE);
        }
        if (index == 0)
        {
          m5lcd.println("[BACK]");
        }
        else
        {
          m5lcd.println(fileList[index - 1].substring(0, 26));
        }
      }
      m5lcd.setTextColor(TFT_WHITE);
/*
      m5lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("U P", 53, 240 - 17, 1);
      m5lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
      m5lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
*/
      m5lcd.drawString("LONG PRESS:SELECT", 0, 200 - 17, 1);
      needRedraw = false;
    }
    M5.update();
    //if (M5.BtnA.pressedFor(500)) {
    //  isLongPress = true;
    //  selectIndex--;
    //  if (selectIndex < 0)
    //  {
    //    selectIndex = fileListCount;
    //  }
    //  needRedraw = true;
    //  delay(100);
   // }
   // if (M5.BtnA.wasReleased())
   // {
    //  if (isLongPress == true)
     // {
     //   isLongPress = false;
     // } else {
     //   selectIndex--;
     //   if (selectIndex < 0)
     //   {
     //     selectIndex = fileListCount;
     //   }
     //   needRedraw = true;
     // }
    //}
    //if (M5.BtnB.pressedFor(1000)) {
    //  if (isBottonBLongPress == false) {
    //    isBottonBLongPress = true;
    //    needRedraw = true;
    //  }
    //}
    if(M5.Btn.pressedFor(1000)){ //長押しになった場合色を変える
      if( isBottonLongPress == false){
             isBottonLongPress = true; 
            needRedraw = true;
      }
    }
    if (M5.Btn.wasReleasefor(1000))
    {
      if (selectIndex == 0)
      {
        //何もせず戻る
        m5lcd.fillScreen(TFT_BLACK);
        delay(10);
        return "";
      }
      else
      {
        Serial.print("select:");
        Serial.println(fileList[selectIndex - 1]);
        delay(10);
        m5lcd.fillScreen(TFT_BLACK);
        //M5Atomはメモリ読み込みのみ
        //if (isBottonBLongPress == false) {
        //  strncpy(mzConfig.tapeFile, fileList[selectIndex - 1].c_str(), 50);
        //  set_mztData(mzConfig.tapeFile);
        //  ts700.cmt_play = 0;

        //  m5lcd.setCursor(0, 0);
        //  m5lcd.print("Set:");
        //  m5lcd.print(fileList[selectIndex - 1]);
        //  delay(2000);
        //  m5lcd.fillScreen(TFT_BLACK);
        //  delay(10);
        //  return fileList[selectIndex - 1];
        //} else {
          m5lcd.setCursor(0, 0);
          m5lcd.print("SET MZT TO MEMORY:");
          m5lcd.print(fileList[selectIndex - 1]);
          delay(2000);
          m5lcd.fillScreen(TFT_BLACK);
          delay(10);
          //メモリに読み込んで
          setMztToMemory(fileList[selectIndex - 1]);
          return fileList[selectIndex - 1];
        //}
      }
    }else if (M5.Btn.wasReleased()){
      selectIndex++;
      if (selectIndex > fileListCount)
      {
        selectIndex = 0;
      }
      needRedraw = true;
    }
    delay(100);
  }
}
int setMztToMemory(String mztFile) {

  String filePath = TAPE_DIRECTORY;
  filePath += "/" + mztFile;
  /*
    File fd = SD.open(filePath, FILE_READ);
    Serial.println("fileOpen");
    if(fd == NULL)
    {
    Serial.print("can't open:");
    Serial.println(filePath);
    delay(100);
    fd.close();
    return false;
    }
    uint8_t header[128];
    fd.read((byte*)header, sizeof(header)) ;
    int size = header[0x12] | (header[0x13] << 8);
    int offs = header[0x14] | (header[0x15] << 8);
    int addr = header[0x16] | (header[0x17] << 8);
    Serial.printf("Read MZT to RAM[size:%X][offs:%X][addr:%X]\n",size,offs,addr);
    fd.read(mem + offs + RAM_START, size) ;
    fd.close();

    //まだうまくうごきません…。
    mz_reset();
    mem[ROM_START+L_TMPEX] = (addr & 0xFF);
    mem[ROM_START+L_TMPEX+1] = (addr >> 8);
    Z80_Reset();
  */

  File fd = SPIFFS.open(filePath, FILE_READ);
  Serial.println("fileOpen");
  if (fd == NULL)
  {
    Serial.print("can't open:");
    Serial.println(filePath);
    delay(100);
    fd.close();
    return false;
  }

  fd.seek(0, SeekEnd);
  fd.flush();
  int remain = fd.position();
  fd.seek(0, SeekSet);

  uint8_t header[128];
  fd.read((byte*)header, 8);


  if (header[0] == 'm' && header[1] == 'z' && header[2] == '2' && header[3] == '0') {
    // this is mzf format
    remain -= 8;

    while (remain >= 128 + 2 + 2) {
      fd.read(header, sizeof(header));
      fd.seek(2, SeekCur); // skip check sum
      remain -= 128 + 2;

      int size = header[0x12] | (header[0x13] << 8);
      int offs = header[0x14] | (header[0x15] << 8);
      int addr = header[0x16] | (header[0x17] << 8);

      Serial.printf("MZ20[size:%X][offs:%X][addr:%X]\n", size, offs, addr);

      if (remain >= size + 2) {
        fd.read(mem + offs + RAM_START, size);
        fd.seek(2, SeekCur); // skip check sum
      }
      remain -= size + 2;
      Z80_Regs StartRegs;
      StartRegs.PC.D = addr;
      Z80_SetRegs(&StartRegs);
    }
  } else {
    // this is mzt format
    fd.seek(0, SeekSet);

    while (remain >= 128) {
      fd.read(header, sizeof(header));
      remain -= 128;

      int size = header[0x12] | (header[0x13] << 8);
      int offs = header[0x14] | (header[0x15] << 8);
      int addr = header[0x16] | (header[0x17] << 8);

      Serial.printf("[size:%X][offs:%X][addr:%X]\n", size, offs, addr);

      if (remain >= size) {
        fd.read(mem + offs + RAM_START, size);
      }
      remain -= size;
      Z80_Regs StartRegs;
      Z80_GetRegs(&StartRegs);
      StartRegs.PC.D = addr;
      Z80_SetRegs(&StartRegs);
    }
  }

  fd.close();

  return true;
}

#define MENU_ITEM_COUNT 8
#define PCG_INDEX 5
#define SOUNT_INDEX 6
#define LCD_INDEX 8
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
    ""
  };

  delay(10);
  m5lcd.fillScreen(TFT_BLACK);
  delay(10);
  m5lcd.setTextSize(1);
  bool needRedraw = true;

  int menuItemCount = 0;
  while (menuItem[menuItemCount] != "") {
    menuItemCount++;
  }

  int selectIndex = 0;
  delay(100);
  M5.update();

  while (true)
  {
    if (needRedraw == true)
    {
      m5lcd.setCursor(0, 0);
      for (int index = 0; index < menuItemCount; index++)
      {
        if (index == selectIndex)
        {
          m5lcd.setTextColor(TFT_GREEN);
        }
        else
        {
          m5lcd.setTextColor(TFT_WHITE);
        }
        String curItem = menuItem[index];
        if (index == PCG_INDEX) {
          curItem = curItem + ((hw700.pcg700_mode == 1) ? String(": ON") : String(": OFF"));
        }
/*
        if (index == SOUNT_INDEX) {
          curItem = curItem + (mzConfig.enableSound ? String(": ON") : String(": OFF"));
        }
        if (index == LCD_INDEX) {
          if (ldcMode == 0) {
            curItem = curItem + ":INTERNAL";
          } else if (ldcMode == 1) {
            curItem = curItem + ":EXTERNAL with AA";
          } else {
            curItem = curItem + ":EXTERNAL without AA";
          }
        }
*/
        m5lcd.println(curItem);
      }
/*
      m5lcd.setTextColor(TFT_WHITE);
      m5lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("U P", 53, 240 - 17, 1);
      m5lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
      m5lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
*/
      m5lcd.setTextColor(TFT_WHITE);
      m5lcd.drawString("LONG PRESS:SELECT", 0, 200 - 17, 1);
      needRedraw = false;
    }
    M5.update();
    

    //if (M5.BtnA.wasReleased())
    //{
    //  selectIndex--;
    //  if (selectIndex < 0)
    //  {
    //    selectIndex = menuItemCount - 1;
    //  }
    //  needRedraw = true;
    //}

    if (M5.Btn.wasReleasefor(1000))
    {
      if (selectIndex == 0)
      {
        m5lcd.fillScreen(TFT_BLACK);
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
          hw700.pcg700_mode = (hw700.pcg700_mode == 1) ? 0 : 1;
          break;
        case SOUNT_INDEX:
          mzConfig.enableSound = mzConfig.enableSound ? false : true;
          break;
        case 7:
          inputStringEx = "load";
          inputStringMode = true;
          m5lcd.fillScreen(TFT_BLACK);
          delay(10);
          return;
        case LCD_INDEX:
          ldcMode = ldcMode + 1;
          if (ldcMode >= 3) {
            ldcMode = 0;
          }
          break;

        default:
          m5lcd.fillScreen(TFT_BLACK);
          delay(10);
          return;
      }
      if (selectIndex >= 1 && selectIndex <= 4) {
        m5lcd.fillScreen(TFT_BLACK);
        m5lcd.setCursor(0, 0);
        m5lcd.print("Reset MZ:");
        m5lcd.println(mzConfig.romFile);
        delay(2000);
        monrom_load();
        mz_reset();
        return;
      }
      m5lcd.fillScreen(TFT_BLACK);
      m5lcd.setCursor(0, 0);
      needRedraw = true;
    }else if (M5.Btn.wasReleased()) {
      selectIndex++;
      if (selectIndex >= menuItemCount)
      {
        selectIndex = 0;
      }
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
    for (int i = 0; i < fileListCount - 1; i++ ) {
      name1 = fileList[i];
      name1.toUpperCase();
      name2 = fileList[i + 1];
      name2.toUpperCase();
      if (name1.compareTo(name2) > 0) {
        temp = fileList[i];
        fileList[i] = fileList[i + 1];
        fileList[i + 1] = temp;
        swapped = true;
      }
    }
  } while (swapped);
}

void gui_msg(const char* msg)         // temporarily display a msg
{
    Serial.println(msg);
    //if(btKeyboardConnect == false){
    //  updateStatusArea(msg);
    //}
}
void sys_msg(const char* msg) {
    Serial.println(msg);
    //if(btKeyboardConnect == false){
    updateStatusArea(msg);
    //}
}

void gui_hid(const uint8_t* hid, int len)  // Parse HID event
{

    if (hid[0] != 0xA1)
        return;
    /*
    for (int i = 0; i < len; i++)
        printf("%02X",hid[i]);
    printf("\n");
    */
    switch (hid[1]) {
        case 0x01: keyboard(hid+1,len-1);   break;   // parse keyboard and maintain 1 key state
//        case 0x32: wii();                   break;   // parse wii stuff: generic?
//        case 0x42: ir(hid+2,len);           break;   // ir joy
    }

}


String stringData[] = {
    " "," "," "," ",
    "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z",
    "1","2","3","4","5","6","7","8","9","0"
};
String stringShiftData[] = {
    " "," "," "," ",
    "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "!","\"","#","$","%","&","'","(",")"," "
};
static int _last_key = 0;
//https://wiki.onakasuita.org/pukiwiki/?HID%2F%E3%82%AD%E3%83%BC%E3%82%B3%E3%83%BC%E3%83%89
static void keyboard(const uint8_t* d, int len)
{
    updateStatusArea("");
    int mods = d[1];          // can we use hid mods instead of SDL? TODO
    int key_code = d[3];      // only care about first key
    //shift Key SHIFT_KEY_80C
    if((mods & 0b00100010) > 0){
      mz_keydown_sub(shift_code);         
    }else{
      mz_keyup_sub(shift_code);
    }

    if (key_code != _last_key) {
        if(0 < _last_key && _last_key < 232){
          mz_keyup_sub(hid_to_emu[_last_key]);
        }
        if (key_code) {
            _last_key = key_code;
        } else {
            _last_key = 0;
        }
        if(0 < key_code && key_code < 232){
          mz_keydown_sub(hid_to_emu[key_code]);
        }
        Serial.printf("mods:%d keyCode:%d\n",mods,key_code);
    }
}
