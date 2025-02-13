/*
 * LVGLMsgBox.cpp
 *
 *  Created on: Oct 7, 2021
 *      Author: jim
 * 20230617 To support advanced DcodeCW "Bug" processes, reworked dispMsg2(void) to handle ASCII chacacter 0x8 "Backspace" symbol
 */
/*20240902 re-instated setStrTxtFlg(bool flg) - mainly to clear F1 Memory */
/*20240903 re-worked keyboard 'delete' key handling */
/*20240904 re-worked cursor management, in send test area, to improve outgoing character hilighting */
/*20240924 reworked Update_textarea() when 'capping the ta buffer to take in account the character width*/
/*20241026 Added 'F9' Scope Screen */
/*20241104 added 'ScopeActive' flag to better syncronize when its OK to update the 'SCOPE' screen*/
/*20241105 added F1 stored memory contents to main screen & Help Screen */
/*20241112 Added 'F8' Day Night mode per request from Carl (VK5CT)*/
/*20241113 reworked Night mode sig strenght bar color*/
/*20241115 Added Night mode to saved settings*/
/*20241124 Modified Night mode checkbox color*/
/*20241230 Added method ClrDcdTA(void) to support 'clear Decoded Text' space/area*/
/*20250203 Moved S/N log calc to dispMsg2()*/
/*20250203 Revised S/N process to show minimum value for displayed character*/
#include <stdio.h>
#include <math.h>
#include "sdkconfig.h"
#include "LVGLMsgBox.h"
#include "main.h"
#include "globals.h"
#include "CWSndEngn.h"

#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
/*Added the following for Waveshare & lvgl support*/
#include <lv_conf.h>
#include "lvgl.h"
#include "hal/lv_hal_tick.h"
#include "widgets/lv_textarea.h" /*GUI support*/
#include "widgets/lv_bar.h"
#include "widgets/lv_checkbox.h"
/*end Waveshare & lvgl includes*/
#include "driver/i2c_master.h"				//added for waveshare touch support
#include "touch/base/esp_lcd_touch_gt911.h" //added for waveshare touch support

CWSNDENGN *cwsnd;

esp_lcd_touch_handle_t kb = NULL;
// we use two semaphores to sync the VSYNC event and the LVGL task, to avoid potential tearing effect
#if CONFIG_MSGBX_AVOID_TEAR_EFFECT_WITH_SEM
SemaphoreHandle_t sem_vsync_end;
SemaphoreHandle_t sem_gui_ready;
#endif
SemaphoreHandle_t lvgl_mux;
SemaphoreHandle_t lvgl_semaphore = NULL;
// static TaskHandle_t Txt_Gen_Task_hndl = NULL;
// static TaskHandle_t vgl_port_task_hndl = NULL;
i2c_master_bus_handle_t i2c_master_bus_handle = nullptr; // added here for waveshare touch support
i2c_master_dev_handle_t Touch_dev_handle = NULL;		 // added here for waveshare touch support

/*GUI Variables*/
lv_obj_t *win3; // help screen
lv_obj_t *win2;
lv_obj_t *win1;
static lv_obj_t *cont1;
static lv_obj_t *cont2;
static lv_obj_t *cont3; // help screen
lv_obj_t *DecdTxtArea;
lv_obj_t *SendTxtArea;
static lv_obj_t *Helpta; // help screen
static lv_obj_t *MyCallta;
static lv_obj_t *WPMta;
static lv_obj_t *MemF2ta;
static lv_obj_t *MemF3ta;
static lv_obj_t *MemF4ta;
static lv_obj_t *MemF5ta;
lv_obj_t *label;
lv_obj_t *label2;
lv_obj_t *wpm_lbl;
lv_obj_t *SN_lbl;
lv_obj_t *Bat_Lvl_lbl;
lv_obj_t *bar1;
lv_obj_t *F1_Str_lbl;
lv_obj_t *F12_SOT_lbl;
lv_obj_t *Dbg_ChkBx;
lv_obj_t *chkbx_lbl;
lv_obj_t *ScrnMode_ChkBx;
lv_obj_t *chkbx_lbl1;
static lv_obj_t *save_btn;
static lv_obj_t *exit_btn;
static lv_obj_t *Hexit_btn;
/*added for scope Sceen*/
int bias_int = 0;
int freq_int = 0;// used in the scope view as current tone frequency
// int NxtRngBfrNdx = 0;
bool SmplSetRdy = false;
static lv_obj_t *ui_Scope;
static lv_obj_t *ui_Help;
static lv_obj_t *ui_Chart1;
static lv_obj_t *ui_Label1;
static lv_obj_t *ui_Label2;
lv_obj_t *ui____initial_actions0;
lv_chart_series_t *ui_Chart1_series_1 = NULL;
;
lv_chart_series_t *ui_Chart1_series_2 = NULL;
;
lv_timer_t *timer;
static lv_indev_drv_t indev_drv; // Input device driver (Touch)
/*Added for two screen (main & settings) support*/
static lv_coord_t title_height = 20;
static lv_obj_t *scr_1; // = lv_win_create(lv_scr_act(), title_height);
static lv_obj_t *scr_2; // = lv_win_create(NULL, title_height);
// static lv_obj_t *scrMaster;
static lv_style_t style_btn;
static lv_style_t style_Slctd_bg;
static lv_style_t style_Deslctd_bg;
static lv_style_t style_BtnDeslctd_bg;
static lv_style_t style_label;
static lv_style_t style_bar;
static lv_style_t style_chkbx;
static lv_style_t TAstyle;
static lv_style_t TASettingsStyle;
static lv_style_t Cursorstyle;
static lv_color_t Dflt_bg_clr;
static bool first_run = false;
// static float Nu_SN = 0.0;
bool flag = false;
bool traceFlg = false;
bool chkbxlocalevnt = true;
char buf[50];
char SNbuf[25];
const char *txt;
// static const char *TAG = "Txt_Test";
int ta_charCnt = 0;
int CurKyBrdCharCnt = 0;
/* int pksig = 0;
uint32_t pksigH = 0;
int sigSmplCnt =0; */
bool bypassMutex = false;
int MutexLckId = 0;
int KBrdCursorPntr = 0;
bool TchEvnt = false;
bool report = false;
bool Msg2Actv = false;
bool NiteMode = false;
bool NMchkbxVal_evnt = false;
int timerID = 0;
uint8_t OldBat_Lvl = 0;
void Bld_Help_scrn(void);
void Bld_Settings_scrn(void);
void Bld_Scope_scrn(void);
void Bld_LVGL_GUI(void);
void Sync_Dflt_Settings(void);
void SaveUsrVals(void);
void _FlipDayNiteMode(void);

static void screen1_event_handler(lv_event_t *e)
{
	const char *TAG1 = "screen1_event_handler";
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
	case LV_EVENT_CLICKED:
	{
		// printf("Main Screen (sc_1) 'Settings' button click event\n");
		setupFlg = true;
	}
	break;

	default:
		break;
	}
}
static void HelpBtn_event_handler(lv_event_t *e)
{
	const char *TAG1 = "HelpBtn_event_handler";
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
	case LV_EVENT_CLICKED:
	{
		// printf("Main Screen (sc_1) 'Help' button click event\n");
		HelpFlg = !HelpFlg;
	}
	break;

	default:
		break;
	}
}

static void ClrBtn_event_handler(lv_event_t *e)
{
	const char *TAG1 = "ClrBtn_event_handler";
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
	case LV_EVENT_CLICKED:
	{
		// printf("Main Screen (sc_1) 'Clear Text' button click event\n");
		lv_textarea_set_text(DecdTxtArea, "");
	}
	break;

	default:
		break;
	}
}

/*Actually the setting screen home/exit button call-back event handler*/
static void Settings_Scrn_evnt_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
	case LV_EVENT_CLICKED:
	{
		Sync_Dflt_Settings();
		Bld_LVGL_GUI();
	}
	break;

	default:
		break;
	}
}

static void Save_evnt_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	switch (code)
	{
	case LV_EVENT_CLICKED:
	{
		lv_color_t curbgclr = lv_obj_get_style_bg_color(e->current_target, LV_PART_MAIN);
		static lv_style_t style_SaveEvnt_bg;
		lv_style_init(&style_SaveEvnt_bg);
		lv_color_t Save_bgclr = lv_palette_main(LV_PALETTE_GREEN);
		lv_style_set_bg_color(&style_SaveEvnt_bg, Save_bgclr);
		lv_obj_add_style(save_btn, &style_SaveEvnt_bg, 0);
		_lv_disp_refr_timer(NULL);
		// do save code
		Sync_Dflt_Settings();
		SaveUsrVals();
		vTaskDelay(pdMS_TO_TICKS(250));
		lv_style_reset(&style_SaveEvnt_bg);
		lv_style_set_bg_color(&style_SaveEvnt_bg, curbgclr);
		lv_obj_add_style(save_btn, &style_SaveEvnt_bg, 0);
		_lv_disp_refr_timer(NULL);
	}
	break;

	default:
		break;
	}
}

static void Debug_chkBx_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *obj = lv_event_get_target(e);
	if (code == LV_EVENT_VALUE_CHANGED)
	{
		chkbxlocalevnt = true;
		const char *txt = lv_checkbox_get_text(obj);
		const char *state = lv_obj_get_state(obj) & LV_STATE_CHECKED ? "Checked" : "Unchecked";
		LV_LOG_USER("LV_EVENT_VALUE_CHANGED %s: %s", txt, state);
	}
	else if (code == LV_EVENT_CLICKED && !chkbxlocalevnt)
	{
		const char *txt = lv_checkbox_get_text(obj);
		if (lv_obj_get_state(obj))
			lv_obj_clear_state(obj, LV_STATE_CHECKED);
		else
			lv_obj_add_state(obj, LV_STATE_CHECKED);
		const char *state = lv_obj_get_state(obj) & LV_STATE_CHECKED ? "Checked" : "Unchecked";
		LV_LOG_USER("LV_EVENT_CLICKED %s: %s", txt, state);
	}
}

