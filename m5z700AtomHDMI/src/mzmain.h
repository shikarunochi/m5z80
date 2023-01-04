#ifndef MZMAIN_H_
#define MZMAIN_H_

#ifdef __cplusplus
extern "C" {
#endif 

// for ROM Select MENU
//enum
//{
//	ROM_KIND_NONE,
//	ROM_KIND_USERSMZ700,
//	ROM_KIND_NEWMON700,
//	ROM_KIND_1Z009,
//	ROM_KIND_1Z013,
//	ROM_KIND_NEWMON80K,
//	ROM_KIND_SP1002,
//	ROM_KIND_MZ1500,
//
//	ROM_KIND_MAX,
//
//};

// State file format....
// HEADER
//--------------------
// UINT32 size of block (0 == END of DATA)
// UINT16 block number
// Block data...
//--------------------
// UINT32 size of block (0 == END of DATA)
// UINT16 block number
// Block data...
//--------------------

//enum
//{
//	MZSBLK_z80 = 0,
//	MZSBLK_ts700,
//	MZSBLK_hw700,
//	MZSBLK_8253dat,
//	MZSBLK_8253stat,
//	MZSBLK_mainram,
//	MZSBLK_textvram,
//	MZSBLK_colorvram,
//	MZSBLK_pcg700_font,
//	MZSBLK_mz1r12,
//
//	MZSBLK_MZ700,
//
//	MZSBLK_ts1500 = 16,
//	MZSBLK_hw1500,
//	MZSBLK_z80pio,
//	MZSBLK_pcgvram,
//	MZSBLK_atrvram,
//	MZSBLK_psg,
//	MZSBLK_pcg1500_blue,
//	MZSBLK_pcg1500_red,
//	MZSBLK_pcg1500_green,
//	MZSBLK_palet,
//	MZSBLK_mz1r18,
//
//	MZSBLK_MZ1500,
//	
//	MZSBLK_EOD = 0xFFFE,	// END OF DATA
//};

// State file header
//typedef struct
//{
//	UINT8	name[16];
//	int		rom1;
//	int		rom2;
//} TMZS_HEAD;

// System status
typedef struct
{
    int     led;    // 英数/カナLEDの状態 0=非搭載 1=カナ(赤) 2=英数(緑)
    int     tape;   // テープの走行進捗(0-100%)
    int     motor;  // CMTのモーター 0=off 1=on
} SYS_STATUS;

// Status Transfer Flag
#define SYST_ALL    0x00000007
#define SYST_LED    0x00000001
#define SYST_CMT    0x00000002
#define SYST_MOTOR  0x00000004
#define SYST_PCG    0x00000008
#define SYST_BOOTUP 0x80000000

//------------------
int mz_alloc_mem(void);
void mz_free_mem(void);

//------------------
//void init_rom_combo(HWND hwnd);
//int rom_check(void);
//void mz_mon_setup(void);
//int rom_load(unsigned char *);
//void bios_patch(const TPATCH *patch_dat);
int set_romtype(void);

//BOOL save_ramfile(LPCSTR filename);
//BOOL load_ramfile(LPCSTR filename);


//BOOL save_state(LPCSTR filename);
//BOOL load_state(LPCSTR filename);

int mz80c_main();

void mainloop(void);
void setup_cpuspeed(int);

int create_thread(void);
void start_thread(void);
int end_thread(void);

bool intByUser(void);
void mz_exit(int);

int setMztToMemory(String mztFile);

typedef struct
{
  char romFile[50 + 1];
  char tapeFile[50 + 1];
  bool enableSound;
  int mzMode;
} MZ_CONFIG;

enum{
  MZMODE_80 = 0,
  MZMODE_700
};

extern MZ_CONFIG mzConfig;

extern bool wonderHouseMode;
extern int wonderHouseKeyIndex;
//http://oldavg.blog.shinobi.jp/%E3%82%8F%20-1-%E3%80%80%E3%83%AF%E3%83%B3%E3%83%80%E3%83%BC%E3%83%8F%E3%82%A6%E3%82%B9/%E3%83%AF%E3%83%B3%E3%83%80%E3%83%BC%E3%83%8F%E3%82%A6%E3%82%B9%E6%94%BB%E7%95%A5%E3%80%80-%E5%AE%8C%E5%85%A8%E3%83%8D%E3%82%BF%E3%83%90%E3%83%AC-
const char wonderHouseKeyData[]= 
"an\n"
"search floor\n"
"take memo\n"
"read memo\n"
"take mat\n"
"look\n"
"take key\n"
"e\n"
"open sw.box\n"
"on switch\n"
"s\n"
"open door\n"
"e\n"
"off switch\n"
"s\n"
"forward\n"
"search floor\n"
"take paper\n"
"read paper\n"
"forward\n"
"e\n"
"forward\n"
"forward\n"
"n\n"
"forward\n"
"forward\n"
"open rack\n"
"search rack\n"
"take driver\n"
"w\n"
"forward\n"
"listen\n"
"say i am fine thank you.\n"
"s\n"
"unlock door\n"
"open door\n"
"forward\n"
"move rack\n"
"forward\n"
"w\n"
"forward\n"
"search floor\n"
"take bird\n"
"s\n"
"forward\n"
"listen\n"
"say my name is mz.\n"
"n\n"
"forward\n"
"e\n"
"forward\n"
"n\n"
"forward\n"
"unlock door\n"
"open door\n"
"forward\n"
"e\n"
"forward\n"
"s\n"
"forward\n"
"forward\n"
"e\n"
"forward\n"
"forward\n"
"say how do you do?\n"
"forward\n"
"search floor\n"
"take book\n"
"open book\n"
"look book\n"
"read book\n"
"search book\n"
"take card\n"
"look card\n"
"s\n"
"forward\n"
"forward\n"
"say open sesame.\n"
"unlock door\n"
"open door\n"
"forward\n"
"e\n"
"forward\n"
"forward\n"
"s\n"
"open rack\n"
"search rack\n"
"take dictionary\n"
"open dictionary\n"
"read dictionary\n"
"e\n"
"forward\n"
"open rack\n"
"search rack\n"
"use driver\n"
"search rack\n"
"take rope\n"
"n\n"
"forward\n"
"open window\n"
"use rope\n"
"forward\n"
"take rope\n"
"w\n"
"forward\n"
"give bird\n"
"s\n"
"forward\n"
"forward\n"
"w\n"
"forward\n"
"open rack\n"
"search rack\n"
"take parachute\n"
"repair parachute\n"
"use rope\n"
"e\n"
"forward\n"
"n\n"
"forward\n"
"w\n"
"forward\n"
"n\n"
"forward\n"
"open window\n"
"e\n"
"use parachute\n"
"e\n"
"jump\n"
"forward\n"
"open sw.box\n"
"search sw.box\n"
"insert card\n"
"w\n"
"forward\n"
"forward\n"
"go pool\n"
"search bottom\n"
"take diamond\n"
"go boy\n"
"listen\n"
"say my name is mz.\n"
"E";



#ifdef __cplusplus
}
#endif 
#endif