/*
 * Goertzel.c
 *
 *  Created on: May 15, 2023
 *      Author: jim (KW4KD)
 */
/*20230729 added calls to DcodeCW SetLtrBrk() & chkChrCmplt() to ensure that the letter break gets refreshed with each ADC update (i.e., every 4ms)*/
/*20240101 Revamped Goertzel code for 8ms sample groups but updating every 4ms
 * also included changes to passing Keystate & letterbreak timing to DeCodeCW.cpp
 * plus changes to keystate detection (curnois lvl)
 */
/*20240103 added void CurMdStng(int MdStng) primarily to switch in extended symbol set timing while in Bg1 mode*/
/*20240114 added small differential term to ToneThresHold to improve noise tracking with widely spaced characters
			Added timing link to AdvPaser to imporve FltrPrd timing (glitch protection/rejection)*/
/*20241221 new dynamic adjustment/correction to squelch/tonethreshold during keydown interval*/
/*20250107 More tweaks to squelch/curNois/ToneThresHold to make it more responsive to changing signal conditions*/
/*20250110 Changed method of passing key state from Goertzel to CW Decoder (DcodeCW.cpp), Now using a task & Queues*/
/*20250115 Tweaks to squelch/curNois/ToneThresHold to improve weak signal tone detection */
/*20250120 another tweak to code to set ToneThresHold mainly intended to improve ingnoring white noise*/
/*20250122 added tweak to code to better handle low noise signals & at low levels*/
/*20250123 reworked, yet again , how to manage tonedetect threshold level for both noisy & quiet conditions*/
/*20250126 more tweaks to threshold setpoint code */
/*20250203 Moved S/N log calc to LVGLMsgBox.pp dispMsg2()*/
/*20250203 Changed method for selecting S/N KeyDwn & KeyUp sample values */
/*20250217 Changed Buffer size from 6 to 3 - Found the extended delay was no longer needed*/
/*20250217 Reworked S/N capture and changed Avgnoise code for white set value to improve false key down detection*/
/*20250219 Reworked AvgNoise, S2N(signal to Noise ratio [in Db]) & ToneThresHold */
#include <stdio.h>
#include <math.h>
#include "Goertzel.h"
#include "DcodeCW.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#define MagBSz 3 //6 // 3
AdvParser advparser;
LVGLMsgBox *ptrmsgbx1;
uint16_t adc_buf[Goertzel_SAMPLE_CNT];
uint8_t LongSmplFlg = 1; // controls high speed sampling VS High Sensitivity ;can not set the value here
uint8_t NuMagData = 0x0;
uint8_t delayLine;

uint8_t LEDRED = 0;
// uint8_t LEDGREEN = 0;
int LEDGREEN = 0;
uint8_t LEDBLUE = 0;
uint8_t LightLvl = 0;
uint8_t LstLightLvl = 0;
uint8_t OLDstate = 1;
bool Ready = true;
bool toneDetect = false;
bool SlwFlg = false;
bool TmpSlwFlg = false;
bool NoisFlg = false; // In ESP32 world NoisFlg is not used & is always false
bool AutoTune = true; // false; //true;
bool Scaning = false;

int ModeVal = 0;
int N = 0; // Block size sample 6 cycles of a 750Hz tone; ~8ms interval
int NL = Goertzel_SAMPLE_CNT / 2;
int NC = 0;
int NH = 0;
int RfshFlg = 0; // used when continuous tone testing is
// int BIAS = 2040;// not used in ESP32 version; bias value set in main.cpp->GoertzelHandler(void *param)
int TonSig = 0;
int MBpntr = 0;
// int SkipCnt =0;
int SkipCnt1 = 0;
int KeyState = 0;	 // used for plotting to show Key "UP" or "DOWN" state
int OldKeyState = 0; // used as a comparitor to test/detect change in keystate
// float feqlratio = 0.97;//BlackPill Ratio////0.958639092;//0.979319546;
// float feqhratio = 1.02;//BlackPill Ratio//1.044029352;//1.022014676;
float feqlratio = 0.95; // ESP32 Ratio////0.958639092;//0.979319546;
float feqhratio = 1.06; // ESP32 Ratio//1.044029352;//1.022014676;
// unsigned long now = 0;
unsigned long NoisePrd = 0;
unsigned long EvntTime = 0;
unsigned long StrtKeyDwn = 0;
unsigned long TmpEvntTime = 0;
unsigned long OldTmpEvntTime = 0;
unsigned long OldCycleTime = 0;
int setcnt = 0;
float NFlrBase = 0;
float avgKeyDwn = 1200 / 15;
float curNois = 0;
float OldcurNois = 0;
int NFlrRatio = 0;
uint8_t GlthCnt = 0;
bool StrngSigFLg = false; // used as part of the glitch detection process

uint8_t Sentstate = 1;
bool GltchFlg = false;
volatile unsigned long noSigStrt; // this is primarily used in the DcodeCW.cpp task; but declared here as part of a external/global declaration
volatile unsigned long wordBrk;	  // this is primarily used in the DcodeCW.cpp task; but declared here as part of a external/global declaration
float TARGET_FREQUENCYC = 750;	  // Hz// For ESP32 version moved declaration to DcodeCW.h
float TARGET_FREQUENCYL;		  // = feqlratio*TARGET_FREQUENCYC;//734.0; //Hz
float TARGET_FREQUENCYH;		  // = feqhratio*TARGET_FREQUENCYC;//766.0; //Hz
float SAMPLING_RATE = 83333;	  // see 'main.cpp' #include "soc/soc_caps.h"
// float SAMPLING_RATE =79600;
float Q1;
float Q2;
float Q1H;
float Q2H;
float Q1C;
float Q2C;
float PhazAng;
float coeff;
float coeffL;
float coeffC;
float coeffH;
float magA = 0;
float magB = 0;
float magC = 0;
/*used for continuous tone autotune processing */
bool CTT = false; // set to true when doing continuous tone diagnostic testing
float magAavg = 0;
float magBavg = 0;
float magCavg = 0;
int MagavgCnt = 50;
int avgCntr = 0;

