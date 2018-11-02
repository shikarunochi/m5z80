//
///* 画面リフレッシュ関係のフラグ */
//#define REFSC_VRAM		0x0001											/* bit00:Render Vram */
//#define REFSC_BLT		0x0002											/* bit01:Blt Virtual Screen */
//#define REFSC_ALL		0x0003											/* Refresh All */

//#ifdef __cplusplus
//extern "C" {
//#endif 

//int get_fullmode(void);
//void mz_palet(int);
//void mz_palet_init(void);

//BOOL chg_screen_mode(int mode);
	
int mz_screen_init(void);
void mz_screen_finish(void);
//void mz_refresh_screen(int);
//void mz_blt_screen(void);
//int get_mz_refresh_screen(void);

int font_load(const char *);

void update_scrn(void);
//void mz_chr_put(int x,int y,int code,int color);
//void mz_pcg8000_put(int x,int y,int code,int color);
//void mz_bgcolor_put(int x,int y,int color);

void updateStatusArea(const char* message);
void updateStatusSubArea(const char* message, int color);
  

#ifndef MZSCRN_H_  
#define MZSCRN_H_

//extern const UINT8	mzcol2dibcol[8];
//extern UINT8		mzcol_palet[8];

#endif
	
//#ifdef __cplusplus
//}
//#endif 
