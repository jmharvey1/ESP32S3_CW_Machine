/*
 Note 1: parts of the BT-Keyboard code (bt_keyboard.cpp) was based on source code with the same file name &
   Copyright (c) 2020 by Guy Turcotte
  see MIT License. Look at file licenses.txt for details.
  Note 2: for IDF 5.2 keneral code, i2c_master.c has been modified, at function 'i2c_master_transmit_receive()',
  after line 'ESP_RETURN_ON_ERROR(s_i2c_asynchronous_transaction(i2c_dev, i2c_ops, DIM(i2c_ops), xfer_timeout_ms), TAG, "I2C transaction failed");'
  added this entry: 'vTaskDelay(2);'
  This stopped random crashes related to gt911 touch procssing.
  Also added this statement, two places, in the same file, 
  free(i2c_dev->master_bus->anyc_write_buffer[i2c_dev->master_bus->index]);
  at lines 984, & 1023.
*/
/*Note: When creating a new lvgl/WaveShare LCD/ESP32s3 project, to run menuconfig, & set the following settings:
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_120M=y [should align with PSRAM]
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_IDF_EXPERIMENTAL_FEATURES=y and CONFIG_SPIRAM_SPEED_120M=y [should align with PSRAM]
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
CONFIG_COMPILER_OPTIMIZATION_PERF=y
#The following LVGL configuration can improve the frame rate (LVGL v8.3):
#define LV_MEM_CUSTOM 1 or CONFIG_LV_MEM_CUSTOM=y
#define LV_MEMCPY_MEMSET_STD 1 or CONFIG_LV_MEMCPY_MEMSET_STD=y
#define LV_ATTRIBUTE_FAST_MEM IRAM_ATTR or CONFIG_LV_ATTRIBUTE_FAST_MEM=y
*/
/*initially had an issue wid stack overflow with  "A stack overflow in task esp_ble_hidh_ev".
went to: esp-idf/components/esp_hid/src/ble_hidd.c line 974
and found code that looked like this:
esp_event_loop_args_t event_task_args = {
        .queue_size = 5,
        .task_name = "ble_hidd_events",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 2048,
        .task_core_id = tskNO_AFFINITY
    };
 changed .task_stack_size = 5120,   
*/
/*20240729 Fully functional but needs refinement*/
/*20240822 Now support Keyboard battery state, but subject to crashing when running app tries to connect to keyboard a 2nd time*/
/*20240828 Added BIAS auto-correction to ADC sampling */
#define USE_KYBrd 1
#include "sdkconfig.h" //added for timer support
#include "globals.h"
#include "main.h"
#if USE_KYBrd
#include "bt_keyboard.h"
#endif /* USE_KYBrd*/
#include <esp_log.h>
#include <iostream>

#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "soc/soc_caps.h" // included just to have quick access view this file
///////////////////////////////
#include "esp_core_dump.h"
#include "TxtNtryBox.h"
/*helper ADC files*/
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
/*Project specific files*/
#include "Goertzel.h"
#include "DcodeCW.h"
#include "LVGLMsgBox.h"
#include "CWSndEngn.h"
/*BlueTooth*/

#include <string.h> // included to support call to 'memset()'
/*Uncomment to plot Raw ADC Samples*/
//#define POSTADC
/* the following defines were taken from "hal/adc_hal.h" 
 and are here for help in finding the true ADC sample rate based on a given declared sample frequency*/
#define ADC_LL_CLKM_DIV_NUM_DEFAULT       15
#define ADC_LL_CLKM_DIV_B_DEFAULT         1
#define ADC_LL_CLKM_DIV_A_DEFAULT         0
#define ADC_LL_DEFAULT_CONV_LIMIT_EN  6    0
//#define TODAY __DATE__

#define TFT_GREY 0x5AEB // New colour

//TFT_eSPI tft = TFT_eSPI(); // Invoke TFT Display library
//////////////////////////////
/*timer interrupt support*/
#include "esp_timer.h"
esp_timer_handle_t DotClk_hndl;
// esp_timer_handle_t DsplTmr_hndl;
TimerHandle_t DisplayTmr;
/* END timer interrupt support          */

/*Debug & backtrace analysis*/
uint8_t global_var;
/*To make these variables available to other files/section of this App, Moved the following to main.h*/

DF_t DFault;
int DeBug = 1; // Debug factory default setting; 0 => Debug "OFF"; 1 => Debug "ON"
char StrdTxt[20] = {'\0'};
/*Factory Default Settings*/
//char RevDate[12] =  TODAY;//"20240608";
char MyCall[10] = "KW4KD";
char MemF2[80] = "VVV VVV TEST DE KW4KD";
char MemF3[80] = "CQ CQ CQ DE KW4KD KW4KD";
char MemF4[80] = "TU 73 ES GL";
char MemF5[80] = "RST 5NN";
char LogBuf[100];
esp_err_t ret;
char Title[120];
bool setupFlg = false;
//bool EnDsplInt = true;
bool clrbuf = false;
bool PlotFlg = false;
bool UrTurn = true;
bool WokeFlg = false;
bool QuequeFulFlg = false;
bool mutexFLG =false;
bool adcON =false;
bool SkippDotClkISR = false;
uint8_t QueFullstate;
float BiasError = 0;
int Smplcnt = 0;
bool SmplSetDone = false;
volatile int CurHiWtrMrk = 0;
static const uint8_t state_que_len = 100;//50;
static QueueHandle_t state_que;
static const int RxSig_que_len = 15;//50
QueueHandle_t RxSig_que;
//static const uint8_t Sampl_que_len = 6 * Goertzel_SAMPLE_CNT;
//static QueueHandle_t Sampl_que;

SemaphoreHandle_t mutex;
SemaphoreHandle_t DsplUpDt_AdvPrsrTsk_mutx;
SemaphoreHandle_t ADCread_disp_refr_timer_mutx;

/* the following 2 entries are for diagnostic capture of raw DMA ADC data*/
#ifdef POSTADC
int Smpl_buf[6 * Goertzel_SAMPLE_CNT];
int Smpl_Cntr = 0;
#endif

int MutexbsyCnt = 0;
unsigned long SmpIntrl = 0;
unsigned long LstNw = 0;
unsigned long EvntStart = 0;
unsigned long ToneStart = 0; // used to calc true adc samplerate
int cur_smpl_rate = 0;
float Grtzl_Gain = 1.0;
/*the following 4 lines are needed when you want to synthesize an input tone of known freq & Magnitude*/
// float LclToneAngle = 0;
// float synthFreq = 750;
// int dwelCnt = 0;
// float AnglInc = 2*PI*synthFreq/SAMPLING_RATE;

/*Global ADC variables */
// #define Goertzel_SAMPLE_CNT   384 // @750Hz tone input & 48Khz sample rate = 64 samples per cycle & 6 cycle sample duration. i.e. 8ms
// #define EXAMPLE_ADC_CONV_MODE           ADC_CONV_SINGLE_UNIT_1
// #define EXAMPLE_ADC_USE_OUTPUT_TYPE1    1
// #define EXAMPLE_ADC_OUTPUT_TYPE         ADC_DIGI_OUTPUT_FORMAT_TYPE1
static adc_channel_t channel[1] = { ADC_CHANNEL_5};//per waveshare example AD code, this is the channel i.e. GPIO6 pin to use 
adc_continuous_handle_t adc_handle = NULL;
TaskHandle_t GoertzelTaskHandle;
static TaskHandle_t DsplUpDtTaskHandle = NULL;
TaskHandle_t CWDecodeTaskHandle = NULL;
TaskHandle_t AdvParserTaskHandle = NULL;
//static TaskHandle_t UpDtRxSigTaskHandle = NULL;
TaskHandle_t BLEscanTask_hndl = NULL;
TaskHandle_t HKdotclkHndl;

static const char *TAG1 = "ADC_Config";
static const char *TAG2 = "PAIR_EVT";
uint32_t ret_num = 0;
uint8_t result[Goertzel_SAMPLE_CNT * SOC_ADC_DIGI_RESULT_BYTES] = {0};
/*tone freq calc variables */
uint32_t SmplCNt = 0;
uint32_t NoTnCntr = 0;
uint8_t PeriodCntr = 0;
int Oldk = 0; //used in the addsmpl() as part of the auto-tune/freq measurement process 
int DemodFreq = 0;
int DmodFrqOld = 0;
bool TstNegFlg = false;
bool CalGtxlParamFlg = false;

LVGLMsgBox lvglmsgbx(StrdTxt);
//CWSNDENGN CWsndengn(&DotClk_hndl, &tft, &lvglmsgbx);
CWSNDENGN CWsndengn(&DotClk_hndl, &lvglmsgbx);
//lvglmsgbx->LinkCWSendEngn(&CWsndengn);
#if USE_KYBrd
BTKeyboard bt_keyboard(&lvglmsgbx, &DFault);
#endif /* USE_KYBrd*/
/*                           */
//BTKeyboard bt_keyboard(&lvglmsgbx, &DFault);
TxtNtryBox txtntrybox(&DotClk_hndl);
/* coredump crash test code */
/* typedef struct{
  int a;
  char *s;
} data_t;

void show_data(data_t *data){
  if(strlen(data->s) >10){
    printf("String too long");
    return;
  }
  printf("here's your string %s", data->s);
} */

