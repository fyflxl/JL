

//#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include "system/includes.h"
#include "app_config.h"
#include "asm/timer.h"

#include "types.h"
#include "si117x_functions.h"
#include "si117x_config.h"
//#include "si117x_auth.h"
#include "ohr_drv_afe.h"
#ifdef OHRLIB_PRESENT
#include "Ohrdrive.h"
#endif
#if 0
//#define LOG_TAG             "[APP_SI1171]"
#define LOG_INFO_ENABLE
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE
#define log_info(format, ...)       PRINTF("[Info] : [APP_SI1171]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif
#endif
#define LOG_TAG             "[APP_222]"
#define LOG_INFO_ENABLE2
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE2
#define log_info(format, ...)       PRINTF("[Info] : [APP_222]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif

#define si_delay2ms(x)  delay_2ms(x)




#define SI117X_FIFO_LENGTH   (SI117X_SAMPLERATE*4/READREQUENCY)
#define SI117X_FIFO_LENGTH_HRV   (SI117X_SAMPLERATE_HRV*4/READREQUENCY)
#define RETVAL_CHECK(retv)   if (retv<0)return retv;

static 	int16_t lastfifocount = 0;
static uint8_t si1181_rev;
static bool yellow_led_enable = false;
typ_fifodata si1171_fifodata;

void setOhrcurrent(uint8_t value)
{
	if(value==0)
	{
			Si117xParamSet(PARAM_PPG1_LED1_CONFIG, 0);
			Si117xParamSet(PARAM_PPG1_LED2_CONFIG, 0);
			if(yellow_led_enable==true)Si117xParamSet(PARAM_PPG1_LED4_CONFIG, 0);
	}
  else
	{
		  if(value>55 && value<59)value = 59;
			Si117xParamSet(PARAM_PPG1_LED1_CONFIG, BIT6 + (value&0x3f));
			Si117xParamSet(PARAM_PPG1_LED2_CONFIG, BIT6 + (value&0x3f));
			if(yellow_led_enable==true)Si117xParamSet(PARAM_PPG1_LED4_CONFIG, BIT6 + (value&0x3f));
	}
}


int16_t Si117x_checkstart(void)
{
  uint8_t data1;
	log_info("si1171 initialize!\r\n");
	//while(1)
	//for(u8 i = 0;i<10;i++)
		{clr_wdt();
	data1= Si117xReadFromRegister(REG_PART_ID);
	//si_delay2ms(30);
		}

	if(data1 < 0x70 || data1 >0x85)return -99;

	return 0 ;
}
/// start of si118x,
///input : current , led current at start up
//          config,  BIT0 - 0 16HZ  for hrv disabled
//                            1  32HZ for hrv enabled
//						         BIT1 - 0  yellow led is disabled
//                        	  1  yellow led is enabled
// return  0 - inital success
//         <0  - error
int16_t Si117x_Start(uint8_t current, uint8_t config)
{

	int16_t  retval=0;

  uint8_t data1;

	log_info("sI1181 start\n");

	data1= Si117xReadFromRegister(REG_PART_ID);

	if(data1 < 0x70 || data1 >0x85)return -99;

	data1= Si117xReadFromRegister(REG_REV_ID);

	if(data1>=2)
	{
		 si1181_rev = data1;
	}
	else return -98;

	retval = Si117xSendCmd(1);//reset
	RETVAL_CHECK(retval);

	si_delay2ms(6);

	if((config&BIT1)==BIT1)yellow_led_enable = true;
	else yellow_led_enable = false;
	Si117xWriteToRegister(REG_HW_KEY, 0x00); // Set HW_KEY to 0

	retval = Si117xReadFromRegister (REG_HW_KEY);

	if(retval!=0)return -2; // Read back HW_KEY to check

	Si117xParamSet(PARAM_TASK_ENABLE, BIT0+BIT1);

	//set Set 16Hz Measure Rate ((1000000/SI117X_SAMPLERATE)/ 50) = 1250 = 0X4E2
	#define FREQ_SET ((1000000/SI117X_SAMPLERATE)/ 50)
	#define FREQ_SET_HRV ((1000000/SI117X_SAMPLERATE_HRV)/ 50)
	if((config & BIT0) == 0)
	{
		  //16HZ
			Si117xParamSet(PARAM_MEASRATE_H, HIGH_BYTE(FREQ_SET));
			Si117xParamSet(PARAM_MEASRATE_L, LOW_BYTE(FREQ_SET));
	}
	else
	{
		  //32HZ for hrv
			Si117xParamSet(PARAM_MEASRATE_H, HIGH_BYTE(FREQ_SET_HRV));
			Si117xParamSet(PARAM_MEASRATE_L, LOW_BYTE(FREQ_SET_HRV));
	}

	Si117xParamSet(PARAM_PPG_MEASCOUNT, 0x01);

	if(current>0)
	{
		Si117xParamSet(PARAM_PPG1_LED1_CONFIG, BIT6 + (current&0x3f));
		Si117xParamSet(PARAM_PPG1_LED2_CONFIG, BIT6 + (current&0x3f));
		if(yellow_led_enable==true)Si117xParamSet(PARAM_PPG1_LED4_CONFIG, BIT6 + (current&0x3f));
	}

	/*  PPG_MODE
	VDIODE  -   BIT7:BIT6  0:0.25V  1:0.35V 2:0.45V 3:0.55V
  EXT_PD  -   BIT5:BIT4  0:internal PD  1:EXT_PD1   2:EXT_PD2
  PD_CFG  -   BIT3:BIT0  BIT0:PD1  BIT1:PD2:  BIT2:PD3  BIT3:PD4
	*/
	Si117xParamSet(PARAM_PPG1_MODE, 0x0f);

	/*PPG_MEASCONFIG:
	PPG_INTERRUPT_MODE  -  BIT5       0: wrsit on dectection, 1: wrist off detection
  PPG_RAW             -  BIT4       0: PPG pure             1: PPG raw
  ADC_AVG             -  BIT2:BIT0  ADC samples 1 - 1, 2 - 2, 3 - 4, 5 - 8
	*/
	Si117xParamSet(PARAM_PPG1_MEASCONFIG, 5);   //2 ADC SAMPLES

	/*
	DECIM      BIT7:BIT6, 0:64 1:128 2:256 3:512
	ADC_RANGE  BIT5:BIT4, 0:3
	CLK_DIV    BIT3:BIT0  0 TO 12 ,CLK_ADC = Fadc / power(2,CLK_DIV)
	*/
	//Si117xParamSet(PARAM_PPG1_ADCCONFIG, BIT7+BIT6+BIT5+BIT4+2);
	Si117xParamSet(PARAM_PPG1_ADCCONFIG, BIT7+BIT6+BIT5+BIT4);

	Si117xParamSet(PARAM_PPG2_MODE,1);
	Si117xParamSet(PARAM_PPG2_MEASCONFIG, 2+BIT4);   //4 ADC SAMPLES + ppg raw
	Si117xParamSet(PARAM_PPG2_ADCCONFIG, BIT4);// + BIT5+BIT4);
	if((config & BIT0) == 0)
	{
	Si117xParamSet(PARAM_FIFO_INT_LEVEL_H, HIGH_BYTE(SI117X_FIFO_LENGTH));
	Si117xParamSet(PARAM_FIFO_INT_LEVEL_L, LOW_BYTE(SI117X_FIFO_LENGTH));
	}
	else
	{
	Si117xParamSet(PARAM_FIFO_INT_LEVEL_H, HIGH_BYTE(SI117X_FIFO_LENGTH_HRV));
	Si117xParamSet(PARAM_FIFO_INT_LEVEL_L, LOW_BYTE(SI117X_FIFO_LENGTH_HRV));
	}
	/*  MEAS_CONTL
	FIFO_TEST      BIT7   1 - FIFO test enabled
	FIFO_DISABLE   BIT6   1 - FIFO disabled
	PPG_SW_AVG     BIT5:BIT3  OSR = power(2,PPG_SW_AVG)
  ECG_SZ         BIT2   1 - 3bytes,  0 - 2 bytes
  PPG_SZ         BIT0   1 - 3bytes,  0 - 2 bytes
	*/
	Si117xParamSet(PARAM_MEAS_CNTL, 0); //2 bytes per PPG sample

	Si117xFlushFIFO();

	/* REG_IRQ_ENABLE
	SAMPLE_INT_EN  BIT1
	FIFO_INT_EN    BIT0
	*/
	Si117xWriteToRegister (REG_IRQ_ENABLE, BIT0); // Enable FIFO interrupt

	lastfifocount = 0;

  Si117xStart();
//	Timer_vled_on = 4;

	log_info("sI1181 start end!!!");
	return 0;
}

