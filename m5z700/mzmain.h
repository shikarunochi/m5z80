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
//	MZSBLK_pcg8000_font,
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

typedef struct
{
  char romFile[50 + 1];
  char tapeFile[50 + 1];
  char ssid[50 + 1];
  char pass[50 + 1];
  bool forceAccessPoint;
  bool enableSound;
  int mzMode;
} MZ_CONFIG;

enum{
  MZMODE_80 = 0,
  MZMODE_700
};

void loadConfig();
void saveConfig();

extern MZ_CONFIG mzConfig;

#ifdef __cplusplus
}
#endif 