static void NiteMode_chkBx_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *obj = lv_event_get_target(e);
	if (code == LV_EVENT_VALUE_CHANGED)
	{
		NMchkbxVal_evnt = true;
		const char *txt = lv_checkbox_get_text(obj);
		const char *state = lv_obj_get_state(obj) & LV_STATE_CHECKED ? "Checked" : "Unchecked";
		// LV_LOG_USER("LV_EVENT_VALUE_CHANGED %s: %s", txt, state);
	}
	if (code == LV_EVENT_CLICKED)
	{ // && !NiteMode){
		const char *txt = lv_checkbox_get_text(obj);
		if (!NMchkbxVal_evnt)
		{ // we didn't get a change value event, so force a state change
			if (NiteMode)
			{
				lv_obj_clear_state(obj, LV_STATE_CHECKED);
				printf("Clear Check BOX; NiteMode:%d\n", (int)NiteMode);
				if (NiteMode)
					NiteMode = false;
			}
			else
			{
				lv_obj_add_state(obj, LV_STATE_CHECKED);
				printf("Set Check BOX; NiteMode:%d\n", (int)NiteMode);
			}
		}
		if (lv_obj_get_state(obj) && NiteMode)
		{
			lv_obj_clear_state(obj, LV_STATE_CHECKED);
			NiteMode = false;
		}
		else if (lv_obj_get_state(obj) && !NiteMode)
		{
			// lv_obj_add_state(obj, LV_STATE_CHECKED);
			NiteMode = true;
			printf("Step2 NiteMode:%d\n", (int)NiteMode);
		}
		else if (!lv_obj_get_state(obj) && NiteMode)
		{
			lv_obj_add_state(obj, LV_STATE_CHECKED);
			NiteMode = true;
			printf("Step3 NiteMode:%d\n", (int)NiteMode);
		}
		// else if(!lv_obj_get_state(obj) && NiteMode){
		// 	//lv_obj_add_state(obj, LV_STATE_CHECKED);
		// 	NiteMode = false;
		// }
		NMchkbxVal_evnt = false;
		const char *state = lv_obj_get_state(obj) & LV_STATE_CHECKED ? "Checked" : "Unchecked";
		LV_LOG_USER("LV_EVENT_CLICKED %s: %s; NiteMode:%d", txt, state, (int)NiteMode);
		if (DFault.NiteMode != NiteMode)
			_FlipDayNiteMode();
	}
}
/*Normal entry point for LVGLMsgBox::dispMsg2. The auto updater for decoded text*/
void Update_textarea(lv_obj_t *TxtArea, char bufChar)
{
	char buf2[25];
	bool tryagn = true;
	bool updateCharCnt = false;
	if (SendTxtArea == TxtArea)
		updateCharCnt = true;
	// if(traceFlg) printf("update_text2(char bufChar) %c\n", bufChar);
	if (bypassMutex)
	{
		int CurPos = 0;
		if (TxtArea == SendTxtArea)
		{
			lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
			CurPos = lv_textarea_get_cursor_pos(TxtArea);
		}
		if (bufChar == 0x08)
		{
			if (TxtArea == SendTxtArea)
			{
				lv_textarea_del_char(TxtArea);
				// const char *p = lv_textarea_get_text(TxtArea);
				// char LstChar = p[CurPos-1];
				// char Mthd = ' ';
				// if(LstChar == 0) LstChar = '?';
				// else if(LstChar == ' ')
				// {
				// 	lv_textarea_del_char(TxtArea);
				// 	LstChar = p[CurPos-1];//'*';
				// 	Mthd = 'D';
				// }
				// int ChrCnt = strlen(p);
				// void *Oldp = (void *)p;
				// //Oldp = (char *)realloc(Oldp, ChrCnt-1);
				// char *TaBuf = (char *)(Oldp);
				// if(ChrCnt>0) TaBuf[(ChrCnt-1)] = 0;//'\0';
				// int ChrCnt2 = strlen(p);
				// printf("CurPos:%d; ChrCnt:%d; ChrCnt2:%d; LstChar:%c; Mthd:%c;\n",CurPos , ChrCnt, ChrCnt2, LstChar, Mthd);
			}
			else
				lv_textarea_del_char(TxtArea);
		}
		else if (bufChar == 0xFF) // CW machine 'clear screen' command
		{
			lv_textarea_set_text(TxtArea, "");
			if (updateCharCnt && (TxtArea == SendTxtArea))
				CurKyBrdCharCnt = 0;
			// printf("CLEAR TEXT AREA 2\n");
		}
		else
		{
			/*Method to cap the max number of characters in storage at any given moment*/
			const char *p = lv_textarea_get_text(TxtArea);
			void *Oldp = (void *)p;
			int max = lv_textarea_get_max_length(TxtArea);
			ta_charCnt = strlen(p);
			if (updateCharCnt && (TxtArea == SendTxtArea))
				CurKyBrdCharCnt = ta_charCnt;
			int del = 1 - (max - ta_charCnt);
			if (del > 0)
			{
				del = 0;
				// p += del;
				int w = 0;
				int lastWrdBrk = 0;
				char *TaBuf = (char *)(Oldp);
				lv_point_t Scrn_pt;
				lv_draw_label_dsc_t label_dsc;
				lv_draw_label_dsc_init(&label_dsc);
				label_dsc.font = &lv_font_montserrat_18;
				char curChr;
				while ((*p != '\n') && (del < 98) && (w < 800)) // && (w < 760)
				{												/* Cap the number of characters deleted to 98 (i.e. ~one line of displayed text), or the 1st 'new line'*/
					del++;
					p++;
					curChr = TaBuf[del];
					if (curChr == 0x20)
						lastWrdBrk = del;
					TaBuf[del] = '\0';

					lv_txt_get_size(&Scrn_pt, TaBuf, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX,
									label_dsc.flag);
					w = Scrn_pt.x;
					// printf("del %d; X: %d; %s\n", del, w, TaBuf);
					TaBuf[del] = curChr;
				} // end while
				if (*p == '\n')
				{
					del++;
					p++;
				}
				else if (lastWrdBrk < del)
				{
					p -= del - lastWrdBrk;
					del = lastWrdBrk;
				}
				// printf("del %d\n\n", del);
				memcpy(Oldp, p, max - del); // shift the remaining text to the original start location
				Oldp = (char *)realloc(Oldp, 1 + (max - del));
				ta_charCnt = (max - del) + 1;
				if (updateCharCnt && (TxtArea == SendTxtArea))
					CurKyBrdCharCnt = ta_charCnt;
				// char *TaBuf = (char *)(Oldp);
				TaBuf[(max - del)] = '\0'; // terminate the newly truncated text buffer w/ a 								//a NULL
				/*now if the send text area is being updated, need to recalc where the current letter being sent
				 * located within the text string*/
				if (TxtArea == SendTxtArea)
					KBrdCursorPntr -= del;
				vTaskDelay(pdMS_TO_TICKS(20));
			} /*END CAP Text stored code*/
			if (TxtArea != SendTxtArea)
				lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
			lv_textarea_add_char(TxtArea, bufChar);
		}
		if (!updateCharCnt)
			lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);

		if (updateCharCnt)
			ta_charCnt = CurKyBrdCharCnt;

		else
			ta_charCnt++;
		if (TxtArea == DecdTxtArea)
		{
			sprintf(buf2, "CharCnt: %d", ta_charCnt);
			lv_label_set_text(label2, buf2);
		}
#ifdef AutoCorrect
		if (TxtArea == DecdTxtArea)
		{
			printf("<%c>", bufChar);
		}
#endif
	} // end bypass set mutex
	else
	{
		int trycnt = 0;
		while (tryagn)
		{
			if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
			{
				MutexLckId = 1;
				if (TxtArea == SendTxtArea)
				{
					lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
					int CurPos = 0;
					if (TxtArea == SendTxtArea)
					{
						lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
						// CurPos = lv_textarea_get_cursor_pos(TxtArea);
						// printf("+CurPos:%d;\n",CurPos);
						// vTaskDelay(4/portTICK_PERIOD_MS);
					}
				}
				if (bufChar == 0x08)
				{
					lv_textarea_del_char(TxtArea);
				}
				else if (bufChar == 0xFF)
				{
					lv_textarea_set_text(TxtArea, "");
					if (updateCharCnt)
						CurKyBrdCharCnt = 0;
					// printf("CLEAR TEXT AREA 1\n");
				}
				else
				{
					lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
					lv_textarea_add_char(TxtArea, bufChar);
				}
				if (TxtArea == SendTxtArea)
					lv_textarea_set_cursor_pos(TxtArea, KBrdCursorPntr);
				else
					lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
				ta_charCnt++;
				if (updateCharCnt && (TxtArea == SendTxtArea))
				{
					const char *p = lv_textarea_get_text(TxtArea);
					CurKyBrdCharCnt = strlen(p);
				}
				if (TxtArea == DecdTxtArea)
				{
					sprintf(buf2, "CharCnt: %d", ta_charCnt);
					lv_label_set_text(label2, buf2);
				}
				xSemaphoreGive(lvgl_semaphore);
				MutexLckId = 0;
				tryagn = false;
			}
			else
			{
				trycnt++;
				if (trycnt > 5)
				{
					trycnt = 5;
					// printf("LVGLMsgBox::update_text2 timed out; MutexLckId = %d\n", MutexLckId);
				}
				vTaskDelay(pdMS_TO_TICKS(20));
			}
		}
	}
	return;
}
/*Text Label that hosts Decoder Stats: WPM/Freq/Mode & other*/
void lvgl_update_RxStats(const char *buf2)
{
	// printf("%s\n",buf2);
	if (bypassMutex)
	{
		lv_label_set_text(label, buf2);
		return;
	}
	if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 20 / portTICK_PERIOD_MS))
	{
		MutexLckId = 2;
		lv_label_set_text(label, buf2);
		_lv_disp_refr_timer(NULL);
		xSemaphoreGive(lvgl_semaphore);
		MutexLckId = 0;
	}
	return;
}
void lvgl_update_KyBrdWPM(const char *buf2)
{
	// printf("%s\n",buf2);
	if (bypassMutex)
	{
		lv_label_set_text(wpm_lbl, buf2);
		return;
	}

	if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 20 / portTICK_PERIOD_MS))
	{
		MutexLckId = 2;
		lv_label_set_text(wpm_lbl, buf2);
		_lv_disp_refr_timer(NULL);
		xSemaphoreGive(lvgl_semaphore);
		MutexLckId = 0;
	}
	return;
}

void lvgl_update_SN(float sn)
{
	char buf2[25];
	sprintf(buf2, "S/N: %4.1fdB", sn);
	if (bypassMutex)
	{
		lv_label_set_text(SN_lbl, buf2);
		return;
	}

	if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 20 / portTICK_PERIOD_MS))
	{
		MutexLckId = 2;
		lv_label_set_text(SN_lbl, buf2);
		_lv_disp_refr_timer(NULL);
		xSemaphoreGive(lvgl_semaphore);
		MutexLckId = 0;
	}
	return;
}

void lvgl_update_Bat_Lvl(uint8_t lvl)
{
	char buf2[50];
	sprintf(buf2, "Kbrd Bat %d%%", (int)lvl);
	if (bypassMutex)
	{
		lv_label_set_text(Bat_Lvl_lbl, buf2);
		return;
	}

	if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 20 / portTICK_PERIOD_MS))
	{
		MutexLckId = 2;
		lv_label_set_text(Bat_Lvl_lbl, buf2);
		_lv_disp_refr_timer(NULL);
		xSemaphoreGive(lvgl_semaphore);
		MutexLckId = 0;
	}
	return;
}
void lvgl_UpdateToneSig(int curval)
{
	/*New approach this gets updated via dispMsg2(int RxSig). So no longer needs to set/clear semaphore */
	lv_bar_set_value(bar1, (uint32_t)curval, LV_ANIM_OFF);
	return;
}

void lvgl_UpdateF1Mem(bool ActvFlg)
{
	/*this gets updated via dispMsg2(int RxSig). So does not need set/clear semaphore */
	if (ActvFlg)
		lv_label_set_text(F1_Str_lbl, "F1 Active");
	else
	{
		char msg[25];
		if (strlen(StrdTxt) == 0)
		{
			sprintf(msg, "F1 Mem");
		}
		else
		{
			sprintf(msg, "F1 (%s)", StrdTxt);
		}
		// lv_label_set_text(F1_Str_lbl, "F1 Mem");
		lv_label_set_text(F1_Str_lbl, msg);
	}
	return;
}

void lvgl_UpdateSOT(bool ActvFlg)
{
	/*this gets update via dispMsg2(int RxSig). So does not need set/clear semaphore */
	if (ActvFlg)
		lv_label_set_text(F12_SOT_lbl, "F12 SOT ON");
	else
		lv_label_set_text(F12_SOT_lbl, "F12 SOT OFF");
	return;
}

void lvgl_update_KyBrdCrsr(bool HiLight)
{
	lv_textarea_set_cursor_pos(SendTxtArea, KBrdCursorPntr);
	if (HiLight)
	{
		lv_textarea_set_cursor_pos(SendTxtArea, KBrdCursorPntr);
		lv_obj_add_state(SendTxtArea, LV_STATE_FOCUSED);
		// printf("HiLight KBrdCursorPntr %d\n", KBrdCursorPntr);
	}
	else
	{
		lv_obj_clear_state(SendTxtArea, LV_STATE_FOCUSED); // hide cursor
		// printf("        KBrdCursorPntr %d\n", KBrdCursorPntr);
	}
};
/*Updates selected setting */
void lvgl_Update_setting(lv_obj_t *TxtArea, char bufChar)
{
	bool updateCharCnt = false;
	if (SendTxtArea == TxtArea)
		updateCharCnt = true;
	/*test & if needed convert to upper case*/
	if ((bufChar >= 97) & (bufChar <= 122))
	{
		bufChar = bufChar - 32;
	}
	// printf("lvgl_Update_setting(char bufChar) %c\n", bufChar);
	if (bypassMutex) // this should always be 'true'
	{
		lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);
		if (bufChar == 0x08)
		{
			lv_textarea_del_char(TxtArea);
		}
		else if (bufChar == 0xFF) // CW machine 'clear screen' command
		{
			lv_textarea_set_text(TxtArea, "");
			if (updateCharCnt)
				CurKyBrdCharCnt = 0;
		}
		else
		{
			/*Method to cap the max number of characters in storage at any given moment*/
			const char *p = lv_textarea_get_text(TxtArea);
			void *Oldp = (void *)p;
			// printf("%s/n", p);
			int need = 1; // strlen(buffer);
			int max = lv_textarea_get_max_length(TxtArea);
			ta_charCnt = strlen(p);
			if (updateCharCnt)
				CurKyBrdCharCnt = ta_charCnt;
			int del = need - (max - ta_charCnt);

			if (del > 0)
			{										// make room - delete earliest
				p += del;							// ta_charCnt;
				while ((*p != '\n') && (del < max)) // && (del < 98) Cap the max number of characters deleted to 98 i.e. one line of displayed text
				{
					del++;
					p++;
				} // delete to a line ending
				if (*p == '\n')
				{
					del++;
					p++;
				}
				memcpy(Oldp, p, max - del); // copy existing
				Oldp = (char *)realloc(Oldp, 1 + (max - del));
				ta_charCnt = (max - del) + 1;
				if (updateCharCnt)
					CurKyBrdCharCnt = ta_charCnt;
				char *TaBuf = (char *)(Oldp);
				TaBuf[(max - del)] = '\0'; // terminate the newly truncated text buffer w/ a a NULL
				vTaskDelay(pdMS_TO_TICKS(20));
				// printf("Deleted %d spaces\n", del);
			}
			/*END CAP Text stored code*/

			lv_textarea_add_char(TxtArea, bufChar);
		}
		lv_textarea_set_cursor_pos(TxtArea, LV_TEXTAREA_CURSOR_LAST);

	} // end bypass set mutex
	return;
}
void lvgl_DeSelct_Param(int paramptr)
{
	switch (paramptr)
	{
	case 0:
		lv_obj_add_style(MyCallta, &style_Deslctd_bg, 0);
		// lv_obj_add_style(MyCallta, &TASettingsStyle, 0);
		// printf("deselected MyCallta\n");
		break;
	case 1:
		lv_obj_add_style(WPMta, &style_Deslctd_bg, 0);
		// printf("deselected WPMta\n");
		break;
	case 2:
		lv_obj_add_style(chkbx_lbl, &style_Deslctd_bg, 0);
		lv_obj_add_style(Dbg_ChkBx, &style_chkbx, 0);
		// printf("deselected Dbg_ChkBx\n");
		break;
	case 3:
		lv_obj_add_style(chkbx_lbl1, &style_Deslctd_bg, 0);
		lv_obj_add_style(ScrnMode_ChkBx, &style_chkbx, 0);
		// printf("deselected ScrnMode_ChkBx\n");
		break;
	case 4:
		lv_obj_add_style(MemF2ta, &style_Deslctd_bg, 0);
		// printf("deselected MemF2ta\n");
		break;
	case 5:
		lv_obj_add_style(MemF3ta, &style_Deslctd_bg, 0);
		printf("deselected MemF3ta\n");
		break;
	case 6:
		lv_obj_add_style(MemF4ta, &style_Deslctd_bg, 0);
		// printf("deselected MemF4ta\n");
		break;
	case 7:
		lv_obj_add_style(MemF5ta, &style_Deslctd_bg, 0);
		// printf("deselected MemF5ta\n");
		break;
	case 8:
		lv_obj_add_style(save_btn, &style_BtnDeslctd_bg, 0);
		// printf("deselected save_btn\n");
		break;
	case 9:
		lv_obj_add_style(exit_btn, &style_BtnDeslctd_bg, 0);
		// printf("deselected exit_btn\n");
		break;
	}
};
/*Seeting Screen - Updates selected setting */
void lvgl_HiLite_Seltcd_Param(int paramptr)
{
	// style_Slctd_bg
	// lv_style_reset(&style_Slctd_bg);
	lv_color_t bgclr = lv_palette_main(LV_PALETTE_YELLOW);
	lv_style_set_bg_color(&style_Slctd_bg, bgclr);

	switch (paramptr)
	{
	case 0:
		lv_obj_add_style(MyCallta, &style_Slctd_bg, 0);
		// printf("Selected MyCallta\n\n");
		break;
	case 1:
		lv_obj_add_style(WPMta, &style_Slctd_bg, 0);
		// printf("Selected WPMta\n\n");
		break;
	case 2:
	{
		lv_obj_add_style(chkbx_lbl, &style_Slctd_bg, 0);
		lv_obj_add_style(Dbg_ChkBx, &style_chkbx, 0);
		// printf("Selected Dbg_ChkBx\n\n");
		break;
	}
	case 3:
	{
		lv_obj_add_style(chkbx_lbl1, &style_Slctd_bg, 0);
		lv_obj_add_style(ScrnMode_ChkBx, &style_chkbx, 0);
		// printf("Selected ScrnMode_ChkBx\n\n");
		break;
	}
	case 4:
		lv_obj_add_style(MemF2ta, &style_Slctd_bg, 0);
		// printf("Selected MemF2ta\n\n");
		break;
	case 5:
		lv_obj_add_style(MemF3ta, &style_Slctd_bg, 0);
		// printf("Selected MemF3ta\n\n");
		break;
	case 6:
		lv_obj_add_style(MemF4ta, &style_Slctd_bg, 0);
		// printf("Selected MemF4ta\n\n");
		break;
	case 7:
		lv_obj_add_style(MemF5ta, &style_Slctd_bg, 0);
		// printf("Selected MemF5ta\n\n");
		break;
	case 8:
		lv_obj_add_style(save_btn, &style_Slctd_bg, 0);
		// printf("Selected save_btn\n\n");
		break;
	case 9:
		lv_obj_add_style(exit_btn, &style_Slctd_bg, 0);
		// printf("Selected exit_btn\n\n");
		break;
	}
}

