
//#include <stdint.h>
//#include <string.h>
#include "app_config.h"

#include "system/includes.h"
//#include "asm/uart_dev.h"
//#include "uartPcmSender.h"
#include "asm/timer.h"

#include "types.h"
#include "qlio_int.h"


#include "ohr_drv_afe.h"


#include "Ohrdrive.h"

#include "si117x_config.h"
#include "qlohrmo2app.h"
#include "ble_rcsp_module.h"

#if 0
#define LOG_TAG             "[APP_OHRDRV]"
#define LOG_INFO_ENABLE
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE
#define log_info(format, ...)       PRINTF("[Info] : [APP_OHRDRV]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif
#endif
#define LOG_TAG             "[APP_O11]"
#define LOG_INFO_ENABLE1
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE1
#define log_info(format, ...)       PRINTF("[Info] : [APP_111]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif


type_OHR_status  OHR_status;

//#define OHR_HRV_ENABLED_FOR_32HZ 1
#define BLE_LOG_RAWDATA

#ifdef BLE_LOG_RAWDATA
uint8_t OhrRawbuf[20*16+1];
#endif


#define TAKE_OFF_HAND_TIME		180
 typ_fifodata acc_fifodata;

 uint16_t take_off_hand_cnt;
 uint8_t hr_power_off_flag;
 uint8_t g_orh_init_flag_check;
 uint8_t sendOhrDataflag;

 #if NOTIFY_1541_ENABLED
 ORH_ONWRIST_DEBUG_INFO g_orh_debug;
 #endif


/**@brief   GPIOTE_IRQHandler
 * @detail
 *
 *
*/

//====================================
void Ohr_PowerOn(void)
{

	//nrf_gpio_cfg_output(OHR_VLED);
	//nrf_gpio_cfg_output(OHR_EN_SENSOR);
	OHR_VLED_OFF();
	OHR_POWER_ON();
	OHR_I2C_init();
//		nrf_delay_ms(100);
}

void direct_close_vled(void)
{
	gpio_set_pull_up(TCFG_IO_OHR_VLED, 1);
    gpio_set_pull_down(TCFG_IO_OHR_VLED, 0);
    gpio_set_die(TCFG_IO_OHR_VLED, 1);
	gpio_set_direction(TCFG_IO_OHR_VLED, 0);
	//nrf_gpio_cfg_output(OHR_VLED);
	OHR_VLED_OFF();
}


//====================================
void Ohr_PowerOff(void)
{
	//return;
	OHR_I2C_uninit();
	OHR_POWER_OFF();
	OHR_VLED_OFF();
	qlio_ext_interrupt_unint();
	  //disable_OHR_handler();
//		nrf_gpio_cfg_default(OHR_INT);

}


/**@brief Function for initializing AFE and g sensor.
 */
uint8_t Ohr_sensors_init(void)
{

		if(Si117x_checkstart() != 0)
		{
			//hw error
			return 1;
		}

  return 0;
}

/**@brief   start an ohr measurement
 * @detail
 *
 *
*/
static int16_t _Ohr_start(void)
{
//	 Ohr_PowerOn();
	log_info("_Ohr_start");
#if OHR_HRV_ENABLED_FOR_32HZ
	 if(Si117x_Start(OHRL_GetLEDcurrent(),1) == 0)
#else
	 if(Si117x_Start(OHRL_GetLEDcurrent(),0) == 0)
#endif
	 {
			OHR_status.running_counter = 3;
			OHR_status.enable_flag = OHRL_ON;
			#if 0
	//		enable_OHR_handler();
//			algorithm_power_on();
#ifdef PPG_IND_CHARGE_DEF
		 if(first_power_on || charge_flag)
#else
     if(first_power_on)
#endif
#endif
		 {
			 OHR_VLED_ON();
     	 }
			return 0;
	 }
	 else
	 {
		  g_orh_init_flag_check = 1;
			OHR_VLED_OFF();
			Ohr_PowerOff();
		  return 1;
	 }
}

