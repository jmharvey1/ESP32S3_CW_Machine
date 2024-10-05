/*
 * main.h
 *
 *  Created on: Feb 19, 2023
 *      Author: jim
 */

#ifndef INC_MAIN_H_
#define INC_MAIN_H_
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_adc/adc_continuous.h"
#include <driver/gptimer.h>
#include "esp_system.h"

struct DF_t
{
	char MyCall[10];
	char MemF2[80];
	char MemF3[80];
	char MemF4[80];
	char MemF5[80];
	int WPM;
	int DeBug;
	int ModeCnt;
	int TRGT_FREQ;
	bool AutoTune;
	float Grtzl_Gain;
	bool SlwFlg;
	bool NoisFlg;
	uint16_t Bias;
};

// typedef struct {
//     timer_group_t timer_group;
//     timer_idx_t timer_idx;
//     int alarm_interval;
//     bool auto_reload;
// } KB_timer_info_t;
/**
 * @brief A sample structure to pass events from the timer ISR to task
 *
 */
// typedef struct {
//     KB_timer_info_t info;
//     uint64_t timer_counter_value;
// } KB_timer_event_t;

//static xQueueHandle s_timer_queue;
extern int DeBug; // factory default setting; 0 => Debug "OFF"; 1 => Debug "ON"
extern TaskHandle_t AdvParserTaskHandle;
extern SemaphoreHandle_t mutex;//JMH Added
extern SemaphoreHandle_t ADCread_disp_refr_timer_mutx;
extern SemaphoreHandle_t DsplUpDt_AdvPrsrTsk_mutx;

extern TaskHandle_t GoertzelTaskHandle;
extern TaskHandle_t CWDecodeTaskHandle;
extern TaskHandle_t BLEscanTask_hndl;
extern QueueHandle_t RxSig_que;
extern DF_t DFault;
extern char StrdTxt[20];
extern char MyCall[10];
extern char MemF2[80];
extern bool clrbuf;
extern bool adcON;
extern bool setupFlg;
extern bool BlkDcdUpDts;
extern COREDUMP_DRAM_ATTR uint8_t global_var;
uint8_t Read_NVS_Str(const char *key, char *value);
template <class T>
uint8_t Read_NVS_Val(const char *key, T &value);
uint8_t Write_NVS_Str(const char *key, char *value);
uint8_t Write_NVS_Val(const char *key,  int value);
extern adc_continuous_handle_t adc_handle;
void BLE_scan_tsk(void *param);

#endif /* INC_MAIN_H_ */