void Bld_Help_scrn(void)
{
	if (ui_Help == NULL)
	{
		ui_Help = lv_obj_create(NULL);
		lv_obj_clear_flag(ui_Help, LV_OBJ_FLAG_SCROLLABLE); /// Flags
		lv_style_reset(&style_btn);
		lv_style_set_border_width(&style_btn, 1);
		lv_style_set_border_opa(&style_btn, LV_OPA_100);
		lv_style_set_border_color(&style_btn, lv_color_black());
		lv_style_reset(&style_label);
		lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
		lv_style_set_text_opa(&style_label, LV_OPA_100);
		lv_style_set_text_color(&style_label, lv_color_black());
		if (win3 == NULL)
			win3 = lv_win_create(ui_Help, title_height);
		lv_win_add_title(win3, "Help (press 'F9' to exit this screen)");
		lv_obj_set_size(win3, 800, 480);
		cont3 = lv_win_get_content(win3);
		Helpta = lv_textarea_create(cont3);
		lv_obj_set_size(Helpta, 760, 400); // width & Height
		lv_obj_set_pos(Helpta, 0, 0);
		lv_textarea_set_text(Helpta, HelpText);
	}
	/*exit button*/
	Hexit_btn = lv_btn_create(cont3);
	lv_obj_add_style(Hexit_btn, &style_btn, 0);
	lv_obj_set_size(Hexit_btn, 100, 30);
	// lv_obj_align(exit_btn, LV_ALIGN_CENTER, 0, 401);
	lv_obj_set_pos(Hexit_btn, 340, 401);
	lv_obj_add_event_cb(Hexit_btn, HelpBtn_event_handler, LV_EVENT_CLICKED, NULL);

	lv_obj_t *Hexit_label = lv_label_create(Hexit_btn);
	lv_obj_add_style(Hexit_label, &style_label, 0);
	lv_label_set_text(Hexit_label, "Home");
	lv_obj_align_to(Hexit_label, Hexit_btn, LV_ALIGN_CENTER, 0, 0);
	lv_scr_load(ui_Help);
	lv_textarea_set_cursor_pos(Helpta, 1 - strlen(HelpText)); //
}

