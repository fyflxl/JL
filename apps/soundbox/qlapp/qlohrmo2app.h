
#ifndef _QLOHRMO2_APP_H_
#define _QLOHRMO2_APP_H_
#include "system/includes.h"
#include "app_config.h"
#if TCFG_OHRMO_EN
#define OHRL_ENCRYPT_LENGTH	    42

#define HR_FIND_TIME					3
//#define encryptbuf  0X77000UL
extern uint32_t encryptbuf[16];
typedef void (*heart_fun)(void);
typedef void (*heart_open_func)(uint8_t mode);

typedef struct{
	u16 ohr_1hztimes;
	u16 ohr_times;
	u16 ohr_wdttimes;
	u32 block_id[OHRL_ENCRYPT_LENGTH];
	uint8_t ohrlogbuff[322];//0:len;1-321 buffer

	//typ_fifodata OHR_fifo;
	//typ_fifodata ACC_fifo;
	uint8_t op_status;
	uint8_t changecurflg;
}type_OHR_use_buf;

extern type_OHR_use_buf use_OHR;

typedef struct {
	float current_speed;
	//type_SPO2_output  spo2output;
	heart_fun closeHeartModule;   	   // close heart module
	heart_open_func openHeartModule;   // open heart hodule
	heart_fun clearSportData;   	   // clear data at 23:59 59

	
	uint32_t  ref_steps;               // 99999 steps
	uint32_t  ref_calorie;      
	uint32_t  sport_time;
	uint32_t  sport_calc_speed_step;

	uint32_t  sport_steps;             // 99999 steps
	uint32_t  sport_calorie;      
	uint32_t  sport_distance;
	uint32_t last_health_step;
		
	uint8_t sport_last_normal_stress;
	uint8_t today_max_stress;
	uint8_t today_min_stress;
	uint8_t today_emotion;
	uint8_t today_stamina;
	uint8_t today_stress;
	
	uint32_t  pre_steps;        	   // useed by sleep calculate
	uint32_t  Steps;            	   // 99999 steps
	uint32_t  Distance;                // 999.99Km
	uint32_t  Calorie;      		   //kaluli
	uint32_t  stride_length;	
	uint16_t  temperature;   		   //temperature * 10
	uint16_t  sport_jump_counter; 
	uint16_t  step_freq;
	
	int8_t     signal_quality; 		   // singal raw data
	uint8_t    onwrist;    			   // 1 - on wrist   0-off wrist
	uint8_t    heartRate;     	   	   // heart rate
	uint8_t    heart_module_flag;  	   // 0-heart module is close, 1-heart module is open 
	uint8_t    heart_alram_flag;       // 0- no triggle   1-heart more than 167 to triggle motor event
	uint8_t    takeoff_timeout_flag;   //0-no timeout	  1-invalid heart rate for 2 minutes
	uint8_t    fit_hand_event;         // fit hand event 1-turn on event 2-turn off event
	uint8_t    heart_module_id;    	   // for test mode 0--ok   1-fail
	uint8_t rawdata_flag;          	   // raw data for test mode
	uint8_t valid_hr_flag;
	uint8_t valid_hr_count;
	uint8_t find_timeout;
	uint8_t find_flag;
	uint8_t rawdata_send_flag;  	   // 0-不发送 1-发送
	uint8_t target_max_heartRate;      // 最大目标心率
	uint8_t target_min_heartRate;
	uint8_t target_heartRate;      	   // 目标心率
	uint8_t sleep_counter;
	uint8_t need_sync_utc;    		   // 0-同步utc,1-不同步utc
	uint8_t hr_mode_sel;      		   // 0-心率模式 1-血氧模式 2-全天模式 3-纯脱腕检测模式 4-工厂测试模式
	uint8_t restore_hr_module;
	uint8_t restore_hr_timeout;
	
	uint8_t last_10min_spo2;  //最近10分钟血氧值
	uint8_t spo2_flag;
	uint8_t pre_mode;
	uint8_t pre_status;
	uint8_t pre_hr;
}sport_data_info_t;


typedef enum
{
	HR_WORK_MODE,
	SPO2_WORK_MODE,
	ALL_DAY_WORK_MODE,
	ON_WRIST_CHECK_MODE,
	FACTORY_TEST_MODE,
}E_HR_WORK_MODE;


//-------------Variable declaration ------------

extern sport_data_info_t sport_data_mem;
uint8_t getrawdata_flag(void);



#if 0
unsigned char I2CDevice_WriteOneByte(unsigned char i2cAddress,unsigned char reg_adr,unsigned char datum);
unsigned char I2CDevice_Read(unsigned char i2cAddress,unsigned char reg_adr,unsigned char *buffer,unsigned char dat_len);
unsigned char I2CDevice_Write(unsigned char i2cAddress,unsigned char reg_adr,unsigned char const *buffer,unsigned char dat_len);

void OHR_I2C_init(void);
void OHR_I2C_uninit(void);

#endif

extern void cardiotachemoterInt(void);
extern void cardiotachometer_poweron(void);
#endif

#endif


