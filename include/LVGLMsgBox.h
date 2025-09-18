/*
 * LVGLMsgBox.h
 *
 *  Created on: Oct 7, 2021
 *      Author: jim
 * Note: Update this file to get the REV DAtE to update on the Main Screen's title line
 * 20250913 Added new method/function NuLineDcdTA(void); Updated 'help' text to reflect new F6 & F7 functions
 * 20250203 changed i2c clock to 100Khz had been 400Khz, but found some display touch chips would not work w/ the faster data clock
 * 20250916 Updated 'help' text to reflect new Up & Down arrow functions
 * 20250918 revised 'help' text to simplify using Copilot AI
 * */
#ifndef INC_LVGLMSGBOX_H_
#define INC_LVGLMSGBOX_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h" // need this to use char type
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
/*Added the following for Waveshare & lvgl support*/
#include <lv_conf.h>
#include "lvgl.h"

#define TODAY __DATE__

/*Waveshare 800x480 display & touch specific parameters*/
#define LV_TICK_PERIOD_MS (100) //JMH chnaged this from 2 to 100; In this application, this timer does nothing usueful

#define CONFIG_MSGBX_AVOID_TEAR_EFFECT_WITH_SEM 1

#define I2C_MASTER_SCL_IO 9 /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 8 /*!< GPIO number used for I2C master data  */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// the following configuration for Waveshare's ESP32s3 7 inch Touch Display //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MSGBX_LCD_PIXEL_CLOCK_HZ (18 * 1000 * 1000)
#define MSGBX_LCD_BK_LIGHT_ON_LEVEL 1
#define MSGBX_LCD_BK_LIGHT_OFF_LEVEL !MSGBX_LCD_BK_LIGHT_ON_LEVEL
#define MSGBX_PIN_NUM_BK_LIGHT -1
#define MSGBX_PIN_NUM_HSYNC 46
#define MSGBX_PIN_NUM_VSYNC 3
#define MSGBX_PIN_NUM_DE 5
#define MSGBX_PIN_NUM_PCLK 7
#define MSGBX_PIN_NUM_DATA0 14  // B3
#define MSGBX_PIN_NUM_DATA1 38  // B4
#define MSGBX_PIN_NUM_DATA2 18  // B5
#define MSGBX_PIN_NUM_DATA3 17  // B6
#define MSGBX_PIN_NUM_DATA4 10  // B7
#define MSGBX_PIN_NUM_DATA5 39  // G2
#define MSGBX_PIN_NUM_DATA6 0	  // G3
#define MSGBX_PIN_NUM_DATA7 45  // G4
#define MSGBX_PIN_NUM_DATA8 48  // G5
#define MSGBX_PIN_NUM_DATA9 47  // G6
#define MSGBX_PIN_NUM_DATA10 21 // G7
#define MSGBX_PIN_NUM_DATA11 1  // R3
#define MSGBX_PIN_NUM_DATA12 2  // R4
#define MSGBX_PIN_NUM_DATA13 42 // R5
#define MSGBX_PIN_NUM_DATA14 41 // R6
#define MSGBX_PIN_NUM_DATA15 40 // R7
#define MSGBX_PIN_NUM_DISP_EN -1

// The pixel number in horizontal and vertical
#define MSGBX_LCD_H_RES 800
#define MSGBX_LCD_V_RES 480

#if CONFIG_MSGBX_DOUBLE_FB
#define MSGBX_LCD_NUM_FB 2
#else
#define MSGBX_LCD_NUM_FB 1
#endif // CONFIG_MSGBX_DOUBLE_FB

// #define MSGBX_LVGL_TICK_PERIOD_MS    2
#define MSGBX_LVGL_TASK_MAX_DELAY_MS 500
#define MSGBX_LVGL_TASK_MIN_DELAY_MS 1 // JMH changed to 4 for My_Txt_test.c; default setting was 1
#define MSGBX_LVGL_TASK_STACK_SIZE (4 * 1024)
#define MSGBX_LVGL_TASK_PRIORITY 3 // JMH changed from 2
/*end Waveshare Params*/

/*Added this to aviod having to reference LVGL_port_v8.h */
#define LVGL_PORT_DISP_WIDTH                    (MSGBX_LCD_H_RES)   // The width of the display
#define LVGL_PORT_DISP_HEIGHT                   (MSGBX_LCD_V_RES)  // The height of the display

