#ifndef MZ700LGX_H_
#define MZ700LGX_H_

#if defined(_M5STICKCPLUS)
#include <M5GFX.h>
#include <M5StickCPlus.h>
#elif defined(_M5ATOMS3)
#include <M5Unified.h>
#elif defined(_M5ATOMS3LITE)
#include <M5GFX.h>
#include <M5Unified.h>
#define LGFX_M5STACK    
#elif defined(_M5STACK)
#include <M5GFX.h>
#include <M5Stack.h>
#elif defined(_M5STAMP)
#include <M5Atom.h>  
#define LGFX_M5STACK   
#elif defined(_M5CARDPUTER)
#include <M5Cardputer.h>
#elif defined(_TOOTHBRUSH)
#include <M5GFX.h>
#include <M5Unified.h>
#define LGFX_M5STACK
#else
#include <M5Atom.h>  
#define LGFX_M5STACK     
#endif


#ifdef USE_EXT_LCD
#include <M5GFX.h>

#ifndef	EXT_SPI_SCLK
#define EXT_SPI_SCLK 25
#endif
#ifndef	EXT_SPI_MOSI
#define EXT_SPI_MOSI 22
#endif
#ifndef EXT_SPI_DC
#define EXT_SPI_DC 19
#endif
#ifndef EXT_SPI_RST
#define EXT_SPI_RST 21
#endif
#ifndef EXT_SPI_CS
#define EXT_SPI_CS -1
#endif
#if defined(USE_ST7735S)
#include <lgfx/v1/panel/Panel_ST7735.hpp>
#elif defined(USE_GC9107)
#include <lgfx/v1/panel/Panel_GC9A01.hpp>
#else
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#endif