void Bld_Scope_scrn(void)
{
	if (ui_Scope == NULL)
	{
		ui_Scope = lv_obj_create(NULL);
		lv_obj_clear_flag(ui_Scope, LV_OBJ_FLAG_SCROLLABLE); /// Flags

		ui_Chart1 = lv_chart_create(ui_Scope);
		lv_obj_set_width(ui_Chart1, 712);
		lv_obj_set_height(ui_Chart1, 370);
		lv_obj_set_x(ui_Chart1, 0);
		lv_obj_set_y(ui_Chart1, 17);
		lv_obj_set_align(ui_Chart1, LV_ALIGN_CENTER);
		lv_chart_set_type(ui_Chart1, LV_CHART_TYPE_LINE);
		lv_chart_set_point_count(ui_Chart1, 700);
		lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, -100, 100);
		// lv_chart_set_div_line_count(ui_Chart1, 50, 24);
		lv_chart_set_div_line_count(ui_Chart1, 50, 37);
		lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_X, 10, 5, 10, 4, true, 50);
		lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 25, 1, true, 50);
		// lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 10, 5, 25, 1, true, 25);
		ui_Chart1_series_1 = lv_chart_add_series(ui_Chart1, lv_color_hex(0xFF0000),
												 LV_CHART_AXIS_PRIMARY_Y);
		static lv_coord_t ui_Chart1_series_1_array[700] = {0};
		lv_chart_set_ext_y_array(ui_Chart1, ui_Chart1_series_1, ui_Chart1_series_1_array);
		// ui_Chart1_series_2 = lv_chart_add_series(ui_Chart1, lv_color_hex(0x1576F6),
		//                                                              LV_CHART_AXIS_SECONDARY_Y);
		// static lv_coord_t ui_Chart1_series_2_array[24] = { 0 };
		// lv_chart_set_ext_y_array(ui_Chart1, ui_Chart1_series_2, ui_Chart1_series_2_array);

		ui_Label1 = lv_label_create(ui_Scope);
		lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);  /// 1
		lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT); /// 1
		lv_obj_set_x(ui_Label1, -100);
		lv_obj_set_y(ui_Label1, -202);
		lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
		lv_label_set_text(ui_Label1, "ADC Bias:");
		lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0xFD0A0A), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_opa(ui_Label1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_font(ui_Label1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

		ui_Label2 = lv_label_create(ui_Scope);
		lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);  /// 1
		lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT); /// 1
		lv_obj_set_x(ui_Label2, 195);
		lv_obj_set_y(ui_Label2, -203);
		lv_obj_set_align(ui_Label2, LV_ALIGN_CENTER);
		lv_label_set_text(ui_Label2, "Frequency:");
		lv_obj_set_style_text_color(ui_Label2, lv_color_hex(0x2669F1), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_opa(ui_Label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_font(ui_Label2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	lv_scr_load(ui_Scope);
}

void Bld_Settings_scrn(void)
{
	int row3 = 0;
	if (!first_run)
	{
		lv_style_init(&style_label);
		lv_style_init(&style_btn);
		lv_style_init(&TASettingsStyle);
		first_run = true;
	}

	lv_style_reset(&style_btn);
	lv_style_reset(&style_label);
	lv_style_reset(&TASettingsStyle);

	lv_style_set_text_font(&style_label, &lv_font_montserrat_14);
	lv_style_set_text_opa(&style_label, LV_OPA_100);
	lv_style_set_text_color(&style_label, lv_color_black());
	lv_style_set_text_color(&TASettingsStyle, lv_color_black());

	if (scr_2 == NULL)
	{
		scr_2 = lv_obj_create(NULL);
		lv_style_init(&style_btn);
		lv_style_set_border_width(&style_btn, 1);
		lv_style_set_border_opa(&style_btn, LV_OPA_100);
		lv_style_set_border_color(&style_btn, lv_color_black());
		if (win2 == NULL)
			win2 = lv_win_create(scr_2, title_height);
		lv_win_add_title(win2, "Settings");
		lv_obj_set_size(win2, 800, 480);
		cont2 = lv_win_get_content(win2);
		/*My Call Setting*/
		lv_obj_t *label = lv_label_create(cont2);
		lv_obj_set_size(label, 58, 20);
		lv_obj_set_pos(label, 5, 25);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(label, "My Call:");
		MyCallta = lv_textarea_create(cont2);
		lv_obj_set_size(MyCallta, 100, 20); // width & Height
		lv_obj_set_pos(MyCallta, 65, 12);
		lv_textarea_set_one_line(MyCallta, true);
		lv_textarea_set_max_length(MyCallta, 10);
		lv_obj_add_style(MyCallta, &TASettingsStyle, 0);
		lv_textarea_set_text(MyCallta, DFault.MyCall); // DFault.MyCall
		Dflt_bg_clr = lv_obj_get_style_bg_color(MyCallta, LV_PART_MAIN);

		/*WPM Setting*/
		lv_obj_t *WPMlabel = lv_label_create(cont2);
		lv_obj_set_size(WPMlabel, 40, 20);
		lv_obj_set_pos(WPMlabel, 180, 25);
		lv_label_set_long_mode(WPMlabel, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(WPMlabel, "WPM:");
		WPMta = lv_textarea_create(cont2);
		lv_obj_set_size(WPMta, 40, 20); // width & Height
		lv_obj_set_pos(WPMta, 225, 12);
		lv_textarea_set_one_line(WPMta, true);
		lv_textarea_set_accepted_chars(WPMta, "0123456789");
		lv_textarea_set_max_length(WPMta, 2);
		/*Convert WPM int parameter to character*/
		char WPM_char[2 + sizeof(char)];
		sprintf(WPM_char, "%d", DFault.WPM);
		lv_textarea_set_text(WPMta, WPM_char);

		/*MemF2 Setting*/
		lv_obj_t *MemF2_lbl = lv_label_create(cont2);
		lv_obj_set_size(MemF2_lbl, 58, 20);
		lv_obj_set_pos(MemF2_lbl, 0, 70);
		lv_label_set_long_mode(MemF2_lbl, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(MemF2_lbl, "F2 mem:");
		MemF2ta = lv_textarea_create(cont2);
		lv_obj_set_size(MemF2ta, 650, 40); // width & Height
		lv_obj_set_pos(MemF2ta, 65, 55);
		// lv_textarea_set_one_line(MemF2ta, true);
		lv_textarea_set_max_length(MemF2ta, 80);
		lv_textarea_set_text(MemF2ta, DFault.MemF2);

		/*MemF3 Setting*/
		int row1 = 70 + 45;
		lv_obj_t *MemF3_lbl = lv_label_create(cont2);
		lv_obj_set_size(MemF3_lbl, 58, 20);
		lv_obj_set_pos(MemF3_lbl, 0, row1);
		lv_label_set_long_mode(MemF3_lbl, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(MemF3_lbl, "F3 mem:");
		MemF3ta = lv_textarea_create(cont2);
		lv_obj_set_size(MemF3ta, 650, 40); // width & Height
		lv_obj_set_pos(MemF3ta, 65, row1 - 15);
		// lv_textarea_set_one_line(MemF3ta, true);
		lv_textarea_set_max_length(MemF3ta, 80);
		lv_textarea_set_text(MemF3ta, DFault.MemF3);

		/*MemF4 Setting*/
		int row2 = row1 + 45;
		lv_obj_t *MemF4_lbl = lv_label_create(cont2);
		lv_obj_set_size(MemF4_lbl, 58, 20);
		lv_obj_set_pos(MemF4_lbl, 0, row2);
		lv_label_set_long_mode(MemF4_lbl, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(MemF4_lbl, "F4 mem:");
		MemF4ta = lv_textarea_create(cont2);
		lv_obj_set_size(MemF4ta, 650, 40); // width & Height
		lv_obj_set_pos(MemF4ta, 65, row2 - 15);
		// lv_textarea_set_one_line(MemF4ta, true);
		lv_textarea_set_max_length(MemF4ta, 80);
		lv_textarea_set_text(MemF4ta, DFault.MemF4);

		/*MemF5 Setting*/
		row3 = row2 + 45;
		lv_obj_t *MemF5_lbl = lv_label_create(cont2);
		lv_obj_set_size(MemF5_lbl, 58, 20);
		lv_obj_set_pos(MemF5_lbl, 0, row3);
		lv_label_set_long_mode(MemF5_lbl, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(MemF5_lbl, "F5 mem:");
		MemF5ta = lv_textarea_create(cont2);
		lv_obj_set_size(MemF5ta, 650, 40); // width & Height
		lv_obj_set_pos(MemF5ta, 65, row3 - 15);
		// lv_textarea_set_one_line(MemF5ta, true);
		lv_textarea_set_max_length(MemF5ta, 80);
		lv_textarea_set_text(MemF5ta, DFault.MemF5);

		/*exit button*/
		int row4 = row3 + 45;
		exit_btn = lv_btn_create(cont2);
		lv_obj_add_style(exit_btn, &style_btn, 0);
		lv_obj_set_size(exit_btn, 80, 30);
		// lv_obj_align(exit_btn, LV_ALIGN_CENTER, 0, 200);
		lv_obj_set_pos(exit_btn, 340, row4);
		lv_obj_add_event_cb(exit_btn, Settings_Scrn_evnt_cb, LV_EVENT_CLICKED, NULL);

		lv_obj_t *exit_btn_label = lv_label_create(exit_btn);
		lv_obj_add_style(exit_btn_label, &style_label, 0);
		lv_label_set_text(exit_btn_label, "Home");
		lv_obj_align_to(exit_btn_label, exit_btn, LV_ALIGN_CENTER, 0, 0);

		/*save button*/
		// int row4 = row3 + 45;
		save_btn = lv_btn_create(cont2);
		lv_obj_add_style(save_btn, &style_btn, 0);
		lv_obj_set_size(save_btn, 80, 30);
		lv_obj_set_pos(save_btn, 240, row4);
		lv_obj_add_event_cb(save_btn, Save_evnt_cb, LV_EVENT_CLICKED, NULL);

		lv_obj_t *save_btn_label = lv_label_create(save_btn);
		lv_obj_add_style(save_btn_label, &style_label, 0);
		lv_label_set_text(save_btn_label, "Save");
		lv_obj_align_to(save_btn_label, save_btn, LV_ALIGN_CENTER, 0, 0);

		/*Debug Checkbox*/
		/*the following label is just used for managing highlighting the selected checkbox*/
		chkbx_lbl = lv_textarea_create(cont2);
		lv_obj_set_size(chkbx_lbl, 80, 24);
		lv_obj_set_pos(chkbx_lbl, 285, 21);

		lv_obj_add_style(chkbx_lbl, &style_chkbx, 0);
		Dbg_ChkBx = lv_checkbox_create(cont2);
		lv_checkbox_set_text(Dbg_ChkBx, "DeBug");
		lv_obj_add_event_cb(Dbg_ChkBx, Debug_chkBx_cb, LV_EVENT_ALL, NULL);
		lv_obj_set_pos(Dbg_ChkBx, 285, 24);
		if (DFault.DeBug)
			lv_obj_add_state(Dbg_ChkBx, LV_STATE_CHECKED); /*Make the chekbox checked*/
		else
			lv_obj_clear_state(Dbg_ChkBx, LV_STATE_CHECKED);

		/*Display Mode*/
		/*the following label is just used for managing highlighting the selected checkbox*/
		chkbx_lbl1 = lv_textarea_create(cont2);
		lv_obj_set_size(chkbx_lbl1, 145, 24);
		lv_obj_set_pos(chkbx_lbl1, 390, 21);

		ScrnMode_ChkBx = lv_checkbox_create(cont2);
		lv_checkbox_set_text(ScrnMode_ChkBx, "Scrn Dark Mode");
		lv_obj_add_event_cb(ScrnMode_ChkBx, NiteMode_chkBx_cb, LV_EVENT_ALL, NULL);
		lv_obj_set_pos(ScrnMode_ChkBx, 390, 24);
		if (DFault.NiteMode)
			lv_obj_add_state(ScrnMode_ChkBx, LV_STATE_CHECKED); /*Make the chekbox checked*/
		else
			lv_obj_clear_state(ScrnMode_ChkBx, LV_STATE_CHECKED);

		/*setup for later use deselected background colors*/
		/*Need 2 colors because buttons are different from all other fields*/
		lv_style_reset(&style_Deslctd_bg);
		lv_style_reset(&style_BtnDeslctd_bg);

		lv_color_t Btn_bgclr = lv_palette_main(LV_PALETTE_LIGHT_BLUE);

		lv_style_set_bg_color(&style_Deslctd_bg, Dflt_bg_clr);
		lv_style_set_bg_color(&style_BtnDeslctd_bg, Btn_bgclr);
	}
	lv_scr_load(scr_2);
}

/*Build GUI (Main Screen)*/
void Bld_LVGL_GUI(void)
{
	if (scr_1 == NULL)
	{
		scr_1 = lv_obj_create(NULL);
		// printf("scr_1 = lv_obj_create(NULL)\n");
	}

	if (!first_run)
	{
		lv_style_init(&style_bar);
		lv_style_init(&TAstyle);
		lv_style_init(&Cursorstyle);
		lv_style_init(&style_label);
		lv_style_init(&style_btn);
		lv_style_init(&style_Slctd_bg);
		lv_style_init(&style_Deslctd_bg);
		lv_style_init(&style_BtnDeslctd_bg);
		first_run = true;
	}
	/*Part of 2 screen support*/
	lv_style_reset(&style_btn);
	lv_style_reset(&style_label);
	if (!NiteMode)
		lv_style_reset(&style_bar);

	lv_style_set_text_font(&style_label, &lv_font_montserrat_18);
	lv_style_set_text_opa(&style_label, LV_OPA_100);
	lv_style_set_text_color(&style_label, lv_color_black());

	lv_style_set_border_width(&style_btn, 1);
	lv_style_set_border_opa(&style_btn, LV_OPA_100);
	lv_style_set_border_color(&style_btn, lv_color_black());

	lv_style_set_text_font(&TAstyle, &lv_font_montserrat_16);
	/*style settings for night view */
	// lv_style_set_bg_color(&style_win, lv_palette_main(LV_PALETTE_NONE));
	//  lv_style_set_text_color(&TAstyle, lv_palette_main(LV_PALETTE_RED));
	/*end night view setup*/
	lv_style_set_bg_color(&Cursorstyle, lv_palette_main(LV_PALETTE_YELLOW)); // set cursor color
	if (win1 == NULL)
	{
		char Title[100];
		char RevDate[12] = TODAY;
		// printf("win1 = lv_win_create(scr_1, title_height);\n");
		title_height = 20;
		win1 = lv_win_create(scr_1, title_height);
		sprintf(Title, " ESP32s3 CW Machine (%s)", RevDate);
		lv_win_add_title(win1, Title);
		lv_obj_set_size(win1, 800, 480);
		// lv_obj_add_style(win1, &style_win, 0);
		cont1 = lv_win_get_content(win1); /*Content can be added here*/

		bar1 = lv_bar_create(scr_1);
		lv_obj_set_size(bar1, 200, 15);
		lv_obj_set_pos(bar1, 110, 23);
		lv_bar_set_value(bar1, 10, LV_ANIM_OFF);
		lv_obj_add_style(bar1, &style_bar, LV_PART_INDICATOR);
		DecdTxtArea = lv_textarea_create(cont1);
		SendTxtArea = lv_textarea_create(cont1);
		lv_obj_set_size(DecdTxtArea, 760, 179); // width & Height
		lv_obj_set_size(SendTxtArea, 760, 179); // width & Height
		lv_obj_set_pos(DecdTxtArea, 0, 0);
		lv_obj_set_pos(SendTxtArea, 0, 220);
		lv_textarea_set_max_length(DecdTxtArea, 804);									// 1000-(2*98) = 804
		lv_textarea_set_max_length(SendTxtArea, 804);									// 1000-(2*98) = 804
		lv_obj_add_style(DecdTxtArea, &TAstyle, 0);										// sets the fonte used
		lv_obj_add_style(SendTxtArea, &TAstyle, 0);										// sets the fonte used
		lv_obj_add_style(SendTxtArea, &Cursorstyle, LV_PART_CURSOR | LV_STATE_FOCUSED); // set cursor color
		lv_obj_set_style_bg_opa(SendTxtArea, LV_OPA_40, LV_PART_CURSOR | LV_STATE_FOCUSED);
		lv_obj_set_style_border_side(SendTxtArea, LV_BORDER_SIDE_NONE, LV_PART_CURSOR | LV_STATE_FOCUSED); // kill the default left side cusor line
		label = lv_label_create(cont1);
		lv_obj_set_size(label, 350, 20);
		lv_obj_set_pos(label, 10, 180);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP); // LV_LABEL_LONG_CLIP
		lv_label_set_text(label, "Decoder Status");
		label2 = lv_label_create(cont1);
		lv_obj_set_size(label2, 350, 20);
		lv_obj_set_pos(label2, 660, 180); // 410
		lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);

		wpm_lbl = lv_label_create(cont1);
		lv_obj_set_size(wpm_lbl, 150, 20);
		lv_obj_set_pos(wpm_lbl, 675, 400);
		lv_label_set_long_mode(wpm_lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(wpm_lbl, "-- WPM");

		SN_lbl = lv_label_create(scr_1);
		lv_obj_set_size(SN_lbl, 80, 20);
		lv_obj_set_pos(SN_lbl, 20, 23);
		lv_label_set_long_mode(SN_lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(SN_lbl, "S/N:");

		F1_Str_lbl = lv_label_create(cont1);
		lv_obj_set_size(F1_Str_lbl, 100, 20);
		lv_obj_set_pos(F1_Str_lbl, 10, 400);
		lv_label_set_long_mode(F1_Str_lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(F1_Str_lbl, "F1 Mem");

		F12_SOT_lbl = lv_label_create(cont1);
		lv_obj_set_size(F12_SOT_lbl, 90, 20);
		lv_obj_set_pos(F12_SOT_lbl, 120, 400);
		lv_label_set_long_mode(F12_SOT_lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(F12_SOT_lbl, "F12 SOT ON");

		Bat_Lvl_lbl = lv_label_create(cont1);
		lv_obj_set_size(Bat_Lvl_lbl, 150, 20);
		lv_obj_set_pos(Bat_Lvl_lbl, 550, 400);
		lv_label_set_long_mode(Bat_Lvl_lbl, LV_LABEL_LONG_CLIP);
		lv_label_set_text(Bat_Lvl_lbl, "-- %");
	}
	else
	{
		// printf("SKIP win1 = lv_win_create(scr_1, title_height);\n");
	}

	/*Setup settings button at bottom of display*/
	lv_obj_t *exit_btn = lv_btn_create(scr_1);
	lv_obj_add_style(exit_btn, &style_btn, 0);
	lv_obj_set_size(exit_btn, 80, 30);
	lv_obj_set_pos(exit_btn, 320, 440);
	lv_obj_add_event_cb(exit_btn, screen1_event_handler, LV_EVENT_CLICKED, NULL);

	lv_obj_t *exit_btn_label = lv_label_create(exit_btn);
	lv_obj_add_style(exit_btn_label, &style_label, 0);
	lv_label_set_text(exit_btn_label, "Settings");
	lv_obj_align_to(exit_btn_label, exit_btn, LV_ALIGN_CENTER, 0, 2);

	/*Setup 'help' button at bottom of display*/
	lv_obj_t *help_btn = lv_btn_create(scr_1);
	lv_obj_add_style(help_btn, &style_btn, 0);
	lv_obj_set_size(help_btn, 80, 30);
	lv_obj_set_pos(help_btn, 410, 440);
	lv_obj_add_event_cb(help_btn, HelpBtn_event_handler, LV_EVENT_CLICKED, NULL);

	lv_obj_t *help_btn_label = lv_label_create(help_btn);
	lv_obj_add_style(help_btn_label, &style_label, 0);
	lv_label_set_text(help_btn_label, "Help");
	lv_obj_align_to(help_btn_label, help_btn, LV_ALIGN_CENTER, 0, 2);

	/*Setup 'Clear' button @ base of Decoder Text Area*/
	lv_obj_t *ClrTxt_btn = lv_btn_create(scr_1);
	lv_obj_add_style(ClrTxt_btn, &style_btn, 0);
	lv_obj_set_size(ClrTxt_btn, 90, 30);
	lv_obj_set_pos(ClrTxt_btn, 320, 221);
	lv_obj_add_event_cb(ClrTxt_btn, ClrBtn_event_handler, LV_EVENT_CLICKED, NULL);

	lv_obj_t *ClrTxt_btn_label = lv_label_create(ClrTxt_btn);
	lv_obj_add_style(ClrTxt_btn_label, &style_label, 0);
	lv_label_set_text(ClrTxt_btn_label, "Clear Text");
	// lv_obj_align_to(ClrTxt_btn_label, ClrTxt_btn, LV_ALIGN_CENTER, 10, 2);
	lv_obj_align_to(ClrTxt_btn_label, ClrTxt_btn, LV_ALIGN_CENTER, 0, 0);
	lv_scr_load(scr_1);

	if (scr_2 != NULL)
	{
		printf("[APP] Free memory: %d bytes\n", (int)esp_get_free_heap_size());
		printf("Hide Screen2:\n");
		setupFlg = false;
	}
}

/*JMH ADD*/
esp_lcd_touch_config_t lcd_touch_config = {
	.x_max = MSGBX_LCD_H_RES,
	.y_max = MSGBX_LCD_V_RES,
	.rst_gpio_num = (gpio_num_t)-1,
	.int_gpio_num = (gpio_num_t)-1,
	.levels = {
		.reset = 0,		// 0: low level, 1: high level
		.interrupt = 0, // 0: low level, 1: high level
	},
	.flags = {
		.swap_xy = false,
		.mirror_x = false,
		.mirror_y = false,
	},
	.process_coordinates = NULL,
	.interrupt_callback = NULL,
	.user_data = NULL,
};
/*lvgl_tick_timer Call back routine*/
static void lvgl_tick(void *arg)
{
	(void)arg;

	lv_tick_inc(LV_TICK_PERIOD_MS);
}

static bool lvgl_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
	BaseType_t high_task_awoken = pdFALSE;
#if CONFIG_MSGBX_AVOID_TEAR_EFFECT_WITH_SEM
	if (xSemaphoreTakeFromISR(sem_gui_ready, &high_task_awoken) == pdTRUE)
	{

		xSemaphoreGiveFromISR(sem_vsync_end, &high_task_awoken);
	}
#endif
	return high_task_awoken == pdTRUE;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
	/*JMH copied from lvgl_port_v8.cpp (2 buffer callback)*/
	// ESP_PanelLcd *lcd = (ESP_PanelLcd *)drv->user_data;
	esp_lcd_panel_handle_t display_handle = (esp_lcd_panel_handle_t)drv->user_data;
	const int offsetx1 = area->x1;
	const int offsetx2 = area->x2;
	const int offsety1 = area->y1;
	const int offsety2 = area->y2;
	/* Switch the current RGB frame buffer to `color_map` */
	// lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)color_map);
	esp_lcd_panel_draw_bitmap(display_handle, offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, color_map);
/* Waiting for the last frame buffer to complete transmission */
// ulTaskNotifyValueClear(NULL, ULONG_MAX);
// ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#if CONFIG_MSGBX_AVOID_TEAR_EFFECT_WITH_SEM
	xSemaphoreGive(sem_gui_ready);
	if (xSemaphoreTake(sem_vsync_end, pdMS_TO_TICKS(100)) == pdTRUE)
		lv_disp_flush_ready(drv); // JMH ADD
	else
		printf("lvgl_flush_cb: failed to take 'sem_vsync_end' Semaphore\n");
#else
	lv_disp_flush_ready(drv);
#endif

	/*JMH end of vgl_port_v8.cpp (2 buffer callback)*/

	// FlushF_cnt++;
	// FlushFired = true;
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
	uint16_t touchpad_x[1] = {0};
	uint16_t touchpad_y[1] = {0};
	uint8_t touchpad_cnt = 0;

	/* Read touch controller data */
	// esp_lcd_touch_read_data((esp_lcd_touch_handle_t)drv->user_data);//JMH Approach
	esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
	tp->read_data(tp);
	// esp_lcd_touch_read_data(tp);

	/* Get coordinates */
	bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
	if (touchpad_pressed && touchpad_cnt > 0)
	{
		// if (MsgBx_lvgl_lock(-1))
		// {
		data->point.x = touchpad_x[0];
		data->point.y = touchpad_y[0];
		data->state = LV_INDEV_STATE_PR;
		//	MsgBx_lvgl_unlock();
		TchEvnt = true;
		//}
	}
	else
	{
		data->state = LV_INDEV_STATE_REL;
	}
	// printf("vgl_touch_cb\n");
}

void _FlipDayNiteMode(void)
{
	if (DFault.NiteMode == NiteMode)
		return; // added this test to abort rerunning this code when moving from main screen to settings screen
	_lv_theme_t *_theme = lv_theme_default_get();
	lv_style_reset(&TAstyle);
	lv_style_reset(&style_bar);
	lv_style_reset(&style_label);
	lv_style_reset(&style_chkbx);
	lv_style_set_text_font(&TAstyle, &lv_font_montserrat_16);
	lv_style_set_text_opa(&style_label, LV_OPA_100);
	lv_style_set_text_color(&style_label, lv_color_black());
	if (NiteMode)
	{ /*style settings for night view */
		_theme = lv_theme_default_init(_theme->disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
									   1, LV_FONT_DEFAULT);

		lv_style_set_text_color(&TAstyle, lv_palette_main(LV_PALETTE_RED));
		lv_style_set_bg_color(&style_bar, lv_palette_main(LV_PALETTE_BROWN));
		lv_style_set_bg_color(&style_Deslctd_bg, Dflt_bg_clr);
		lv_style_set_text_color(&style_Deslctd_bg, lv_color_white());
		// lv_style_set_text_color(&TASettingsStyle, lv_color_white());// Dflt_bg_clr);
		lv_style_set_text_color(&style_Slctd_bg, lv_color_black());
		lv_style_set_text_color(&style_chkbx, lv_color_black());
		/*end night view setup*/
	}
	else
	{
		_theme = lv_theme_default_init(_theme->disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
									   0, LV_FONT_DEFAULT);
		lv_style_set_bg_color(&style_Deslctd_bg, lv_color_white());
	}
	DFault.NiteMode = NiteMode;
};

/**
 * @brief i2c master initialization
 * using new/current ESPRESSIF I2C ESPIDF 5.2 I2C protocol
 */
static esp_err_t i2c_master_init(void)
{
	static const char *TAG = "i2c_master_init";
	i2c_master_bus_config_t i2c_mst_config = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
		.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 64,
		.flags = {
			.enable_internal_pullup = true,
		},
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_master_bus_handle));

	i2c_device_config_t lcd_touch_panel_io_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = (0x5D), // GT911 chip address
		.scl_speed_hz = 400000,
	};
	/*JMH ADD*/
	ESP_LOGD(TAG, "Create I2C BUS");
	/*OR maybe this way, based on ESPRESSIF ESPIDF Programming Guide*/
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_master_bus_handle, &lcd_touch_panel_io_config, &Touch_dev_handle));

	return ESP_OK;
}

LVGLMsgBox::LVGLMsgBox(char *StrdTxt)
{

	pStrdTxt = StrdTxt;
	txtpos = 0;
	RingbufPntr1 = 0;
	RingbufPntr2 = 0;
	CursorPntr = 0;
	cursorY = 0;
	cursorX = 0;
	cnt = 0; // used in scrollpage routine
	curRow = 0;
	offset = 0;
	oldstate = 0;
	BlkState = false;
	SOTFlg = true;
	OldSOTFlg = true;
	StrTxtFlg = false;
	OldStrTxtFlg = false;
	ToneFlg = false;
	SpdFlg = false;
	KBrdWPMFlg = false;
	Bump = false;
	PgScrld = false;
	BGHilite = false;
	ToneColor = 0;
	OLDRxSig = 0;
};

void LVGLMsgBox::InitDsplay(void)
{
	static const char *TAG = "LVGLMsgBox::Init";
	// cwsnd = cwsnd_ptr;
	UpdtKyBrdCrsr = false;
	static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
	static lv_disp_drv_t disp_drv;		// contains callback functions
										// esp_log_level_set("*", ESP_LOG_VERBOSE); // ESP_LOG_NONE,       /*!< No log output */
	//  ESP_LOG_ERROR,      /*!< Critical errors, software module can not recover on its own */
	//  ESP_LOG_WARN,       /*!< Error conditions from which recovery measures have been taken */
	//  ESP_LOG_INFO,       /*!< Information messages which describe normal flow of events */
	//  ESP_LOG_DEBUG,      /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
	//  ESP_LOG_VERBOSE     /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */

#if CONFIG_MSGBX_AVOID_TEAR_EFFECT_WITH_SEM
	ESP_LOGI(TAG, "Create semaphores");
	sem_vsync_end = xSemaphoreCreateBinary();
	assert(sem_vsync_end);
	sem_gui_ready = xSemaphoreCreateBinary();
	assert(sem_gui_ready);
#endif

#if MSGBX_PIN_NUM_BK_LIGHT >= 0
	ESP_LOGI(TAG, "Turn off LCD backlight");
	gpio_config_t bk_gpio_config = {
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask = 1ULL << MSGBX_PIN_NUM_BK_LIGHT};
	ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

	ESP_LOGI(TAG, "Install RGB LCD Display driver");
	esp_lcd_panel_handle_t display_handle = NULL;

	esp_lcd_rgb_panel_config_t display_config = {
		.clk_src = LCD_CLK_SRC_DEFAULT,
		.timings = {
			.pclk_hz = (14 * 1000 * 1000),
			.h_res = MSGBX_LCD_H_RES,
			.v_res = MSGBX_LCD_V_RES,
			.hsync_pulse_width = 4,
			.hsync_back_porch = 8,
			.hsync_front_porch = 8,
			.vsync_pulse_width = 4,
			.vsync_back_porch = 8,
			.vsync_front_porch = 8,
			.flags = {
				.hsync_idle_low = 0,
				.vsync_idle_low = 0,
				.de_idle_high = 0,
				.pclk_active_neg = 0,
				.pclk_idle_high = 0,
			},
		},
		.data_width = 16,								 // 8 | 16
		.bits_per_pixel = 16,							 // 24 | 16
		.num_fbs = 1,									 // 1/2/3
		.bounce_buffer_size_px = (MSGBX_LCD_H_RES * 10), // Bounce buffer size in bytes. This function is used to avoid screen drift.
		.sram_trans_align = 4,
		.psram_trans_align = 64,
		.hsync_gpio_num = MSGBX_PIN_NUM_HSYNC,
		.vsync_gpio_num = MSGBX_PIN_NUM_VSYNC,
		.de_gpio_num = MSGBX_PIN_NUM_DE,
		.pclk_gpio_num = MSGBX_PIN_NUM_PCLK,
		.disp_gpio_num = -1, // not used
		.data_gpio_nums = {
			MSGBX_PIN_NUM_DATA0,
			MSGBX_PIN_NUM_DATA1,
			MSGBX_PIN_NUM_DATA2,
			MSGBX_PIN_NUM_DATA3,
			MSGBX_PIN_NUM_DATA4,
			MSGBX_PIN_NUM_DATA5,
			MSGBX_PIN_NUM_DATA6,
			MSGBX_PIN_NUM_DATA7,
			MSGBX_PIN_NUM_DATA8,
			MSGBX_PIN_NUM_DATA9,
			MSGBX_PIN_NUM_DATA10,
			MSGBX_PIN_NUM_DATA11,
			MSGBX_PIN_NUM_DATA12,
			MSGBX_PIN_NUM_DATA13,
			MSGBX_PIN_NUM_DATA14,
			MSGBX_PIN_NUM_DATA15,
		},
		.flags = {
			.disp_active_low = 0,
			.refresh_on_demand = 0,
			.fb_in_psram = 1,
			.double_fb = 0,
			.no_fb = 0,
			.bb_invalidate_cache = 0,
		},
	};
	/*Register display & get a pointer/handle to it*/
	ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&display_config, &display_handle));
	ESP_LOGI(TAG, "Register Display event callbacks");
	esp_lcd_rgb_panel_event_callbacks_t cbs = {
		.on_vsync = lvgl_vsync_event,
	};
	ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(display_handle, &cbs, &disp_drv));

	ESP_LOGI(TAG, "Initialize RGB LCD Display");
	ESP_ERROR_CHECK(esp_lcd_panel_reset(display_handle));
	ESP_ERROR_CHECK(esp_lcd_panel_init(display_handle));

#if MSGBX_PIN_NUM_BK_LIGHT >= 0
	ESP_LOGI(TAG, "Turn on LCD backlight");
	gpio_set_level(MSGBX_PIN_NUM_BK_LIGHT, MSGBX_LCD_BK_LIGHT_ON_LEVEL);
#endif
	/* setup I2C link to GT911 touch sensor */
	ESP_ERROR_CHECK(i2c_master_init());
	ESP_LOGI(TAG, "I2C Master initialized successfully");

	ESP_LOGI(TAG, "Initialize GT911 I2C touch sensor");
	esp_lcd_touch_handle_t tp = NULL;
	esp_lcd_panel_io_handle_t tp_io_handle = NULL;
	esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(); // defined in esp_lcd_touch_gt911.h

	/* instantuate touch object */
	if (esp_lcd_new_panel_io_i2c_v2(i2c_master_bus_handle, &tp_io_config, &tp_io_handle) != ESP_OK)
	{
		ESP_LOGE(TAG, "failed to create GT911 touch interface");
	}
	else
	{
		ESP_LOGI(TAG, "touch interface handle %p", tp_io_handle);
	}

	/* Initialize touch using, esp_lcd_touch_gt911.c, found at /lib/ESP32_Display_Panel/src/touch/base */
	ESP_LOGI(TAG, "Initialize/configure GT911 touch controller");
	ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &lcd_touch_config, &tp));
	printf("lcd_touch_handle %p\n", tp);
	lvgl_semaphore = xSemaphoreCreateMutex();
	ESP_LOGI(TAG, "Initialize LVGL library");
	lv_init();
	void *buf1 = NULL; // will be used by lvgl as a staging area (memory space) to create display images/maps before posting to display ST7262
	void *buf2 = NULL; // same as above, is the 2nd of two buffer spaces
