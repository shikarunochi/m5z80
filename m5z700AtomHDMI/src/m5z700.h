# pragma GCC optimize ("Ofast")
/* Typedef */
typedef	unsigned int UINT32;
typedef	unsigned short UINT16;
typedef	unsigned char UINT8;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned       DWORD;

// FDC有効？
//#define ENABLE_FDC

/* メッセージで日本語を使うか */
#define JAPANESE		0

/* */
#define ROM_START  0     /* monitor rom */
#define ROM2_START  (4*1024)  /* monitor rom-2 */
#define VID_START (10*1024) /* video ram */
#define RAM_START (14*1024) /* 64k normal ram */

/* */
#define ROM1_ADDR 0x0000    /* ROM1 mz addr */
#define ROM2_ADDR 0xE800    /* ROM2 mz addr */


#define ROMFONT_SIZE 4096    /* ROMFONT SIZE  */
#define PCG700_SIZE 2048		/* PCG-700 SIZE  */

#define CPU_SPEED 3580000		/* 3.580 MHz */
#define CPU_SPEED_BASE 3580000	/* 3.580 MHz */
//#define CPU_SPEED 2000000    /* 2.000 MHz */
//#define CPU_SPEED_BASE 2000000  /* 2.000 MHz */

#define FRAMES_PER_SECOND 60

#define VRAM_ACCESS_WAIT 42
#define TEMPO_TSTATES ((CPU_SPEED*10)/381/2)
#define VBLANK_TSTATES 45547	

/* モニタＲＯＭ１のタイプ定数 */
enum TMON1_TYPE
{
  MON_EMUL = 0,                           /*  0:EMUL ROM(NO ROM FILE) */
  MON_OTHERS,                             /*  1:OTHERS */
  MON_1Z009,                              /*  2:1Z-009A/B */
  MON_1Z013,                              /*  3:1Z-013A/B */
  MON_NEWMON700,                            /*  4:MZ700-NEW MONITOR */
  MON_NEWMON80K,                            /*  5:MZ-NEW MONITOR */

  MON_LAST,
};

/* モニタＲＯＭ２のタイプ定数 */
enum TMON2_TYPE
{
	MON_80FIO = 1,														/*  1:FD BOOT ROM */
};

/* MZTファイルのヘッダ */
typedef struct
{
  BYTE mz_fmode;                            /* ＭＺのファイルモード */
  BYTE mz_filename[17];                       /* ＭＺのファイル名 */
  WORD mz_filesize;                         /* ファイルサイズ */
  WORD mz_topaddr;                          /* 先頭アドレス */
  WORD mz_execaddr;                         /* 実行アドレス */
  
  BYTE dummy[0x80-0x18];                        /* パディング */
} MZTHED;

/* キーテーブル Patch用構造体 */
typedef struct
{
  WORD addr;                              /* patch address */
  BYTE data;                              /* patch data */
  
} TPATCH;

/* for MONITOR PATCH */ 
typedef struct
{
  WORD addr;                              /* patch addr */
  unsigned short code;                        /* em code */
} TMONEMU;

// メッセージキューID
enum MSGQID
{
  QID_KBD = 1001,
};

#define ROM_DIRECTORY ""
#define TAPE_DIRECTORY ""
//#define DEFAULT_ROM_FILE  "1Z009.ROM"
#define DEFAULT_ROM_FILE  "NEWMON7.ROM"
#define DEFAULT_TAPE_FILE  "WH_newmon.mzt"
#define DEFAULT_FONT_FILE  "MZ700FON.JP"

// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0     0
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000
// SPEAKER PIN
//#define SPEAKER_PIN        25
