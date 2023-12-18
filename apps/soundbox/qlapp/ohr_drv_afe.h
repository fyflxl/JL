#ifndef _OHR_DRV_AFE_H_
#define _OHR_DRV_AFE_H_
#include "app_config.h"

#include "system/includes.h"


#include "types.h"
//#include "sdk_config.h"
#define OHRL_ON		1
#define OHRL_OFF 	0
#include "ohr_i2c_driver.h"


extern  uint8_t sendbleDataflag;

#define OHR_VLED_OFF()      gpio_set_output_value(TCFG_IO_OHR_VLED, 0)      
#define OHR_VLED_ON()     	gpio_set_output_value(TCFG_IO_OHR_VLED, 1)
#define OHR_POWER_ON()      gpio_set_output_value(TCFG_IO_OHR_3V3, 1)        
#define OHR_POWER_OFF()     gpio_set_output_value(TCFG_IO_OHR_3V3, 0)

typedef struct {
	uint8_t enable_flag;  // 
	uint8_t running_counter;
} type_OHR_status;

extern type_OHR_status  OHR_status;		


/**@brief   start an ohr measurement
 * @detail	
 * @param[in]    current               0     startup current with auto gain control enabled 
                                       1-63	startup current with fixed current					 
 * @retval    0   ppg start success    
 *            1   ppg start fail
 *           0xff ohrl encryption verify fail               
*/
uint8_t ohr_start(uint8_t current);

/**@brief   stop an ohr measurement
 * @detail	
 *          
 *          
*/
void Ohr_stop(void);

/**@brief   ohr handle
 * @detail	check and run ohr algorithm after ppg data ready,
 *         
 *          
*/
void ohr_handle(void);


/**@brief   ohr_check_running_at_1hz
 * @detail	check ohr running at 1hz to handle in case miss ppg_int interrupt 
 *          put it at 1hz event
 *          
*/
void ohr_check_running_at_1hz(void* priv);


/**@brief   Ohr_sensors_init
 * @detail	check sensor at system power up
 *          
 *          
*/
uint8_t Ohr_sensors_init(void);

/**@brief   ohr_heartrate_get
 * @detail	get ohr module heartrate
 *          
 *          
*/
uint8_t ohr_heartrate_get(void);

void Ohr_PowerOn(void);
uint8_t Log_sendout(uint8_t * logbuf);	

void direct_close_vled(void);

extern uint8_t OhrRawbuf[20*16+1];

#if NOTIFY_1541_ENABLED
typedef struct
{
	uint32_t utc;
	uint16_t count;
	uint8_t onwrist;
	uint8_t flag;
}ORH_ONWRIST_DEBUG_INFO;

extern ORH_ONWRIST_DEBUG_INFO g_orh_debug;
extern uint8_t g_orh_init_flag_check;
uint8_t ohr_starttest(uint8_t current);
void send_logBuf(void);
void ohr_drv_init(void);
void ohr_4hz_event(void);
void qlOhrInit(void);
uint8_t sendOhrDataToHkt(uint8_t * buffer);


#endif
#endif