/**@brief   start ohr
 * @detail	current - 0 - auto gain mode
 *                    1~63 - fixed current mode
 *
*/
uint8_t ohr_start(uint8_t current)
{
	  if(OHR_status.enable_flag == OHRL_ON)return 0x2;//all day mode
#if OHR_HRV_ENABLED_FOR_32HZ
		#ifdef BLE_LOG_RAWDATA
		if(OHRL_init((uint32_t*)encryptbuf,current,51,1,OhrRawbuf)==OHRL_ENCRYPT_CHECK_PASS)
		#else
		if(OHRL_init((uint32_t*)encryptbuf,current,51,1,NULL)==OHRL_ENCRYPT_CHECK_PASS)
		#endif
#else
		#ifdef BLE_LOG_RAWDATA
		if(OHRL_init((uint32_t*)encryptbuf,current,51,0,OhrRawbuf)==OHRL_ENCRYPT_CHECK_PASS)
		#else
		if(OHRL_init((uint32_t*)encryptbuf,current,51,0,NULL)==OHRL_ENCRYPT_CHECK_PASS)
		#endif
#endif
		{
		//log_info("ohr_start?3?");
			if(_Ohr_start()==0)
			{
					log_info("_Ohr_start SUCCESS");
					return 0;
			}
			return 1;
		}
    return 0xff;
}

uint8_t ohr_starttest(uint8_t current)
{
	if(_Ohr_start()==0)
			{
					log_info("_Ohr_start SUCCESS????");
					return 0;
			}
}


/**@brief   stop ohr
 * @detail
 *
 *
*/

void Ohr_stop(void)
{

  if(g_orh_init_flag_check == 0)
	{
		OHR_VLED_OFF();
		Ohr_PowerOff();
	}
	g_orh_init_flag_check = 0;
	if(OHR_status.enable_flag==OHRL_OFF)return;

	OHR_status.enable_flag = OHRL_OFF;

}
//ble send data
#ifdef BLE_LOG_RAWDATA
uint8_t Log_sendout(uint8_t * logbuf)
{
	static uint8_t index = 0;

	#if 0

	if(logbuf==NULL)return 0;
	if(isEnabledRawDataNotify == false)
		return 0;
	if(isBleBusy)
		return 0;
//	if(is_allow_ble_logdata()==false)return 0;	//WWang:TODO
	if(index!=logbuf[0])
	{
		 isBleBusy = 1;
		 ble_hr_rawdata(&logbuf[index*20+1],20);
//		service_2_dat3_send(&logbuf[index*20+1],20);//WWang:TODO
		if(++index==16)index=0;
		return(1);
	}
	#else
	if(index!=logbuf[0])
	{
		 //isBleBusy = 1;
		 //ble_hr_rawdata(&logbuf[index*20+1],20);
		 log_info("\nlog->%x,%x,%x,%x,%x,%x",index,logbuf[0],logbuf[index*20+1],logbuf[index*20+2],logbuf[index*20+3],logbuf[index*20+4]);
//		service_2_dat3_send(&logbuf[index*20+1],20);//WWang:TODO
		if(++index==16)index=0;
		return(1);
	}
	#endif
	return(0);
}
#endif


//PPG中断后调
void ohr_handle(void)
{
	return;
	if(OHR_status.enable_flag==OHRL_ON)
	{
			OHR_status.running_counter = 2;
		  typ_fifodata fifo;
		   GetACCdata(&fifo);//????
			HeartRateMonitor_interrupt_handle(&fifo);

//		  nrf_gpio_pin_set(22);
			OHRL_algorithm();
//		  nrf_gpio_pin_clear(22);
//			if(rawdata_flag)
			{
//					while(Log_sendout(OhrRawbuf)==1);
			}


	}
}