float magC2 = 0;
float magL = 0;
float magH = 0;
float CurLvl = 0;
float NSR = 0;
int NowLvl = 0;
float OldLvl = 0;
float AdjSqlch = 0;
float OldAvg = 0;
float AvgLo = 0;
float AvgHi = 0;
float AvgVal = 0;
float DimFctr = 1.2;
float TSF = 1.2; // TSF = Tone Scale Factor; used as part of calc to set/determine Tone Signal to Noise ratio; Tone Component
// float NSF = 1.4;//1.6;//1.24; //1.84; //1.55; //2.30;//2.85;//0.64; //NSF = Noise Scale Factor; used as part of calc to set determine Tone Signal to Noise ratio; Noise Component
float NSF = 1.7;		// 1.8;//2.2;//1.6;
float ClipLvlF = 18000; // based on 20230811 configuration//100000;//150000;//1500000;
float ClipLvlS = 25000; // based on 20230811 configuration

float NoiseFlr = 0;
// float SigDect = 0;//introduced primarily for Detecting Hi-speed CW keydown events
float OLDNoiseFlr = 0;
float SigSlope = 1;
float OldNowLvl = 0;
float SigPk = 0;
float OldSigPk = 0;
int prntcnt = 0;
float MagBuf[MagBSz];
float NoisBuf[2 * MagBSz];
unsigned long EvntTimeBuf[MagBSz];
#define OldLvlBufSize 7
int OldLvlBufptr =0;
float OldLvlBuf[OldLvlBufSize];
int NoisPtr = 0;
float ToneThresHold = 0;
int ClimCnt = 0;
int KeyDwnCnt = 0;
int GData[Goertzel_SAMPLE_CNT];
bool prntFlg; // used for debugging
bool SndS_N = false;
bool SndS_N2 = false;
uint8_t KeyUpSmplCnt = 0;
int CapturdSN = 0;
////////////////////////////////////////
void CurMdStng(int MdStng)
{
	ModeVal = MdStng;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
void PlotIfNeed2(void)
{
	if (NuMagData)
	{
		// if (TonPltFlg && NuMagData) {
		//  int PltGudSig = -3000;
		//  if(GudSig) PltGudSig = -2200;
		int NFkeystate = -100;
		if (Sentstate == 1)
			NFkeystate = -1700; // KeyUp
		// if(GltchFlg) PltGudSig = -10000;
		NuMagData = 0x0;
		// int scaledNSR = (int)(NSR*1000000);
		//  int scaledNSR = 0;
		//  if(NSR>0) scaledNSR = (int)(1000.00/NSR);
		char PlotTxt[200];
		/*Generic plot values*/
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)magH, (int)magL, (int)magC, (int)AdjSqlch, (int)NoiseFlr, KeyState, (int)AvgNoise);//good for continuous tone testing
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\n", (int)magC, (int)SqlchLvl, (int)NoiseFlr, KeyState, (int)AvgNoise, PltGudSig);
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)CurLvl, (int)SqlchLvl, (int)NoiseFlr, KeyState, (int)AvgNoise, (int)AdjSqlch-500, NFkeystate, PltGudSig);//standard plot display
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)ToneThresHold, (int)CurLvl, (int)SigDect, KeyState, NFkeystate, (int)AvgNoise, (int)AdjSqlch-90, ClimCnt);

		sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)ToneThresHold, (int)CurLvl, (int)NoiseFlr, KeyState, NFkeystate, (int)AvgNoise, (int)curNois); // ltrCmplt//standard plot display
		
		//sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)ToneThresHold, (int)CurLvl, (int)NoiseFlr, (int)CapturdSN, NFkeystate, (int)AvgNoise, (int)curNois);
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)CurLvl, (int)NFlrBase, NFlrRatio, (int)NoiseFlr, KeyState, (int)AvgNoise, (int)AdjSqlch-500, NFkeystate);
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\n", (int)now, (int)NoisePrd, (int)avgDit, keystate);
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\n", (int)CurLvl, (int)SqlchLvl, (int)NoiseFlr, KeyState, (int)AvgNoise, (int)TARGET_FREQUENCYC);
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\n", (int)magH, (int)magL, (int)magC), (int)0;
		/*Uncomment the following to Study RGB LED tone Light*/
		// sprintf(PlotTxt, "%d\t%d\t%d\t%d\t%d\t%d\n", (int)LightLvl, (int)LEDGREEN, (int)LEDBLUE, (int)LEDRED, (int)NowLvl, (int)ClipLvl);
		// sprintf(PlotTxt, "%d\t%d\n", (int)avgDit, (int)avgKeyDwn);
		printf(PlotTxt);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
void ResetGoertzel(void)
{
	Q2 = 0;
	Q1 = 0;
	Q2C = 0;
	Q1C = 0;
	Q2H = 0;
	Q1H = 0;
	AvgVal = 0;
	TARGET_FREQUENCYL = feqlratio * TARGET_FREQUENCYC; // 734.0; //Hz
	TARGET_FREQUENCYH = feqhratio * TARGET_FREQUENCYC; // 766.0; //Hz
}

