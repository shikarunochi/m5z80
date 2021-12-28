#ifdef __cplusplus
extern "C" {
#endif 

//
enum
{
  MB_ROM1 = 0,
  MB_ROM2,
  MB_RAM,
  MB_VRAM,
  MB_DUMMY,
  MB_FONT,
  MB_PCGB,
  MB_PCGR,
  MB_PCGG,
};

//
typedef struct
{
  UINT16    ofs;                  // メモリオフセット
  UINT8   base;                 // メモリブロック番号
  UINT8   attr;                 // メモリ読み書きアトリビュート
} TMEMBANK;

/* HARDWARE STATUS (MZ-700) */
typedef struct
{
  TMEMBANK  memctrl[32];      /* Memory Bank Controller */
	int		pb_select;					/* selected kport to read */
	int		pcg700_data;
	int		pcg700_addr;
	int		cursor_cou;
	UINT16	pcg700_mode;
	UINT8	tempo_strobe;
  UINT8 motor;            /* bit0:カセットモーター bit2:データレコーダのスイッチセンス*/
	UINT8	retrace;
	UINT8	vgate;
	UINT8	led;
	UINT8	wdata;
} THW700_STAT;

/* TSTATES (MZ-700) */
typedef struct
{
	int		cpu_tstates;
	int		tempo_tstates;
	int		vsync_tstates;
	int		pit_tstates;
	int		cmt_tstates;
	int		cmt_play;
	int		mzt_start;
	int		mzt_elapse;
	int		mzt_bsize;
	int		mzt_percent;
	int		mzt_settape;
	int		mzt_period;
} T700_TS;

/* ８２５３ステータス型 */
typedef struct
{
	UINT8 bcd;															/* BCD */
	UINT8 mode;															/* MODE */
	UINT8 rl;															/* RL */
	UINT8 rl_bak;														/* RL Backup */
	int lat_flag;														/* latch flag */
	int running;
	int counter;														/* Counter */
	WORD counter_base;													/* CounterBase */
	WORD counter_out;													/* Out */
	WORD counter_lat;													/* Counter latch */
	WORD bit_hl;														/* H/L */

} T8253_STAT;

typedef struct
{
	UINT8 bcd;
	UINT8 m;
	UINT8 rl;
	UINT8 sc;
	//UINT8 int_mask;
	//UINT8 beep_mask;
	UINT8 makesound;
	UINT8 setsound;
} T8253_DAT;

#define DOSND_NULL	0x0000
#define DOSND_FREQ	0x0001
#define DOSND_VOL	0x0002
#define DOSND_ALL	0x0003
#define DOSND_STOP	0x0004

#define PWM_LEADER1 856000
#define PWM_MARK1L	68480+PWM_LEADER1
#define PWM_MARK1S	34240+PWM_MARK1L
#define PWM_P1H		856+PWM_MARK1S
#define PWM_P1L		856+PWM_P1H
#define PWM_P2H		856+PWM_P1L
#define PWM_P2L		856+PWM_P2H

//void update_membank(void);
//
int mmio_in(int);
void mmio_out(int,int);

//---- Z80 ------------
void calc_state(int codest);

#ifdef __cplusplus
extern "C"  {
#endif
void Z80_Reti(void);
void Z80_Retn(void);
int Z80_Interrupt(void);
#ifdef __cplusplus
};
#endif

void Z80_set_carry(Z80_Regs *, int);
int Z80_get_carry(Z80_Regs *);
void Z80_set_zero(Z80_Regs *, int);
int Z80_get_zero(Z80_Regs *);

//------------------
void mzsnd_init(void);
void mz_reset(void);
void mz_main(void);

void mz_keyport_init(void);
void mz_keydown(int);
void mz_keyup(int);
void mz_keydown_sub(UINT8);
void mz_keyup_sub(UINT8);

void makePWM(void);

void pit_init(void);
int pitcount_job(int,int);

void play8253(void);

int pit_count(void);
void vblnk_start(void);

int set_mztData(String);

//	
#ifndef MZHW_H_  

extern int IFreq;
extern int CpuSpeed;

/* HARDWARE STATUS WORK */
extern THW700_STAT		hw700;
extern T700_TS			ts700;

/* 8253関連ワーク */
extern T8253_DAT		_8253_dat;
extern T8253_STAT		_8253_stat[3];

extern int rom1_mode;													/* ROM-1 ・ｽ・ｽ・ｽﾊ用・ｽt・ｽ・ｽ・ｽO */
extern int rom2_mode;													/* ROM-2 ・ｽ・ｽ・ｽﾊ用・ｽt・ｽ・ｽ・ｽO */

/* memory */
extern UINT8	*mem;													/* Main Memory */
extern UINT8	*junk;													/* to give mmio somewhere to point at */
extern UINT32 	*mzt_buf;

/* FONT ROM/RAM */	
extern UINT8 *mz_font;//[256][8][8];										// 256chars*8lines*8dots
extern UINT8 *pcg700_font;//[256][8][8];								/* PCG-700 font (2K) */

extern int mzt_size;
extern int pwmTable[256][3];
extern int mzt_leader[80];
	
extern UINT8	keyports[10];
#endif

#ifdef __cplusplus
}
#endif 