//1hz,prevent be locked after ppg interrupt
void ohr_check_running_at_1hz(void* priv)
{
	//uint8_t heartValue;		// heart rate,  30 to 220
	//log_info("ohr_check_running_at_1hz");

	if(OHR_status.enable_flag==OHRL_ON)
	{
		static uint8_t pre_heart;
		static uint8_t hold_hr_cnt;
		static uint8_t hr_record;
		if(OHRL_data.heartrate)
			hr_record = OHRL_data.heartrate;
		if(sport_data_mem.restore_hr_module)
		{
			uint8_t onwrist = OHRL_data.onwrist;
			OHRL_data.onwrist = 0;
			sport_data_mem.onwrist = 1;
			if((onwrist == 0) && OHRL_data.heartrate)
			{
				sport_data_mem.restore_hr_timeout = 0;
				sport_data_mem.restore_hr_module = 0;
			}
			sport_data_mem.heartRate = hr_record;

			OHRL_data.heartrate = hr_record;

			if(sport_data_mem.restore_hr_timeout)
			{
				if(--sport_data_mem.restore_hr_timeout)
				{
					sport_data_mem.restore_hr_module = 0;
				}
			}
		}

		clr_wdt();
		if(pre_heart == 0)
		{
			pre_heart = OHRL_data.heartrate;
		}
      	uint8_t diff = OHRL_data.heartrate >= pre_heart ? OHRL_data.heartrate - pre_heart : pre_heart - OHRL_data.heartrate;
        if(diff > 25)
		{
			hold_hr_cnt = 2;
		}
		else
		{
			if(hold_hr_cnt)
			{
				hold_hr_cnt--;
			}
			else
			{
				sport_data_mem.heartRate = OHRL_data.heartrate;
			}
		}
		pre_heart = OHRL_data.heartrate;
		sport_data_mem.onwrist = (OHRL_data.onwrist == 0);
		sport_data_mem.signal_quality = OHRL_data.quality;
		clr_wdt();
		if(!sport_data_mem.onwrist)
		{
			sport_data_mem.heartRate = 0;
			OHRL_data.heartrate = 0;

			if(take_off_hand_cnt > 0)
			{
				if(--take_off_hand_cnt == 0)  //  power off
				{
					// g_rtd_hr.len = 0;
					#if 0
					if((s_discharge_flag == 0) && (hr_mode_5v_test == 0))
					{

							record_poweroff_info(3,__LINE__,(char*)__FILE__);
							sport_data_mem.takeoff_timeout_flag = 0;
							jump_screen_page(POWER_OFF_PAGE_NAME_STR);                     //自动关机

							return;

					}
					#endif
				}
				log_info("take_off_hand_cnt %d",take_off_hand_cnt);
			}
			if(sport_data_mem.find_timeout)
			{
				if(--sport_data_mem.find_timeout == 0)
				{
					sport_data_mem.find_flag = 1;
				}
			}
		}
		else
		{

			if(sport_data_mem.find_timeout < HR_FIND_TIME)
			{
				sport_data_mem.find_timeout++;
			}
			if(sport_data_mem.find_timeout == HR_FIND_TIME)
			{
				if(sport_data_mem.find_flag == 1)
				sport_data_mem.find_flag = 0;
				else if((sport_data_mem.valid_hr_count == 5) && sport_data_mem.heartRate)
				sport_data_mem.find_flag = 2;
			}

			take_off_hand_cnt = TAKE_OFF_HAND_TIME;   // 30 seconds   //30分钟

			if(OHRL_data.heartrate && (sport_data_mem.valid_hr_count < 5))
			{
				if(++sport_data_mem.valid_hr_count == 5)
				{
					sport_data_mem.valid_hr_flag = 1;
				}
			}
		}
		if(sport_data_mem.heartRate == 0)
		{
			if(sport_data_mem.find_flag == 2)
			{
				sport_data_mem.find_flag = 1;
				sport_data_mem.valid_hr_count = 0;
			}
		}
		clr_wdt();
		if(((sport_data_mem.heartRate >= 30) && (sport_data_mem.heartRate <= 240)) || sport_data_mem.heartRate == 0)
		{
	//		onwrist_handler();                 //detele by qszeng 2023-7-19
			if(sport_data_mem.pre_hr)
			{
				sport_data_mem.heartRate = sport_data_mem.pre_hr;
				sport_data_mem.onwrist = 1;

				if(OHRL_data.heartrate && (OHRL_data.onwrist == 0) && sport_data_mem.valid_hr_flag)
				{
					sport_data_mem.pre_hr = 0;
				}
				//sendHeartData(sport_data_mem.heartRate);
			}
			else if(sport_data_mem.valid_hr_flag)
			{
				#if HR_NO_CHANGED_FLAG
				static uint8_t same_cnt;
				static uint8_t pre_hr;
				if((pre_hr == sport_data_mem.heartRate) && sport_data_mem.heartRate)
				{
				if(++same_cnt > 60)
				{
				same_cnt = 0;
				Ohr_stop();
				//nrf_delay_ms(500);
				//s_ohr_op_status = 0;
				log_info("same_cnt valid");
				}
				}
				else
				{
				same_cnt = 0;
				}
				pre_hr = sport_data_mem.heartRate;
				#endif

				//sendHeartData(sport_data_mem.heartRate);
				//motor_module_heartrate_detection(sport_data_mem.heartRate);
			}
		}
		//log_info("OHR_status.running_counter %d", OHR_status.running_counter);
		if(OHR_status.running_counter==0)
		{
			OHR_status.running_counter = 2;
			//typ_fifodata acc_fifodata;
			clr_wdt();
			GetACCdata(&acc_fifodata);
			HeartRateMonitor_interrupt_handle(&acc_fifodata);//
			//log_info("HR11");//=%d,onwrist=%d",OHRL_data.heartrate,OHRL_data.onwrist);
			clr_wdt();
			OHRL_algorithm();        //
			clr_wdt();

			log_info("HR#=%d,onwrist=%d",OHRL_data.heartrate,OHRL_data.onwrist);
			take_off_hand_cnt = TAKE_OFF_HAND_TIME;   // 30 分钟        //add by qszeng 2023-7-19

			log_info("3d-----acc_fifodata.lenth %d", acc_fifodata.datalen);
			clr_wdt();
		}
		else
		{
			--OHR_status.running_counter;
		}
		#if 1
		if(rcsp_Is_work_state_connected())
		{
			ble_send_heart_data(&OHRL_data);
			ble_send_pedo_data(&PedoSummary);
			sendOhrDataflag = 1;
		}
		#endif
		#if 0
		if((OHRL_data.onwrist) )//&& (charge_flag == 0))
		{
			heartValue = 0;
#if NOTIFY_1541_ENABLED
			if(g_orh_debug.flag == 0)
			{
				g_orh_debug.flag = 1;
				g_orh_debug.utc = rtcm_get_sys_utc();
				g_orh_debug.count++;
			  g_orh_debug.onwrist = OHRL_data.onwrist;
			}

#endif
#ifndef SAIHU_VERSION_DEF
				if(take_off_hand_cnt > 0)
				{
					if(--take_off_hand_cnt == 0)  //  power off
					{
							hr_power_off_flag = 1;
							//switchPowerOff();/////////////
					}
				}
			#endif
		}
		else
		{
			reset_kake_off_hand_counter();

			hr_power_off_flag = 0;
#if NOTIFY_1541_ENABLED
			g_orh_debug.flag = 0;
#endif
		}
		//#endif
		//if(((heartValue >= 40) && (heartValue <= 220)) || heartValue == 0)
		//{
			//heartrate_meas_notify(heartValue);
	//#if SELECT_FUNCTION_VERSION != 30
	//		motor_module_heartrate_detection(heartValue);//WWang:add 2020.03.28
	//#endif
		//}
		log_info("HR=%d,%d,onwrist=%d",OHRL_data.heartrate,OHRL_data.onwrist);



		if(OHR_status.running_counter==0)
		{
			 OHR_status.running_counter = 2;
			 typ_fifodata fifo;
			 //GetACCdata(&fifo);////////////
			 HeartRateMonitor_interrupt_handle(&fifo);
		}
		else --OHR_status.running_counter;
		#endif
	}
}
void Sensor_manager(void)
{
		
		GetACCdata(&acc_fifodata);

//	     NRF_LOG_INFO("len %d x %d y %d z %d",acc_fifodata.datalen,acc_fifodata.data.words[0],\
//	acc_fifodata.data.words[1],acc_fifodata.data.words[2]);
		clr_wdt();
	#if 0//MANAGE_3D_SLEEP_ENABLED
	      if(g_3d_sleep_flag)
	         return;
#endif
			  /*
					获取3D数据给另外算法使用
			  */
				if(OHR_status.enable_flag==OHRL_ON)
				{
					//printf("1->\n");
					OHR_status.running_counter = 2;
					HeartRateMonitor_interrupt_handle(&acc_fifodata);
				}
				else
				{
					//OHRL_pushXYZ_8G(&acc_fifodata);
					//OHRL_pushXYZ_4G(&acc_fifodata);
					//PedoSummary.TotalSteps_ul;
				}
				OHRL_algorithm();
				log_info("HR=%d,onwrist=%dstep=%d,",OHRL_data.heartrate,OHRL_data.onwrist,PedoSummary.TotalSteps_ul);
				//log_info("%d,%d,%d,%d,%d",getRunstatus(),get_Activity(),OHRL_get_onwristMovement(),Get_OHR_mode(),Get_ohr_ledoff());
				//log_info("ver:%s,ver1:%s",OHRL_GetOHRLVERSION(),OHRL_GetBUILDVERSION());
				//printf("A*>\n");
				clr_wdt();
#if OHR_FITHAND_ENABLED
				handup_process();
#endif

		#ifdef BLE_LOG_RAWDATA
			//ppg_Log_sendout();
		#endif
}

