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
#if defined(_M5STICKCPLUS)
#include <M5GFX.h>
#include <M5StickCPlus.h>
#elif defined(_M5STACK)
#include <M5Stack.h>
#include <M5GFX.h>
#elif defined(_M5ATOMS3)
#include <M5Unified.h>
#include <Wire.h>
//USBキーボード
//https://github.com/touchgadget/esp32-usb-host-demos
#include <elapsedMillis.h>
#include <usb/usb_host.h>
#include "usbhhelp.hpp"
bool isKeyboard = false;
bool isKeyboardReady = false;
uint8_t KeyboardInterval;
bool isKeyboardPolling = false;
elapsedMillis KeyboardTimer;

const size_t KEYBOARD_IN_BUFFER_SIZE = 8;
usb_transfer_t *KeyboardIn = NULL;
void check_interface_desc_boot_keyboard(const void *p);
void prepare_endpoint(const void *p);
void show_config_desc_full(const usb_config_desc_t *config_desc)
{
   // Full decode of config desc.
  const uint8_t *p = &config_desc->val[0];
  static uint8_t USB_Class = 0;
  uint8_t bLength;
  for (int i = 0; i < config_desc->wTotalLength; i+=bLength, p+=bLength) {
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1);
      switch (bDescriptorType) {
        case USB_B_DESCRIPTOR_TYPE_DEVICE:
          ESP_LOGI("", "USB Device Descriptor should not appear in config");
          break;
        case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
          //show_config_desc(p);
          break;
        case USB_B_DESCRIPTOR_TYPE_STRING:
          ESP_LOGI("", "USB string desc TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE:
          //USB_Class = show_interface_desc(p);
          check_interface_desc_boot_keyboard(p);
          break;
        case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
          //show_endpoint_desc(p);
          if (isKeyboard && KeyboardIn == NULL) prepare_endpoint(p);
          break;
        case USB_B_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
          // Should not be config config?
          ESP_LOGI("", "USB device qual desc TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION:
          // Should not be config config?
          ESP_LOGI("", "USB Other Speed TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_POWER:
          // Should not be config config?
          ESP_LOGI("", "USB Interface Power TBD");
          break;
        case 0x21:
          if (USB_Class == USB_CLASS_HID) {
            //show_hid_desc(p);
          }
          break;
        default:
          ESP_LOGI("", "Unknown USB Descriptor Type: 0x%x", bDescriptorType);
          break;
      }
    }
    else {
      ESP_LOGI("", "USB Descriptor invalid");
      return;
    }
  }
}
#else
#include <M5Atom.h>  
#endif

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

#if defined(USE_HID)
#include "hid_server/hid_server.h"
#include "hid_server/hci_server.h"
#endif

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

#if defined(USE_HID)
void gui_hid(const uint8_t* hid, int len);  // Parse HID event
#endif

static bool saveMZTImage();

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

bool wonderHouseMode;
int wonderHouseKeyIndex;
int wonderHouseKey();

#define SyncTime	17									/* 1/60 sec.(milsecond) */

#define MAX_FILES 255 // this affects memory

String selectMzt();
bool firstLoadFlag;

int q_kbd;
typedef struct KBDMSG_t {
  long mtype;
  char len;
  unsigned char msg[80];
} KBDMSG;

#if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5STACK)
#else
CRGB dispColor(uint8_t g, uint8_t r, uint8_t b) {
  return (CRGB)((g << 16) | (r << 8) | b);
}
#endif

#define SHIFT_KEY_700 0x80
#define SFT7 0x80

//左シフト0x85,右シフト0x80
#define SHIFT_KEY_80C 0x80
#define SFT8 0x80

//MZ-80K/C キーマトリクス参考：http://www43.tok2.com/home/cmpslv/Mz80k/EnrMzk.htm
unsigned char ak_tbl_80c[] =
{
  0xff, 0xff, 0xff, SFT8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x65, 0xff, SFT8, 0xff, 0x84, 0xff, 0xff,
  0xff, 0x92, SFT8, 0x83, SFT8, 0x90, SFT8, 0x81, SFT8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

  // " "    !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
  0x91, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, 0x73, 0x05, 0x64, 0x74,
  //  0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
  0x14, 0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, SFT8, 0x54, SFT8, 0x25, SFT8, SFT8,
  //  @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
  SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8,
  //  P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
  SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, SFT8, 0xff, 0xff,
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
0xff,//0	0x00	Reserved (no event indicated)gui_hid
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
0x95,//47	0x2F	Keyboard [ and {
0x75,//48	0x30	Keyboard ] and }
0xff,//49	0x31	Keyboard \ and ｜
0xff,//50	0x32	Keyboard Non-US # and ~
0x54,//51	0x33	Keyboard ; and :
0xff,//52	0x34	Keyboard ' and "
0xff,//53	0x35	Keyboard Grave Accent and Tilde
0x73,//54	0x36	Keyboard, and <
0x64,//55	0x37	Keyboard . and >
0x74,//56	0x38	Keyboard / and ?
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
  //mzt_buf = (UINT32*)malloc(4 * 64 * 1024);
  //if (mzt_buf == NULL)
  //{
  //    return -1;
  //}

  /* ROM FONT */
  //mz_font = (uint8_t*)ps_malloc(ROMFONT_SIZE);
  #if defined (USE_EXT_LCD)||defined(_M5STICKCPLUS)
  #else
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
  #endif

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

  //if (mzt_buf)
  //{
//    free(mzt_buf);
//  }

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

  #if defined(_M5STACK)
  if (SD.exists(romPath) == false) {
  #else
  if (SPIFFS.exists(romPath) == false) {
  #endif
    m5lcd.println("ROM FILE NOT EXIST");
    m5lcd.printf("[%s]", romPath);
  }
  #if defined(_M5STACK)
  File dataFile = SD.open(romPath, FILE_READ);
  #else
  File dataFile = SPIFFS.open(romPath, FILE_READ);
  #endif
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
  
  #if defined(USE_HID)
  hid_init("emu32"); 
  m5lcd.println("HID START"); 
  #endif
  btKeyboardConnect = false;
  #if defined(_M5ATOMS3)
  usbh_setup(show_config_desc_full);
  #endif
  #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5STACK)
  #else
  M5.dis.drawpix(0, dispColor(50,0,0));
  #endif
  #if defined(USE_SPEAKER_G26)
  m5lcd.println("SPEAKER: ENABLE[G26]");
  m5lcd.println("CardKB: DISABLE");
  #elif defined(USE_SPEAKER_G25)
  m5lcd.println("SPEAKER: ENABLE[G25]");
  #elif defined(_M5STICKCPLUS)
  m5lcd.println("SPEAKER: ENABLE");
  m5lcd.println("CardKB: ENABLE");
  #endif

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

  firstLoadFlag = true;

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

#ifndef USE_SPEAKER_G26
    //M5Atomでスピーカーピンに G26使った場合、GROVE WIRE と排他
    keyCheck();
#endif
#if defined(USE_HID)
    hid_update(); //ESP32 + espressif32 5.2.0だと、なぜか、ここで落ちるようになってしまった…。

    uint8_t buf[64];
    
    int n = hid_get(buf,sizeof(buf));    // called from emulation loop
    if(n != -1){ //Bluetoothキーボード接続完了…と思われる。
      if(btKeyboardConnect == false){
        btKeyboardConnect = true;
        //接続したらLEDを緑に。
        #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)||defined(_M5STACK)
        #else
        M5.dis.drawpix(0, dispColor(0,50,0));
        #endif
        updateStatusArea("");
      }
    }
    if (n > 0){
          gui_hid(buf,n);
    }
#endif
#if defined(_M5ATOMS3)
  usbh_task();

  if (isKeyboardReady && !isKeyboardPolling && (KeyboardTimer > KeyboardInterval)) {
    KeyboardIn->num_bytes = 8;
    esp_err_t err = usb_host_transfer_submit(KeyboardIn);
    if (err != ESP_OK) {
      ESP_LOGI("", "usb_host_transfer_submit In fail: %x", err);
    }
    isKeyboardPolling = true;
    KeyboardTimer = 0;
  }
#endif
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
    #if defined(_M5STICKCPLUS)
    if (M5.BtnB.wasReleased()) {
    #elif defined(_M5STACK)
    if (M5.BtnC.wasReleased()) {
    #elif defined(_M5ATOMS3)
    if (M5.BtnA.pressedFor(1000)) {
      while(M5.BtnA.wasReleased()==false){ //離されるのを待つ
        M5.update();
        delay(100);
      }
    #else
    if (M5.Btn.wasReleasefor(1000)) {
    #endif
      //メニュー表示
      set_scren_update_valid_flag(false);
      suspendScrnThreadFlag = true;
      delay(100);
      systemMenu();
      delay(100);
      m5lcd.fillScreen(TFT_BLACK);
      suspendScrnThreadFlag = false;
      set_scren_update_valid_flag(true);
    #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)
    }else if (M5.BtnA.wasReleased()) {
    #elif defined(_M5STACK)
    }else if (M5.BtnB.wasReleased()) {
    #else
    }else if (M5.Btn.wasReleased()) {
    #endif
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
int delayInputKeyCode = 0;  //SHIFT併用時にkeyDownを少し遅らせる
void keyCheck() {
  //入力がある場合、5回は入力のまま。
  if (preKeyCode != -1) {
    keyCheckCount++;
    if(delayInputKeyCode != 0){
       mz_keydown_sub(delayInputKeyCode);
       delayInputKeyCode  = 0;
    }
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

    if (inKeyCode == 0 && wonderHouseMode == true) {
      inKeyCode = wonderHouseKey();
    }

    if (inKeyCode == 0) {
      return;
    }
    //Serial.printf("inkeyCode:%#x:%#x:%#x\n", inKeyCode,ak_tbl[inKeyCode],ak_tbl_s[inKeyCode]);

    if (ak_tbl[inKeyCode] == 0xff)
    {
      //何もしない
    }
    else if (ak_tbl[inKeyCode] == shift_code)
    {
      mz_keydown_sub(shift_code);
      //mz_keydown_sub(ak_tbl_s[inKeyCode]);
      delayInputKeyCode = ak_tbl_s[inKeyCode];
      preKeyCode = ak_tbl_s[inKeyCode];
    }
    else
    {
      mz_keydown_sub(ak_tbl[inKeyCode]);
      preKeyCode = ak_tbl[inKeyCode];
    }
    
  } else {
    if (ctrlFlag == true) {
      mz_keydown_sub(0x86);
      preKeyCode = 0x86;
      ctrlFlag = false;
    }
    if (shiftFlag == true) {
      mz_keydown_sub(shift_code);
      preKeyCode = shift_code;
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

long wonderHouseKeyDelayMillis = 0;


int wonderHouseKey(){
  //時間待ち
  if(wonderHouseMode == false){
    return 0;
  }
  long nowMillis = millis();
  char inputKey = 0;
  if(nowMillis > wonderHouseKeyDelayMillis){
    inputKey = wonderHouseKeyData[wonderHouseKeyIndex];
    if(inputKey == 'E'){
      wonderHouseMode = false;
      return 0;
    }
    if(wonderHouseKeyIndex == 0){
      wonderHouseKeyDelayMillis = nowMillis + 200 * 10;
    }else if(wonderHouseKeyIndex == 1){
      wonderHouseKeyDelayMillis = nowMillis + 1000 * 10;
    }else if(inputKey == '\n'){
      wonderHouseKeyDelayMillis = nowMillis + 1000 * 5;
      inputKey = 0x0D;
    }else{
      wonderHouseKeyDelayMillis = nowMillis + 100;
    }
    wonderHouseKeyIndex++;
  }
  return inputKey;
}

String selectMzt() {
	#if defined(USE_SPEAKER_G25)||defined(USE_SPEAKER_G26)||defined(_M5STICKCPLUS)
    ledcWriteTone(LEDC_CHANNEL_0, 0); // stop the tone playing:
	#endif	
  File fileRoot;
  String fileList[MAX_FILES];

  m5lcd.fillScreen(TFT_BLACK);
  m5lcd.setCursor(0, 0);
  #if defined(_M5STACK)
  m5lcd.setTextSize(2);
  #else
  m5lcd.setTextSize(1);
  #endif
  String fileDir = TAPE_DIRECTORY; 
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
  #if defined(_M5STACK)
  fileRoot = SD.open(fileDir);
  #else
  fileRoot = SPIFFS.open(fileDir);
  #endif
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
  bool isButtonLongPress = false;
  #if defined(_M5STICKCPLUS)||(_M5STACK)
  int dispfileCount = 12;
  #elif defined(_M5ATOMS3)||defined(USE_ST7735S)
  int dispfileCount = 15;
  #else
  int dispfileCount = 21;
  #endif
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
      endIndex = startIndex + dispfileCount;
      if (endIndex > fileListCount)
      {
        endIndex = fileListCount;
        //startIndex = endIndex - 12;
        startIndex = endIndex - dispfileCount;
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
          if (isButtonLongPress == true) {
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

  #if defined(_M5STICKCPLUS)
      m5lcd.drawString("LONG PRESS:SELECT", 0, 135 - 17, 1);
  #elif defined(_M5STACK)
      m5lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("U P", 53, 240 - 17, 1);
      m5lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
      m5lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
  #else
      m5lcd.drawString("LONG PRESS:SELECT", 0, 200 - 17, 1);
  #endif
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
    #if defined(_M5STACK)
    //長押し選択無し
    #else
    #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)
    if(M5.BtnA.pressedFor(1000)){ //長押しになった場合色を変える
    #else
    if(M5.Btn.pressedFor(1000)){ //長押しになった場合色を変える
    #endif
      if( isButtonLongPress == false){
             isButtonLongPress = true; 
            needRedraw = true;
      }
    }
    #endif
    #if defined(_M5STICKCPLUS)
    if (M5.BtnA.wasReleasefor(1000))
    #elif defined(_M5STACK)
    if (M5.BtnB.wasReleased())
    #elif defined(_M5ATOMS3)
    if (M5.BtnA.wasReleased() && isButtonLongPress == true)
    #else
    if (M5.Btn.wasReleasefor(1000))
    #endif
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

          if(firstLoadFlag == true){
            firstLoadFlag = false;
            //起動・またはリセット後の1回目はメモリ読み込み許可
            m5lcd.setCursor(0, 0);
            m5lcd.print("SET MZT FILE:");
            m5lcd.print(fileList[selectIndex - 1]);
            delay(2000);
            m5lcd.fillScreen(TFT_BLACK);
            delay(10);
            //メモリに読み込む
            setMztToMemory(fileList[selectIndex - 1]);
            return fileList[selectIndex - 1];
          }else{
            m5lcd.setCursor(0, 0);
            m5lcd.print("SET MZT TO CMT IMAGE AND START:");
            m5lcd.print(fileList[selectIndex - 1]);
            //delay(2000);読み込み遅いのでdelay無くても大丈夫。
            set_mztData(fileList[selectIndex - 1]);
            ts700.cmt_play = 1;
            ts700.mzt_start = ts700.cpu_tstates;
            ts700.cmt_tstates = 1;
            setup_cpuspeed(5);
            sysst.motor = 1; 
            xferFlag |= SYST_MOTOR;
            return fileList[selectIndex - 1];
          }
        //}
      }
    #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)
    }else if (M5.BtnA.wasReleased()){
    #elif defined(_M5STACK)
    }else if (M5.BtnC.wasReleased()){
    #else
    }else if (M5.Btn.wasReleased()){
    #endif
      selectIndex++;
      if (selectIndex > fileListCount)
      {
        selectIndex = 0;
      }
      needRedraw = true;
    #if defined(_M5STICKCPLUS)||defined(_M5STACK)
    #if defined(_M5STACK)
    }else if (M5.BtnA.wasReleased()){
    #else
    }else if (M5.BtnB.wasReleased()){
    #endif
      selectIndex--;
      if (selectIndex < 0)
      {
        selectIndex = fileListCount-1;
      }
      needRedraw = true;
    }
    #endif
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
 #if defined(_M5STACK)
  File fd = SD.open(filePath, FILE_READ);
 #else
  File fd = SPIFFS.open(filePath, FILE_READ);
#endif
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
      byte mode =  header[0]; //1:マシン語 2:BASIC
      int size = header[0x12] | (header[0x13] << 8);
      int offs = header[0x14] | (header[0x15] << 8);
      int addr = header[0x16] | (header[0x17] << 8);

      //mode = 1 以外、または、場合、CMTをイメージをセットする。
      if(mode != 1){
        fd.close();
        m5lcd.print("\n[SET TO CMT IMAGE]");
        set_mztData(mztFile);
        ts700.cmt_play = 1;
        
        //カセットのセットだけして、回転開始はエミュレータ側に任せるほうがいいかどうか…
        ts700.mzt_start = ts700.cpu_tstates;
        ts700.cmt_tstates = 1;
        setup_cpuspeed(5);
        sysst.motor = 1;
        xferFlag |= SYST_MOTOR;

        return true;
      }
      m5lcd.print("\n[SET TO MEMORY]");
      delay(500);

      Serial.printf("[size:%X][offs:%X][addr:%X]\n", size, offs, addr);

      if (remain >= size) {
//         if(mode == 2){ //BASICの場合、1バイトずつ処理。SP-5030用 / mode=5 S-BASIC？ワークエリア違ってるから読めないです…。
//           //http://www43.tok2.com/home/cmpslv/Mz80k/Mzsp5030.htm
//           WORD address = offs;//0x4806; //BASICプログラムエリア
//           WORD nextAddress = 0;
//           uint8_t readBuffer[128];
//           while(true){
//             //次のアドレス
//             fd.read(readBuffer, 2);
//             remain -= 2;
//             WORD deltaAddress = readBuffer[0] | (readBuffer[1] << 8);
//             if(deltaAddress <= 0){
//               break;
//             }
//             nextAddress = address + deltaAddress;
//             Serial.printf("%x:%x:%x\n" ,address,deltaAddress,nextAddress);
//             *(mem + RAM_START + address)= ((uint8_t*)&(nextAddress))[0];
//             *(mem +  RAM_START + address + 1)= ((uint8_t*)&(nextAddress))[1];
//             fd.read(mem + RAM_START + address + 2, deltaAddress - 2); 
//             address = nextAddress;
//             remain -= deltaAddress;
//           }
//           *(mem + RAM_START + address) = 0;
//           *(mem +  RAM_START + address + 1)= 0;
//           address = address + 2;
//             //BASICワークエリアを更新
//             //０４６３４Ｈ－０４６３５Ｈ：ＢＡＳＩＣプログラム最終アドレス
//             //０４６３６Ｈ－０４６３７Ｈ：２重添字つきストリング変数エリア　ポインタ  (ＢＡＳＩＣプログラム最終アドレス+2バイト)
//             //０４６３８Ｈ－０４６３９Ｈ：　　添字つきストリング変数エリア　ポインタ  (+2バイト)
//             //０４６３ＡＨ－０４６３ＢＨ：　　　　　　ストリング変数エリア　ポインタ  (+2バイト)
//             //０４６３ＥＨ－０４６３ＦＨ：　　　　　　　　　　　配列エリア　ポインタ  (+2バイト)
//             //０４６４０Ｈ－０４６４１Ｈ：　　　　　　　　　　　変数エリア　ポインタ  (+2バイト)
//             //０４６４２Ｈ－０４６４３Ｈ：　　　　　　　　　　　ストリング　ポインタ  (+2バイト)
//             //０４６４４Ｈ－０４６４５Ｈ：　　　　　　　変数へ代入する数値　ポインタ  (+2バイト)

//             for(int workAddress = 0x4634;workAddress<0x4645;workAddress=workAddress + 2){
//               *(mem + RAM_START + workAddress)= ((uint8_t*)&(address))[0];
//               *(mem + RAM_START + workAddress + 1)= ((uint8_t*)&(address))[1];
//   //           Serial.printf("WORK:%x:%x\n" ,workAddress,address);
//               address = address + 2;
//           }
//           //これは必要ないかも？
//           //*(mem + RAM_START + 0x4801)= 0x5a;
//           //*(mem + RAM_START + 0x4802)= 0x44;

// /* TEST
//           for(int testAddress = 0x4634;testAddress < 0x4660;testAddress=testAddress+16){
//             Serial.printf("%08x  " ,testAddress);
//             for(int j=0;j<16;j++){
//               Serial.printf("%02x " ,*(mem + RAM_START + testAddress + j));
//             }
//             Serial.println();
//           }
// */

//         }else{
          fd.read(mem + offs + RAM_START, size);
//        }
      }
      remain -= size;
      if(mode == 1){
        Z80_Regs StartRegs;
        Z80_GetRegs(&StartRegs);
        StartRegs.PC.D = addr;
        Z80_SetRegs(&StartRegs);
      }
      // if(mode == 5){ //S-BASICワークエリア
      //   WORD address = offs + size;
      //   Serial.printf("address :%x " ,address);
      //   //WORD workAreaAddress = 0x6B09;
      //   WORD workAreaAddress =0x6B49; //状況によって場所が異なるかも？
      //   *(mem + RAM_START + workAreaAddress)= ((uint8_t*)&(address))[0];
      //   *(mem + RAM_START + workAreaAddress + 1)= ((uint8_t*)&(address))[1];
      //   address = address + 1;
      //   *(mem + RAM_START + workAreaAddress + 2)= ((uint8_t*)&(address))[0];
      //   *(mem + RAM_START + workAreaAddress + 3)= ((uint8_t*)&(address))[1];
      //   address = address + 0x222;
      //   *(mem + RAM_START + workAreaAddress + 4)= ((uint8_t*)&(address))[0];
      //   *(mem + RAM_START + workAreaAddress + 5)= ((uint8_t*)&(address))[1];
      //   //workAreaAddress = 0x6B57;
      //   //*(mem + RAM_START + workAreaAddress + 14)= 0xB4;
      //   for(int testAddress = 0x6B09;testAddress < 0x6B60;testAddress=testAddress+16){
      //       Serial.printf("%08x  " ,testAddress);
      //       for(int j=0;j<16;j++){
      //         Serial.printf("%02x " ,*(mem + RAM_START + testAddress + j));
      //       }
      //       Serial.println();
      //   }
      // }
    }
  }

  fd.close();

  return true;
}

#define SET_TO_MEMORY_INDEX 5
#define SAVE_IMAGE_INDEX 6
#define WONDER_HOUSE_MODE_INDEX 7
#define PCG_OR_ROTATE_LCD_INDEX 8

void systemMenu()
{
  static String menuItem[] =
  {
    "[BACK]",
    "RESET:NEW MONITOR7",
    "RESET:NEW MONITOR",
    "RESET:MZ-1Z009",
    "RESET:SP-1002",
    "SET MZT TO MEMORY",
    "SAVE MZT Image",
    "Wonder House Mode",
  #if defined (USE_EXT_LCD)||defined(_M5ATOMS3)
    "Rotate LCD",
  #elif defined(_M5STICKCPLUS)
  #else
    "PCG",
  #endif
    ""
  };
	#if defined(USE_SPEAKER_G25)||defined(USE_SPEAKER_G26)||defined(_M5STICKCPLUS)
    ledcWriteTone(LEDC_CHANNEL_0, 0); // stop the tone playing:
	#endif	
  delay(10);
  m5lcd.fillScreen(TFT_BLACK);
  delay(10);
  #if defined(_M5STACK)
  m5lcd.setTextSize(2);
  #else
  m5lcd.setTextSize(1);
  #endif
  bool needRedraw = true;

  int menuItemCount = 0;
  while (menuItem[menuItemCount] != "") {
    menuItemCount++;
  }

  int selectIndex = 0;
  delay(100);
  M5.update();
  bool isButtonLongPress = false;
  while (true)
  {
    if (needRedraw == true)
    {
      m5lcd.setCursor(0, 0);
      for (int index = 0; index < menuItemCount; index++)
      {
        if (index == selectIndex)
        {
          if (isButtonLongPress == true) {
              m5lcd.setTextColor(TFT_RED);
            } else {
              m5lcd.setTextColor(TFT_GREEN);
            }
        }
        else
        {
          m5lcd.setTextColor(TFT_WHITE);
        }
        String curItem = menuItem[index];
        if( index == SET_TO_MEMORY_INDEX ){
          curItem = curItem + ((firstLoadFlag == true) ? String(": ON") : String(": OFF"));
        }
        if (index == PCG_OR_ROTATE_LCD_INDEX) {
        #if defined (USE_EXT_LCD)||defined(_M5ATOMS3)
        #else
          curItem = curItem + ((hw700.pcg700_mode == 1) ? String(": ON") : String(": OFF"));
        #endif
        }
/*
        if (index == SOUNT_INDEX) {
          curItem = curItem + (mzConfig.enableSound ? String(": ON") : String(": OFF"));
        }
        if (index == LCD_INDEX) {
          if (lcdMode == 0) {
            curItem = curItem + ":INTERNAL";
          } else if (lcdMode == 1) {
            curItem = curItem + ":EXTERNAL with AA";
          } else {
            curItem = curItem + ":EXTERNAL without AA";
          }
        }
*/
        m5lcd.println(curItem);
      }
      m5lcd.setTextColor(TFT_WHITE);
      #if defined(_M5STACK)
      m5lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("U P", 53, 240 - 17, 1);
      m5lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
      m5lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
      m5lcd.drawCentreString("DOWN", 266, 240 - 17, 1);
      #else
      m5lcd.drawString("LONG PRESS:SELECT", 0, 200 - 17, 1);
      #endif
      needRedraw = false;
    }
    M5.update();
  #if defined(_M5STACK)
  //長押し無し
  #else
  #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)
    if(M5.BtnA.pressedFor(1000)){ //長押しになった場合色を変える
  #else
    if(M5.Btn.pressedFor(1000)){ //長押しになった場合色を変える
  #endif
      if( isButtonLongPress == false){
           isButtonLongPress = true; 
           needRedraw = true;
      }
    }
  #endif
  #if defined(_M5STACK)
    if (M5.BtnA.wasReleased())
    {
      selectIndex--;
      if (selectIndex < 0)
      {
        selectIndex = menuItemCount - 1;
      }
      needRedraw = true;
    }
  #endif
  #if defined(_M5STICKCPLUS)
    if (M5.BtnA.wasReleasefor(1000))
  #elif defined(_M5STACK)
  if (M5.BtnB.wasReleased())
  #elif defined(_M5ATOMS3)
    if (M5.BtnA.wasReleased() && isButtonLongPress == true)
  #else
    if (M5.Btn.wasReleasefor(1000))
  #endif
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
        case SET_TO_MEMORY_INDEX:
          firstLoadFlag = !firstLoadFlag;
          break;
        case SAVE_IMAGE_INDEX:
          saveMZTImage();
          break;
        case WONDER_HOUSE_MODE_INDEX:
          wonderHouseMode = true;
          wonderHouseKeyIndex = 0;
          break;
        case PCG_OR_ROTATE_LCD_INDEX:
        #if defined (USE_EXT_LCD)||defined(_M5ATOMS3)
          if(lcdRotate == 0){
            lcdRotate = 1;
            m5lcd.setRotation(3);
          }else{
            lcdRotate = 0;
            m5lcd.setRotation(0);
          }
        #else
          hw700.pcg700_mode = (hw700.pcg700_mode == 1) ? 0 : 1;
        #endif
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
        firstLoadFlag = true;
        return;
      }
      m5lcd.fillScreen(TFT_BLACK);
      m5lcd.setCursor(0, 0);
      needRedraw = true;
      isButtonLongPress = false;
    #if defined(_M5STICKCPLUS)||defined(_M5ATOMS3)
    }else if (M5.BtnA.wasReleased()) {
    #elif defined(_M5STACK)
    }else if (M5.BtnC.wasReleased()) {
    #else
    }else if (M5.Btn.wasReleased()) {
    #endif
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
#if defined(USE_HID)
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
#endif
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


static bool saveMZTImage(){
  //上書きで保存
  //MZTファイル形式でメモリを保存する。
  //・ヘッダ (128 バイト)
  //0000h モード (01h バイナリ, 02h S-BASIC, C8h CMU-800 データファイル)
  //0001h ～ 0011h ファイル名 (0Dh で終わり / 0011h = 0Dh?)
  //0012h ～ 0013h ファイルサイズ
  //0014h ～ 0015h 読み込むアドレス
  ///0016h ～ 0017h 実行するアドレス
  //0018h ～ 007Fh 予約
  //・データ (可変長)
  //0080h ～ データがファイルサイズ分続く

  m5lcd.fillScreen(BLACK);
  m5lcd.setCursor(0,0);
  m5lcd.println("SAVE MZT IMAGE");
  File saveFile;
  String romDir = ROM_DIRECTORY;
  if (mzConfig.mzMode == MZMODE_80) {
  #if defined(_M5STACK)  
      saveFile = SD.open(romDir + "/0_M5Z80MEM.MZT", FILE_WRITE);
  }else{
      saveFile = SD.open(romDir + "/0_M5Z700MEM.MZT", FILE_WRITE);
  #else
      saveFile = SPIFFS.open("/0_M5Z80MEM.MZT", FILE_WRITE);
  }else{
      saveFile = SPIFFS.open("/0_M5Z700MEM.MZT", FILE_WRITE);
  #endif
  }
  if(saveFile == false){
    return false;
  }

  saveFile.write(0x01);
  saveFile.write('M');
  saveFile.write('5');
  saveFile.write('Z');
  saveFile.write('M');
  saveFile.write('E');
  saveFile.write('M');
  for(int i = 0;i < 11;i++){
    saveFile.write(0x0D);
  }
  //ファイルサイズ = (0xCFFF-0x1200) = BDFF
  saveFile.write(0xFF);
  saveFile.write(0xBD);

  //読み込みアドレス 0x1200
  saveFile.write(0x00);
  saveFile.write(0x12);
  
  //実行アドレス：現在のPC
  Z80_Regs regs;
  Z80_GetRegs(&regs);
  int addr = regs.PC.D;

  saveFile.write(((uint8_t*)&(addr))[1]);
  saveFile.write(((uint8_t*)&(addr))[0]);
  
  for(int i = 0x18;i <= 0x7f;i++){
    saveFile.write(0);
  }
  BYTE	*mainRamMem = mem + RAM_START;
  //RAM 1200h～CFFF を保存
  m5lcd.println();
  for(int index = 0x1200;index <= 0xCFFF;index++){
    saveFile.write(*(mainRamMem + index));
    if(index % 100 == 0){
      m5lcd.print("*");
    }
  }  
  saveFile.close();
  m5lcd.println();
  m5lcd.println("COMPLTE!");
  delay(2000);
  return true;
}
#if defined(_M5ATOMS3)
void keyboard_transfer_cb(usb_transfer_t *transfer)
{
  if (Device_Handle == transfer->device_handle) {
    isKeyboardPolling = false;
    if (transfer->status == 0) {
      if (transfer->actual_num_bytes == 8) {
        uint8_t *const p = transfer->data_buffer;
        //bluetoothキーボードとあわせる。[1]にmodiry[3]にキーコード
        uint8_t keyData[4];
        keyData[1] = p[0];
        keyData[3] = p[2];
        
        //M5.Display.setCursor(0,0);
        //M5.Display.setTextColor(WHITE,BLACK);
        //M5.Display.printf("HID report: %02x %02x %02x %02x %02x %02x %02x %02x",
         //   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

            keyboard(keyData,4);

     }
    }
  }
}
void check_interface_desc_boot_keyboard(const void *p)
{
  const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;

  if ((intf->bInterfaceClass == USB_CLASS_HID) &&
      (intf->bInterfaceSubClass == 1) &&
      (intf->bInterfaceProtocol == 1)) {
    isKeyboard = true;
    ESP_LOGI("", "Claiming a boot keyboard!");
    esp_err_t err = usb_host_interface_claim(Client_Handle, Device_Handle,
        intf->bInterfaceNumber, intf->bAlternateSetting);
    if (err != ESP_OK) ESP_LOGI("", "usb_host_interface_claim failed: %x", err);
  }
}

void prepare_endpoint(const void *p)
{
  const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)p;
  esp_err_t err;

  // must be interrupt for HID
  if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_INT) {
    ESP_LOGI("", "Not interrupt endpoint: 0x%02x", endpoint->bmAttributes);
    return;
  }
  if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
    err = usb_host_transfer_alloc(KEYBOARD_IN_BUFFER_SIZE, 0, &KeyboardIn);
    if (err != ESP_OK) {
      KeyboardIn = NULL;
      ESP_LOGI("", "usb_host_transfer_alloc In fail: %x", err);
      return;
    }
    KeyboardIn->device_handle = Device_Handle;
    KeyboardIn->bEndpointAddress = endpoint->bEndpointAddress;
    KeyboardIn->callback = keyboard_transfer_cb;
    KeyboardIn->context = NULL;
    isKeyboardReady = true;
    KeyboardInterval = endpoint->bInterval;
    ESP_LOGI("", "USB boot keyboard ready");
  }
  else {
    ESP_LOGI("", "Ignoring interrupt Out endpoint");
  }
}
#endif