/*ADC callback event; Fires when ADC buffer is full & ready for processing*/
bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Notify GoertzelTaskHandle that the buffer is full.

  /* At this point GoertzelTaskHandle should not be NULL as a ADC conversion completed. */
  EvntStart = pdTICKS_TO_MS(xTaskGetTickCount());
  vTaskNotifyGiveFromISR(GoertzelTaskHandle, &xHigherPriorityTaskWoken); // start Goertzel Task
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  
  /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
  should be performed to ensure the interrupt returns directly to the highest
  priority task.  The macro used for this purpose is dependent on the port in
  use and may be called portEND_SWITCHING_ISR(). */
  // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  return (xHigherPriorityTaskWoken == pdTRUE);
}
/* Setup & initialize DMA ADC process */
static void Cw_Machine_ADC_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
  adc_continuous_handle_t handle = NULL;

  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = (SOC_ADC_DIGI_RESULT_BYTES * Goertzel_SAMPLE_CNT)*8, //2048,
      .conv_frame_size = SOC_ADC_DIGI_RESULT_BYTES * Goertzel_SAMPLE_CNT, 
  };
  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));
  uint32_t freq = (uint32_t)SAMPLING_RATE;// 83333 max freq allowed per 'soc_caps.h' 
  
  adc_continuous_config_t dig_cfg = {
      .sample_freq_hz = freq,
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
  };
  /* uint32_t interval = APB_CLK_FREQ / (ADC_LL_CLKM_DIV_NUM_DEFAULT + ADC_LL_CLKM_DIV_A_DEFAULT / ADC_LL_CLKM_DIV_B_DEFAULT + 1) / 2 / freq;
  char buf[25];
  sprintf(buf, "interval: %d\n", (int)interval); */

  adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
  dig_cfg.pattern_num = channel_num;
  for (int i = 0; i < channel_num; i++)
  {
    uint8_t unit = ADC_UNIT_1;
    uint8_t ch = channel[i] & 0x7;
    adc_pattern[i].atten = ADC_ATTEN_DB_12; // ADC_ATTEN_DB_0;
    adc_pattern[i].channel = ch;
    adc_pattern[i].unit = unit;
    adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    ESP_LOGI(TAG1, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
    ESP_LOGI(TAG1, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
    ESP_LOGI(TAG1, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
  }
  dig_cfg.adc_pattern = adc_pattern;
  ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

  *out_handle = handle;
}
/*commented out for Wavshare version; No other reference to this function found */
// static bool check_valid_data(const adc_digi_output_data_t *data)
// {
//   if (data->type1.channel >= SOC_ADC_CHANNEL_NUM(ADC_UNIT_1))
//   {
//     return false;
//   }
//   return true;
// }
////////////////////////////////////////////
void addSmpl(int k, int i, int *pCntrA)
{
  /*The following is for diagnostic testing; it generates a psuesdo tone input of known freq & magnitude */
  // dwelCnt++;
  // if (dwelCnt>20*Goertzel_SAMPLE_CNT){ //Inc psuedo tone freq by 5 hz every 20 sample groups; i.e.~ every 80ms
  //   dwelCnt = 0;
  //   synthFreq +=5;
  //   if(synthFreq>950){
  //     synthFreq = 500;
  //   }
  //   AnglInc = 2*PI*synthFreq/SAMPLING_RATE;
  // }
  // LclToneAngle += AnglInc;
  // if(LclToneAngle >= 2*PI) LclToneAngle = LclToneAngle - 2*PI;
  // k = (int)(800*sin(LclToneAngle));

  /*Calculate Tone frequency*/
  /*the following is part of the auto tune process */
  if(!SmplSetDone){
    BiasError += k; 
    Smplcnt++;
    if(Smplcnt == 200000)
    {
      BiasError = BiasError/200000;
      Smplcnt = 0;
      SmplSetDone = true;
    }
  }
  const int ToneThrsHld = 100; // minimum usable peak tone value; Anything less is noise
  if (AutoTune)
  {
    /*if we have a usable signal; start counting the number of samples needed capture 40 periods of the incoming tone;
     i.e., a 500Hz tone used to send 2/3 a dah @ 50WPM */
    if ((SmplCNt == 0))
    {
      if (k > ToneThrsHld && Oldk <= ToneThrsHld)
      {
        /*armed with a positive going signal; ready to start new measuremnt; reset counters & wait for it to go negative*/
        TstNegFlg = true;
        NoTnCntr = 0; // set "No Tone Counter" to zero
        PeriodCntr = 0;
        SmplCNt++;
      }
    }
    else
    { // SmplCNt != 0; we're up and counting now
      SmplCNt++;
      NoTnCntr++; // increment "No Tone Counter"
      if (TstNegFlg)
      { // looking for the signal to go negative
        if ((k < -ToneThrsHld) && (Oldk >= -ToneThrsHld))
        {               /*this one just went negative*/
          NoTnCntr = 0; // reset "No Tone Counter" to zero
          TstNegFlg = false;
        }
      }
      else
      { //  TstNegFlg = false ; Now looking for the signal to go positve
        if ((k > ToneThrsHld) && (Oldk <= ToneThrsHld))
        {               /* this one just went positive*/
          PeriodCntr++; // we've lived one complete cycle of some unknown frequency
          NoTnCntr = 0; // reset "No Tone Counter" to zero
          TstNegFlg = true;
        }
      }
    }
    if (NoTnCntr == 200 || (PeriodCntr == 40 && (SmplCNt < 3900))) // 4020
    {                                                              /*We processed enough samples; But params are outside a usable frequency; Reset & try again*/
      SmplCNt = 0;
    }
    else if (PeriodCntr == 40)
    {
      /*if here, we have a usable signal; calculate its frequency*/
      DemodFreq = ((int)PeriodCntr * (int)SAMPLING_RATE) / (int)SmplCNt;
      if (DemodFreq > 450)
      {
        
        if (DemodFreq > DmodFrqOld - 2 && DemodFreq < DmodFrqOld + 2)
        {
          DemodFreq = (4*DmodFrqOld + DemodFreq)/5;
          //sprintf(Title, "Tone: %d\t%d\n", DemodFreq, (int)SmplCNt);
          //printf(Title);
          CalGtxlParamFlg = true;
          
        }
        DmodFrqOld = DemodFreq;
      }

      /*reset for next round of samples*/
      SmplCNt = 0;
    }
    Oldk = k;
  }

  ProcessSample(k, i);
/* uncomment for diagnostic testing; graph raw ADC samples*/
#ifdef POSTADC
  if ((*pCntrA < (6 * Goertzel_SAMPLE_CNT)) && UrTurn)
  {
    if ((*pCntrA == Goertzel_SAMPLE_CNT) || (*pCntrA == 2 * Goertzel_SAMPLE_CNT) || (*pCntrA == 3 * Goertzel_SAMPLE_CNT) || (*pCntrA == 4 * Goertzel_SAMPLE_CNT) || (*pCntrA == 5 * Goertzel_SAMPLE_CNT))
    {
      Smpl_buf[*pCntrA] = k; // use this line if marker is NOT needed
      // Smpl_buf[*pCntrA] = 2000+(*pCntrA); // place a marker at the end of each group
    }
    else if (*pCntrA != 0)
      Smpl_buf[*pCntrA] = k;
    *pCntrA += 1;
  }
  if ((*pCntrA == (6 * Goertzel_SAMPLE_CNT)) || !UrTurn)
  {
    UrTurn = false;
    *pCntrA = 0;
    // Smpl_buf[*pCntrA] = 2000; // place a marker at the the begining of the next set
  }
#endif
  /* END code for diagnostic testing; graph raw ADC samples*/
}

/////////////////////////////////////////////
/* Goertzel Task; Process ADC buffer*/
void GoertzelHandler(void *param)
{
  static uint32_t thread_notification;
  static const char *TAG2 = "ADC_Read";
  // uint16_t curclr = 0;
  // uint16_t oldclr = 0;
  int curclr = 0; // modified to support lvgl based display
  int oldclr = 0; // modified to support lvgl based display
  int pksig = 0;
  int k;
  int offset = 0;
  /*for Waveshare/ESP32s3 & based on 11 bit ADC output; Set bias to 1024*/
  int BIAS = 1800; // 1844+150; // based reading found when no signal applied to ESP32Cw_Machine_ADC_init
  int Smpl_CntrA = 0;
  bool FrstPass = true;
  int pksigH = 0;
  int sigSmplCnt = 0;
  // bool updt = false;
  InitGoertzel(); // make sure the Goertzel Params are setup & ready to go
  /*The following is just for time/clock testing/veification */
  // unsigned long GrtzlStart = pdTICKS_TO_MS(xTaskGetTickCount());
  // unsigned long GrtzlDone = pdTICKS_TO_MS(xTaskGetTickCount());
  bool skip1;
  while (1)
  {
    skip1 = false;
    /* Sleep until we are notified of a state change by an
     * interrupt handler. Note the first parameter is pdTRUE,
     * which has the effect of clearing the task's notification
     * value back to 0, making the notification value act like
     * a binary (rather than a counting) semaphore.  */

    thread_notification = ulTaskNotifyTake(pdTRUE,
                                           portMAX_DELAY);

    if (thread_notification)
    { // Goertzel data samples ready for processing
      unsigned long Now = EvntStart; //pdTICKS_TO_MS(xTaskGetTickCount());
      if(SmplSetDone){
        BIAS = BIAS + BiasError;
        //printf("BIAS: %d; error: %f\n", BIAS, BiasError);
        BiasError = 0;
        SmplSetDone = false;
        
      }
      

      /*the following 2 flags were added for LVGL support to manage the use of
      the 'ADCread_disp_refr_timer_mutx' Semaphore*/
      bool rdy = false;
      bool relseSemphr = false;
      ret = adc_continuous_read(adc_handle, result, Goertzel_SAMPLE_CNT * SOC_ADC_DIGI_RESULT_BYTES, &ret_num, 0);
      if (ret == ESP_OK)
      {
        if (SlwFlg)
          FrstPass = false; // 20231231 added to support new 4ms sample interval, W/ 8ms dataset
        if (FrstPass)
        {
          // InitGoertzel(cur_smpl_rate);
          ResetGoertzel();
        }
        if (SlwFlg && !FrstPass)
          offset = Goertzel_SAMPLE_CNT;
        else
          offset = 0;

        /*  ESP_LOGI("TASK_ADC", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);*/
        if (CWsndengn.IsActv() && CWsndengn.GetSOTflg())
        {
          if (xSemaphoreTake(ADCread_disp_refr_timer_mutx, pdMS_TO_TICKS(3)) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
          {
            rdy = true;
            relseSemphr = true;
          }
        }
        else
          rdy = true;
        if (rdy)
        {
          for (int i = 0; i < Goertzel_SAMPLE_CNT / 2; i++) // for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
          {
            /*Used this approach because I found the ADC data were being returned in alternating order.
            So by taking them a pair at a time,I could restore (& process them) in their true chronological order
            BUT for the Sharewave ESP32S3, it appears that the data sample pairs don't need to be reversed*/
            int pos0 = 2 * i;
            int pos1 = pos0 + 1;
            adc_digi_output_data_t *p0 = (adc_digi_output_data_t *)&result[pos0 * SOC_ADC_DIGI_RESULT_BYTES];
            adc_digi_output_data_t *p1 = (adc_digi_output_data_t *)&result[pos1 * SOC_ADC_DIGI_RESULT_BYTES];
            k = ((int)(p0->type2.data) - BIAS);     // for esp32s3 changed from type1 to type2 & samples are added in 'normal' order
            addSmpl(k, pos0 + offset, &Smpl_CntrA); // addSmpl(k, pos1, &Smpl_CntrA);
            k = ((int)(p1->type2.data) - BIAS);     // for esp32s3 changed from type1 to type2 & samples are added in 'normal' order
            addSmpl(k, pos1 + offset, &Smpl_CntrA); // addSmpl(k, pos0, &Smpl_CntrA);
          }
        }
        /* We have finished accessing the shared resource.  Release the
             semaphore. */
        if (relseSemphr)
          xSemaphoreGive(ADCread_disp_refr_timer_mutx);

#ifdef POSTADC
        skip1 = true;

        if (!skip1)
        {
#endif          
          /*logic to manage the number of data samples to take before going on to compute goertzel magnitudes*/
          // if(SlwFlg && FrstPass){
          //   FrstPass = false;
          // } else if(SlwFlg){
          //   FrstPass = true;
          // }
          if (SlwFlg && FrstPass)
          {
            FrstPass = false;
          }
          else
          {
            FrstPass = true;
          }

          if (!CWsndengn.IsActv() || !CWsndengn.GetSOTflg())
          {
            if (FrstPass)
              ComputeMags(EvntStart); //"EvntStart" is the time stamp for this dataset
            curclr = ToneClr();       // Returns Sig level as calculated by Goetzel.cpp
            // printf("curclr: %d\n", curclr);
          } // else printf("%d\n", CWsndengn.GetState());//just for diagnostic testing
          /*The following is just for time/clock testing/veification */
          unsigned long sampleIntrvl = Now - ToneStart;
          ToneStart = Now;
          cur_smpl_rate = ((ret_num) * 250) / sampleIntrvl;
#ifdef POSTADC        
        }// end if(!skip)
#endif        
      }
      else if (ret == ESP_ERR_TIMEOUT)
      {
        // We try to read `ADC SAMPLEs/DATA` until API returns timeout, which means there's no available data
        /*JMH - This ocassionally happens; Why I'm not sure; But seems to recover & goes on with little fuss*/
        ESP_LOGI(TAG2, "BREAK from GoertzelHandler TASK");
        // break; //Commented out for esp32 decoder task
      }
      else
      {
        ESP_LOGI(TAG2, "NO ADC Data Returned");
      }

    } // end if(thread_notification)
  } // end while(1) loop
  /** JMH Added this to ESP32 version to handle random crashing with error,"Task Goertzel Task should not return, Aborting now" */
  vTaskDelete(NULL);
}
///////////////////////////////////////////////////////////////////////////////////
/* DisplayUpDt Task; 
Collects data from events that happened in the last 33 ms & initiates a
Display update */
void DisplayUpDt(void *param)
{
  static uint32_t thread_notification;
  uint8_t state;
  // int QueueEmptyCnt = 0;
  int RxSig = 0;
  int CurSig = 0;
  while (1)
  {
    /* Sleep until we are notified of a state change by an
     * interrupt handler. Note the first parameter is pdTRUE,
     * which has the effect of clearing the task's notification
     * value back to 0, making the notification value act like
     * a binary (rather than a counting) semaphore.  */
    thread_notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (thread_notification)
    {
      
       //sprintf(LogBuf,"DisplayUpDt Started But Queque Reports FULL\n");
      
      if (mutexFLG)
        printf("ERROR timer driven DisplayUpDt out of Sync\n");
      // if (mutex != NULL)
      // {
      /* See if we can obtain the semaphore.  If the semaphore is not
      available wait 10 ticks to see if it becomes free.
      note:  The macro pdMS_TO_TICKS() converts milliseconds to ticks */
      // bool tryagn = true;
      // while (tryagn)
      // {
      /*TODO verify that for LVGL (ShareWave Display) that this is needed;
      Don't think it is, since Sharewave is NOT a SPI based display*/
      // if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE)
      // {
      /* We were able to obtain the semaphore and can now access the
      shared resource. */
      //  mutexFLG = true;
      if (CWsndengn.UpDtWPM)
      {
        CWsndengn.UpDtWPM = false;
        CWsndengn.RefreshWPM();
      }
      RxSig = 0;
      int cnt = 0;
      bool readQueue = true;
      //sprintf(LogBuf,"DisplayUpDt Step 1 complete\n");
      /* get latest goertzel tone magnitude */
      while (readQueue)
      {
        readQueue = (xQueueReceive(RxSig_que, (void *)&CurSig, pdMS_TO_TICKS(3)) == pdTRUE);
        if (readQueue)
        {
          RxSig = CurSig;
          // printf("RxSig %d\n", RxSig);
        }

        // cnt++;
      }
      
      //sprintf(LogBuf,"DisplayUpDt Step 2 complete\n");
      /*make sure AdvanceParserTask isn't active before preceeding*/
      if (xSemaphoreTake(DsplUpDt_AdvPrsrTsk_mutx, pdMS_TO_TICKS(15)) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
      {
        bool lpagn = true;
        while (lpagn) //(TickType_t)10
        {
          if (xQueueReceive(state_que, (void *)&state, pdMS_TO_TICKS(10)) == pdTRUE)
          {
            lvglmsgbx.IntrCrsr(state);
            //printf("state %d\n", state);
          }
          else
          {
           lpagn = false;
          }
        }
        lvglmsgbx.Str_KyBrdBat_level(bt_keyboard.get_battery_level());
        //sprintf(LogBuf,"DisplayUpDt Step 3 complete\n");
        /*update tone bar graph*/
        lvglmsgbx.dispMsg2(RxSig);
        //sprintf(LogBuf,"DisplayUpDt Step 4 complete\n");
        /* We have finished accessing the shared resource.  Release the
        semaphore. */
        xSemaphoreGive(DsplUpDt_AdvPrsrTsk_mutx);
      }
      //    mutexFLG = false;
      // tryagn = false;

      // } // END while(tryagn) loop
      // } // END if (mutex != NULL)
      //sprintf(LogBuf,"DisplayUpDt complete\n");
      QuequeFulFlg = false;
      //printf("Cur ADC Sample Rate: %d\n", cur_smpl_rate);// just for diagnostic testing
    } // END if (thread_notification)
  } // end while(1) loop
  /** JMH Added this to ESP32 version to handle random crashing with error,"Task Goertzel Task should not return, Aborting now" */
  printf("ERROR DisplayUpDt task DELETED\n");
  vTaskDelete(NULL);
}
//////////////////////////////////////////////////////////////////////////////////////
/* CW Decoder Task; CW decoder main loop*/
void CWDecodeTask(void *param)
{
  static uint32_t thread_notification;
  static const char *TAG3 = "Decode Task";
  char Smpl[10];
  int sample;
  StartDecoder(&lvglmsgbx);
  while (1)
  {
    /* Sleep until we are notified of a state change by an
     * interrupt handler. Note the first parameter is pdTRUE,
     * which has the effect of clearing the task's notification
     * value back to 0, making the notification value act like
     * a binary (rather than a counting) semaphore.  */

    thread_notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (thread_notification)
    {
      //printf("CWDecodeTask CoreID: %d\n", xPortGetCoreID());
      Dcodeloop();
      vTaskDelay(pdMS_TO_TICKS(2)); //this task under normal conditions runs all the time So insert a breather to allow Watchdog to reset
      //printf("CWDecodeTask complete\n");
      /* uncomment for diagnostic testing; graph ADC samples; Note: companion code in addSmpl(int k, int i, int *pCntrA) needs to be uncommented too */
      /* Pull accumulated "ADC sample" values from buffer & print to serial port*/
#ifdef POSTADC      
      if(!UrTurn){
        /* 
        uint32_t freq = 120*1000;//48*1000;
        uint32_t interval = APB_CLK_FREQ / (ADC_LL_CLKM_DIV_NUM_DEFAULT + ADC_LL_CLKM_DIV_A_DEFAULT / ADC_LL_CLKM_DIV_B_DEFAULT+1) /4 / freq;
        uint32_t Smpl_rate = 1000000/interval;
        char buf[40];
        sprintf(buf, "interval: %d; SmplRate: %d\n", (int)interval, (int)Smpl_rate);
        printf(buf);
        */
       if(bt_keyboard.PairFlg)
        for (int Smpl_CntrA = 0; Smpl_CntrA < 6 * Goertzel_SAMPLE_CNT; Smpl_CntrA++)
        {
          if(Smpl_buf[Smpl_CntrA] !=0){
            sprintf(Smpl, "%d\n", Smpl_buf[Smpl_CntrA]);
            printf(Smpl);
            
          }
        }
         UrTurn = true;
      }
#endif /* POSTADC */     
      /* END code for diagnostic testing; graph ADC samples; */
      
      xTaskNotifyGive(CWDecodeTaskHandle);
      
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
      ESP_LOGI(TAG3, "BREAK from CWDecodeTask TASK");
      break;
    }
    if(WokeFlg){ //report void IRAM_ATTR DotClk_ISR(void *arg) result
      WokeFlg = false;
      printf("!!WOKE!!\n");
    }
    if(QuequeFulFlg){ //report void IRAM_ATTR DotClk_ISR(void *arg) result
      //QuequeFulFlg = false;
      char buf[40];
      sprintf(buf, "!!state QUEUE FULL!! %d\n", (int)QueFullstate);
      //printf(buf);
    }
  }
}
/*look for 'advertising' BLE keyboards
note: task remains active until one is found & 'paired'('PairFlg' is set to true)*/
void BLE_scan_tsk(void *param)
{
  const char *TAG2 = "BLE_scan_tsk";
  const char *TAG2A = "\nBLE_scan_tsk";
  esp_log_level_set("*", ESP_LOG_DEBUG);
  /*Used diagnose Advance parser CPU usage*/
  // UBaseType_t uxHighWaterMark;
  // unsigned long AdvPStart = 0;
  bool loop = true;
  SkippDotClkISR = true;
  while (loop)
  {
    bt_keyboard.PairFlg = false;
    ESP_LOGI(TAG2, "BLE SCAN - START");
    uint32_t EvntStart = pdTICKS_TO_MS(xTaskGetTickCount());
    uint16_t dev_scan_intrvl = 0;
    bt_keyboard.devices_scan(2); // Required to discover new keyboards and for pairing
                                 // Default duration is 3 seconds
    //while(dev_scan_intrvl<2000)
    //{
    //  dev_scan_intrvl = (uint16_t)(pdTICKS_TO_MS(xTaskGetTickCount()) - EvntStart);
    //}
    //ESP_LOGI(TAG2, "BLE SCAN - DONE (interval: %d)", dev_scan_intrvl);
    ESP_LOGI(TAG2, "BLE SCAN - DONE\n\n");
    if (bt_keyboard.GetPairFlg1())
    { 
      bt_keyboard.PairFlg = true;
      /*Clear/Erase Keyboard text area*/
      if (!DFault.DeBug)
      {
        Title[0] = 0xFF;
        Title[1] = 0x0;
        lvglmsgbx.dispKeyBrdTxt(Title, TFT_ORANGE); // clear send keyboard text area
      }
      sprintf(Title, "KEYBOARD READY\n");
      lvglmsgbx.dispKeyBrdTxt(Title, TFT_ORANGE);
      ESP_LOGI(TAG2, "Pairing Complete; KEYBOARD READY");
    }
    if (bt_keyboard.PairFlg)
    {
      //printf("EXIT SCAN\n");
      ESP_LOGI(TAG2, "EXIT SCAN");
      loop = false;
      // vTaskSuspend(NULL); // NULL = suspend this task
    }
  } // end while(loop)
  /** JMH Added this to ESP32 version to handle random crashing with error,"Task Goertzel Task should not return, Aborting now" */
  SkippDotClkISR = false;
  vTaskDelete(NULL);
}

///////////////////////////////////////////////////////////////////////////////////
/* AdvParserTask Task; Post Parser Code execution
*  Note: this task runs on core 1 
*/
void AdvParserTask(void *param)
{
  // static uint32_t thread_notification;// not used in AdvParserTask
  /*Used diagnose Advance parser CPU usage*/
  // UBaseType_t uxHighWaterMark;
  // unsigned long AdvPStart = 0;
  while (1)
  {
    /* Sleep until instructed to resume from DcodeCW.cpp */
    // printf("AdvParserTask Launched\n"); //added to verify task management
    /*Used diagnose Advance parser CPU usage*/
    // AdvPStart = pdTICKS_TO_MS(xTaskGetTickCount());
    advparser.EvalTimeData();
    /*Now compare Advparser decoded text to original text; If not the same,
    replace displayed with Advparser version*/
    bool same = true;
    bool Tst4Match = true;
    int i;
    int FmtchPtr; // now only used for debugging
    /*Scan/compare last word displayed w/ advpaser's version*/
    int NuMsgLen = advparser.GetMsgLen();
    int LtrPtr = advparser.LtrPtr;
    wrdbrkFtcr = advparser.wrdbrkFtcr;
    // printf("NuMsgLen = %d; LtrPtr %d\n", NuMsgLen, LtrPtr);
    if (NuMsgLen > LtrPtr)
    { // if the advparser test string is longer, then delete the last word printed
      same = false;
      i = LtrPtr;
    }
    else
    {
      for (i = 0; i < LtrPtr; i++)
      {
        if (advparser.Msgbuf[i] == 0)
          Tst4Match = false;
        if ((advparser.LtrHoldr[i] != advparser.Msgbuf[i]) && Tst4Match)
        {
          FmtchPtr = i; // now only used for debugging
          same = false;
        }
        if (advparser.LtrHoldr[i] == 0)
          break;
      }
    }

    /*If they don't match, replace displayed text with AdvParser's version*/
    int deletCnt = 0; // moved to here to support lvgl debugging
    char spacemarker = 'Y';
    uint8_t LstChr = 0;
    if (!same)
    {
      /*need to block display update task during this 'if()' code */
      if (xSemaphoreTake(DsplUpDt_AdvPrsrTsk_mutx, portMAX_DELAY) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
      {
        bool oldDltState = dletechar;
        // int deletCnt = 0;
        // char spacemarker = 'Y';
        // uint8_t LstChr = 0;
        LstChr = lvglmsgbx.GetLastChar();
        // if (LtrPtr <= 11)
        // {
        //   while (LstChr != 0x20)
        //   {
        //     vTaskDelay(5 / portTICK_PERIOD_MS);
        //     LstChr = lvglmsgbx.GetLastChar();
        //   }
        // }
        // else
        // {
        //   vTaskDelay(50 / portTICK_PERIOD_MS); // wait long enough for the TFT display to get updated
        //   LstChr = lvglmsgbx.GetLastChar();     // get the last character posted via the DcodeCW process (To test below for a 'space' [0x20])
        // }
        // printf("lvglmsgbx.GetLastChar = %d\n", LstChr);// added for testing with lvgl display handler
        if (LstChr == 0x20) // test to see if a word break space has been applied
        {
          deletCnt = 1 + i; // number of characters + space to be deleted
          // printf("MsgChrCnt[1] = %d\n", 1+ i);
        }
        else
        {
          spacemarker = 'N';
          deletCnt = i; // number of characters (No space) to be deleted
          // printf("MsgChrCnt[1] = %d\n", i);
        }
        /*Configure DCodeCW.cpp to erase/delete original text*/
        // MsgChrCnt[1] = deletCnt; // Load delete buffer w/ the number of characters to be deleted
        // dletechar = true;

        lvglmsgbx.Delete(true, deletCnt);
        /*Add word break space to post parsed text*/
        advparser.Msgbuf[NuMsgLen] = 0x20;
        advparser.Msgbuf[NuMsgLen + 1] = 0x0;
        if (advparser.Dbug)
          printf("old txt %s;  new txt %s delete cnt %d; advparser.LtrPtr %d ; new txt length %d; Space Corrected = %c/%d \n", advparser.LtrHoldr, advparser.Msgbuf, deletCnt, LtrPtr, NuMsgLen, spacemarker, LstChr);
        // printf("Pointer ERROR\n");/ printf("No Match @ %d; %d; %d\n", FmtchPtr, LtrHoldr[FmtchPtr], advparser.Msgbuf[FmtchPtr]);
        CptrTxt = false;
        lvglmsgbx.dispDeCdrTxt(advparser.Msgbuf, TFT_GREEN); // Added for lvgl; Note: color value is meaningless
        CptrTxt = true;
        dletechar = oldDltState;
        /*now should be safe resume displayupdate task*/
        xSemaphoreGive(DsplUpDt_AdvPrsrTsk_mutx);
      }
    } // else printf("Same\n");
    if (advparser.Dbug)
      printf("%s\n", advparser.LtrHoldr);
    if (same)
    {
    } // printf("END Scan SAME true\n");
    else
      printf("old txt %s;  new txt %s delete cnt %d; advparser.LtrPtr %d ; new txt length %d; Space Corrected = %c/%d \n", advparser.LtrHoldr, advparser.Msgbuf, deletCnt, LtrPtr, NuMsgLen, spacemarker, LstChr);
    // erase contents of LtrHoldr & reset its index pointer (LtrPtr)
    for (int i = 0; i < LtrPtr; i++)
      advparser.LtrHoldr[i] = 0;
    if (advparser.Dbug)
      printf("--------\n\n");
    /*The following code is priimarilry for debugging stack alocation & understanding how much time the advance parser needs*/
    // uint16_t AdvPIntrvl = (uint16_t)(pdTICKS_TO_MS(xTaskGetTickCount())-AdvPStart);
    // printf("AdvPIntrvl: %d\n", AdvPIntrvl);
    // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    // printf("AdvParserTask stack: %d\n", (int)uxHighWaterMark);
    // printf("AdvParserTask CoreID: %d\n", xPortGetCoreID());
    // printf("Done\n");
    /* Sleep until instructed to resume from DcodeCW.cpp */
    // printf("AdvParser Task Done\n"); //added to verify task management
    vTaskSuspend(AdvParserTaskHandle);
  } // end while(1) loop
  /** JMH Added this to ESP32 version to handle random crashing with error,"Task Goertzel Task should not return, Aborting now" */
  vTaskDelete(NULL);
}

///////////////////////////////////////////////////////////////////////////////////

extern "C"
{
  void app_main();
}
/*Template for keyboard entry handler function detailed futrther down in this file*/
void ProcsKeyEntry(uint8_t keyVal);
/*Settings screen keybrd entry loop*/
void SettingsLoop(void);
/*        Display Update timer ISR Template        */
void DsplTmr_callback(TimerHandle_t xtimer);
/*        DotClk timer ISR Template                */
void IRAM_ATTR DotClk_ISR(void *arg);

#if USE_KYBrd
void pairing_handler(uint32_t pid)
{
  sprintf(Title, "Please enter the following pairing code:\n");
  lvglmsgbx.dispDeCdrTxt(Title, TFT_WHITE);
  /*take the pin # and parse it into a string of 2 digit number groups separated by a space*/
  char rawpinstr[10];
  char Prsdpin_str[10];
  // for (int i = 0; i < 10; i++)
  // {
  //   rawpinstr[i] = 0;
  //   Prsdpin_str[i] =0 ;
  // }
  sprintf(rawpinstr, "%6d", (int)pid);//convert pin from interger to text
  for (int i = 0; i < 3; i++)
  {
    int grpptr = 3 * i;
    int rawptr = 2 * i;
    Prsdpin_str[grpptr] = rawpinstr[rawptr];
    if (rawpinstr[rawptr + 1] != 0)
    {
      Prsdpin_str[grpptr + 1] = rawpinstr[rawptr + 1];
      Prsdpin_str[grpptr + 2] = ' ';
    }
    else
      Prsdpin_str[grpptr + 1] = rawpinstr[rawptr + 1];
    Prsdpin_str[grpptr + 3] = 0; //make sure the current string is NULL terminated 
  }
  sprintf(Title, "     %s\n", Prsdpin_str);
  lvglmsgbx.dispDeCdrTxt(Title, TFT_BLUE);
  sprintf(Title, "followed with ENTER on your keyboard: \n\n");
  lvglmsgbx.dispDeCdrTxt(Title, TFT_WHITE);
  //bt_keyboard.PairFlg = true;
  vTaskDelay(100 / portTICK_PERIOD_MS);
  //bt_keyboard.PairFlg = bt_keyboard.GetPairFlg1();
}
#endif /* USE_KYBrd*/

void app_main()
{

  ModeCnt = 0;
  static const char *TAG = "Main Start";
  esp_log_level_set("*", ESP_LOG_DEBUG);
  // Configure CW send IO pin aka 'KEY'
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << KEY);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  /*TODO BT Keyboard CW Send related code*/
  gpio_config(&io_conf);
  //digitalWrite(KEY, Key_Up); // key 'UP' state
  gpio_set_level(KEY, Key_Up); // key 'UP' state
  state_que = xQueueCreate(state_que_len, sizeof(uint8_t));
  RxSig_que  = xQueueCreate(RxSig_que_len, sizeof(int));
  mutex = xSemaphoreCreateMutex();
  DsplUpDt_AdvPrsrTsk_mutx =  xSemaphoreCreateMutex();
  ADCread_disp_refr_timer_mutx = xSemaphoreCreateMutex();
  /*create DisplayUpDate Task*/
  xTaskCreatePinnedToCore(DisplayUpDt, "DisplayUpDate Task", 8192, NULL, 3, &DsplUpDtTaskHandle, 0);
  vTaskSuspend( DsplUpDtTaskHandle);

  /*Selected 2092 based on results found by using uxTaskGetStackHighWaterMark( NULL ) in AdvParserTask*/
  xTaskCreatePinnedToCore(
      AdvParserTask, /* Function to implement the task */
      "AdvParser Task", /* Name of the task */
      4096,  /* Stack size in words */
      NULL,  /* Task input parameter */
      2,  /* Priority of the task */
      &AdvParserTaskHandle,  /* Task handle. */
      1); /* Core where the task should run */
  if (AdvParserTaskHandle == NULL)
    ESP_LOGI(TAG, "AdvParser Task handle FAILED");

  vTaskSuspend( AdvParserTaskHandle );
/////////////////////////////////////////////////
/*Not currently using this approach/method*/
  //  xTaskCreatePinnedToCore(
  //    UpDtRxSigTask, /* Function to implement the task */
  //     "UpDtRxSig Task", /* Name of the task */
  //     4096,  /* Stack size in words */
  //     NULL,  /* Task input parameter */
  //     1,  /* Priority of the task */
  //     &UpDtRxSigTaskHandle,  /* Task handle. */
  //     1); /* Core where the task should run */
  // if (UpDtRxSigTaskHandle == NULL)
  //   ESP_LOGI(TAG, "UpDtRxSig Task handle FAILED");

  // vTaskSuspend( UpDtRxSigTaskHandle );
///////////////////////////////////////////////  
  
  xTaskCreatePinnedToCore(
      CWDecodeTask, /* Function to implement the task */
      "CW Decode Task", /* Name of the task */
      8192,  /* Stack size in words */
      NULL,  /* Task input parameter */
      4,  /* Priority of the task */
      &CWDecodeTaskHandle,  /* Task handle. */
      0); /*Core 0 or 1 */
  
  if (CWDecodeTaskHandle == NULL)
    ESP_LOGI(TAG, "CW Decoder Task handle FAILED");

  xTaskCreate(GoertzelHandler, "Goertzel Task", 8192, NULL, 5, &GoertzelTaskHandle); // priority used to be 3
  if (GoertzelTaskHandle == NULL)
    ESP_LOGI(TAG, "Goertzel Task Task handle FAILED");

  /*Setup Software Display Refresh/Update timer*/
  DisplayTmr = xTimerCreate(
      "Display-Refresh-Timer",
      pdMS_TO_TICKS(33), //33 / portTICK_PERIOD_MS i.e. 30Fps
      pdTRUE,
      (void *)1,
      DsplTmr_callback);
  /*designated Memory space for ADC sample sets to land at)*/
  memset(result, 0xcc, (SOC_ADC_DIGI_RESULT_BYTES * Goertzel_SAMPLE_CNT)); // 1 byte for the channel # & 1 byte for the data
  
  // ADC_smpl_mutex = xSemaphoreCreateMutex();

  /*setup Hardware DOT Clock timer & link to timer ISR*/
  const esp_timer_create_args_t DotClk_args = {
      .callback = &DotClk_ISR,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_ISR,
      .name = "DotClck"};

  ESP_ERROR_CHECK(esp_timer_create(&DotClk_args, &DotClk_hndl));

  
  /* The timer has been created but is not running yet */
//intr_matrix_set(xPortGetCoreID(), ETS_TG0_T1_LEVEL_INTR_SOURCE, 26); // ESP32
intr_matrix_set(xPortGetCoreID(), XCHAL_TIMER1_INTERRUPT, 26);// ESP32S3 added this entry/setting for waveshare display dev baord
 ESP_INTR_ENABLE(26); // display

 // To test the Pairing code entry, uncomment the following line as pairing info is
  // kept in the nvs. Pairing will then be required on every boot.
  // ret = nvs_flash_init();
  // ESP_ERROR_CHECK(nvs_flash_erase());
  ret = nvs_flash_init();
  if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /*Initialize display and load splash screen*/
  lvglmsgbx.InitDsplay();
  // sprintf(Title, " ESP32s3 CW Machine (%s)\n", RevDate); // sprintf(Title, "CPU CORE: %d\n", (int)xPortGetCoreID());
  // lvglmsgbx.dispDeCdrTxt(Title, TFT_SKYBLUE);

  /* test/check NVS to see if user setting/param have been stored */
  uint8_t Rstat = Read_NVS_Str("MyCall", DFault.MyCall);
  if (Rstat != 1)
  {
    /*No settings found; load factory settings*/
    sprintf(DFault.MyCall, "%s", MyCall);
    sprintf(DFault.MemF2, "%s", MemF2);
    sprintf(DFault.MemF3, "%s", MemF3);
    sprintf(DFault.MemF4, "%s", MemF4);
    sprintf(DFault.MemF5, "%s", MemF5);
    DFault.DeBug = DeBug;
    DFault.WPM = CWsndengn.GetWPM();
    DFault.ModeCnt = ModeCnt;
    DFault.AutoTune = AutoTune;
    DFault.TRGT_FREQ = (int)TARGET_FREQUENCYC;
    DFault.Grtzl_Gain = Grtzl_Gain;
    DFault.SlwFlg = SlwFlg;
    DFault.NoisFlg = NoisFlg;
    sprintf(Title, "\n        No stored USER params Found\n   Using FACTORY values until params are\n   'Saved' via the Settings Screen\n");
    lvglmsgbx.dispDeCdrTxt(Title, TFT_ORANGE);
  #if USE_KYBrd
  bt_keyboard.trapFlg = false;
  #endif /* USE_KYBrd*/
  }
  else
  {
    /*found 'mycall' stored setting, go get the other user settings */
    Rstat = Read_NVS_Str("MemF2", DFault.MemF2);
    Rstat = Read_NVS_Str("MemF3", DFault.MemF3);
    Rstat = Read_NVS_Str("MemF4", DFault.MemF4);
    Rstat = Read_NVS_Str("MemF5", DFault.MemF5);
    Rstat = Read_NVS_Val("DeBug", DFault.DeBug);
    Rstat = Read_NVS_Val("WPM", DFault.WPM);
    Rstat = Read_NVS_Val("ModeCnt", DFault.ModeCnt);
    Rstat = Read_NVS_Val("TRGT_FREQ", DFault.TRGT_FREQ);
    int intGainVal; // uint64_t intGainVal;
    Rstat = Read_NVS_Val("Grtzl_Gain", intGainVal);
    DFault.Grtzl_Gain = (float)intGainVal / 10000000.0;
    int strdAT;
    Rstat = Read_NVS_Val("AutoTune", strdAT);
    DFault.AutoTune = (bool)strdAT;
    Rstat = Read_NVS_Val("SlwFlg", strdAT);
    DFault.SlwFlg = (bool)strdAT;
    Rstat = Read_NVS_Val("NoisFlg", strdAT);
    DFault.NoisFlg = (bool)strdAT;
    /*pass the decoder setting(s) back to their global counterpart(s) */
    AutoTune = DFault.AutoTune;
    SlwFlg = DFault.SlwFlg;
    NoisFlg = DFault.NoisFlg;
    ModeCnt = DFault.ModeCnt;
    DeBug = DFault.DeBug;
    TARGET_FREQUENCYC = (float)DFault.TRGT_FREQ;
    Grtzl_Gain = DFault.Grtzl_Gain;
  }
  /* Start the Display timer */
  xTimerStart(DisplayTmr, portMAX_DELAY);
  vTaskResume( DsplUpDtTaskHandle);
  
  InitGoertzel();
  /*This delay is just to give time for an external USB serrial monitor to get up and running*/
  //vTaskDelay(5000 / portTICK_PERIOD_MS);
 
    #if USE_KYBrd
  bt_keyboard.inPrgsFlg = false;
  if(DFault.DeBug)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    printf("\n");
  }  
  /*start bluetooth pairing/linking process*/
    if (bt_keyboard.setup(pairing_handler, &bt_keyboard )) // Must be called once
  { 
    /*Start BLE Scan */ 
    xTaskCreatePinnedToCore(BLE_scan_tsk, "BLE Scan Task", 8192, NULL, 1, &BLEscanTask_hndl, 0);
  }
  else
  {
    printf("bt_keyboard.setup(pairing_handler) FAILED!!\n");
    printf("!!PROGRAM HALTED!!\n");
    sprintf(Title, "\n*******  RESTART!  ******\n** NO KEYBOARD PAIRED ***");
    lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
    while (true)
    {
      /*do nothiing*/
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  /*create a scan task for reconnect*/
  // xTaskCreatePinnedToCore(BLE_scan_tsk, "BLE Scan Task", 8192, NULL, 1, &BLEscanTask_hndl, 0);
  // vTaskSuspend( BLEscanTask_hndl);
  #endif /* USE_KYBrd*/
  /*initialize & start continuous DMA ADC conversion process*/
  Cw_Machine_ADC_init(channel, sizeof(channel) / sizeof(adc_channel_t), &adc_handle);
  
  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = s_conv_done_cb,
  };
  ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
  ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
  adcON = true;
  xTaskNotifyGive(CWDecodeTaskHandle);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  
  ESP_LOGI(TAG, "Start DotClk interrupt Config");
  //ESP_ERROR_CHECK(esp_intr_alloc(ETS_TG0_T1_LEVEL_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3, DotClk_ISR, DotClk_args.arg, NULL));
  ESP_ERROR_CHECK(esp_intr_alloc(XCHAL_TIMER1_INTERRUPT, ESP_INTR_FLAG_LEVEL3, DotClk_ISR, DotClk_args.arg, NULL));// ESP32S3 added this entry/setting for waveshare display dev baord
  ESP_LOGI(TAG, "End DotClk interrupt Config"); 
 /* Start the timer dotclock (timer)*/
  ESP_ERROR_CHECK(esp_timer_start_periodic(DotClk_hndl, 60000)); // 20WPM or 60ms
  CWsndengn.RfrshSpd = true;
  CWsndengn.ShwWPM(DFault.WPM); // calling this method does NOT recalc/set the dotclock & show the WPM
  CWsndengn.SetWPM(DFault.WPM); // 20230507 Added this seperate method call after changing how the dot clocktiming gets updated
  CWsndengn.UpDtWPM = true;
  

  /* main CW keyboard loop*/
  
  while (true)
  {

    #if 1 // 0 = scan codes retrieval, 1 = augmented ASCII retrieval
    /* note: this loop only completes when there is a key entery from a paired/connected Bluetooth Keyboard */

    vTaskDelay(pdMS_TO_TICKS(10)); // give the watchdogtimer a chance to reset
    //printf("main loop\n");
    if (setupFlg)
    {
      printf("setupFlg: 'true'\n");
      /*if true, exit main loop and jump to "settings" screen */
      bool IntSOTstate = CWsndengn.GetSOTflg();
      if (IntSOTstate)
        CWsndengn.SOTmode();   // do this to stop any outgoing txt that might currently be in progress before switching over to settings screen
      lvglmsgbx.SaveSettings(); // save keyboard app's current display configuration; i.e., ringbuffer pointeres, etc. So that when the user closes the setting screen the keyboard app can conitnue fro where it left off
      /* if in decode mode 4, the adc DMA scan was never started*/
      if (ModeCnt != 4)
      {
        ESP_ERROR_CHECK(adc_continuous_stop(adc_handle)); // true; user has pressed Ctl+S key, & wants to configure default settings
        adcON = false;
        printf("CASE 1\n");
        vTaskSuspend(GoertzelTaskHandle);
        ESP_LOGI(TAG1, "SUSPEND GoertzelHandler TASK");
        vTaskSuspend(CWDecodeTaskHandle);
        ESP_LOGI(TAG1, "SUSPEND CWDecodeTaskHandle TASK");
        vTaskDelay(20);
      }
      /*Now ready to jump to "settings" screen */
      /*TODO for lvgl/waveshare need to work out how to navigate settings screen */
      lvglmsgbx.BldSettingScreen();
      SettingsLoop();// go here while settings screen is active
      CWsndengn.RefreshWPM();
      if (IntSOTstate && !CWsndengn.GetSOTflg())
        CWsndengn.SOTmode(); // Send On Type was enabled when we went to 'settings' so re-enable it
      if (ModeCnt != 4)
      {
        // Cw_Machine_ADC_init(channel, sizeof(channel) / sizeof(adc_channel_t), &adc_handle);
        // ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
        ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
        adcON = true;
        ESP_LOGI(TAG1, "RESUME CWDecodeTaskHandle TASK");
        vTaskResume(CWDecodeTaskHandle);
        ESP_LOGI(TAG1, "RESUME GoertzelHandler TASK");
        vTaskResume(GoertzelTaskHandle);
        vTaskDelay(20);
      }
    }


    /*Added this to support 'open' paired BT keyboard event*/
    
    switch (bt_keyboard.Adc_Sw)
    {
    case 1: // need to shut down all other spi activities, so the pending BT event has exclusive access to SPI
      bt_keyboard.Adc_Sw = 0;
      if (adcON && !CWsndengn.GetSOTflg()) // added "if" just to make sure we're not gonna do something that doesn't need doing
      {
        ESP_LOGI(TAG1, "!!!adc_continuous_stop!!!");
        printf("CASE 2\n");
        ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
        adcON = false;
        vTaskSuspend(GoertzelTaskHandle);
        ESP_LOGI(TAG1, "SUSPEND GoertzelHandler TASK");
        vTaskSuspend(CWDecodeTaskHandle);
        ESP_LOGI(TAG1, "SUSPEND CWDecodeTaskHandle TASK");
      }
      break;

    case 2: // BT event complete restore/resume all other spi activities
      bt_keyboard.Adc_Sw = 0;
      if (!adcON) // added "if" just to make sure we're not gonna do something that doesn't need doing
      {
        ESP_LOGI(TAG1, "***adc_continuous_start***");
        ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
        adcON = true;
        ESP_LOGI(TAG1, "RESUME CWDecodeTaskHandle TASK");
        vTaskResume(CWDecodeTaskHandle);
        ESP_LOGI(TAG1, "RESUME GoertzelHandler TASK");
        vTaskResume(GoertzelTaskHandle);
        if (bt_keyboard.PairFlg && bt_keyboard.inPrgsFlg)
        {
          xTimerStart(DisplayTmr, portMAX_DELAY);
          //vTaskResume(DsplUpDtTaskHandle);
          bt_keyboard.PairFlg = false;
          bt_keyboard.inPrgsFlg = false;
          Title[0] = 0xFF;
          Title[0] = 0x0; 
          lvglmsgbx.dispKeyBrdTxt(Title, TFT_ORANGE);
          ESP_LOGI(TAG2, "Pairing Complete; Display ON");
        }
      }
      break;

    default:
      break;
    } // end switch
    if (bt_keyboard.PairFlg && !bt_keyboard.inPrgsFlg)
    {
      xTimerStop(DisplayTmr, portMAX_DELAY);
      ESP_LOGI(TAG2, "Pairing Start; Display OFF");
      bt_keyboard.inPrgsFlg = true;
    }
    uint8_t key = 0;
    if (0) // set to '1' for flag debugging
    {
      if (bt_keyboard.OpnEvntFlg)
        printf("OpnEvntFlgTRUE");
      else
        printf("OpnEvntFlgFALSE");
      if (bt_keyboard.trapFlg)
        printf("; trapFlgTRUE\n");
      else
        printf("; trapFlgFALSE\n");

      // else
      //   printf("; PairFlgFALSE\n");
    }
    if (bt_keyboard.OpnEvntFlg && !bt_keyboard.trapFlg) // this gets set to true when the K380 KB generates corrupt keystroke data
      key = bt_keyboard.wait_for_ascii_char(false);          // true  // by setting to "true" this task/loop will wait 'forever' for a keyboard key press

    /*test key entry & process as needed*/
    if (key != 0)
    {
      ProcsKeyEntry(key);
      
    }

#else
#if USE_KYBrd
    BTKeyboard::KeyInfo inf;

    bt_keyboard.wait_for_low_event(inf);

    printf("RECEIVED KEYBOARD EVENT - Mod: %#X,  Keys: %#X, %#X, %#X \n", (uint8_t)inf.modifier, inf.keys[0], inf.keys[1], inf.keys[2]);
    // std::cout << "RECEIVED KEYBOARD EVENT: "
    //           << std::hex
    //           << "Mod: "
    //           << +(uint8_t)inf.modifier
    //           << ", Keys: "
    //           << +inf.keys[0] << ", "
    //           << +inf.keys[1] << ", "
    //           << +inf.keys[2] << std::endl;
#endif /* USE_KYBrd*/
#endif/* if(1||0)*/
  }/*End while loop*/
} /*End Main loop*/

/*Timer interrupt ISRs*/

void DsplTmr_callback(TimerHandle_t xtimer)
{
  // uint8_t state;
  // BaseType_t TaskWoke;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(DsplUpDtTaskHandle, &xHigherPriorityTaskWoken); // start DisplayUpDt Task
  if (xHigherPriorityTaskWoken == pdTRUE)
  {
      /*pdTRUE then a context switch should be performed to ensure 
      the interrupt returns directly to the highest priority task. */
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
      
  }
    if (QuequeFulFlg)
      {
        printf("DisplayUpDt timer Runing But Queque Reports FULL\n");
        printf(LogBuf);
        
      }
}
/*                                          */
/* DotClk timer ISR                 */
void IRAM_ATTR DotClk_ISR(void *arg)
{
  BaseType_t Woke;
  uint8_t state =0;
  if(SkippDotClkISR) return; //this will be true while BLE scan task is active
  // if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) // pdMS_TO_TICKS()//portMAX_DELAY
  // {
  //   /* We were able to obtain the semaphore and can now access the
  //   shared resource. 
  //   this was placed here because CWsndengn.Intr() can cause the SHOWWPM function to write to the display*/
  //   mutexFLG = true;
    state = CWsndengn.Intr(); // check CW send Engine & process as code as needed
                                      /* We have finished accessing the shared resource.  Release the semaphore. */
  //   xSemaphoreGive(mutex);
  //   mutexFLG = false;
  // }
  if (state != 0)
  {
    vTaskSuspend(CWDecodeTaskHandle);
    clrbuf = true;
  }
  else
  {
    if (clrbuf)
    {
      clrbuf = false;
      CLrDCdValBuf(); // added this to ensure no DeCode Vals accumulate while the CWGen process is active; In other words the App doesn't decode itself
      vTaskResume(CWDecodeTaskHandle);
    }
  }
  /*Push returned "state" values on the que */
  if (xQueueSendFromISR(state_que, &state, &Woke) == pdFALSE)
  {
    QuequeFulFlg = true;
    QueFullstate = state;
  }
  if (Woke == pdTRUE)
    portYIELD_FROM_ISR(Woke);
  /*Woke == pdTRUE, if sending to the queue caused a task to unblock,
  and the unblocked task has a priority higher than the currently running task.
  If xQueueSendFromISR() sets this value to pdTRUE,
  then a context switch should be requested before the interrupt is exited.*/
  // WokeFlg = true;
}
///////////////////////////////////////////////////////////////////////////////////

/*This routine checks the current key entry; 1st looking for special key values that signify special handling
if none are found, it hands off the key entry to be treated as a standard CW character*/
void ProcsKeyEntry(uint8_t keyVal)
{
  bool addspce = false;
  char SpcChr = char(0x20);

  if (keyVal == 0x8)
  {                                  //"BACKSpace" key pressed
    int ChrCnt = CWsndengn.Delete(); // test to see if there's an "unsent" character that can be deleted
    if (ChrCnt > 0)
    {
      if (adcON && !CWsndengn.GetSOTflg()) // added "if" just to make sure we're not gonna do something that doesn't need doing
      {
        // ESP_LOGI(TAG1, "!!!adc_continuous_stop!!!");
        ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
        printf("CASE 3\n");
        adcON = false;
        vTaskSuspend(GoertzelTaskHandle);
        // ESP_LOGI(TAG1, "SUSPEND GoertzelHandler TASK");
        // vTaskSuspend(CWDecodeTaskHandle);
        // ESP_LOGI(TAG1, "SUSPEND CWDecodeTaskHandle TASK");
      }
      if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) // wait forever
      {
        /* We were able to obtain the semaphore and can now access the
        shared resource. */
        mutexFLG = true;
        /*TODO When Keyboard is enabled, this needs work*/
        lvglmsgbx.Delete(false, ChrCnt);
        /* We have finished accessing the shared resource.  Release the
        semaphore. */
        xSemaphoreGive(mutex);
        mutexFLG = false;
      }
      if (!adcON) // added "if" just to make sure we're not gonna do something that doesn't need doing
      {
        // ESP_LOGI(TAG1, "***adc_continuous_start***");
        ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
        adcON = true;
        // ESP_LOGI(TAG1, "RESUME CWDecodeTaskHandle TASK");
        // vTaskResume(CWDecodeTaskHandle);
        // ESP_LOGI(TAG1, "RESUME GoertzelHandler TASK");
        vTaskResume(GoertzelTaskHandle);
      }
    }
    return;
  }
  else if (keyVal == 0x98)
  { // PG/arrow UP
    CWsndengn.IncWPM();
    DFault.WPM = CWsndengn.GetWPM();
    return;
  }
  else if (keyVal == 0x97)
  { // PG/arrow DOWN
    CWsndengn.DecWPM();
    DFault.WPM = CWsndengn.GetWPM();
    return;
  }
  else if (keyVal == 0x8C)
  { // F12 key (Alternate action SOT [Send On type])
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
      /* We were able to obtain the semaphore and can now access the
      shared resource. */
      mutexFLG = true;
      CWsndengn.SOTmode();
      /* We have finished accessing the shared resource.  Release the semaphore. */
      xSemaphoreGive(mutex);
      mutexFLG = false;
    }
    return;
  }
  else if (keyVal == 0x81)
  { // F1 key (store TEXT)
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
      /* We were able to obtain the semaphore and can now access the
      shared resource. */
      mutexFLG = true;
      CWsndengn.StrTxtmode();
      /* We have finished accessing the shared resource.  Release the semaphore. */
      xSemaphoreGive(mutex);
      mutexFLG = false;
    }
    return;
  }
  else if (keyVal == 0x82)
  { // F2 key (Send MemF2)
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(DFault.MemF2, sizeof(DFault.MemF2));
    return;
  }
  else if (keyVal == 0x83)
  { // F3 key (Send MemF3)
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(DFault.MemF3, sizeof(DFault.MemF3));
    return;
  }
  else if (keyVal == 0x84)
  { // F4 key (Send MemF4)
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(DFault.MemF4, sizeof(DFault.MemF4));
    return;
  }
  else if (keyVal == 0x85)
  { // F5 key (Send MemF5)
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(DFault.MemF5, sizeof(DFault.MemF5));
    return;
  }
  else if (keyVal == 0x95)
  { // Right Arrow Key (Alternate action SOT [Send On type])
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
      /* We were able to obtain the semaphore and can now access the
      shared resource. */
      mutexFLG = true;
    CWsndengn.SOTmode();
    /* We have finished accessing the shared resource.  Release the semaphore. */
      xSemaphoreGive(mutex);
      mutexFLG = false;
    }
    return;
  }
  else if (keyVal == 0x96)
  { // Left Arrow Key (store TEXT)
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
      /* We were able to obtain the semaphore and can now access the
      shared resource. */
      mutexFLG = true;
    CWsndengn.StrTxtmode();
    /* We have finished accessing the shared resource.  Release the semaphore. */
      xSemaphoreGive(mutex);
      mutexFLG = false;
    }
    return;
  }
  else if (keyVal == 0x1B)
  { // ESC key (Kill Send)
    CWsndengn.AbortSnd();
    return;
  }
  else if ((keyVal == 0x9B))
  { // Cntrl+"T"
    CWsndengn.Tune();
    return;
  }
  else if ((keyVal == 0x9C))
  { // Cntrl+"S"
    setupFlg = !setupFlg;
    return;
  }
  else if ((keyVal == 0x9D))
  { // Cntrl+"F"; auto-tune/ freqLocked
    AutoTune = !AutoTune;
    DFault.AutoTune = AutoTune;
    vTaskDelay(20);
    showSpeed();
    vTaskDelay(250);
    return;
  }
  else if ((keyVal == 0xA1))
  { // Cntrl+"G"; Sample interval 4ms / 8ms
    // int GainCnt =0;
    // if(NoisFlg) GainCnt = 2;
    // if(SlwFlg && !NoisFlg) GainCnt = 1;
    //   GainCnt++;
    // if (GainCnt > 1)  GainCnt = 0;//20231029 decided that the 3rd gain mode was no longer needed, so locked it out
    //   switch(GainCnt){
    // case 0:
    //   SlwFlg = false;
    //   NoisFlg = false;
    //   break;
    // case 1:
    //   SlwFlg = true;
    //   NoisFlg = false;
    //   break;
    // case 2:
    //   SlwFlg = true;
    //   NoisFlg = true;
    //   break;
    /*20240123 Changed So that Ctrl+G now Enables/Disables Debug */
    if (DeBug)
    {
      DeBug = false;
      DFault.DeBug = false;
    }
    else
    {
      DeBug = true;
      DFault.DeBug = true;
    }
    return;
  }
  /*20240123 Disabled Ctrl+D function; replaced by AdvParser Class & methods*/
  // else if ((keyVal == 0x9E))
  // { // LEFT Cntrl+"D"; Decode Modef()

  //     /*Normal setup */
  //     ModeCnt++;
  //     if (ModeCnt > 3)
  //     ModeCnt = 0;
  //     DFault.ModeCnt = ModeCnt;
  //     SetModFlgs(ModeCnt);
  //     CurMdStng(ModeCnt);//added 20230104
  //     vTaskDelay(20);
  //     showSpeed();
  //     vTaskDelay(250);
  //     return;
  // }
  else if ((keyVal == 0xA0))
  { // RIGHT Cntrl+"D"; Decode Modef()

    /*Normal setup */
    // if (ModeCnt == 4)
    // {
    //   ESP_LOGI(TAG1, "RESUME CWDecodeTaskHandle TASK");
    //   vTaskResume(CWDecodeTaskHandle);
    //   ESP_LOGI(TAG1, "RESUME GoertzelHandler TASK");
    //   vTaskResume(GoertzelTaskHandle);
    //   ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    //   adcON =true;
    //   vTaskDelay(20);
    // }
    ModeCnt--;
    if (ModeCnt < 0)
      ModeCnt = 3;
    DFault.ModeCnt = ModeCnt;
    SetModFlgs(ModeCnt);
    CurMdStng(ModeCnt); // added 20230104
    vTaskDelay(20);
    showSpeed();
    vTaskDelay(250);
    return;
  }
  else if ((keyVal == 0x9F))
  { // Cntrl+"P"; CW decode ADC plot Enable/Disable
    PlotFlg = !PlotFlg;
    // DFault.AutoTune = AutoTune;
    vTaskDelay(250);
    return;
  }
  else if ((keyVal == 0xD))
  { // "ENTER" Key send myCallSign
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(DFault.MyCall, sizeof(DFault.MyCall));
    return;
  }
  else if ((keyVal == 0x9A))
  { // "Cntrl+ENTER" send StrdTxt call
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    CWsndengn.LdMsg(StrdTxt, 20);
    return;
  }
  else if ((keyVal == 0x99))
  { // "shift+ENTER" send both calls (StrdTxt & MyCall)
    if (CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
      CWsndengn.AddNewChar(&SpcChr);
    // char buf[20]="";
    sprintf(Title, "%s DE %s", StrdTxt, DFault.MyCall);
    CWsndengn.LdMsg(Title, 20);
    return;
  }
  // else if ((keyVal ==  0xA1))
  // { /* special test for Left ctr+'g'
  //   reduce Goertzel gain*/
  //   Grtzl_Gain = Grtzl_Gain/2;
  //   if (Grtzl_Gain < 0.00390625) Grtzl_Gain  = 0.00390625;
  //   DFault.Grtzl_Gain = Grtzl_Gain;
  //   return;
  // }
  // else if ((keyVal ==  0xA2))
  // { /* special test for Right ctr+'g'
  //   increase Goertzel gain*/
  //   Grtzl_Gain = 2* Grtzl_Gain;
  //   if (Grtzl_Gain > 1.0) Grtzl_Gain  = 1.0;
  //   DFault.Grtzl_Gain = Grtzl_Gain;
  //   return;
  // }
  if ((keyVal >= 97) & (keyVal <= 122))
  {
    keyVal = keyVal - 32;
  }
  char Ltr2Bsent = (char)keyVal;
  switch (Ltr2Bsent)
  {
  case '=':
    addspce = true; //<BT>
    break;
  case '+':
    addspce = true; //<KN>
    break;
  case '%':
    addspce = true;
    ; //<SK>
    break;
  case '>':
    addspce = true; //<BT>
    break;
  case '<':
    addspce = true; //<BT>
    break;
  }
  if (addspce && CWsndengn.IsActv() && !CWsndengn.LstNtrySpc())
    CWsndengn.AddNewChar(&SpcChr);
  CWsndengn.AddNewChar(&Ltr2Bsent);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SettingsLoop(void)
{
  int paramPtr = 0;
  int paramCnt = 9;
  bool FocusChngd = false;
  int oldparamPtr = 0;
  lvglmsgbx.HiLite_Seltcd_Setting(paramPtr, oldparamPtr); 
  while (setupFlg)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
    // printf("while(setupFlg)\n");
    uint8_t key = bt_keyboard.wait_for_ascii_char(false);
    if (key == 0x9C) //= "crtl+s"
    {
      /*user wants to exit setting screen
      So 1st pass the current sttetings entries back to their respective DFault parameters*/
      lvglmsgbx.syncDfltwSettings();
      CWsndengn.SetWPM(DFault.WPM);//syncDfltwSettings could have updated/changed the WPM setting so we need to make sure the send engine has the latest
      setupFlg = false;
    }
    else if (key == 0x98)
    { // Arrow UP
      key = 0;
      //NtryBoxGrp[paramPtr].KillCsr();
      oldparamPtr = paramPtr;
      paramPtr--;
      FocusChngd = true;
    }
    else if (key == 0x97)
    { // Arrow DOWN
      key = 0;
      //NtryBoxGrp[paramPtr].KillCsr();
      oldparamPtr = paramPtr;
      paramPtr++;
      FocusChngd = true;
    }
    if (paramPtr == paramCnt)
      paramPtr = 0;
    if (paramPtr < 0)
      paramPtr = paramCnt - 1;
    if(FocusChngd)
    {
      lvglmsgbx.HiLite_Seltcd_Setting(paramPtr, oldparamPtr);
      FocusChngd = false;
    }  
    else if (key != 0)
    {
      lvglmsgbx.KBentry(key, paramPtr);
    }
    if (!setupFlg)
    {
      lvglmsgbx.Exit_Settings(paramPtr);
      lvglmsgbx.ReStrtMainScrn();
    }
  }
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* the following code handles the read from & write to NVS processes needed to handle the User CW prefences/settings  */

uint8_t Read_NVS_Str(const char *key, char *value)
/* read string data from NVS*/
{
  uint8_t stat = 0;
  // const uint16_t* p = (const uint16_t*)(const void*)&value;
  //  Handle will automatically close when going out of scope or when it's reset.
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &ret);
  if (ret != ESP_OK)
  {
    sprintf(Title, "Error (%s) opening READ NVS string handle for: %s!\n", esp_err_to_name(ret), key);
    lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
  }
  else
  {
    size_t required_size;
    ret = handle->get_item_size(nvs::ItemType::SZ, key, required_size);
    switch (ret)
    {
    case ESP_OK:

      if (required_size > 0 && required_size < 100)
      {
        char temp[100];
        ret = handle->get_string(key, temp, required_size);
        int i;
        for (i = 0; i < 100; i++)
        {
          value[i] = temp[i];
          if (temp[i] == 0)
            break;
        }
        if (DFault.DeBug)
        {
          sprintf(Title, "%d characters copied to %s\n", i, key);
          lvglmsgbx.dispDeCdrTxt(Title, TFT_BLUE);
        }
        stat = 1;
      }
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      // sprintf(Title, "%s: has not been initialized yet!\n", key);
      // lvglmsgbx.dispMsg(Title, TFT_RED);
      break;
    default:
      sprintf(Title, "Error (%s) reading %s!\n", esp_err_to_name(ret), key);
      lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      //delay(3000);
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
  return stat;
}
/////////////////////////////////////////////////////////////
template <class T>
uint8_t Read_NVS_Val(const char *key, T &value)
/* read numeric data from NVS*/
{
  uint8_t stat = 0;
  // const uint16_t* p = (const uint16_t*)(const void*)&value;
  //  Handle will automatically close when going out of scope or when it's reset.
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &ret);
  if (ret != ESP_OK)
  {
    sprintf(Title, "Error (%s) opening READ NVS value handle for: %s!\n", esp_err_to_name(ret), key);
    lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
  }
  else
  {
    ret = handle->get_item(key, value);
    switch (ret)
    {
    case ESP_OK:
      stat = 1;
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      sprintf(Title, "%s value is not initialized yet!\n", key);
      lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      break;
    default:
      sprintf(Title, "Error (%s) reading %s!\n", esp_err_to_name(ret), key);
      lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      //delay(3000);
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
  return stat;
}

/*Save data to NVS memory routines*/

uint8_t Write_NVS_Str(const char *key, char *value)
/* write string data to NVS */
{
  uint8_t stat = 0;
  //  Handle will automatically close when going out of scope or when it's reset.
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &ret);
  if (ret != ESP_OK)
  {
    // printf("Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
    sprintf(Title, "Error (%s) opening WRITE NVS string handle for: %s!\n", esp_err_to_name(ret), key);
    lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
  }
  else
  {

    ret = handle->set_string(key, value);
    switch (ret)
    {
    case ESP_OK:
      /* write operation worked; go ahead and /lock the data in */
      ret = handle->commit();
      if (ret != ESP_OK)
      {
        sprintf(Title, "Commit Failed (%s) on %s!\n", esp_err_to_name(ret), key);
        lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      }
      else
      {
        /* exit point when everything works as it should */
        // sprintf(Title, "%s string saved %d characters\n", key, sizeof(&value));
        // lvglmsgbx.dispMsg(Title, TFT_GREEN);
        // delay(3000);
        stat = 1;
        break;
      }
    default:
      sprintf(Title, "Error (%s) reading %s!\n", esp_err_to_name(ret), key);
      lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      //delay(3000);
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
  return stat;
}
/////////////////////////////////////////////////////////////

uint8_t Write_NVS_Val(const char *key, int value)
/* write numeric data to NVS; If the data is "stored",return with an exit status of "1" */
{
  uint8_t stat = 0;
  // const uint16_t* p = (const uint16_t*)(const void*)&value;
  //  Handle will automatically close when going out of scope or when it's reset.
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &ret);
  if (ret != ESP_OK)
  {
    sprintf(Title, "Error (%s) opening WRITE NVS value handle for: %s!\n", esp_err_to_name(ret), key);
    lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
  }
  else
  {
    ret = handle->set_item(key, value);
    switch (ret)
    {
    case ESP_OK:
      ret = handle->commit();
      if (ret != ESP_OK)
      {
        sprintf(Title, "Commit Failed (%s) on %s!\n", esp_err_to_name(ret), key);
        lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      }
      else
        /* exit point when everything works as it should */
        stat = 1;
      break;
    default:
      sprintf(Title, "Error (%s) reading %s!\n", esp_err_to_name(ret), key);
      lvglmsgbx.dispDeCdrTxt(Title, TFT_RED);
      //delay(3000);
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
  return stat;
}