#if CONFIG_MSGBX_DOUBLE_FB
	ESP_LOGI(TAG, "Use frame buffers as LVGL draw buffers");
	ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(display_handle, 2, &buf1, &buf2));
	// initialize LVGL draw buffers
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, MSGBX_LCD_H_RES * MSGBX_LCD_V_RES);
#else
	ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM");
	int buffer_size = (LVGL_PORT_DISP_WIDTH) * (LVGL_PORT_DISP_HEIGHT);
	/*JMH commented out & replaced w/ the following:
	Note: inspite of above config setting we are actually using two buffers*/
	// buf1 = heap_caps_malloc(MSGBX_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
	buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM); // MALLOC_CAP_DMA
	assert(buf1);
	buf2 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM); // MALLOC_CAP_DMA
	assert(buf2);

	// initialize LVGL draw buffers
	lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size); // MSGBX_LCD_H_RES * 100
#endif // CONFIG_MSGBX_DOUBLE_FB

	ESP_LOGI(TAG, "Register display driver to LVGL");
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = MSGBX_LCD_H_RES;
	disp_drv.ver_res = MSGBX_LCD_V_RES;
	disp_drv.flush_cb = lvgl_flush_cb;
	disp_drv.draw_buf = &disp_buf;
	disp_drv.user_data = display_handle;
#if CONFIG_MSGBX_DOUBLE_FB
	disp_drv.full_refresh = true; // the full_refresh mode can maintain the synchronization between the two frame buffers
#endif
	lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

	disp_drv.full_refresh = true; // the full_refresh mode can maintain the synchronization between the two frame buffers

	ESP_LOGI(TAG, "Install LVGL tick timer");
	// Tick Interface for LVGL using esp_timer to generate 2ms periodic event
	const esp_timer_create_args_t lvgl_tick_timer_args =
		{
			.callback = &lvgl_tick,
			.name = "lvgl_tick",
			.skip_unhandled_events = true};
	esp_timer_handle_t lvgl_tick_timer;
	ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
	/*Delete the Default display refresh timer*/
	lv_timer_del(disp->refr_timer);
	disp->refr_timer = NULL;
	/*Note: Call '_lv_disp_refr_timer(NULL);'
	whenever you want to refresh the dirty areas */
	/*Only doing this to supress warning timer message from lv_timer.c;
	In this application LVGLMsgBox::dispMsg2 actually manages the scan & display refresh */
	/*in this project this will fire 10 time a second, mainly to keep the display 'touch' working*/
	ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LV_TICK_PERIOD_MS * 1000)); // here time is in micro seconds
																						  // Tick Interface for LVGL using esp_timer to generate 2ms periodic event

	// static lv_indev_drv_t indev_drv; // Input device driver (Touch)
	lv_indev_drv_init(&indev_drv);
	/*Set project/application specific parameters*/
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.disp = disp;
	indev_drv.read_cb = lvgl_touch_cb;
	indev_drv.user_data = tp;

	lv_indev_drv_register(&indev_drv);
	// lv_timer_del(indev_drv.read_timer);
	indev_drv.read_timer->paused = true;
	ESP_LOGI(TAG, "indev_drv.read_timer %p  read_cb: %p", indev_drv.read_timer, indev_drv.read_cb);
	ESP_LOGI(TAG, "Bld LVGL GUI");
	Bld_LVGL_GUI();
};

