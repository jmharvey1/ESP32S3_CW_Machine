// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_

#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
//#define SpclTst  //uncomment to debug/see how, & when, the AdvParser code is engaged. 
                    //Note: 'SpclTst' also enables/reports DAC/DMA timimg skipps.
//#define DBugLtrBrkTiming
//#define DeBgCrash //uncomment to debug displayupdate lockup/crash
//#define DeBgQueue //uncomment to debug Key up & down event time queues
//#define AutoCorrect //uncomment to debug Post parser delete & replace process 
/*Added here just to get past compile error for references no longer needed for lvgl version of this project*/
// Default color definitions
#define TFT_BLACK       0x0000      /*   0,   0,   0 */
#define TFT_NAVY        0x000F      /*   0,   0, 128 */
#define TFT_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define TFT_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define TFT_MAROON      0x7800      /* 128,   0,   0 */
#define TFT_PURPLE      0x780F      /* 128,   0, 128 */
#define TFT_OLIVE       0x7BE0      /* 128, 128,   0 */
#define TFT_LIGHTGREY   0xD69A      /* 211, 211, 211 */
#define TFT_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define TFT_BLUE        0x001F      /*   0,   0, 255 */
#define TFT_GREEN       0x07E0      /*   0, 255,   0 */
#define TFT_CYAN        0x07FF      /*   0, 255, 255 */
#define TFT_RED         0xF800      /* 255,   0,   0 */
#define TFT_MAGENTA     0xF81F      /* 255,   0, 255 */
#define TFT_YELLOW      0xFFE0      /* 255, 255,   0 */
#define TFT_WHITE       0xFFFF      /* 255, 255, 255 */
#define TFT_ORANGE      0xFDA0      /* 255, 180,   0 */
#define TFT_GREENYELLOW 0xB7E0      /* 180, 255,   0 */
#define TFT_PINK        0xFE19      /* 255, 192, 203 */ //Lighter pink, was 0xFC9F
#define TFT_BROWN       0x9A60      /* 150,  75,   0 */
#define TFT_GOLD        0xFEA0      /* 255, 215,   0 */
#define TFT_SILVER      0xC618      /* 192, 192, 192 */
#define TFT_SKYBLUE     0x867D      /* 135, 206, 235 */
#define TFT_VIOLET      0x915C      /* 180,  46, 226 */


extern char LogBuf[];
extern int curOwner;
extern bool mutexFLG;//JMH Added
extern bool Touch_mutex;
extern bool ForcedWrdBrk;
extern bool noiseflg;
extern bool KillSplashScrn;

#endif /* INC_GLOBALS_H_ */