class LGFX : public lgfx::LGFX_Device
{
#if defined(USE_ST7735S)
  lgfx::Panel_ST7735S      _panel_instance;
#elif defined(USE_GC9107)
  lgfx::Panel_GC9107     _panel_instance;
#else
  lgfx::Panel_ST7789      _panel_instance;
#endif
  lgfx::Bus_SPI       _bus_instance;   // SPIバスのインスタンス
  public:
    LGFX(void)
    {
      { // バス制御の設定を行います。
        auto cfg = _bus_instance.config();    // バス設定用の構造体を取得します。
#if defined(_M5ATOMS3LITE)||defined(_TOOTHBRUSH)
        cfg.spi_host = SPI2_HOST ;     // ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
#else
        cfg.spi_host = VSPI_HOST;     // 使用するSPIを選択  (VSPI_HOST or HSPI_HOST)
#endif
#if defined(USE_ST7735S)
#if defined(_TOOTHBRUSH)
        cfg.spi_mode = 3;             // SPI通信モードを設定 (0 ~ 3)
#else
        cfg.spi_mode = 0;             // SPI通信モードを設定 (0 ~ 3)
#endif
#elif defined(USE_GC9107)
        cfg.spi_mode = 0;             // SPI通信モードを設定 (0 ~ 3)
#else
        cfg.spi_mode = 3;             // SPI通信モードを設定 (0 ~ 3)
#endif
        cfg.freq_write = 40000000;    // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
        cfg.freq_read  = 16000000;    // 受信時のSPIクロック
        cfg.spi_3wire  = false;        // 受信をMOSIピンで行う場合はtrueを設定
        cfg.use_lock   = true;        // トランザクションロックを使用する場合はtrueを設定
        cfg.dma_channel = 1;          // Set the DMA channel (1 or 2. 0=disable)   使用するDMAチャンネルを設定 (0=DMA不使用)
#if  defined(_M5ATOMS3LITE)
        cfg.pin_sclk = 7;            // SPIのSCLKピン番号を設定
        cfg.pin_mosi = 8;            // SPIのMOSIピン番号を設定
        cfg.pin_miso = -1;            // SPIのMISOピン番号を設定 (-1 = disable)
        cfg.pin_dc   = 5;            // SPIのD/Cピン番号を設定  (-1 = disable)
#elif defined(_M5STAMP)
        cfg.pin_sclk = EXT_SPI_SCLK;            // SPIのSCLKピン番号を設定
        cfg.pin_mosi = EXT_SPI_MOSI;            // SPIのMOSIピン番号を設定
        cfg.pin_miso = -1;            // SPIのMISOピン番号を設定 (-1 = disable)
        cfg.pin_dc   = EXT_SPI_DC;            // SPIのD/Cピン番号を設定  (-1 = disable)
#elif defined(_TOOTHBRUSH)
        cfg.pin_sclk = EXT_SPI_SCLK;            // SPIのSCLKピン番号を設定
        cfg.pin_mosi = EXT_SPI_MOSI;            // SPIのMOSIピン番号を設定
        cfg.pin_miso = 2;              // SPIのMISOピン番号を設定 (-1 = disable)
        cfg.pin_dc   = EXT_SPI_DC;            // SPIのD/Cピン番号を設定  (-1 = disable)
#else
        cfg.pin_sclk = 23;            // SPIのSCLKピン番号を設定
        cfg.pin_mosi = 33;            // SPIのMOSIピン番号を設定
        cfg.pin_miso = -1;            // SPIのMISOピン番号を設定 (-1 = disable)
        cfg.pin_dc   = 22;            // SPIのD/Cピン番号を設定  (-1 = disable)
#endif
        _bus_instance.config(cfg);    // 設定値をバスに反映します。
        _panel_instance.setBus(&_bus_instance);      // バスをパネルにセットします。
      }

      { // 表示パネル制御の設定を行います。
        auto cfg = _panel_instance.config();    // 表示パネル設定用の構造体を取得します。
#if defined(_TOOTHBRUSH)
        cfg.pin_cs           =          10;  // CSが接続されているピン番号   (-1 = disable)
#else
        cfg.pin_cs           =    -1;  // CSが接続されているピン番号   (-1 = disable)
#endif
#if  defined(_M5ATOMS3LITE)
        cfg.pin_rst          =    6;  // RSTが接続されているピン番号  (-1 = disable)
#elif defined(_M5STAMP)||defined(_TOOTHBRUSH)
        cfg.pin_rst          =    EXT_SPI_RST;  // RSTが接続されているピン番号  (-1 = disable)
#else
        cfg.pin_rst          =    19;  // RSTが接続されているピン番号  (-1 = disable)
#endif
        cfg.pin_busy         =    -1;  // BUSYが接続されているピン番号 (-1 = disable)

        // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。
#if defined(USE_ST7735S)
#if defined(_TOOTHBRUSH)
        cfg.memory_width     =   132;  // ドライバICがサポートしている最大の幅
        cfg.memory_height    =   162;  // ドライバICがサポートしている最大の高さ
        cfg.panel_width      =   80;  // 実際に表示可能な幅
        cfg.panel_height     =   160;  // 実際に表示可能な高さ
        cfg.offset_x         =     26;  // パネルのX方向オフセット量
        cfg.offset_y         =     1;  // パネルのY方向オフセット量
        cfg.offset_rotation  =     0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
        cfg.invert           =  true;  // パネルの明暗が反転してしまう場合 trueに設定
        cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
#else
        cfg.memory_width     =   128;  // ドライバICがサポートしている最大の幅
        cfg.memory_height    =   160;  // ドライバICがサポートしている最大の高さ
        cfg.panel_width      =   128;  // 実際に表示可能な幅
        cfg.panel_height     =   160;  // 実際に表示可能な高さ
        cfg.offset_x         =     2;  // パネルのX方向オフセット量
        cfg.offset_y         =     -1;  // パネルのY方向オフセット量
        cfg.offset_rotation  =     3;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
        cfg.invert           =  true;  // パネルの明暗が反転してしまう場合 trueに設定 
        cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
#endif
#elif defined(USE_GC9107)
        cfg.memory_width     = 128;
        cfg.memory_height    = 160; // ドライバICがサポートしている最大の高さ
        cfg.panel_width      =   128;  // 実際に表示可能な幅
        cfg.panel_height     =   128;  // 実際に表示可能な高さ
        cfg.offset_x         =     0;  // パネルのX方向オフセット量
        cfg.offset_y         =     32;  // パネルのY方向オフセット量
        cfg.offset_rotation  =     0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
        cfg.invert           =  false;  // パネルの明暗が反転してしまう場合 trueに設定
        cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
#else
        cfg.memory_width     =   240;  // ドライバICがサポートしている最大の幅
        cfg.memory_height    =   320;  // ドライバICがサポートしている最大の高さ
        cfg.panel_width      =   240;  // 実際に表示可能な幅
        cfg.panel_height     =   240;  // 実際に表示可能な高さ
        
        cfg.offset_y         =     0;  // パネルのY方向オフセット量
#if defined(MZ1D05_MODE)
        cfg.offset_x         =     20;  // パネルのX方向オフセット量
        cfg.offset_rotation  =     3;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
#else
        cfg.offset_x         =     0;  // パネルのX方向オフセット量
        cfg.offset_rotation  =     0;  // 回転方向の値のオフセット 0~7 (4~7は上下反転)
#endif
#if defined(_M5STAMP)
        cfg.invert           =  true;  // パネルの明暗が反転してしまう場合 trueに設定
        cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
#else
        cfg.invert           =  true;  // パネルの明暗が反転してしまう場合 trueに設定
        cfg.rgb_order        = false;  // パネルの赤と青が入れ替わってしまう場合 trueに設定
#endif
        
#endif
        cfg.dummy_read_pixel =     8;  // ピクセル読出し前のダミーリードのビット数
        cfg.dummy_read_bits  =     1;  // ピクセル以外のデータ読出し前のダミーリードのビット数
        cfg.readable         =  false;  // データ読出しが可能な場合 trueに設定        
        cfg.dlen_16bit       = false;  // データ長を16bit単位で送信するパネルの場合 trueに設定
        cfg.bus_shared       =  false;  // SDカードとバスを共有している場合 trueに設定(drawJpgFile等でバス制御を行います)

        _panel_instance.config(cfg);
      }
      setPanel(&_panel_instance); // 使用するパネルをセットします。
    }
};
extern LGFX m5lcd;

#else
#if defined(_M5STICKCPLUS)
extern M5GFX  m5lcd;
#elif defined(_M5ATOMS3)
extern M5GFX m5lcd;
#elif defined(_M5STACK)
extern M5GFX m5lcd;
#elif defined(_M5CARDPUTER)
extern M5GFX m5lcd;
#else

#include <M5AtomDisplay.h>
extern M5AtomDisplay m5lcd; 
#endif
#endif

extern int lcdMode;
extern int lcdRotate;
#ifdef _M5STAMP 
extern Button extBtn;
#endif

#if defined(USE_MIDI)
#include <M5UnitSynth.h>
extern M5UnitSynth synth;
#endif

#endif 