/*Originally written to be called after returning from setup/settings screen*/
void LVGLMsgBox::ReBldDsplay(void)
{
	if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
	{
		/* We were able to obtain the semaphore and can now access the
		shared resource. */
		mutexFLG = true;
		/* We have finished accessing the shared resource.  Release the semaphore. */
		xSemaphoreGive(mutex);
		mutexFLG = false;
	}
	ReStrSettings();
	// TODO this may need more thought
	RingbufPntr2 = 0;

	/*reset pointers & counters*/
	cursorX = cursorY = curRow = cnt = offset = CursorPntr = 0;
	RingbufPntr1--;
	if (RingbufPntr1 < 0)
		RingbufPntr1 = RingBufSz - 1;
	Bump = true;
	bool curflg = SOTFlg;
	setSOTFlg(curflg); // one way to refresh the SOT/STR status box without having to reinvent the wheel
	while (RingbufPntr1 != RingbufPntr2)
	{
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	CursorPntr = StrdPntr;
}
/* normally called by the Keyboard parser process
 * the main feature of this entry point is to set
 * the cursor position pointer
 */
void LVGLMsgBox::KBentry(char Ascii, int paramptr)
{
	if (setupFlg)
	{
		// printf("LVGLMsgBox::KBentry(char Ascii)\n");
		SettingsKBrdNtry(Ascii, paramptr);
		return;
	}
	char buf[2];
	uint16_t color = TFT_WHITE;
	buf[0] = Ascii;
	buf[1] = 0;

	if (StrTxtFlg)
	{
		pStrdTxt[txtpos] = Ascii;
		pStrdTxt[txtpos + 1] = 0;
		txtpos++;
		if (txtpos >= 19)
			txtpos = 18;
		else
			color = TFT_YELLOW;
	}
	/*Removed for lvgl/waveshare cursor management*/
	// if (CursorPntr == 0)
	// 	CursorPntr = cnt;
	// printf("Ascii:'%c'; AsciiVal %d\n", Ascii, (int)Ascii);
	traceFlg = true;
	dispKeyBrdTxt(buf, color);
};
void LVGLMsgBox::SettingsKBrdNtry(char Ntry, int paramptr)
{
	bool tryagn = true;
	int trycnt = 0;
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 11;
			tryagn = false;
			report = false;
			bypassMutex = true;
			// Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::SettingsKBrdNtry(char Ntry) timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	/*select focused field & update entry*/
	if (!tryagn)
	{
		switch (paramptr)
		{
		case 0:
			lvgl_Update_setting(MyCallta, Ntry);
			break;
		case 1:
			lvgl_Update_setting(WPMta, Ntry);
			break;
		case 2: // user updating the debug checkbox status
			if (Ntry == 0x95 || Ntry == 0x96 || Ntry == 0x0d)
			{
				// lv_arc_set_value(obj, lv_arc_get_value(obj) + 1);
				chkbxlocalevnt = false;
				lv_res_t res = lv_event_send(Dbg_ChkBx, LV_EVENT_CLICKED, NULL);
				if (res != LV_RES_OK)
					printf("Dbg_ChkBx event processed\n");
			}
			else
			{
				printf("KeyBrd Ntry: %02x\n", Ntry);
			}
			// lvgl_Update_setting(Dbg_ChkBx, Ntry);
			break;
		case 3: // user updating the ScrnMode checkbox status
			if (Ntry == 0x95 || Ntry == 0x96 || Ntry == 0x0d)
			{
				// NiteMode = false;
				lv_res_t res = lv_event_send(ScrnMode_ChkBx, LV_EVENT_CLICKED, NULL);
				if (res != LV_RES_OK)
					printf("ScrnMode_ChkBx event processed\n");
			}
			else
			{
				printf("KeyBrd Ntry: %02x\n", Ntry);
			}
			// lvgl_Update_setting(Dbg_ChkBx, Ntry);
			break;
		case 4:
			lvgl_Update_setting(MemF2ta, Ntry);
			break;
		case 5:
			lvgl_Update_setting(MemF3ta, Ntry);
			break;
		case 6:
			lvgl_Update_setting(MemF4ta, Ntry);
			break;
		case 7:
			lvgl_Update_setting(MemF5ta, Ntry);
			break;
		case 8: // save button has focus
			if (Ntry == 0x0d)
			{
				lv_res_t res = lv_event_send(save_btn, LV_EVENT_CLICKED, NULL);
				if (res != LV_RES_OK)
					printf("save_btn event processed\n");
			}
			else
			{
				printf("KeyBrd Ntry: %02x\n", Ntry);
			}
			break;
		case 9:				  // exit button has focus
			if (Ntry == 0x0d) // enter key was hit
			{
				lv_res_t res = lv_event_send(exit_btn, LV_EVENT_CLICKED, NULL);
				if (res != LV_RES_OK)
					printf("exit_btn event processed\n");
			}
			else // for info only, print all other key entries
			{
				printf("KeyBrd Ntry: %02x\n", Ntry);
			}
			break;
		}
		xSemaphoreGive(lvgl_semaphore);
	}
};
/* Typically called when exiting a settins session
passes back to the Defaults collection the current values of the params shown
on the settings screen at exit */
void LVGLMsgBox::syncDfltwSettings(void)
{
	bool tryagn = true;
	int trycnt = 0;
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 15;
			tryagn = false;
			report = false;
			bypassMutex = true;
			// Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::syncDfltwSettings(void) timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	/*if false, ready to pass current settings entries back to their default counterparts */
	if (!tryagn)
	{
		Sync_Dflt_Settings();
		// const char *p = lv_textarea_get_text(MyCallta);
		// sprintf(DFault.MyCall, "%s", p);
		// p = lv_textarea_get_text(MemF2ta);
		// sprintf(DFault.MemF2, "%s", p);
		// p = lv_textarea_get_text(MemF3ta);
		// sprintf(DFault.MemF3, "%s", p);
		// p = lv_textarea_get_text(MemF4ta);
		// sprintf(DFault.MemF4, "%s", p);
		// p = lv_textarea_get_text(MemF5ta);
		// sprintf(DFault.MemF5, "%s", p);
		// p = lv_textarea_get_text(WPMta);
		// /*recover WPM value (an integer from a character array*/
		// int intVal = 0;
		// int len = 2;
		// for (int i = 0; i < len; i++)
		// {
		// 	if (p[i] > 0)
		// 		intVal = (i * 10 * intVal) + (p[i] - '0');
		// }
		// DFault.WPM = intVal;
		// lv_state_t state = lv_obj_get_state(Dbg_ChkBx);
		// if(state == LV_STATE_CHECKED) DFault.DeBug = 1;
		// else  DFault.DeBug = 0;
		xSemaphoreGive(lvgl_semaphore);
	}
	return;
};
/*this is the normal input point for text that's to appear in the decoded text area of the lvgl display*/
void LVGLMsgBox::dispDeCdrTxt(char Msgbuf[50], uint16_t Color)
{
	int msgpntr = 0;

/* Add the contents of the Msgbuf to ringbuffer */
#ifdef AutoCorrect
	printf("[%s]", Msgbuf);
#endif
	while (Msgbuf[msgpntr] != 0)
	{
		if (RingbufPntr1 < RingBufSz - 1)
			DeCdrRingbufChar[RingbufPntr1 + 1] = 0;
		else
			DeCdrRingbufChar[0] = 0;
		DeCdrRingbufChar[RingbufPntr1] = Msgbuf[msgpntr];
/*Added for lvgl*/
#ifdef AutoCorrect
		if (DeCdrRingbufChar[RingbufPntr1] == ' ')
			printf("%c; RingbufPntr1 %d\n", DeCdrRingbufChar[RingbufPntr1], RingbufPntr1);
#endif
		// printf("%c; RingbufPntr1 %d\n",Msgbuf[msgpntr], RingbufPntr1);
		/*Not needed for vlgl display*/
		// DeCdrRingbufClr[RingbufPntr1] = Color;
		RingbufPntr1++;
		if (RingbufPntr1 == RingBufSz)
			RingbufPntr1 = 0;
		msgpntr++;
	}
};
/*New for lvgl based screen
Pathway to display text in the keyboard (CW send space)
Note: for lvgl, color parameter is ignored (legacy parameter from TFTSpi Displays)*/
void LVGLMsgBox::dispKeyBrdTxt(char Msgbuf[50], uint16_t Color)
{
	int msgpntr = 0;
	int ChrCnt = strlen(Msgbuf);
	// printf("%d\n", ChrCnt);
	if (ChrCnt == 2)
	{
		if (Msgbuf[0] == 0x08 && Msgbuf[1] == 0x20)
			Msgbuf[1] = 0;
	}
	/* Add the contents of the Msgbuf to ringbuffer */

	while (Msgbuf[msgpntr] != 0)
	{
		// printf("%d. bufChar '%c'; Val %d\n", msgpntr,  Msgbuf[msgpntr], (int)Msgbuf[msgpntr]);
		if (KeyBrdPntr1 < RingBufSz - 1)
			KeyBrdRingbufChar[KeyBrdPntr1 + 1] = 0;
		else
			KeyBrdRingbufChar[0] = 0;
		KeyBrdRingbufChar[KeyBrdPntr1] = Msgbuf[msgpntr];
		// KeyBrdRingbufClr[KeyBrdPntr1] = Color;
		KeyBrdPntr1++;
		if (KeyBrdPntr1 == RingBufSz)
			KeyBrdPntr1 = 0;
		msgpntr++;
	}
};

///////////////////////////////////////////////////////////////////////////////////
/* timer interrupt driven method. drives the Ringbuffer pointer to ensure that
 * character(s) sent to TFT Display actually get printed (when time permits)X WPM & RX Tone Strenght/color
 */
void LVGLMsgBox::dispMsg2(int RxSig)
{
	bool NuTxt = false;
	bool NuToneVal = false;
	bool JstDoIt = true;
	char bias[24];
	char freq[24];
	if (ScopeActive)
	{ // ready to refresh "scope" view
		if (SmplSetRdy)
		{
			sprintf(bias, "ADC Bias: %d", bias_int);
			lv_label_set_text(ui_Label1, bias);
			sprintf(freq, "Frequency: %d Hz", freq_int);
			lv_label_set_text(ui_Label2, freq);
			lv_chart_refresh(ui_Chart1);
			SmplSetRdy = false;
		}
	}
	if (OLDRxSig != RxSig)
	{
		NuToneVal = true;
		OLDRxSig = RxSig;
	}

	if (ToneFlg)
	{
		ToneFlg = false;
		int Wdth = 15;
		int Hght = 15;
		// ptft->fillRect(StusBxX - 15, StusBxY + 10, Wdth, Hght, ToneColor);
	}

	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 6;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::dispMsg2 timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
			// vTaskDelay(pdMS_TO_TICKS(5));
		}
	}
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake; Sucess\n");
	/*Test to see if touch sensing can be managed under the normal screen refresh task*/
	if (!indev_drv.read_timer->paused)
		printf("indev_drv.read_timer No longer 'PAUSED'\n");
	// if (xSemaphoreTake(Touch_mutex, pdMS_TO_TICKS(5)) == pdTRUE) // Most of the time this will be true. But NOT while a BLE connect operation is happening
	if (Touch_mutex)
	{
		lv_indev_read_timer_cb(indev_drv.read_timer);
		// xSemaphoreGive(Touch_mutex);
	}

	if (NuToneVal & !setupFlg)
	{

		// printf("RxSig %d\n", RxSig);
		lvgl_UpdateToneSig(RxSig);
	}
	if (SpdFlg & !setupFlg)
	{
		SpdFlg = false;
		// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  lvgl_update_RxStats; Start\n");
		lvgl_update_RxStats(SpdBuf);
		// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  lvgl_update_RxStats; Complete\n");
	}
	if (OldBat_Lvl != Get_KyBrdBat_level())
	{
		OldBat_Lvl = Get_KyBrdBat_level();
		lvgl_update_Bat_Lvl(OldBat_Lvl);
	}
	if (KBrdWPMFlg & !setupFlg)
	{
		lvgl_update_KyBrdWPM(WPMbuf);
	}
	if (RingbufPntr2 != RingbufPntr1)
	{ //we have a decoded character to print, So, from the accumulated keyup events(ratios), find the minimum S/N used in the construction of this character
		float Nu_SN = 0.0;
		float Min_SN = 10000; //equivalent to S/N = 80db
		while(xQueueReceive(ToneSN_que, (void *)&Nu_SN, pdMS_TO_TICKS(3)) == pdTRUE)
		{
			if(Nu_SN < Min_SN) Min_SN = Nu_SN;
		}
		if(Min_SN != 10000) lvgl_update_SN(20 * log10(Min_SN));//calculate S/N in dBs, & pass the found value to lvgl display 
	}
	if ((OldStrTxtFlg != StrTxtFlg) & !setupFlg)
	{
		OldStrTxtFlg = StrTxtFlg;
		lvgl_UpdateF1Mem(OldStrTxtFlg);
	}
	if ((OldSOTFlg != SOTFlg) & !setupFlg)
	{
		OldSOTFlg = SOTFlg;
		if (SOTFlg)
			KBrdCursorPntr = SOToffCursorPntr;
		else
			SOToffCursorPntr = KBrdCursorPntr; // save keyboard cursor pointer when SOT was turned off. will be used later to set the cursor back
		lvgl_UpdateSOT(OldSOTFlg);
	}
	if ((UpdtKyBrdCrsr) & !setupFlg)
	{
		lvgl_update_KyBrdCrsr(BGHilite);
		UpdtKyBrdCrsr = false;
	}

	// if (xSemaphoreTake(DsplUpDt_AdvPrsrTsk_mutx, pdMS_TO_TICKS(1)) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
	if (!BlkDcdUpDts)
	{
		while (RingbufPntr2 != RingbufPntr1)
		{
			NuTxt = true;
			char curChar = DeCdrRingbufChar[RingbufPntr2];
			Pgbuf[0] = curChar;
			// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  update_text2(); Start\n");
			//  post  this character at the end of text shown in the decoded text space on the waveshare display
			Update_textarea(DecdTxtArea, curChar);
			// printf("Decoded textarea char %c\n", curChar);
			//  sprintf(LogBuf,"LVGLMsgBox::dispMsg2  update_text2(); Complete\n");
			if (curChar == 0x8) // test for "Backspace", AKA delete ASCII symbol
			{
				// sprintf(LogBuf,"Msg2 Delete Initiated/n");
				// if (!this->Delete(true, 1))
				// 	printf("Msg2 Delete FAILED!!!/n");
				// sprintf(LogBuf,"Msg2 Delete Completed/n");
			}
			else if (curChar == 10)
			{	// test for "line feed" character
				/*LVGL version shouldn't  a /DisplCrLf() function*/
				// DisplCrLf();
				//  printf("cnt:%d; \n", cnt);
			}
			else // at this point, whatever is left shoud be regular text
			{
				//
			}

			RingbufPntr2++;
			if (RingbufPntr2 == RingBufSz)
				RingbufPntr2 = 0;
			// NxtRngBfrNdx = 	RingbufPntr2;
		} // End while (RingbufPntr2 != RingbufPntr1)
		//	xSemaphoreGive(DsplUpDt_AdvPrsrTsk_mutx);
	}
	/*now update any pending keyboard text entries*/
	while (KeyBrdRingbufPntr2 != KeyBrdPntr1)
	{
		NuTxt = true;
		/*test if this next entry is going to generate a scroll page event
		& are we in the middle of sending a letter; if true, skip this update.
		This was done to fix an issue with the esp32 & not being able to give the dotclock ISR
		priority over the display ISR */
		if (CWActv && (((cnt + 1) - offset) * fontW >= displayW) && (curRow + 1 == ROW))
		{
			// char buf[50];
			// sprintf(buf, "currow: %d; row: %d", curRow, row);
			// dispStat(buf, TFT_GREENYELLOW);//update status line
			// setSOTFlg(false);//changes status square to yellow
			break;
		}
		// ptft->setCursor(cursorX, cursorY);
		char curChar = KeyBrdRingbufChar[KeyBrdRingbufPntr2];
		// printf("bufChar '%c'; Val %d\n", curChar, (int)curChar);
		//  sprintf(LogBuf,"LVGLMsgBox::dispMsg2  update_text2(); Start\n");
		//   post  this character at the end of text shown in the decoded text space on the waveshare display
		Update_textarea(SendTxtArea, curChar);
		// printf("%c", curChar);
		// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  update_text2(); Complete\n");
		// if (curChar == 0x8) // test for "Backspace", AKA delete ASCII symbol
		// {
		// 	// sprintf(LogBuf,"Msg2 Delete Initiated/n");
		// 	if (!this->Delete(false, 1))
		// 		printf("Msg2 Delete FAILED!!!/n");
		// 	// sprintf(LogBuf,"Msg2 Delete Completed/n");
		// }
		// // else if (curChar == 10)
		// // {	// test for "line feed" character
		// // 	/*LVGL version shouldn't  a /DisplCrLf() function*/
		// // 	// DisplCrLf();
		// // 	//  printf("cnt:%d; \n", cnt);
		// // }
		// else // at this point, whatever is left shoud be regular text
		// {

		// 	cnt++;
		// }

		KeyBrdRingbufPntr2++;
		if (KeyBrdRingbufPntr2 == RingBufSz)
			KeyBrdRingbufPntr2 = 0;
	} // End while (KeyBrdRingbufPntr2 != KeyBrdPntr1)
	////////////////////////////////////////////////////////////////////////////////////////
	// if ((MutexLckId == 6) && (NuTxt || NuToneVal || SpdFlg))
	// {
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  _lv_disp_refr_timer(); Start\n");
	if (xSemaphoreTake(ADCread_disp_refr_timer_mutx, pdMS_TO_TICKS(15)) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
	{
		// if(setupFlg) printf("_lv_disp_refr_timer(NULL)\n");
		_lv_disp_refr_timer(NULL);
		/* We have finished accessing the shared resource.  Release the
			 semaphore. */
		xSemaphoreGive(ADCread_disp_refr_timer_mutx);
	}
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  _lv_disp_refr_timer(); Complete\n");
	// }
	// else
	// {
	/* printf("MutexLckId %d;  NuTxt %d;  NuToneVal %d;  SpdFlg %d; RxSig %d\n" \
	 , MutexLckId, (int)NuTxt, (int)NuToneVal, (int)SpdFlg, RxSig);
	*/
	// }
	TchEvnt = false;
	// SpdFlg = false;
	KBrdWPMFlg = false;

	int lpcnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  lv_timer_handler(); Start\n");
	/*This replaces the need for the MsgBx_lvgl_port_task */
	uint32_t task_delay_ms = lv_timer_handler();
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  lv_timer_handler(); Complete\n");
	xSemaphoreGive(lvgl_semaphore);
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreGive(); Complete\n");
	Msg2Actv = false;
	MutexLckId = 0;
	bypassMutex = false;
	vTaskDelay(pdMS_TO_TICKS(2)); // was 20 changed to 2
								  // sprintf(LogBuf,"LVGLMsgBox::dispMsg2 vTaskDelay(); Complete\n");

	if (Bump) // Bump gets set only when coming back from settings screen
	{
		Bump = false;
		RingbufPntr1++;
		if (RingbufPntr1 == RingBufSz)
			RingbufPntr1 = 0;
		// dispStat(LastStatus, LastStatClr);
		// sprintf(LogBuf,"LVGLMsgBox::dispMsg2 BUMP true, lvgl_update_RxStats(); Start\n");
		lvgl_update_RxStats(LastStatus);
		// sprintf(LogBuf,"LVGLMsgBox::dispMsg2 BUMP true, lvgl_update_RxStats(); Complete\n");
	}
	int blks = BlkStateCntr;
	// if(BlkStateCntr > 0) sprintf(LogBuf,"LVGLMsgBox::while (BlkStateCntr > 0); Start\n");
	while (BlkStateCntr > 0)
	{
		IntrCrsr(BlkStateVal[blks - BlkStateCntr]);
		BlkStateCntr--;
	}
	// printf("[APP] Free memory: %d bytes\n", (int)esp_get_free_heap_size());
	//  sprintf(LogBuf,"LVGLMsgBox::dispMsg2(); Complete\n");
};
//////////////////////////////////////////////////////////////////////
/*Normally called via dispMsg2()
 *BUT the AdvParserTask (main.cpp) can also call this direct as part of the overwirte process
 * Note: On lvgl displays the ''DeCdTxtFlg' controls which text area the delete applies to
 */