/* Call this once, to precompute the constants. */
void InitGoertzel(void)
{
	int CYCLE_CNT; //, k;// 6.0
	float omega;
	float CyRadians;
	// if(NuSmplRt !=0) SAMPLING_RATE = (float)NuSmplRt;
	ResetGoertzel(); // make sure you're working with the current set of frequeincies needed for this set of parameters
	/* For the lowest frequency of interest, Find the Maximum Number of whole Cycles we can look at  */
	if (SlwFlg)
		CYCLE_CNT = (int)((((float)(2 * Goertzel_SAMPLE_CNT)) * TARGET_FREQUENCYL) / SAMPLING_RATE);
	else
		CYCLE_CNT = (int)((((float)(Goertzel_SAMPLE_CNT)) * TARGET_FREQUENCYL) / SAMPLING_RATE);
	// floatnumSamples = (float) (Goertzel_SAMPLE_CNT);
	// k = (int) (0.5 + ((floatnumSamples * TARGET_FREQUENCYL) / (float)SAMPLING_RATE));
	CyRadians = (2.0 * M_PI * CYCLE_CNT);
	// omega = (2.0 * PI * k) / floatnumSamples;
	NL = (int)(0.5 + ((SAMPLING_RATE / TARGET_FREQUENCYL) * (float)(CYCLE_CNT)));
	omega = CyRadians / (float)NL;
	coeffL = 2 * cos(omega);

	NC = (int)(0.5 + ((SAMPLING_RATE / TARGET_FREQUENCYC) * (float)(CYCLE_CNT)));
	// k = (int) (0.5 + ((floatnumSamples * TARGET_FREQUENCYC) / (float)SAMPLING_RATE));
	// CyRadians = (2.0 * PI * CYCLE_CNT);
	// omega = (2.0 * PI * k) / floatnumSamples;
	omega = CyRadians / (float)NC;
	coeffC = 2 * cos(omega);

	NH = (int)(0.5 + ((SAMPLING_RATE / TARGET_FREQUENCYH) * (float)(CYCLE_CNT)));
	// k = (int) (0.5 + ((floatnumSamples * TARGET_FREQUENCYH) / (float)SAMPLING_RATE));
	// CyRadians = (2.0 * PI * CYCLE_CNT);
	// omega = (2.0 * PI * k) / floatnumSamples;
	omega = CyRadians / (float)NH;
	coeffH = 2 * cos(omega);

	/* uncomment for Debug/Diagnostic testing*/
	// char buf[50];
	// sprintf(buf, "CYCLE_CNT: %d; SmplCnt: %d; Frq: %4.1f; Coef: %3.3f\n", CYCLE_CNT, NC, TARGET_FREQUENCYC, coeffC );
	// printf( buf);
}

/* Call this routine for every sample. */
void ProcessSample(int sample, int Scnt)
{
	char Smpl[10];
	if (Scnt >= Goertzel_SAMPLE_CNT)
		GData[Scnt - Goertzel_SAMPLE_CNT] = sample;
	// if(Scnt ==0 ) prntFlg = true; //just used for debugging
	if (Scnt > NL)
	{
		// just used for debugging
		//  if(prntFlg){
		//  	prntFlg = false;
		//  	sprintf( Smpl,"%d\n", Scnt);
		//  	printf( Smpl);
		//  }
		return; // don't look or care about anything beyond the lowest number of samples needed for this frequency set
	}
	float Q0;
	float FltSampl = (float)sample;

	Q0 = (coeffL * Q1) - Q2 + FltSampl;
	Q2 = Q1;
	Q1 = Q0;
	if (Scnt > NC)
	{
		return;
	}
	Q0 = (coeffC * Q1C) - Q2C + FltSampl;
	Q2C = Q1C;
	Q1C = Q0;

	if (Scnt > NH)
		return;
	Q0 = (coeffH * Q1H) - Q2H + FltSampl;
	Q2H = Q1H;
	Q1H = Q0;
}