#define HiRes // uncomment for 480x340 screens
#define RingBufSz 400
extern SemaphoreHandle_t lvgl_semaphore;
extern QueueHandle_t ToneSN_que;
extern int MutexLckId;
extern lv_chart_series_t * ui_Chart1_series_1;
extern int freq_int;// used in the scope view as current tone frequency
extern int bias_int;
extern bool SmplSetRdy;
extern bool ScopeFlg;
extern bool HelpFlg;
extern bool ScopeActive;
extern bool NiteMode;
const char HelpText[] =
	"Keyboard Encoder Notes\n"
	"\n"
	"Special Keys (send special Morse):\n"
	"  '='   <BT>\n"
	"  '+'   <KN>\n"
	"  '>'   <AR>\n"
	"  '<'   <AS>\n"
	"  '%'   <SK>\n"
	"  Unassigned Keys (e.g. '{', ']') send 6 dits (error)\n"
	"\n"
	"Special Functions:\n"
	"  Ctrl+T             Continuous key down (toggle)\n"
	"  Enter              Send \"My Call\" memory\n"
	"                           (see 'Enter key send modes' below)\n"
	"  Delete            Backspace (delete unsent)\n"
	"  Esc                   Abort/dump outgoing text\n"
	"  Ctrl+S              Settings screen (toggle)\n"
	"  Ctrl+H              Help screen (toggle)\n"
	"  F1                     Save up to 10 chars (DX call)\n"
	"                           Press F1 again to stop save\n"
	"  F2-F4               Send stored F2/F3/F4 message\n"
	"  F12                   Suspend outgoing text (toggle)\n"
	"                           \"F12 SOT OFF\" while suspended\n"
	"  \xef\x81\x94 (Arrow)        Same as F12\n"
	"  \xef\x81\x93 (Arrow)        Same as F1\n"
	"  \xef\x81\xb7/\xef\x81\xb8 Arrows     Inc/Dec Send WPM\n"
	"\n"
	"CW Decoder & Other Keys:\n"
	"  Ctrl+F        Cycle tone tune mode 'AF'/'FF'\n"
	"  Ctrl+G        Debug On/Off\n"
	"  Ctrl+P        Plot On/Off\n"
	"  F6              Clear Decoded Text\n"
	"  F7              New Line (Decoded Text)\n"
	"  F8              Light/Dark Theme\n"
	"  F9              Scope View On/Off\n"
	"\n"
	"Enter key send modes:\n"
	"  Enter               Send your call only\n"
	"  Shift+Enter     Send F1 + your call\n"
	"  Ctrl+Enter       Send F1 only\n"
	"\n"
	"Scrolling (Main Screen):\n"
	"  Shift+\xef\x81\xb7         Scroll Up Decoded Text\n"
	"  Shift+\xef\x81\xb8         Scroll Down Decoded Text\n"
	"  Ctrl+\xef\x81\xb7           Scroll Up Send Text\n"
	"  Ctrl+\xef\x81\xb8           Scroll Down Send Text\n"
	"\n"
	"Status:\n"
	"  'F1 Mem/Active'   Saving typed chars to F1 memory\n"
	"  'F12 SOT ON'         Send on type Enabled\n"
	"  'Kbrd Bat nn%'     Keyboard Battery Charge %\n"
	"  'nn WPM'              CW Send Speed %\n"
	"\n"
	"Notes:\n"
	"  F1 is for storing call signs, can be cleared by cycling F1.\n"
	"  To avoid sending CW while loading F1, set F12 to \"SOT OFF\".\n"
	"  After loading F1, press F1 (\"F1 Mem\"), press Esc to flush buffer.\n"
	"  Then press F12 to return to \"SOT ON\".\n"
	"\n"
	"Use \xef\x81\xb7/\xef\x81\xb8 arrows to scroll this help text.\n"
	"\n"
	"Edited with assistance from GitHub Copilot AI.\n";

bool lvgl_port_lock(int timeout_ms);
bool lvgl_port_unlock(void);

class LVGLMsgBox
{
private:
	char DeCdrRingbufChar[RingBufSz];
	char KeyBrdRingbufChar[RingBufSz];
	uint16_t DeCdrRingbufClr[RingBufSz];
	uint16_t KeyBrdRingbufClr[RingBufSz];
	bool BGHilite;
	char Msgbuf[50];
	char LastStatus[50];
	uint16_t LastStatClr;
	int txtpos;
	int RingbufPntr1;
	int RingbufPntr2;
	int KeyBrdPntr1;
	int KeyBrdRingbufPntr2;
	int StrdRBufPntr1;
	int StrdRBufPntr2;
    uint8_t KyBrdBatLvl = 0;
	

#ifdef HiRes
#define fontsize 2 /* 320x480 screen*/
#define CPL 40	   /*number of characters each line that a 3.5" screen can contain*/
#define ROW 15
#define ScrnHght 320
#define ScrnWdth 480
#define FONTW 12
#define FONTH 20 // 16