bool LVGLMsgBox::Delete(bool DeCdTxtFlg, int ChrCnt)
{
	bool tryagn = true;
	int trycnt = 0;
	bool DeltCmplt = false;
	int lpcnt = 0;

	while (tryagn && !Msg2Actv)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 7;
			tryagn = false;
			bypassMutex = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				printf("LVGLMsgBox::Delete timed out; MutexLckId = %d\n", MutexLckId);
			}
			vTaskDelay(pdMS_TO_TICKS(20));
		}
	}

	while (ChrCnt != 0)
	{ // delete display of ever how many characters were printed in the last decodeval (may be more than one letter generated)
		// first,buzz thru the pgbuf array until we find the the last character (delete the last character in the pgbuf)
		/*not needed for lvgl displays*/
		// int TmpPntr = 0;
		//  while (Pgbuf[TmpPntr] != 0)
		//  	TmpPntr++;
		//  if (TmpPntr > 0)
		//  	Pgbuf[TmpPntr - 1] = 0; // delete last character in the array by replacing it with a "0"
		cnt--;
		// int xoffset = cnt;
		// // use xoffset to locate the character position (on the display's x axis)
		// curRow = 0;
		// while (xoffset >= CPL)
		// {
		// 	xoffset -= CPL;
		// 	curRow++;
		// }
		// cursorX = xoffset * (fontW);
		// cursorY = curRow * (fontH);
		// if (xoffset == (CPL - 1))
		// 	offset = offset - CPL;
		// printf("DELETE\n");
		if (DeCdTxtFlg)
			Update_textarea(DecdTxtArea, (char)0x08); // Ascii value for 'Back-Space'
		else
		{
			Update_textarea(SendTxtArea, (char)0x08);
		}
		--ChrCnt;
		/*Now if we are also storing characters (via "F1" command) need to remove last entry from that buffer too */
		if (StrTxtFlg && (txtpos > 0))
		{
			txtpos--;
			pStrdTxt[txtpos] = 0;
		}
	} // end 'while()' loop
	if (!Msg2Actv)
	{
		if (MutexLckId == 7) // Make sure this method really 'owns' the change (delete) before attempting to updat the display
		{
			_lv_disp_refr_timer(NULL); // start the lvgl display update process
			xSemaphoreGive(lvgl_semaphore);
		}
		MutexLckId = 0;
		bypassMutex = false;
		vTaskDelay(pdMS_TO_TICKS(20));
	}
	DeltCmplt = true;

	return DeltCmplt;
};
//////////////////////////////////////////////////////////////////////
void LVGLMsgBox::DisplCrLf(void)
{
	/* Pad the remainder of the line with space */
	/*Not needed for lvgl display*/
	return;
	// if ((cnt - offset) == 0)
	// 	return;
	// int curOS = offset;
	// while ((cnt - curOS) * fontW <= displayW)
	// {
	// 	// ptft->print(" "); // ASCII "Space"
	// 	/* If needed add this character to the Pgbuf */
	// 	if (curRow > 0)
	// 	{
	// 		Pgbuf[cnt - CPL] = 32;
	// 		Pgbuf[cnt - (CPL - 1)] = 0;
	// 		PgbufColor[cnt - CPL] = TFT_BLACK;
	// 	}
	// 	cnt++;
	// 	if ((cnt - offset) * fontW >= displayW)
	// 	{
	// 		curRow++;
	// 		cursorX = 0;
	// 		cursorY = curRow * (fontH);
	// 		offset = cnt;
	// 		// ptft->setCursor(cursorX, cursorY);
	// 		if (curRow + 1 > ROW)
	// 		{
	scrollpg();
	// 			return;
	// 		}
	// 	}
	// 	else
	// 	{
	// 		cursorX = (cnt - offset) * fontW;
	// 	}
	// 	if (((curOS + CPL) - cnt) == 0)
	// 		break;
	// }
};
//////////////////////////////////////////////////////////////////////
void LVGLMsgBox::scrollpg()
{
	/*not needed for lvgl display*/
	return;
	// BlkState = true;
	// BlkStateCntr = 0;
	// cursorX = 0;
	// cnt = 0;
	// cursorY = 0;
	// curRow = 0;
	// offset = 0;
	// PgScrld = true;
	// bool PrintFlg = true;
	// int curptr = RingbufPntr1;
	// if (RingbufPntr2 > RingbufPntr1)
	// 	curptr = RingbufPntr1 + RingBufSz;
	// if ((curptr - RingbufPntr2) > CPL - 1)
	// 	PrintFlg = false;
	// //  enableDisplay(); //this is for touch screen support
	// if (PrintFlg)
	// {
	// 	// ptft->fillRect(cursorX, cursorY, displayW, ROW * (fontH), TFT_BLACK); // erase current page of text
	// 	// ptft->setCursor(cursorX, cursorY);
	// }
	// while (Pgbuf[cnt] != 0 && curRow + 1 < ROW)
	// { // print current page buffer and move current text up one line
	// 	if (PrintFlg)
	// 	{
	// 		// ptft->setTextColor(PgbufColor[cnt]);
	// 		// ptft->print(Pgbuf[cnt]);
	// 	}
	// 	Pgbuf[cnt] = Pgbuf[cnt + CPL]; // shift existing text character forward by one line
	// 	PgbufColor[cnt] = PgbufColor[cnt + CPL];
	// 	cnt++;
	// 	if (((cnt)-offset) * fontW >= displayW)
	// 	{
	// 		curRow++;
	// 		offset = cnt;
	// 		cursorX = 0;
	// 		cursorY = curRow * (fontH);
	// 		if (PrintFlg)
	// 		{
	// 			// ptft->setCursor(cursorX, cursorY);
	// 		}
	// 	}
	// 	else
	// 	{
	// 		cursorX = (cnt - offset) * (fontW);
	// 		// ptft->setCursor(cursorX, cursorY);
	// 	}

	// } // end While Loop

	// /* And if needed, move the CursorPntr up one line*/
	// if (CursorPntr - CPL > 0)
	// 	CursorPntr = CursorPntr - CPL;

	// BlkState = false;
};
/*Manage highlighting the CW character currently being sent*/
/*this routine gets executed everytime the dotclockgenerates an interrupt*/
/*The stat value that gets passed to it, controls how the character & its background is colorized*/
void LVGLMsgBox::IntrCrsr(int state)
{
	/* state codes
		0 No activity
		1 Processing
		2 Letter Complete
		3 Start Space
		4 Start Character
		5 End Space & Start Next Character
	 */
	/*needed just for debugging cursorptr issue*/
	// char temp[50];
	// if (oldstate !=0 && state == 0) printf("**************\n");
	// oldstate = state;
	// if(state !=0){
	// sprintf(temp,"STATE: %d; CsrPtr: %d; cntr: %d\n", state, CursorPntr, cnt );
	// printf(temp);
	// }
	/*Don't need this with the latest queue method for managing the timer interrurpts*/
	// if (BlkState)
	// {
	// 	BlkStateVal[BlkStateCntr] = state; // save this state, so it can be processed after the scroll page routine completes
	// 	BlkStateCntr++;
	// 	return;
	// }
	switch (state)
	{
	case 0: // No activity
		// if (SOTFlg)
		/*Added for lvgl/waveshare keybrd cursor management*/
		KBrdCursorPntr = CurKyBrdCharCnt + 1;
		break; // do nothing
	case 1:	   // Processing
		/*Removed for lvgl/waveshare keybrd cursor management*/
		// if (CursorPntr > cnt)
		// 	CursorPntr = cnt;
		break;		 // do nothing
	case 2:			 // Letter Complete
		RestoreBG(); // restore normal Background
		break;
	case 3: // Start Space

		HiLiteBG(); // Highlight Background
		break;
	case 4: // Start Letter
		/*Removed for lvgl/waveshare keybrd cursor management*/
		// if (CursorPntr >= cnt)
		// 	CursorPntr = cnt - 1;

		HiLiteBG(); // Highlight Background
		break;
	case 5: // restore Backgound & Highlight Next Character or Space
		/*Removed for lvgl/waveshare keybrd cursor management*/
		// if (CursorPntr + 1 >= cnt)
		// 	CursorPntr = cnt - 1; // test, just in case something got screwed up
		RestoreBG(); // restore normal Background
		HiLiteBG();	 // Highlight Background
		break;
	case 6:			 // Abort restore Backgound & Highlight Next Character or Space
		RestoreBG(); // restore normal Background
		// CursorPntr = cnt;
		/*Added for lvgl/waveshare keybrd cursor management*/
		KBrdCursorPntr = CurKyBrdCharCnt + 1;
		break;
	}
	return;
};
void LVGLMsgBox::HiLiteBG(void)
{

	PosHiLiteCusr();
	// ptft->setTextColor(PgbufColor[CursorPntr - CPL], TFT_RED);
	// ptft->print(Pgbuf[CursorPntr - CPL]);
	// ptft->setTextColor(TFT_WHITE, TFT_BLACK); // restore normal color scheme
	//  dont need to put Cursor back, other calls to print will do that
	BGHilite = true;
	UpdtKyBrdCrsr = true;
};
void LVGLMsgBox::RestoreBG(void)
{
	// PosHiLiteCusr(); //in the lvgl world, this does nothing
	//  ptft->setTextColor(PgbufColor[CursorPntr - CPL], TFT_BLACK);
	//  ptft->print(Pgbuf[CursorPntr - CPL]);
	/*following was added to support lvgl/waveshare cursor management*/
	KBrdCursorPntr++;
	if (KBrdCursorPntr > CurKyBrdCharCnt + 1)
		KBrdCursorPntr = CurKyBrdCharCnt + 1;
	BGHilite = false;
	UpdtKyBrdCrsr = true;
	// if(CursorPntr == cnt) CursorPntr = 0;
};
void LVGLMsgBox::PosHiLiteCusr(void) {
	/* figure out where the HighLight Y cursor needs to be set */
	/*lvgl/waveshare display doesn't use this method*/

	// int HLY = 0;
	// int HLX = 0;
	// int tmppntr = 0;
	// int tmprow = 1;
	// while (tmppntr + CPL <= CursorPntr)
	// {
	// 	tmprow++;
	// 	tmppntr += CPL;
	// }
	// HLY = (tmprow - 1) * (fontH);
	// HLX = (CursorPntr - ((tmprow - 1) * CPL)) * fontW;
	// ptft->setCursor(HLX, HLY);
};

void LVGLMsgBox::dispStat(char Msgbuf[50], uint16_t Color)
{
	lvgl_update_RxStats(Msgbuf);
	// int i;
	// LastStatClr = Color;
	// int LdSpaceCnt = 0;
	// //ptft->setCursor(StatusX, StatusY);//old way
	// //ptft->setCursor(StusBxX+30, StatusY);//new way, based on where the sot status box ends
	// //ptft->setTextColor(Color, TFT_BLACK);
	// /* 1st figure out how many leading spaces to print to "center"
	//  * the Status message
	//  */

	// //int availspace = int(((ScrnWdth - 60) - StatusX) / fontW);
	// int availspace = int(((ScrnWdth -(FONTW*6)) - (StusBxX+30)) / fontW);
	// i = 0;
	// while (Msgbuf[i] != 0)
	// 	i++;
	// if (i < availspace)
	// {
	// 	LdSpaceCnt = int((availspace - i) / 2);
	// 	if (LdSpaceCnt > 0)
	// 	{
	// 		for (i = 0; i < LdSpaceCnt; i++)
	// 		{
	// 			//ptft->print(" ");
	// 		}
	// 	}
	// }
	// for (i = 0; i < availspace; i++)
	// {
	// 	LastStatus[i] = Msgbuf[i]; // copy current status to last status buffer; In case needed later to rebuild main screen
	// 	if (Msgbuf[i] == 0)
	// 		break;
	// 	//ptft->print(Msgbuf[i]);
	// }
	// /*finish out remainder of line with blank space */
	// //	ptft->setTextColor(TFT_BLACK);
	// if (LdSpaceCnt > 0)
	// {
	// 	for (i = 0; i < LdSpaceCnt; i++)
	// 	{
	// 		//ptft->print(" ");
	// 	}
	// }
};