//================================================================
int16_t  clear1171int()
{
	uint8_t retval = Si117xGetIrqStatus(si1181_rev);
	Si117xWriteToRegister(REG_HOSTIN0, retval);
	return Si117xSendCmd(0xD);	//clear interrupt
}


void Si117x_Stop(void)
{
	//clear1171int();
	Si117xStop();
}
//==================================================================
void Si117xReadFifo(typ_fifodata * fifo)
{
	int16_t bytesToRead = 0;
	int16_t fifocount = 0;
	clear1171int();

	fifocount = Si117xUpdateFifoCount(si1181_rev);

  bytesToRead = fifocount - lastfifocount;
	if(bytesToRead<0)bytesToRead+=2048;

	lastfifocount = fifocount;

	if(bytesToRead==0)
	{
			fifo->datalen = 0;
			log_info("Si117x bytesToRead is 0");
			return;
	}

	if(bytesToRead>256)
	{
		log_info("Si117xFlushFIFO, data len = %d",bytesToRead);
		Si117xFlushFIFO();
		fifo->datalen = 0;

		return;
	}
	else
		Si117xBlockRead(REG_FIFO_READ, bytesToRead, fifo->data.bytes);

	fifo->datalen = bytesToRead;
	clear1171int();
}


/**************************************************************************//**
 * @brief Main interrupt processing routine for Si117x.
 *****************************************************************************/
void  HeartRateMonitor_interrupt_handle(typ_fifodata *acc_fifodata)
{
	
	uint8_t change;
	#ifdef OHR_SUSPEND_ENABLED
	if(OHR_status.suspended_flag==0)
	{
		Si117xReadFifo(&si1171_fifodata);//read si117x data from fifo
	}
	else
	{
		si1171_fifodata.datalen = 0;
	}
	#else
		Si117xReadFifo(&si1171_fifodata);//read si117x data from fifo
	#endif
	#ifdef OHRLIB_PRESENT
	log_info("sififolen=%x,%x\n",si1171_fifodata.datalen);
	change = OHRL_IrqDatatProcess(&si1171_fifodata,acc_fifodata);
	//log_info("f*\n");
	if(change!=OHRL_CURRENT_NOTCHANGE)
	{
		log_info("--->current/n");
		setOhrcurrent(OHRL_GetLEDcurrent());
	}
	#endif

}