/* Optimized Goertzel */
/* Call this after every block to get the RELATIVE magnitude squared. */
float GetMagnitudeSquared(float q1, float q2, float Coeff, int SmplCnt)
{
	float result;
	float SclFctr = (float)SmplCnt / 2.0;
	if (SlwFlg)
		SclFctr = 2 * SclFctr;
	// float CyRadians = (2.0 * M_PI * CYCLE_CNT);
	// float omega = CyRadians / (float)floatN;
	// float cosine = cos(omega);
	result = ((q1 * q1) + (q2 * q2) - (q1 * q2 * Coeff)) / SclFctr;
	// float real = (q1 - q2 * cosine);
	// float imag = (q2 * sine);
	// result = (real * real) + (imag * imag);
	// PhazAng = 360 * ((atan(real / imag)) / (2 * M_PI));
	return result;
}
/*Main entry point calc next Goertezl sample set*/
void ComputeMags(unsigned long now)
{
	TmpEvntTime = now;
	// magC = Grtzl_Gain * 4.0*sqrt(GetMagnitudeSquared(Q1C, Q2C, coeffC, NC));
	// magH = Grtzl_Gain * 4.0*sqrt(GetMagnitudeSquared(Q1H, Q2H, coeffH, NH));
	// magL = Grtzl_Gain * 4.0*sqrt(GetMagnitudeSquared(Q1, Q2, coeffL, NL));
	magC = Grtzl_Gain * 10.0 * sqrt(GetMagnitudeSquared(Q1C, Q2C, coeffC, NC));
	magH = Grtzl_Gain * 10.0 * sqrt(GetMagnitudeSquared(Q1H, Q2H, coeffH, NH));
	magL = Grtzl_Gain * 10.0 * sqrt(GetMagnitudeSquared(Q1, Q2, coeffL, NL));
	/*Added the following to preload the current sample set to be combined with next incoming data set*/
	if (SlwFlg)
	{
		// printf( "KK");
		ResetGoertzel();
		for (int i = 0; i < Goertzel_SAMPLE_CNT; i++)
		{
			ProcessSample(GData[i], i);
		}
	}
	/*End of preload process */
	CurLvl = (magC + magL + magH) / 3;
	// if (CurLvl < 50)
	// 	CurLvl = NowLvl; // something went wrong use last datapoint
	NowLvl = CurLvl;	 //'NowLvl will be used later for showing current LED state
	/* ESP32 Plot code to do a simple look at the sampling & conversion process */
	// char buf[20];
	// sprintf(buf, "%d, %d, %d, %d, \n", (int)magH, (int)magL, (int)magC, 0);
	// printf( buf);
/*Now calc S/N using buffered keydwn Mag value & current mag value 
(which should be one of the 1st samples taken after the keyup detection)*/
//CapturdSN = (int)OldLvlBuf[OldLvlBufptr];
//printf("%d, %8.1f\n", CapturdSN, (float)NowLvl);//%8.1f
	if (SndS_N) KeyUpSmplCnt ++;
	if (SndS_N && KeyUpSmplCnt ==2)
	{
		SndS_N = false;
		KeyUpSmplCnt = 0;
		//float S2N = OldLvlBuf[3] / NowLvl;
		float S2N = OldLvlBuf[OldLvlBufptr] / (float)NowLvl;
		//CapturdSN = (int)OldLvlBuf[OldLvlBufptr];
		/*Uncomment For testing only */
		// int prntptr = OldLvlBufptr;
		// for(int i = 0; i< OldLvlBufSize; i++ )
		// {
		// 	if(prntptr+i == OldLvlBufSize) prntptr -= OldLvlBufSize; 
		// 	printf("%d, ", (uint16_t)OldLvlBuf[prntptr+i]);
		// } 
		// printf("%d\n", (uint16_t)NowLvl);

		if (xQueueSend(ToneSN_que, &S2N, pdMS_TO_TICKS(2)) == pdFALSE)//used byLVGLMsgBox to drive the S/N shown on the Display
		{
			printf("Failed to push 'S2N' to 'ToneSN_que' \n");
		}
		if (xQueueSend(ToneSN_que2, &S2N, pdMS_TO_TICKS(2)) == pdFALSE)// Used ultimately by the Advanced Post Parser to seperate good key timing from questionable entries
		{
			printf("Failed to push 'S2N' to 'ToneSN_que2' \n");
			float dummy;
			int IndxPtr = 0;
			while (xQueueReceive(ToneSN_que2, (void *)&dummy, pdMS_TO_TICKS(3)) == pdTRUE)
			{
				IndxPtr++;
			}
		}
		//printf("ToneSN_que2, &S2N: %5.1f; now:%d\n", S2N, (uint16_t)now);
	}
	OldLvlBuf[OldLvlBufptr] = NowLvl;
	OldLvlBufptr++;
	if(OldLvlBufptr == OldLvlBufSize) OldLvlBufptr = 0;


	MagBuf[MBpntr] = CurLvl;
	EvntTimeBuf[MBpntr]  = now;
	MBpntr++;
	if (MBpntr == MagBSz)
		MBpntr = 0;
		
	// int OldestDataPt = MBpntr;//get the oldest sample in the data buffer
	OLDNoiseFlr = NoiseFlr; // save NoiseFlr found last period
	/*perpare/initializ NoiseFlr & SigPk to determine their values for the current period*/
	NoiseFlr = MagBuf[0];
	SigPk = MagBuf[0];

	for (int i = 0; i < MagBSz; i++)
	{
		if (MagBuf[i] < NoiseFlr)
			NoiseFlr = MagBuf[i];
		if (MagBuf[i] > SigPk)
			SigPk = MagBuf[i];
	}
	bool NoisUp = true;
	curNois = (SigPk - NoiseFlr);
	//curNois *= 2;
	if (OldcurNois > curNois)
		NoisUp = false;

	float curpk = 1.7*curNois;
	if(NowLvl>curpk) curpk = NowLvl;	
	if(ToneThresHold <  curpk && (NowLvl>curNois))
	{
		ToneThresHold = ((5*ToneThresHold) + (curpk))/6;
	} 
	else ToneThresHold = ((45*ToneThresHold) + (0.9*ToneThresHold))/46;
	/*20250123 the next line was added to stop overshoot of the curNois (threshold point/value) for loud high speed Signals*/
	// if (curNois > 90000)
	// 	curNois = 90000;
	// if (curNois > 30000)
	// {
	// 	if (NoisUp)
	// 	{
	// 		curNois *= 0.6;
	// 		if (curNois < 30000)
	// 			curNois = 30000;
	// 		ToneThresHold = ((19 * ToneThresHold) + (curNois)) / 20;
	// 	}
	// 	else
	// 	{
	// 		ToneThresHold = ((59 * ToneThresHold) + (OldcurNois)) / 60;
	// 	}
	// }
	// else
	// {
	// 	if (NoisUp)
	// 	{
	// 		//curNois *= 1.7;
	// 		if (curNois > 40000)
	// 			curNois = 40000;
			 
	// 		/*this establishes the tone detect level during white noise conditions
	// 		and tends to keep it at, or above, the noise peaks*/
	// 		//ToneThresHold = ((4*ToneThresHold) + (8*curNois))/5;
	// 		ToneThresHold = ((45*ToneThresHold) + (8*curNois))/46;
	// 	}
	// 	else
	// 	{
	// 		//ToneThresHold = ((19 * ToneThresHold) + (6*OldcurNois)) / 20;
	// 		ToneThresHold = ((69*ToneThresHold) + (6*curNois))/90;
	// 	}
	// }
	/*now to aviod false key dtection, set minimum noise value*/
	if (curNois< 1500) curNois = 1500;
	OldcurNois = curNois;
	
	/*Now look to see if the noise is increasing
	And if it is, Add a differential term/componet to the ToneThresHold*/
	int LstNLvlIndx = NoisPtr - 1;
	if (LstNLvlIndx < 0)
		LstNLvlIndx = 2 * MagBSz - 1;
	
	/*save this noise value to histrory ring buffer for later comparison */
	NoisPtr++;
	if (NoisPtr == 2 * MagBSz)
		NoisPtr = 0;
	NoisBuf[NoisPtr] = ToneThresHold;

	if (NoiseFlr > NFlrBase)
	{
		NFlrRatio = (int)(1000.0 * NoiseFlr / NFlrBase);
		if (NFlrRatio > 30000)
			NFlrRatio = 30000;
		NFlrBase = ((399 * NFlrBase) + NoiseFlr) / 400;
	}
	else
		NFlrBase = ((19 * NFlrBase) + NoiseFlr) / 20;
	NSR = NoiseFlr / SigPk;
	
	if (CurLvl < 0)
		CurLvl = 0;
	magB = ((magB) + CurLvl) / 2; //((2*magB)+CurLvl)/3; //(magC + magL + magH)/3; //
	/* try to establish what the long term noise floor is */
	/* This sets the squelch point with/when only white noise is applied*/
	if (!toneDetect)
	{
		AvgNoise = ((99 * AvgNoise) + (1.4 * SigPk)) / 100;
	}
	else
	{
		prntcnt = 0;
	}
	/* If we're in a key-down state, and the Noise floor just Jumped up, as a reflection,
	 * of this state, give up the gain in "curnoise" that happened due to the time lag in
	 * noisefloor */
	/* the following should never happen, but if "true" reset noise level to something realistic */
	if (AvgNoise < 0)
		AvgNoise = (SqlchLvl);
	/* Now try to establish "key down" & "key up" average levels */
	if ((NSF * NoiseFlr) > SqlchLvl)
	{
		/* Key Down bucket */
		AvgHi = ((5 * AvgHi) + (1.2 * magB)) / 6; //((3*AvgHi)+magC)/4;
	}
	else
	{
		/* Key Up bucket, & bleed off some of the key down level; but never let it go below the current average low*/
		AvgLo = ((49 * AvgLo) + magB) / 50;
		if (AvgHi > AvgLo)
			AvgHi -= 0.0015 * AvgHi;
		else if (AvgHi < AvgLo)
			AvgHi = AvgLo;
	}
	/* use the two Hi & Lo buckets to establish the current mid point */
	// float Avg = ((AvgHi-AvgLo)/40)+(AvgLo);
	float Avg = ((AvgHi - AvgLo) / 2) + (AvgLo);
	/* add this current mid point to the running average mid point */
	SqlchLvl = ((25 * SqlchLvl) + Avg) / 26;
	if (OldLvl > CurLvl)
	{
		SqlchLvl -= 0.2 * (OldLvl - CurLvl); //-= 0.5*(OldLvl-CurLvl);
	}
	if (SqlchLvl < 0)
		SqlchLvl = 0;
	OldLvl = CurLvl;
	/* Now based on the current key down level see if a differential correction needs to be added into the average */
	/* Finally, make sure the running average is never less than the current noise floor */
	/* removed the following line for esp32; To give a more consistant noise floor to gauge where squelch action needs to take place*/

	if (SqlchLvl <= AvgNoise)
	{
		/*Test for a decent tone by looking at the current noise floor value, & if true, correct AvgNoise (squelch level) value to be below the noisefloor level*/
		// if((NoiseFlr> 0.75*SigPk)&&(AvgNoise>4000) && (NoiseFlr>60000)){
		if ((NoiseFlr > 0.75 * SigPk) && (AvgNoise > 4000) && (NoiseFlr > 30000))
		{
			GudSig = 1;
			SkipCnt1 = 0;
		}

		AdjSqlch = AvgNoise;
		if (SlwFlg)
			AdjSqlch = 1.20 * AdjSqlch;
	}
	else
	{
		AdjSqlch = SqlchLvl;
		GudSig = 1;
		SkipCnt1 = 0;
	}

	/*This 'if' stops the Blue plot line from going below the Grey line*/
	if (ToneThresHold < AvgNoise)
		ToneThresHold = AvgNoise; // 20231022 added this to lessen the chance that noise might induce a false keydown at the 1st of a letter/character
	AdjSqlch = ToneThresHold;
	if (AdjSqlch < 1500)
		AdjSqlch = 1500; // 20230814 make sure squelch lvl is always above the "no input signal" level; this stops the decoder from generating random characters when no signal is applied
	if ((NoiseFlr > AvgNoise) && ((NoiseFlr / SigPk) > 0.65))
	{
		GudSig = 1;
		SkipCnt1 = 0;
	}
	if ((NoiseFlr < AvgNoise) && ((SigPk / NoiseFlr) > 5.0))
	{
		SkipCnt1++;
		/* For ESP32,extended countout to 20 to improve WPM speed detection/correction*/
		if (SkipCnt1 > 7)
		{
			SkipCnt1 = 7;
			GudSig = 0;
		}
	}
	Ready = true;
	NuMagData = 0x1;
	/* The Following (Commented out) code will generate the CW code for "5" with a Calibrated 31ms "keydown" period */
	//	next1++;
	//	if(next1==2){
	//		next1 =0;
	//		next++;
	//		if(next<20){
	//			if(next % 2 == 0){
	//				toggle1 ^= 1; //exclusive OR
	//				if(toggle1) HAL_GPIO_WritePin(DMA_Evnt_GPIO_Port, DMA_Evnt_Pin, GPIO_PIN_RESET);//PIN_HIGH(LED_GPIO_Port, LED_Pin);
	//				else HAL_GPIO_WritePin(DMA_Evnt_GPIO_Port, DMA_Evnt_Pin, GPIO_PIN_SET);//PIN_LOW(LED_GPIO_Port, LED_Pin);
	//			}
	//		}else if(next>40) next = 0;
	//		else{
	//			HAL_GPIO_WritePin(DMA_Evnt_GPIO_Port, DMA_Evnt_Pin, GPIO_PIN_SET);
	//			toggle1 = 0;
	//		}
	//	}
	/* END Calibrated TEST Code generation; Note when using the above code the Ckk4KeyDwn routine should be commented out */
	

	Chk4KeyDwn(NowLvl);
}
////////////////////////////////////////////////////////////////////////////
/*Added 'NowLvl' to better sync the LED with the incoming tone */
void Chk4KeyDwn(float NowLvl)
{
	/* to get a Keydown condition "toneDetect" must be set to "true" */
	// float ToneLvl = magB; // tone level delayed by six samples
	bool GudTone = true;
	unsigned long FltrPrd = 0;
	// printf("Chk4KeyDwn\n");
	if (avgDit <= 1200 / 35) // WPM is the denominator here
	{						 /*High speed code keying process*/
		if (CurLvl > ToneThresHold)
		{
			if ((OLDNoiseFlr < NoiseFlr) && (OLDNoiseFlr < AvgNoise))
			{
				float NuNoise = 0.5 * (NoiseFlr - OLDNoiseFlr) + OLDNoiseFlr;
				if (NuNoise < NoiseFlr)
					AvgNoise = (5 * AvgNoise + NuNoise) / 6;
			}
			toneDetect = true;
			if (!GudTone)
				GudTone = true;
		}
		else if (CurLvl < ToneThresHold)
		{
			toneDetect = false;
		}
	}
	else
	{												 /*slow code keying process*/
		if ((CurLvl < 0.6 * AdjSqlch) && toneDetect) // toneDetect has the value found in the previous sample (not the current sample)
		{
			GudTone = false;
			toneDetect = false;
		}
		if (!toneDetect && !Scaning) // if (((NoiseFlr > AdjSqlch)  ) && !toneDetect && !Scaning)
		{
			// if(((CurLvl  > 2*AdjSqlch)||(NoiseFlr > AdjSqlch))) toneDetect = true;
			// if((CurLvl > AdjSqlch) && SlwFlg) toneDetect = true; //used to dectect key down state while using 8ms sample interval
			if (((NoiseFlr > 2 * AdjSqlch) || (NoiseFlr > AdjSqlch)))
				toneDetect = true;
			if ((NoiseFlr > AdjSqlch) && SlwFlg)
				toneDetect = true; // used to dectect key down state while using 8ms sample interval
		}
	}
	/*initialize in KeyUp state */
	uint8_t state = 1; // Keyup, or "no tone" state
	KeyState = -2000;  // Keyup, or "no tone" state

	if (toneDetect)
	{					 /** Fast code method */
		KeyState = -400; // Key Down
		state = 0;
		/*go back & get noise lvl from the sample before the keydown signal*/
		int OldestSmpl = NoisPtr + 1;
		if (OldestSmpl == 2 * MagBSz)
			OldestSmpl = 0;
		KeyDwnCnt++;
		if (KeyDwnCnt > 4)
		{
			KeyDwnCnt--;
			ToneThresHold = AvgNoise;
		}
		else
			ToneThresHold = NoisBuf[OldestSmpl];

		OldestSmpl++;
		if (OldestSmpl == 2 * MagBSz)
			OldestSmpl = 0;
		NoisBuf[OldestSmpl] = ToneThresHold;
	}
	else
		KeyDwnCnt = 0;

	/*now if this last sample represents a keydown state(state ==0),
	Lets set the AvgNoise to be mid way between the lowest curlevel and the NFlrBase value*/
	if (!state)
	{ // KeyDown
		float tmpcurnoise;
		tmpcurnoise = ((CurLvl - NFlrBase) / 2) + NFlrBase;
		if (state != OLDstate)
		{
			AvgNoise = 0.75*tmpcurnoise; //20250219 changed from, AvgNoise = tmpcurnoise
			OLDstate = state;
			// printf("tmpcurnoise = ((CurLvl-NFlrBase)/2) + NFlrBase\n");
		}

		if (!state)
		{
			if (OldNowLvl > 0)
			{
				SigSlope = ((NowLvl - OldNowLvl) + OldNowLvl) / OldNowLvl;
				if (NowLvl < OldNowLvl)
					SigSlope *= -1.5;
				if (SigSlope < 3 && SigSlope > -6)
				{
					if (SigSlope > 0)
						AvgNoise += (SigSlope) * (0.001 * AvgNoise);
					else
						AvgNoise += (SigSlope) * (0.004 * AvgNoise);
					// printf("SigSlope %5.2f\n", SigSlope);
				}
			}
			OldNowLvl = NowLvl;
		}

		OldNowLvl = NowLvl;
	}
	else // KeyUp
		/*20250123 new approach to finding where/when to reset the threshold tone detect level
		Note: the 'or' curNois vs NowLvl test is based on the curNois dropping below the CurLvl(nowlvl)
		even in a noisy environment as a way to detect the presence of a tone */
		if ((state != OLDstate))//if (state && (state != OLDstate)) // || (curNois < NowLvl/4))//(curNois < 0.75* NowLvl))
		{ /* Just transistion from keydown to keyup */
			float tmpcurnoise = ((AvgNoise - NFlrBase) / 2) + NFlrBase;
			AvgNoise = tmpcurnoise;
			OLDstate = state;
			SndS_N = true;
		}

	/* new auto-adjusting glitch detection; based on average dit length */
	unsigned long Now2 = TmpEvntTime; // set Now2 to the current sampleset's timestamp
	/*Added to support ESP32 CW decoding process*/
	if (OldKeyState != KeyState)
	{
		if (!state)
			StrtKeyDwn = TmpEvntTime; // if !state and we're here, then we're at the start of a keydown event
		else
		{ // if we're here, we just ended a keydown event
			float thisKDprd = (float)(TmpEvntTime - StrtKeyDwn);
			//printf("thisKDprd = %5.0f\n", thisKDprd);
			if ((thisKDprd > 1200 / 60) && (thisKDprd < 1200 / 5))
			{ // test to see if this interval looks like a real morse code driven event
				if (thisKDprd > 2.5 * avgKeyDwn)
					thisKDprd /= curRatio; // looks like a dah compared to the last 50 entries, So cut this interval by the dit to dah ratio currently found in DcodeCW.cpp
				avgKeyDwn = ((49 * avgKeyDwn) + thisKDprd) / 50;
			}
		}
		// just used for debugging
		//  char Smpl[10];
		//  sprintf( Smpl,"%d\n", 1200/(int)avgDit);
		//  printf( Smpl);
		// if((avgDit > 1200 / 35)) avgDit = (unsigned long)advparser.DitIntrvlVal;//20240521 added this becauxe now believe this is a more reliable value
		// if ((avgDit < 1200 / 30)) // note: DcodeCW.cpp sets the avgDit value
		// {					   // 20231031 changed from 1200/30 to 1200/35
		// 	TmpSlwFlg = false; // we'll determine the flagstate here but wont engage it until we have a letter complete state
		// }
		// else if (avgDit > 1200 / 28)
		// {					  // 20231031 added else if()to auto swap both ways
		// 	TmpSlwFlg = true; // we'll determine the flagstate here but wont engage it until we have a letter complete state
		// }
		//GltchFlg = true;//uncomment this to lock out glitch detection.
		if (!GltchFlg && (avgDit >= 1200 / 30))
		{ // don't use "glitch" detect/correction for speeds greater than 30 wpm

			/*Some straight keys & Bug senders have unusually small dead space between their dits (and Dahs).
			When thats the case, use the DcodeCW's avgDeadspace to set the duration of the glitch period */
			// if(FltrPrd > AvgSmblDedSpc/2) FltrPrd = (unsigned long)((AvgSmblDedSpc)/3.0);
			if (AvgSmblDedSpc < advparser.AvgSmblDedSpc)
			{
				FltrPrd = (unsigned long)((AvgSmblDedSpc) / 3.0);
			}
			else
				FltrPrd = (unsigned long)((advparser.AvgSmblDedSpc) / 3.0);
			if (ModeCnt == 3)
				FltrPrd = 4; // we are running in cootie mode, so lock the glitch inerval to a fixed value of 8ms.
			NoisePrd = Now2 + FltrPrd;
			OldKeyState = KeyState;
			GltchFlg = true;
			EvntTime = TmpEvntTime; // capture/remember when this state change occured
			// printf("SET NoisePrd %d\n", (int)NoisePrd);
		}
		else
		{ //
			NoisePrd = Now2;
			OldKeyState = KeyState;
			GltchFlg = true;
			EvntTime = TmpEvntTime;
		}
	}
	if (GltchFlg && !state)
	{
		// 20231230  just got a keydown condition but its noisy, so add glitch delay to letter complete interval
		if (FltrPrd > 0 && ModeVal == 1)
			StrechLtrcmplt(1.25 * FltrPrd);
	}
	if ((Now2 <= NoisePrd) && (Sentstate == state))
	{ /*We just had a 'glitch event, load/reset advparser watchdog timer, to enable 'glitch check'*/
		// printf("\nRESET LstGltchEvnt: %d \n", (int)TmpEvntTime);
		advparser.LstGltchEvnt = 10000 + TmpEvntTime; // used by advParser.cpp to know if it needs to appply glitch detection
	}
	
	if (OldKeyState != KeyState)
	{
		/*the following never happens*/
		// printf("OldKeyState != KeyState\n");
		if (Now2 <= NoisePrd)
		{
			if (GltchFlg)
			{
				/*We had keystate change but it returned back to its earlier state before the glitch interval expired*/
				OldKeyState = KeyState;
				GltchFlg = false;
				GlthCnt++;
				if (GlthCnt >= 3)
				{
					/*3 consectutive glicth fixes in 1 keydown interval; thats too many; readj the avg keydown period */
					avgKeyDwn = avgKeyDwn / 2;
					GlthCnt = 0;
				}
			}
		}
	}
	else
	{
		if (Now2 >= NoisePrd)
		{
			if (Sentstate != state)
			{ /*We had keystate change ("tone" on, or "tone" off)  & passed the glitch interval; Go evaluate the key change*/
				GltchFlg = false;
				Sentstate = state; //'Sentstate' is what gets plotted
				OldKeyState = KeyState;
				GlthCnt = 0;
				if (1)
					GudSig = 1;
				if (state == 1)
					LEDGREEN = 0;
				else
					LEDGREEN = (int)CurLvl;
				if (Sentstate)
					chkcnt++;
				if(Sentstate)
				{   uint16_t CycleInterval = (uint16_t)(TmpEvntTime- OldCycleTime);
					if( ((CycleInterval) < 69) && OldCycleTime!=0) // incountered a keyup keydwn & back to keyup in an interval that represents >35wpm
					{
						if(setcnt>0)
						{ // sustained keying intrvals are shorter than 35 wpm
							TmpSlwFlg = false;
							// printf("\t%d. CycleInterval: %d\n", setcnt, CycleInterval);
						}
						setcnt = 6;
					} else 
					
					//SndS_N = true;
					if(setcnt>0)
					{
						setcnt--;
					}
					else TmpSlwFlg = true; // ok, we're now consistenly less than 35 wpm
					OldCycleTime = TmpEvntTime;	
				} 
				if(!Sentstate && TmpSlwFlg)	//we're sending a 'keydown' state which actually happened 6 samples back so need to correct/update the time stamp
				{
					// int tmpptr = MBpntr;
					// tmpptr--;
					// if(tmpptr<0) tmpptr = MagBSz-1;
					// tmpptr--;
					// if(tmpptr<0) tmpptr = MagBSz-1;
					// tmpptr--;
					// if(tmpptr<0) tmpptr = MagBSz-1;
					// tmpptr--;
					// if(tmpptr<0) tmpptr = MagBSz-1;
					TmpEvntTime = EvntTimeBuf[MBpntr];
	
				}
				
				if(xQueueSend(KeyEvnt_que, &TmpEvntTime, (TickType_t)0) == pdFALSE)
				{ 
					printf("!!! KeyEvnt_que FULL !!!\n");
					unsigned long dummy;
					int IndxPtr = 0;
					while (xQueueReceive(KeyEvnt_que, (void *)&dummy, pdMS_TO_TICKS(3)) == pdTRUE)
					{
						IndxPtr++;
					}
				}
				if(xQueueSend(KeyState_que, &Sentstate, (TickType_t)0) == pdFALSE)
				{ 
					printf("!!! KeyState_que !!!\n");
					uint8_t dummy;
					int IndxPtr = 0;
					while (xQueueReceive(KeyState_que, (void *)&dummy, pdMS_TO_TICKS(3)) == pdTRUE)
					{
						IndxPtr++;
					}
				}
				//printf("\tKeyEvnt_que, EvntTime: %d; State: %d\n", (uint16_t)TmpEvntTime, Sentstate);
				//if(!Sentstate) printf("\tKeyEvnt_que, &TmpEvntTime: &d\n", (uint16_t)TmpEvntTime);//testing debugging
				KeyEvntSR(Sentstate, TmpEvntTime);
			}
		}
		
	}
	if (Sentstate)
	{ // 1 = Keyup, or "no tone" state; 0 = Keydown
		if (chkChrCmplt() && SlwFlg != TmpSlwFlg)
		{ // key is up
			SlwFlg = TmpSlwFlg;
		}
		if (CalGtxlParamFlg)
		{
			CalGtxlParamFlg = false;
			CalcFrqParams((float)AvgToneFreq); // recalculate Goertzel parameters, for the newly selected target grequency
			showSpeed();
		}
	}
	else
		SetLtrBrk(); // key is down; so reset/recalculate new letter-brake clock value

	int pksigH = (int)(LEDGREEN / 1000);
	/*Push returned "state" values on the que */
	if (xQueueSend(RxSig_que, &pksigH, pdMS_TO_TICKS(2)) == pdFALSE)
	{
		// printf("Failed to push 'pksigH' to 'RxSig_que' \n");
	}
	/*the following lines support plotting tone processing to analyze "tone in" vs "key state"*/
	if (PlotFlg) // 1 ||
		PlotIfNeed2();

} /* END Chk4KeyDwn(float NowLvl) */