void ohr_4hz_event(void)
{
	//BATTERY_INFO_T*batt = get_battery_info();
	//if(batt->s_powerOffFlag == SLEEP_MODE)
	//	return;
	if(sport_data_mem.heart_module_flag)
	{
		Sensor_manager();


	  //uint16_t rr_buf[10];
		//uint8_t len = get_hrvRR(rr_buf);
		#ifdef BLE_LOG_RAWDATA
	  //set_RR_by_ble(rr_buf,len);
	  #endif

#if ANT_HRM_ENABLED
        setPage4Info(sport_data_mem.heartRate);
#endif
	}

}

uint8_t ohr_heartrate_get(void)
{

	return OHRL_data.heartrate;
}

void send_logBuf(void)
{
	#ifdef BLE_LOG_RAWDATA
	log_info("\nlog->%x\n",OhrRawbuf[0]);
	//Log_sendout(OhrRawbuf);
	#else
	return;
	#endif
}
void ohr_drv_init(void)
{
	uint16_t i;
	#ifdef BLE_LOG_RAWDATA
	//log_info("\nlog1->%x,%x,%x,%x\n",OhrRawbuf,&OhrRawbuf[20*16],20*16,i);
	for(i = 0;i<(20*16+1);i++)
	{
		//clr_wdt();
		//log_info("| %x,%x,",i,&OhrRawbuf[i]);
		OhrRawbuf[i] = 0;//被

	}
	//log_info("?i = %x,%x,",i,&OhrRawbuf[20*16]);
	#endif
	//OHR_status.enable_flag = OHRL_OFF;
	//OHR_status.running_counter = 4;
	//OHRL_data.heartrate=70;
	//OHRL_data.onwrist =0;
	sport_data_mem.heart_module_flag = 1;
	//take_off_hand_cnt = 
	sendOhrDataflag = 0;

}
void qlOhrInit(void)
{
	sendOhrDataflag=0;
}
uint8_t IssendOhrDataflag(void)
{
	 return sendOhrDataflag;
}
uint8_t sendOhrDataToHkt(uint8_t * buffer)
{
	uint8_t i,len = 0;

	OHRL_data.heartrate = 0x31;
	OHRL_data.quality = 0x32;
	OHRL_data.onwrist = 0x33;
	OHRL_data.ppg = 0x3435;
	OHRL_data.amb = 0x3637;
	OHRL_data.ppg_maxmin = 0x3839;
	buffer[1] = OHRL_data.heartrate;
	buffer[2] = buffer[1];
	buffer[3] = OHRL_data.quality;
	buffer[4] = buffer[1];
	buffer[5] = OHRL_data.onwrist;
	buffer[6] = buffer[1];
	buffer[7] = (uint8_t)((OHRL_data.ppg&0xff00)>>8);
	buffer[8] = buffer[1];
	buffer[9] = (uint8_t)((OHRL_data.ppg&0x00ff));
	buffer[10] = buffer[1];
	buffer[11] = (uint8_t)((OHRL_data.amb&0xff00)>>8);
	buffer[12] = buffer[1];
	buffer[13] = (uint8_t)((OHRL_data.amb&0x00ff));
	buffer[14] = buffer[1];
	buffer[15] = (uint8_t)((OHRL_data.ppg_maxmin&0xff00)>>8);
	buffer[16] = buffer[1];
	buffer[17] = (uint8_t)((OHRL_data.ppg_maxmin&0x00ff));
	buffer[18] = buffer[1];
	//memcpy(&buffer[1], &OHRL_data, sizeof(type_OHRL_output));
	//len = sizeof(type_OHRL_output);
	//memcpy(&buffer[len+1], &PedoSummary, sizeof(type_PedoSummary));
	PedoSummary.instantsteps = 0x31;
	PedoSummary.TotalSteps_ul = 0x32333435;
	PedoSummary.PedoSleep_flag = 0x36;
	PedoSummary.PedoStatuschanged_flag= 0x36;
	PedoSummary.pace= 0x38;
	PedoSummary.handdown_check_timer= 0x3930;
	PedoSummary.handup_Disp_flg = 0x31;
	buffer[19] = PedoSummary.instantsteps;
	buffer[20] = buffer[1];
	buffer[21] = (uint8_t)((PedoSummary.TotalSteps_ul&0xff000000)>>24);
	buffer[22] = buffer[1];
	buffer[23] = (uint8_t)((PedoSummary.TotalSteps_ul&0x00ff0000)>>16);
	buffer[24] = buffer[1];
	buffer[25] = (uint8_t)((PedoSummary.TotalSteps_ul&0x0000ff00)>>8);
	buffer[26] = buffer[1];
	buffer[27] = (uint8_t)((PedoSummary.TotalSteps_ul&0x000000ff));
	buffer[28] = buffer[1];	
	buffer[29] = PedoSummary.PedoSleep_flag;
	buffer[30] = buffer[1];
	buffer[31] = PedoSummary.PedoStatuschanged_flag;
	buffer[32] = buffer[1];
	buffer[33] = PedoSummary.pace;
	buffer[34] = buffer[1];
	buffer[35] = (uint8_t)((PedoSummary.handdown_check_timer&0xff00)>>8);
	buffer[36] = buffer[1];
	buffer[37] = (uint8_t)((PedoSummary.handdown_check_timer&0x00ff));
	buffer[38] = buffer[1];
	buffer[39] = PedoSummary.handup_Disp_flg;
	buffer[40] = buffer[1];
	len= 40;
	#if 0
	printf("len->%d",len);
	for(i = 0;i<len;i++)
	{
		clr_wdt();
		printf("*%c",buffer[i+1]);
	}
	printf("\n");
	#endif
	buffer[0] = len;

}
/*
current debug:

1. ble adv on, led on, no log_sendout, 5v off, no OHRL_algorithm, 1181 on, readdata  -- 0.620mA
2. ble adv on, led off, no log_sendout, 5v off, no OHRL_algorithm, 1181 stop           -- 0.190mA
3. ble adv on, led off, no log_sendout, 5v off, no OHRL_algorithm, 1181 on, readdata   -- 0.292mA

4. ble adv on, led off, no log_sendout, 5v on, current 0, no OHRL_algorithm - 0.635mA
4. ble adv on, led off, no log_sendout, 5v on, current 30, no OHRL_algorithm - 0.674mA
5. ble adv on, led off, no log_sendout, 5v on, current 45, no OHRL_algorithm - 0.825mA
6. ble adv on, led off, no log_sendout, 5v on, current 51, no OHRL_algorithm - 0.98 mA

7. ble adv on, led off, no log_sendout, 5v on, current 45, OHRL_algorithm on - 1.8 mA

*/
