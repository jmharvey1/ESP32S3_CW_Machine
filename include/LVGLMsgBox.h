/*
 * LVGLMsgBox.h
 *
 *  Created on: Oct 7, 2021
 *      Author: jim
 * Note: Update this file to get the REV DAtE to update on the Main Screen's title line
 * 20250913 Added new method/function NuLineDcdTA(void); Updated 'help' text to reflect new F6 & F7 functions
 * 20250203 changed i2c clock to 100Khz had been 400Khz, but found some display touch chips would not work w/ the faster data clock
 * 20250916 Updated 'help' text to reflect new Up & Down arrow functions
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
const char HelpText[] = {"KeyBoard Encoder Notes:\n\
Special Keys & their functions:\n\
Keys that send special Morse Characters\n\
1. \"=\"	<BT>\n\
2. \"+\"	<KN>\n\
3. \">\"	<AR>\n\
4. \"<\"	<AS>\n\
5. \"%\"	<SK>\n\
6. All other unassigned keys (i.e. \"{\", \"]\",...) send 6 dits, CW error code\n\n\
Special Functions:\n\
1. cntrl+T	Generates continuous key down state. Press \"cntrl+T\" again (or another key) to stop.\n\
2. Enter [pressed by itself] sends the \"My Call\" memory entry. (see settings screen)\n\
3. \"delete\" Back space, to delete unsent buffered code.\n\
4. Right Arrow	Same as F12.\n\
5. Left Arrow Same as F1.\n\
6. \"Lshift+Enter\" Send \"F1\" stored text plus Your call sign.\n\
7. \"Cntrl+Enter\" Send \"F1\" stored text.\n\
8. \"Esc\"   Abort/dump outgoing text\n\ 	
10. Cntrl+S Go to settings Screen; Press Cntrl+S again to return to normal CW mode.(Note: No CW sent while in the \"settings\"mode); i.e., Default WPM. Call Sign, F2 memory, F3..., \n\
11. F1 Save up to ten characters (usually the DX call sign) to be sent when L. Shift (or Cntrl)+enter is pressed, Press F1 again to stop the \"save\"  mode (Note: F1 Active while in \"save\" mode)\n\
12. F2 Send stored F2 message\n\
13. F3 Send stored F3 message\n\
14. F4 Send stored F4 message\n\
15. F12 suspend outgoing text; Press F12 again to resume sending outgoing text (Note: \"F12 SOT OFF\" while in \"suspend\" mode)\n\
16. Up, & Down Arrows, Increment/Decrement Send WPM respectively\n\
\n\
***********************************************************************************\n\
CW Decoder & other Keys:\n\
Special Keys & their functions:\n\
1. Left Ctrl+f: index through select tone tune modes\n\
        FF  (Fixed Frequency)\n\
        AF  (Auto-Tune 500 to 900Hz)\n\
2. Left Ctrl+G: Enable/Disable Debug\n\
3. Left Ctrl+P:  Plot On/off\n\
4. F6: Clear Decoded Text Space\n\
4. F7: New Line (in Decoded Text Space)\n\
5. F8: Light/Dark Display theme\n\
6. F9: Enable/Disable Scope View\n\    
\n\
\"F1 Mem\" and \"F1 Active\" enables typed characters to be saved into F1 memory\n\
\n\
The \"Enter\" key has 3 \"send\" modes;\n\ 
1. \"Enter\"       (send your call sign only);\n\ 
2. \"Shift+Enter\" (Send F1 call sign + your call sign);\n\
3. \"Ctrl+Enter\"  (Send F1 Call sign only). \n\
4. \"Shift+Up Arrow\" Scroll Up Decoded text\n\
5. \"Shift+Down Arrow\" Scroll Down Decoded text\n\
6. \"Ctrl+Up Arrow\" Scroll Up Send text\n\
7. \"Ctrl+Down Arrow\" Scroll Send text\n\
\n\
Note, if \"F12\" is in the \"SOT ON\" mode, the key presses will also be sent to your Tx (via the uart), allowing you to both return the \"far end\" call, & \"save\" the call, at the same time (for future returns/ \"overs\"). On the other hand, if you dont want to be sending (CW), while loading the F1 memory, press the F12 key, to read \"F12 SOT OFF\". Now this part gets a bit tricky. When you finished loading the F1 memory, press F1 (so it now reads F1 Mem), and while F12 reads \"F12 SOT OFF\", press the \"escape\" key (this will flush the F12 SOT buffer). Then press F12 (to get it back to \"F12 SOT ON\"), and youll have F1 loaded, and ready to go, without actually having sent the \"Far end\" call over the air.\n\
\n\
Remember, F1 is different from the other memories, as its intended just for storing \"call signs\" and to be changed \"on the fly\" Also note too, you can clear the F1 memory, just by cycling the F1 key (and not typing anything, while it reads \"F1 Active\")\n\
keyboard 'UP' & 'DOWN' Arrows will scroll this text\n\
"};

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