/* Added to support ESP32/RTOS based processing */
int ToneClr(void)
{
	return LEDGREEN;
}
// uint16_t ToneClr(void)
// {
// 	uint8_t r = LEDRED;
// 	uint8_t g = LEDGREEN;
// 	uint8_t b = LEDBLUE;
// 	uint16_t color = ((r & 0xF8) << 8) | ((g & 0xF8) << 3) | (b >> 3);
// 	return color;
// }
////////////////////////////////////////////////////////////////////////////
void ScanFreq(void)
{
	/*  this routine will start a Sweep the audio tone range by decrementing the Goertzel center frequency when a valid tome has not been heard
	 *  within 4 wordbreak intervals since the last usable tone was detected
	 *  if nothing is found at the bottom frquency (500Hz), it jumps back to the top (900 Hz) and starts over
	 */
	float DltaFreq = 0.0;

	unsigned long waitinterval = (60 * 3 * 5); // 5 * wordBrk// normal wordBrk = avgDah; use for diagnositic continuous tone testing
	float max = 900.0;
	float min = 550.0;
	if (CTT)
	{
		max = 1500.0;
		min = 150.0;
	}
	if (!CTT)
		waitinterval = 5 * wordBrk; // normal wordBrk = avgDah;

	// if(!CTT) magC *= 1.02;
	if (magC > AdjSqlch)
	{					 // We have a valid tone.
		Scaning = false; // enable the tonedetect process to allow key closer events.
		magAavg += magL;
		magBavg += magC;
		magCavg += magH;
		avgCntr++;
		if (CTT)
		{
			noSigStrt = pdTICKS_TO_MS(xTaskGetTickCount()); // included here just to assure we have recent "No Signal" Marker; needed when a steady tone is applied
			// magAavg += magL;
			// magBavg += magC;
			// magCavg += magH;
			// avgCntr++;
			if (avgCntr > MagavgCnt)
			{
				/*We've collected 50 Goertzel data points. Find their average values*/
				avgCntr = 0;
				magL = magAavg / 50;
				magC = magBavg / 50;
				magH = magCavg / 50;
				magAavg = magBavg = magCavg = 0;
			}
			else
				return; // do nothing more, we dont have a enough data points yet
		}
		else
		{ // normal cw mode; Not Continuous tone testing
			if (avgCntr > 10)
			{
				/*We've collected 10 Goertzel data points. Find their average values*/
				avgCntr = 0;
				magL = magAavg / 10;
				magC = magBavg / 10;
				magH = magCavg / 10;
				magAavg = magBavg = magCavg = 0;
			}
			else
				return; // do nothing more, we dont have a enough data points yet
		}
		// ok, now have an averaded reading

		if ((magC > magL) && (magC > magH))
		{
			if (!CTT)
				return; // the current center frequency had the best overall magnitude. So no frequency correction needed. Go and collect another set of samples
		}
		if (magH > magL)
			DltaFreq = +1.0; //+0.5;//2.5;
		else if (magL > magH)
			DltaFreq = -1.0; //-0.5;//2.5;
	}
	else
	{ // no signal detected
		if ((pdTICKS_TO_MS(xTaskGetTickCount()) - noSigStrt) > waitinterval)
		{ // no signal, after what should have been several "word" gaps. So lets make a big change in the center frequency, & see if we get a useable signal
			DltaFreq = -20.0;
			Scaning = true; // lockout the tonedetect process while we move to a new frequency; to prevent false key closer events while in the frequency hunt mode
		}
	}
	if (DltaFreq == 0.0)
		if (!CTT)
			return; // Go back; Nothing needs fixing
	TARGET_FREQUENCYC += DltaFreq;
	if (TARGET_FREQUENCYC < min)
		TARGET_FREQUENCYC = max; // start over( back to the top frequency); reached bottom of the frequency range (normal loop when no signals are detected)
	if (TARGET_FREQUENCYC > max)
		TARGET_FREQUENCYC = min; // not likely to happen. But could when making small frequency adjustments

	CalcFrqParams(TARGET_FREQUENCYC); // recalculate Goertzel parameters, for the newly selected target grequency
	if (CTT)						  //
	{								  // if we're runing in continuous tone mode, we need to periodically update the display's bottom status line. mainly to show the current frequency value the auto-tune process has selected
		RfshFlg++;
		if (RfshFlg > 2)
		{
			RfshFlg = 0;
			showSpeed();
			// printf("showSpeed\n");
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcFrqParams(float NewToneFreq)
{
	// NewToneFreq *= 0.985;
	TARGET_FREQUENCYC = NewToneFreq;			 // Hz
	TARGET_FREQUENCYL = feqlratio * NewToneFreq; // Hz
	TARGET_FREQUENCYH = feqhratio * NewToneFreq; // Hz
	InitGoertzel();
	/* TODO Display current freq, only if we are running BtnSuprt.cpp setup loop */
	// if(setupflg && !Scaning)ShwUsrParams();
}
//////////////////////////////////////////////////////////////////////////