void LVGLMsgBox::showSpeed(char Msgbuf[50], uint16_t Color)
{
	SpdClr = Color;
	SpdFlg = true;
	sprintf(SpdBuf, "%s", Msgbuf);
	// printf(SpdBuf);
	// printf("\n\r");
};

void LVGLMsgBox::ShwKeybrdWPM(int wpm)
{
	KBrdWPMFlg = true;
	sprintf(WPMbuf, "%d WPM", wpm);
};

// void LVGLMsgBox::ShwDcodeSN(float sn)
// {
// 	if(!SNFlg)
// 	{
// 		//Nu_SN = sn;
// 		SNFlg = true;
// 	}
// 	//printf("S2N: %4.1f\n", sn);
// };
void LVGLMsgBox::setSOTFlg(bool flg)
{
	SOTFlg = flg;
	// /*Now Use the max3421 interrupt box to show "Send On Text State"(SOT) mode */
	// uint16_t color;
	// int Xpos = StusBxX;
	// int Ypos = StusBxY;
	// int Wdth = 15;
	// int Hght = 15;
	// if (SOTFlg && !StrTxtFlg)
	// 	color = TFT_GREEN;
	// else if (SOTFlg && StrTxtFlg)
	// 	color = TFT_WHITE;
	// else
	// 	color = TFT_YELLOW;
	// ptft->fillRect(StusBxX+10, StusBxY + 10, Wdth, Hght, color);
};
/*Under Waveshare/vlgl the main purpose of this function
is to erase/clear F1 memory when F1 is activated */
void LVGLMsgBox::setStrTxtFlg(bool flg)
{
	StrTxtFlg = flg;
	// /*Now Use the max3421 interrupt box to show "Store Text" mode */
	// uint16_t color;
	// int Xpos = StusBxX;
	// int Ypos = StusBxY;
	// int Wdth = 15;
	// int Hght = 15;
	if (!StrTxtFlg && SOTFlg)
	{
		return; // color = TFT_GREEN;
	}
	else if (!StrTxtFlg && !SOTFlg)
	{
		return; // color = TFT_YELLOW;
	}
	else
	{
		// color = TFT_WHITE;
		pStrdTxt[0] = 0;
		txtpos = 0;
	}
	// ptft->fillRect(StusBxX+10, StusBxY + 10, Wdth, Hght, color);
};
void LVGLMsgBox::SaveSettings(void)
{
	StrdPntr = CursorPntr;
	StrdY = cursorY;
	StrdX = cursorX;
	Strdcnt = cnt; // used in scrollpage routine
	StrdcurRow = curRow;
	Strdoffset = offset;
	StrdRBufPntr1 = RingbufPntr1;
	StrdRBufPntr2 = RingbufPntr2;
};
void LVGLMsgBox::ReStrSettings(void)
{
	CursorPntr = StrdPntr;
	cursorY = StrdY;
	cursorX = StrdX;
	cnt = Strdcnt; // used in scrollpage routine
	curRow = StrdcurRow;
	offset = Strdoffset;
	RingbufPntr1 = StrdRBufPntr1;
	RingbufPntr2 = StrdRBufPntr2;
};

void LVGLMsgBox::setCWActv(bool flg)
{
	// if(flg) setSOTFlg(true);// just needed for debugging
	CWActv = flg;
};

bool LVGLMsgBox::getBGHilite(void)
{
	return BGHilite;
};
/*This sets up to activate the tone statusbox the next time  LVGLMsgBox::dispMsg2(void) method fires via DsplTmr_callback routine*/
void LVGLMsgBox::ShwTone(uint16_t color)
{
	// char PrntBuf[20];
	ToneFlg = true;
	ToneColor = color;
	// sprintf(PrntBuf, "ToneColor:%d\n\r", (int)ToneColor);
	// printf(PrntBuf);
	// int Xpos = StusBxX-15;
	// int Ypos = StusBxY;
	// int Wdth = 30;
	// int Hght = 30;
	// if(RingbufPntr1 != RingbufPntr2 ) return;
	// ptft->fillRect(Xpos, Ypos, Wdth, Hght, color);
};
////////////////////////////////////////////////////////
char LVGLMsgBox::GetLastChar(void)
{
	/*Modified for lvgl*/
	return Pgbuf[0];
	// return Pgbuf[cnt - (CPL + 1)];
};
/*Not used inlvgl version */
void LVGLMsgBox::UpdateToneSig(int curval)
{
	return;

	// return;
};
void LVGLMsgBox::BldSettingScreen(void)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 19;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::BldSettingScreen timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	Bld_Settings_scrn();
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
void LVGLMsgBox::BldScopeScreen(void)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 17;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::BldScopeScreen timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	Bld_Scope_scrn();
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
void LVGLMsgBox::BldHelpScreen(void)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 18;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::BldHelpScreen timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	Bld_Help_scrn();
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
void LVGLMsgBox::FlipDayNiteMode(void)
{
	bool tryagn = true;
	int trycnt = 0;
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 21;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::FlipDayNiteMode timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	DFault.NiteMode = !NiteMode; // forcing the DFault value to be the opsite of its stored value to force lvglmsgbx.FlipDayNiteMode to update
	_FlipDayNiteMode();

	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
void LVGLMsgBox::ReStrtMainScrn(void)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 10;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::ReStrtMainScrn timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	Bld_LVGL_GUI();
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
/*Remote entry call to clear/reset decode text area space*/
void LVGLMsgBox::ClrDcdTA(void)
{
	bool tryagn = true;
	int trycnt = 0;
	MutexLckId = 20;
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 14;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::Exit_Settings timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	/*insert line space in decoded text area*/
	Update_textarea(DecdTxtArea, '\n'); // lv_textarea_set_text(DecdTxtArea, "");
	Update_textarea(DecdTxtArea, '\n'); // lv_textarea_set_text(DecdTxtArea, "");
	MutexLckId = 0;
	xSemaphoreGive(lvgl_semaphore);
};

void LVGLMsgBox::HiLite_Seltcd_Setting(int paramptr, int oldparamptr)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 13;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::HiLite_Seltcd_Setting timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	lvgl_DeSelct_Param(oldparamptr);
	lvgl_HiLite_Seltcd_Param(paramptr);
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};
void LVGLMsgBox::Exit_Settings(int paramptr)
{
	bool tryagn = true;
	int trycnt = 0;
	// sprintf(LogBuf,"LVGLMsgBox::dispMsg2  xSemaphoreTake;  Start\n");
	while (tryagn)
	{
		if (pdTRUE == xSemaphoreTake(lvgl_semaphore, 100 / portTICK_PERIOD_MS))
		{
			MutexLckId = 14;
			tryagn = false;
			report = false;
			bypassMutex = true;
			Msg2Actv = true;
		}
		else
		{
			trycnt++;
			if (trycnt > 5)
			{
				trycnt = 5;
				report = true;
				printf("LVGLMsgBox::Exit_Settings timed out; MutexLckId = %d; timerID = %d\n", MutexLckId, timerID);
			}
		}
	}
	lvgl_DeSelct_Param(paramptr);
	// lvgl_HiLite_Seltcd_Param(paramptr);
	xSemaphoreGive(lvgl_semaphore);
	MutexLckId = 0;
};

bool LVGLMsgBox::TestRingBufPtrs(void)
{
	if (RingbufPntr1 == RingbufPntr2)
		return true;
	else
		return false;
};
int LVGLMsgBox::XferRingbuf(char Bfr[50])
{
	/*find end of Buffer*/
	int i = 0;
	// printf("ringbuf content:%c",'"');
	// while(Bfr[i] !=0) i++;
	while (RingbufPntr1 != RingbufPntr2)
	{
		Bfr[i] = DeCdrRingbufChar[RingbufPntr2];
		// printf("%c", DeCdrRingbufChar[RingbufPntr2]);
		i++;
		// Bfr[i] = 0;
		// if(i == 49){// not likely to ever happen
		//	Bfr[i-1] = 0;
		//	return;
		// }
		RingbufPntr2++;
		if (RingbufPntr2 == RingBufSz)
			RingbufPntr2 = 0;
	}
	// printf("%c\n",'"');
	return i;
};
/*This executes when the 'Home' button event fires*/
void Sync_Dflt_Settings(void)
{

	const char *p = lv_textarea_get_text(MyCallta);
	sprintf(DFault.MyCall, "%s", p);
	p = lv_textarea_get_text(MemF2ta);
	sprintf(DFault.MemF2, "%s", p);
	p = lv_textarea_get_text(MemF3ta);
	sprintf(DFault.MemF3, "%s", p);
	p = lv_textarea_get_text(MemF4ta);
	sprintf(DFault.MemF4, "%s", p);
	p = lv_textarea_get_text(MemF5ta);
	sprintf(DFault.MemF5, "%s", p);
	p = lv_textarea_get_text(WPMta);
	/*recover WPM value (an integer from a character array*/
	int intVal = 0;
	int len = 2;
	for (int i = 0; i < len; i++)
	{
		if (p[i] > 0)
			intVal = (i * 10 * intVal) + (p[i] - '0');
	}
	DFault.WPM = intVal;
	lv_state_t state = lv_obj_get_state(Dbg_ChkBx);
	if (state == LV_STATE_CHECKED)
		DFault.DeBug = 1;
	else
		DFault.DeBug = 0;

	return;
};

void SaveUsrVals(void)
{
	/* Unlock the Flash Program Erase controller */
	char buf[30];
	bool GudFlg = true;
	char call[10];
	char mem[80];
	int i;
	uint8_t Rstat;
	bool zeroFlg = false;
	/*Set up to save MyCall*/
	for (i = 0; i < sizeof(call); i++)
	{
		if (zeroFlg)
		{
			call[i] = 0;
		}
		else
		{
			if (DFault.MyCall[i] == 0)
			{
				zeroFlg = true;
				call[i] = 0;
			}
			else
				call[i] = DFault.MyCall[i];
		}
	}
	Rstat = Write_NVS_Str("MyCall", call);
	if (Rstat != 1)
		GudFlg = false;
	/*Set up to save MemF2*/
	zeroFlg = false;
	for (i = 0; i < sizeof(mem); i++)
	{
		if (zeroFlg)
		{
			mem[i] = 0;
		}
		else
		{
			if (DFault.MemF2[i] == 0)
			{
				zeroFlg = true;
				mem[i] = 0;
			}
			else
				mem[i] = DFault.MemF2[i];
		}
	}
	Rstat = Write_NVS_Str("MemF2", mem);
	if (Rstat != 1)
		GudFlg = false;
	/*Set up to save MemF3*/
	zeroFlg = false;
	for (i = 0; i < sizeof(mem); i++)
	{
		if (zeroFlg)
		{
			mem[i] = 0;
		}
		else
		{
			if (DFault.MemF3[i] == 0)
			{
				zeroFlg = true;
				mem[i] = 0;
			}
			else
				mem[i] = DFault.MemF3[i];
		}
	}
	Rstat = Write_NVS_Str("MemF3", mem);
	if (Rstat != 1)
		GudFlg = false;

	/*Set up to save MemF4*/
	zeroFlg = false;
	for (i = 0; i < sizeof(mem); i++)
	{
		if (zeroFlg)
		{
			mem[i] = 0;
		}
		else
		{
			if (DFault.MemF4[i] == 0)
			{
				zeroFlg = true;
				mem[i] = 0;
			}
			else
				mem[i] = DFault.MemF4[i];
		}
	}
	Rstat = Write_NVS_Str("MemF4", mem);
	if (Rstat != 1)
		GudFlg = false;

	/*Set up to save MemF5*/
	zeroFlg = false;
	for (i = 0; i < sizeof(mem); i++)
	{
		if (zeroFlg)
		{
			mem[i] = 0;
		}
		else
		{
			if (DFault.MemF5[i] == 0)
			{
				zeroFlg = true;
				mem[i] = 0;
			}
			else
				mem[i] = DFault.MemF5[i];
		}
	}
	Rstat = Write_NVS_Str("MemF5", mem);
	if (Rstat != 1)
		GudFlg = false;

	/*Save Debug Setting*/
	Rstat = Write_NVS_Val("DeBug", DFault.DeBug);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder NiteMode value */
	DFault.NiteMode = NiteMode;
	Rstat = Write_NVS_Val("NiteMode", (int)DFault.NiteMode);
	if (Rstat != 1)
		GudFlg = false;
	/*Save WPM Setting*/
	Rstat = Write_NVS_Val("WPM", DFault.WPM);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder ModeCnt value */
	Rstat = Write_NVS_Val("ModeCnt", DFault.ModeCnt);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder AutoTune value */
	Rstat = Write_NVS_Val("AutoTune", (int)DFault.AutoTune);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder SlwFlg value */
	Rstat = Write_NVS_Val("SlwFlg", (int)DFault.SlwFlg);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder NoisFlg value */
	Rstat = Write_NVS_Val("NoisFlg", (int)DFault.NoisFlg);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Decoder TARGET_FREQUENCYC value; Note DFault.TRGT_FREQ was last updated in DcodeCW.showSpeed(void)  */
	Rstat = Write_NVS_Val("TRGT_FREQ", DFault.TRGT_FREQ);
	if (Rstat != 1)
		GudFlg = false;
	/* Save current Grtzl_Gain value; Note this is an unsigned factional (1.0 or smaller) float value. But NVS library can't handle floats,
	so 1st convert to unsigned 64bit int    */
	// uint64_t intGainVal = uint64_t(10000000 * DFault.Grtzl_Gain);
	int intGainVal = (int)(10000000 * DFault.Grtzl_Gain);
	Rstat = Write_NVS_Val("Grtzl_Gain", intGainVal);
	if (Rstat != 1)
		GudFlg = false;
	if (GudFlg)
	{
		sprintf(buf, "User Params SAVED");
		if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
		{
			/* We were able to obtain the semaphore and can now access the
			shared resource. */
			mutexFLG = true;
			/*TODO need to come up with some alturnative method signifying that the save was successful*/
			/// ptftmsgbx->dispStat(buf, TFT_GREEN);
			/* We have finished accessing the shared resource.  Release the semaphore. */
			xSemaphoreGive(mutex);
			mutexFLG = false;
		}
	}
	else
	{
		sprintf(buf, "SAVE FAILED");
		if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
		{
			/* We were able to obtain the semaphore and can now access the
			shared resource. */
			/*TODO need to come up with some alturnative method signifying that the save FAILED*/
			mutexFLG = true;
			// ptftmsgbx->dispStat(buf, TFT_RED);
			/* We have finished accessing the shared resource.  Release the semaphore. */
			xSemaphoreGive(mutex);
			mutexFLG = false;
		}
	}
}