	const int StatusX = 100;
#else
#define fontsize 1 & 240x320 screen * /
#define CPL 40 // number of characters each line that a 2.8" screen can contain
#define ROW 15 // number of usable rows on a 2.8" screen
#define ScrnHght 240
#define ScrnWdth 320
#define FONTW = 8
#define FONTH 14
	const int StatusX = 80;
#endif
	const int fontH = FONTH;
	const int fontW = FONTW;
	const int StatusY = ScrnHght - 20;
	const int displayW = ScrnWdth;
	/* F1/F12 Status Box position */
	const int StusBxX = 10;
	const int StusBxY = ScrnHght - 30;
	char Pgbuf[CPL * ROW];
	char SpdBuf[50];
	char WPMbuf[25];
	uint16_t PgbufColor[CPL * ROW];
	uint16_t ToneColor;
	uint16_t SpdClr;
	int CursorPntr;
	int cursorY;
	int cursorX;
	int cnt; // used in scrollpage routine
	int curRow;
	int offset;
	int StrdPntr;
	int StrdY;
	int StrdX;
	int Strdcnt; // used in scrollpage routine
	int StrdcurRow;
	int Strdoffset;
	int BlkStateVal[5];
	int BlkStateCntr;
	int SOToffCursorPntr;
	int oldstate; // jmh added just for debugging cw highlight cursor position
	bool BlkState;
	bool SOTFlg;
	bool OldSOTFlg;
	bool StrTxtFlg;
	bool OldStrTxtFlg;
	bool ToneFlg;
	bool SpdFlg;
	bool KBrdWPMFlg;
	bool SNFlg;
	bool UpdtKyBrdCrsr;
	bool Bump;
	bool PgScrld; // flag to indicate whether that the 'scroll' routine has ever run; i.e initially false. but always true once the process has
	bool CWActv;  // flag controlled by CWsendEngn; lets this class know when the sendengn is between letters/characters. used to block page scrolling while in the middle of a letter
	int OLDRxSig;
	// TFT_eSPI *ptft;
	char *pStrdTxt;
	void scrollpg(void);
	void HiLiteBG(void);
	void RestoreBG(void);
	void PosHiLiteCusr(void);
	void SettingsKBrdNtry(char Ntry, int paramptr);

public:
	// LVGLMsgBox( TFT_eSPI *tft_ptr, char *StrdTxt);
	LVGLMsgBox(char *StrdTxt);
	void InitDsplay(void);
	void ReBldDsplay(void);
	void KBentry(char Ascii, int paramptr);
	bool Delete(bool DeCdTxtFlg, int ChrCnt);
	void syncDfltwSettings(void);
	void dispDeCdrTxt(char Msgbuf[50], uint16_t Color);
	void dispKeyBrdTxt(char Msgbuf[50], uint16_t Color);
	void dispMsg2(int RxSig);
	void DisplCrLf(void);
	void IntrCrsr(int state);
	void dispStat(char Msgbuf[50], uint16_t Color);
	void showSpeed(char Msgbuf[50], uint16_t Color);
	void ShwKeybrdWPM(int wpm);/*New for waveshare/lvgl Display*/
	//void ShwDcodeSN(float sn);
	void setSOTFlg(bool flg);
	void setStrTxtFlg(bool flg);
	void SaveSettings(void);
	void ReStrSettings(void);
	void setCWActv(bool flg);
	bool getBGHilite(void);
	void ShwTone(uint16_t color);
	char GetLastChar(void);
	void UpdateToneSig(int curval);
	void BldSettingScreen(void);
	void BldScopeScreen(void);
	void BldHelpScreen(void);
	void BldSplashScreen(void);
	void FlipDayNiteMode(void);
	void ReStrtMainScrn(void);
	void NuLineDcdTA(void);
	void ClrDcdTA(void);
	void HiLite_Seltcd_Setting(int paramptr, int oldparamptr);
	void Exit_Settings(int paramptr);
	bool TestRingBufPtrs(int LastKnownPtr);
	int XferRingbuf(char Bfr[50], int oldPtr);
	void ScrollTA(bool up, int TAid);
	inline uint8_t Get_KyBrdBat_level(void) { return KyBrdBatLvl; }
	inline void Str_KyBrdBat_level(uint8_t Lvl) { KyBrdBatLvl = Lvl; }
	inline int Get_RingbufPntr1(void) { return RingbufPntr1; }
	// void DelLastNtry(void);
};
#ifdef __cplusplus
}
#endif

#endif /* INC_LVGLMSGBOX_H_ */
