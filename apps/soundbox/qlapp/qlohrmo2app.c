#include "app_config.h"

#include "system/includes.h"
//#include "asm/uart_dev.h"
//#include "uartPcmSender.h"
#include "asm/timer.h"
#include "Ohrdrive.h"
#include "types.h"
#include "qlio_int.h"
#include "ohr_drv_afe.h"
#include "si117x_config.h"
#include "qlohrmo2app.h"
#if TCFG_OHRMO_EN
#define LOG_TAG             "[APP_OHR]"
#define LOG_INFO_ENABLE
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE
#define log_info(format, ...)       PRINTF("[Info] : [APP_OHR]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif
sport_data_info_t sport_data_mem = {
	.rawdata_send_flag = 1
};
uint32_t encryptbuf[16];
#define  SPORT_TOP_TIME_VAL  (99*3600+59*60+59)

#define TAKE_OFF_HAND_TIME		900   // 自动关机时间改成15分钟


#define heart_iic_init(iic)                       soft_iic_init(iic)
#define heart_iic_uninit(iic)                     soft_iic_uninit(iic)
#define heart_iic_start(iic)                      soft_iic_start(iic)
#define heart_iic_stop(iic)                       soft_iic_stop(iic)
#define heart_iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define heart_iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define heart_iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define heart_iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define heart_iic_suspend(iic)                    soft_iic_suspend(iic)
#define heart_iic_resume(iic)                     soft_iic_resume(iic)


#define IIC_NO 0
//extern uint8_t testval;
bool qlohrmo2_iic_read(u8 addr,u8 reg,u8 *buf,u8 len)
{
	u8 i;
    heart_iic_start(IIC_NO);

    if (0 == heart_iic_tx_byte(IIC_NO, addr)) {
        heart_iic_stop(IIC_NO);
        log_info("IIC read fail1!\n");

        return false;
    }
	//log_info("IIC read ok!\n");
    delay_2ms(1);
	clr_wdt();
    //udelay(200);
    if (0 == heart_iic_tx_byte(IIC_NO, reg)) {
        heart_iic_stop(IIC_NO);
        log_info("IIC  read fail2!\n");
        return false;
    }
    delay_2ms(1);
    heart_iic_start(IIC_NO);
    if (0 == heart_iic_tx_byte(IIC_NO, addr | 1)) {
        heart_iic_stop(IIC_NO);
        log_info("bmp280 read fail3!\n");
        return false;
    }

    for (i = 0; i < len - 1; i++) {
        buf[i] = heart_iic_rx_byte(IIC_NO, 1);
		clr_wdt();
        udelay(10);
    }
    buf[i] = heart_iic_rx_byte(IIC_NO, 0);
    heart_iic_stop(IIC_NO);
	return true;

}


u8 qlohrmo2_iic_write(u8 addr, u8 *buf, u32 len)
{
    heart_iic_start(IIC_NO);
    if (0 == heart_iic_tx_byte(IIC_NO, addr)) {
		heart_iic_stop(IIC_NO);
        log_info("\nqlohrmo2_iic_write err 0\n");
        return false;
    }
	udelay(200);
	clr_wdt();
	heart_iic_write_buf(IIC_NO,buf,len);
    heart_iic_stop(IIC_NO);
    return true;
}

//const nrf_drv_twi_t m_twi_ohr = NRF_DRV_TWI_INSTANCE(0);

/**@brief write one byte to i2c slave
 *
 * @details  write one byte to i2c slave
 *
 * @note
 * @param[in]  i2cAddress
 * @param[in]  reg_adr
 * @param[in]  datum
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_WriteOneByte(unsigned char i2cAddress,unsigned char reg_adr,unsigned char datum)
{
	unsigned char buf[2] = {0};
	buf[0] = reg_adr;
	buf[1] = datum;
	//nrf_drv_twi_tx(&m_twi_ohr, i2cAddress, buf, 2, false);
	qlohrmo2_iic_write(i2cAddress,buf,2);

	return 0;
}

/**@brief  Write block of data to i2c.
 *
 * @details  write numbers of byte to i2c slave
 *
 * @note
 * @param[in]  i2cAddress
 * @param[in]  reg_adr
 * @param[in]  buffer     pointer of data to write
 * @param[in]  dat_len    length of bytes to write
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_Write(unsigned char i2cAddress,unsigned char reg_adr,unsigned char const *buffer,unsigned char dat_len)
{
	unsigned char buf[20] = {0};
	buf[0] = reg_adr;
	memcpy(&buf[1], buffer, dat_len);
  //nrf_drv_twi_tx(&m_twi_ohr, i2cAddress, buf, dat_len+1, false);
  	qlohrmo2_iic_write(i2cAddress,buf,dat_len+1);
  #if 0
  	log_info("\n i2cw->add%x,len%x\n",i2cAddress,dat_len);
	for(u8 i=0;i<dat_len+1;i++)
	{
		log_info("%x,",buf[i]);
	}
	log_info("end\n");
	#endif
	clr_wdt();
	return 0;
}

/**@brief Read block of data from i2c.
 *
 * @details  read numbers of byte from i2c slave
 *
 * @note
 * @param[in]  i2cAddress
 * @param[in]  reg_adr
 * @param[in]  buffer     pointer to put read data
 * @param[in]  dat_len    length of bytes to read
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_Read(unsigned char i2cAddress,unsigned char reg_adr,unsigned char *buffer,unsigned char dat_len)
{
	 //nrf_drv_twi_tx(&m_twi_ohr, i2cAddress, &reg_adr, 1, true);
	 //nrf_drv_twi_rx(&m_twi_ohr, i2cAddress, buffer, dat_len);
	 qlohrmo2_iic_read(i2cAddress,reg_adr,buffer,dat_len);
	
	 //log_info("\n i2cRR->add%x,%x,len%x,data-%x\n",i2cAddress,reg_adr,dat_len,buffer[0]);
	  #if 0
	for(u8 i=0;i<dat_len;i++)
	{
		log_info("%x,",buffer[i]);
	}
	log_info("end\n");
	#endif
	clr_wdt();
	return 0;
}

static uint8_t iic_flag = 0;
uint8_t get_ohr_i2c_init_flag(void)
{
	return iic_flag;
}
//extern unsigned char ohr_power_is_open;
void OHR_I2C_init(void)
{
	uint32_t err_code;
	if(0 == iic_flag){
		iic_flag  =1;
		// I2C Master
		heart_iic_init(IIC_NO);
	#if 0
		//nrf_gpio_pin_set(OHR_SCL_PIN);
		//nrf_gpio_pin_set(OHR_SDA_PIN);
		//nrf_gpio_cfg_output(OHR_SCL_PIN);
		//nrf_gpio_cfg_output(OHR_SDA_PIN);

			const nrf_drv_twi_config_t twi_mma_config = {
		   .scl                = OHR_SCL_PIN,
		   .sda                = OHR_SDA_PIN,
		   .frequency          = NRF_DRV_TWI_FREQ_400K,
		   .interrupt_priority = APP_IRQ_PRIORITY_HIGH
		};

			err_code = nrf_drv_twi_init(&m_twi_ohr, &twi_mma_config, NULL, NULL);
		APP_ERROR_CHECK(err_code);

			nrf_drv_twi_enable(&m_twi_ohr);
			#endif
	}
//    ohr_power_is_open = true;
}
/*
*   TWI  uninitialization.
*/
void OHR_I2C_uninit(void)
{
	if(1 == iic_flag){
		iic_flag  = 0;
		heart_iic_uninit();
	#if 0
		 nrf_drv_twi_disable(&m_twi_ohr);
	   nrf_drv_twi_uninit(&m_twi_ohr);//, &config, ohr_twi_handler, NULL);
	   nrf_gpio_cfg_output(OHR_SCL_PIN);
	   nrf_gpio_cfg_output(OHR_SDA_PIN);
		 nrf_gpio_pin_clear(OHR_SCL_PIN);
		 nrf_gpio_pin_clear(OHR_SDA_PIN);	  //心率模块放电
		 #endif
	}
}



#define IIC_ENBLE_GSENSOR 1
#define I2C_ADD 0XD4  //SD0=0


#if 0
bool qlohrmo2_iic_read_byte(u8 reg,u8 *bat)
{

    soft_iic_start(IIC_NO);
    if (0 == heart_iic_tx_byte(IIC_NO, I2C_ADD)) {
        heart_iic_stop(IIC_NO);
        log_info("IIC read fail1!\n");
        return false;
    }
    delay_2ms(1);
    if (0 == heart_iic_tx_byte(IIC_NO, reg)) {
        heart_iic_stop(IIC_NO);
        log_info("IIC  read fail2!\n");
        return false;
    }
    delay_2ms(1);
    heart_iic_start(IIC_NO);
    if (0 == heart_iic_tx_byte(IIC_NO, I2C_ADD | 1)) {
        heart_iic_stop(IIC_NO);
        log_info("bmp280 read fail3!\n");
        return false;
    }

    //for (u8 i = 0; i < len - 1; i++) {
    //    buf[i] = soft_iic_rx_byte(IIC_NO, 1);
    //    delay_2ms(bmp280_iic_info->iic_delay);
    //}
    *bat = heart_iic_rx_byte(IIC_NO, 0);
    heart_iic_stop(IIC_NO);
	return true;

}
void qlohrmo2_self_i2c_test(void)
{

  uint8_t whoamI;
  u8 reg = 0x0f;//who am i add

  /* Check device ID */

	log_info("--->gsensor writer %x\n",reg);
	if(qlohrmo2_iic_read_byte(reg,&whoamI)==true)
	{
		log_info("--->gsensor i2c read %x,%x\n",reg,whoamI);
	}
	else
	{
		log_info("--->gsensor i2c writer error\n");
	}

  if ((whoamI != LSM6DSL_ID)&&(whoamI != 0x69))
  {
		log_info("--->gsensor ID bad\n");
		return;
  }
   log_info("--->gsensor ID ok\n");
}

void qlohrmo2_self_test(void)
{

	log_info("--->gsensor iic test\n");
	qlohrmo2_self_i2c_test();

}
#endif
#define PCB_Z_UP_X_WRIST 0
#define PCB_Z_UP_X_ARM   1
#define PCB_Z_UP_Y_WRIST 2
#define PCB_Z_UP_Y_ARM   3
#define PCB_Z_DN_X_WRIST 4
#define PCB_Z_DN_X_ARM   5
#define PCB_Z_DN_Y_WRIST 6
#define PCB_Z_DN_Y_ARM   7

//electric current
#define ELECTRIC_CUR_AUTO 0
#define ELECTRIC_CUR_ADJ  10//1~63
#define ELECTRIC_CUR_MAX  63

#define OHR_STATUS_IDLE 0
#define OHR_STATUS_6D_INIT 1
#define OHR_STATUS_INIT 2
#define OHR_STATUS_START 3
#define OHR_STATUS_DELAY 4
#define OHR_STATUS_WAIT_RUN 5
#define OHR_STATUS_RUN   6
#define OHR_STATUS_END   7
//#define OHR_STATUS_ERROR 4



type_OHR_use_buf use_OHR;

void cardioPinCfgint(void)
{
	gpio_set_pull_up(TCFG_IO_OHR_VLED, 1);
    gpio_set_pull_down(TCFG_IO_OHR_VLED, 0);
    gpio_set_die(TCFG_IO_OHR_VLED, 1);
	gpio_set_direction(TCFG_IO_OHR_VLED, 0);

	gpio_set_pull_up(TCFG_IO_OHR_3V3, 1);
    gpio_set_pull_down(TCFG_IO_OHR_3V3, 0);
    gpio_set_die(TCFG_IO_OHR_3V3, 1);
	gpio_set_direction(TCFG_IO_OHR_3V3, 0);
	OHR_POWER_OFF();
	OHR_VLED_OFF();
}
void cardioPinCfgUnint(void)
{
	//配置 start
	#if 0
	gpio_set_pull_up(TCFG_IO_OHR_VLED, 1);
    gpio_set_pull_down(TCFG_IO_OHR_VLED, 0);
    gpio_set_die(TCFG_IO_OHR_VLED, 1);
	gpio_set_direction(TCFG_IO_OHR_VLED, 0);
	gpio_set_pull_up(TCFG_IO_OHR_3V3, 1);
    gpio_set_pull_down(TCFG_IO_OHR_3V3, 0);
    gpio_set_die(TCFG_IO_OHR_3V3, 1);
	gpio_set_direction(TCFG_IO_OHR_3V3, 0);
	OHR_5V_OFF;
	OHR_3V_OFF;
	#endif

	//配置 end

}
uint8_t getrawdata_flag(void)
{
	return sport_data_mem.rawdata_flag;
}

/**
* @brief 退出运动模式时调用，清除卡路里，步数数据数据
*/
static void clear_sport_step_calorie_data(void)
{
	sport_data_mem.sport_calorie = 0;
	sport_data_mem.sport_steps = 0;
	sport_data_mem.sport_time = 0;
	sport_data_mem.sport_jump_counter = 0;
}

/*
* @brief set sport reference value
*/
static void set_sport_ref(void)
{
	sport_data_mem.ref_calorie = sport_data_mem.Calorie;
	sport_data_mem.ref_steps = sport_data_mem.Steps;
}

static void start_ohr_module(uint8_t mode)
{

	if(mode != sport_data_mem.hr_mode_sel)// || sport_data_mem.heart_module_flag == 0)           //add by qszeng 2023-7-21
	{
		#if 0
		if(sport_data_mem.heart_module_flag)
		{
			sport_data_mem.closeHeartModule();
			nrf_delay_ms(10);
		}
		#endif
		if(sport_data_mem.hr_mode_sel != SPO2_WORK_MODE)
		{
			;//memset(&sport_data_mem.spo2output,0,sizeof(sport_data_mem.spo2output));
			//spo2_timeout = 0;
		}

		//spo2_timeout = 0;
		//s_ohr_op_status = 0;
		sport_data_mem.takeoff_timeout_flag = 0;
		sport_data_mem.valid_hr_flag = 0;
		sport_data_mem.valid_hr_count = 0;
		sport_data_mem.find_flag = 0;
		sport_data_mem.find_timeout = HR_FIND_TIME;

	}

	sport_data_mem.hr_mode_sel = mode;
	clear_sport_step_calorie_data();

	set_sport_ref();


#if ANT_HRM_ENABLED
//	ant_hrm_tx_start();
#endif
#if MANAGE_3D_SLEEP_ENABLED
	exit_3d_sleep();
#endif
}
void resetOHR_module(void)
{
	sport_data_mem.restore_hr_module = 1;

   if(sport_data_mem.restore_hr_timeout == 0)
 	{
	 sport_data_mem.restore_hr_timeout = 20;
 	}
	 sport_data_mem.openHeartModule(sport_data_mem.hr_mode_sel);
}


void HRS_exit(void)
{
	Ohr_stop();

	sport_data_mem.onwrist = 0;
	sport_data_mem.valid_hr_flag = 0;
	sport_data_mem.valid_hr_count = 0;
	sport_data_mem.find_flag = 0;
	sport_data_mem.find_timeout = 0;

//	OHRL_logStep(true);
#if ANT_HRM_ENABLED
//	ant_hrm_tx_close();
#endif


		//totalstep_lasminute = sport_data_mem.Steps; // PedoSummary.TotalSteps_ul;
		//calc_step_timer = 0;
		sport_data_mem.heartRate = 0;

		//g_temperature_data.temp_valid = 0;          //add by qszeng 2023-7-18

 // if(sport_data_mem.restore_hr_module == 0)
		//hr_rr_sleep_evt_manage();
#if MANAGE_3D_SLEEP_ENABLED
	s_3d_sleep_counter = 4*3;
#endif
}

/**
* @brief 清除卡路里数据
*/
static void clear_colarie_data(void)
{
	log_info("clear sport data");
	//temp_min_calorie = 0;
	sport_data_mem.Calorie = 0;
	sport_data_mem.Distance = 0;
	sport_data_mem.Steps = 0;
	PedoSummary.TotalSteps_ul = 0;
	//totalstep_lasminute = 0;

	sport_data_mem.ref_steps = 0;            // 99999 steps
	sport_data_mem.ref_calorie = 0;
	sport_data_mem.last_health_step  = 0;
	sport_data_mem.pre_steps = 0;
	sport_data_mem.need_sync_utc = 0;

}
static void clear_wdg(void* prv)
{
	clr_wdt();
}

void OhrProStatus(void* prv)
{
	static uint8_t old_opstatus=0xff;
	static uint8_t cnt=0;
	static uint8_t retry_cnt = 0;
	static uint8_t times= 0;
	if(old_opstatus!=use_OHR.op_status)
	{
		log_info("change %x->%x\n",old_opstatus,use_OHR.op_status);
		old_opstatus = use_OHR.op_status;
	}
	//else
		//return;

	switch(use_OHR.op_status)
	{
		case OHR_STATUS_IDLE:// 0
			cnt = 0;
			resetOHR_module();//????
			Ohr_PowerOn();
			use_OHR.op_status = OHR_STATUS_6D_INIT;
			log_info("OHR_STATUS_IDLE->%x\n",cnt);
			break;
		case OHR_STATUS_6D_INIT:
			cnt = 0;
			//lsm6d_init();
			log_info("OHR_STATUS_6D_INIT->%x\n",cnt);
			use_OHR.op_status = OHR_STATUS_INIT;
			break;
		case OHR_STATUS_INIT:// 2
			//log_info("OHR_STATUS_INIT->%x\n",cnt);
			if(++cnt >4)//7)//1s
			{
				//udelay(200);
				cnt = 0;
				log_info("OHR_STATUS_INIT1->%x\n",cnt);
				//break;
				lsm6d_init();
				if(Ohr_sensors_init())
				{
					//error

					use_OHR.op_status = OHR_STATUS_END;
				}
				else
				{
					use_OHR.op_status = OHR_STATUS_START;
				}
			}
			break;
		case OHR_STATUS_START://3
			cnt = 0;
			log_info("OHR_STATUS_START->%x\n",cnt);
			if(ohr_start(0)==0)
			{
				//success
				use_OHR.op_status = OHR_STATUS_WAIT_RUN;
				if(getrawdata_flag())
				{
						//OHRL_logStep(true);
				}
				if(use_OHR.ohr_1hztimes==0)
					use_OHR.ohr_1hztimes = usr_timer_add(NULL, ohr_check_running_at_1hz, 1000, 1);//1000ms

			}
			else
			{
				use_OHR.op_status = OHR_STATUS_DELAY;
				Ohr_stop();
			}
			break;
		case OHR_STATUS_DELAY://4
			if(++cnt>3)//0.75s
			{
				cnt = 0;
				if(++retry_cnt>3)
				{
					use_OHR.op_status = OHR_STATUS_END;
				}
				else
				{
					use_OHR.op_status = OHR_STATUS_INIT;
					log_info("wait %x\n",retry_cnt);
				}
			}
			break;
		case OHR_STATUS_WAIT_RUN:
			//if(++cnt>10)//1s
			{
				cnt = 0;
				use_OHR.op_status = OHR_STATUS_RUN;
			}
		case OHR_STATUS_RUN://   4
			{
			 	//uint16_t rr_buf[10];
				//uint8_t len = get_hrvRR(rr_buf);
	  			//set_RR_by_ble(rr_buf,len);
	  			log_info("K");
	  			ohr_4hz_event();
			}
			if(++times >4)
			{
				times = 0;
				send_logBuf();
			}
			cnt = 0;
			retry_cnt = 0;
			break;
		case OHR_STATUS_END://   5
			cnt = 0;
			retry_cnt = 0;
			Ohr_stop();
			if(use_OHR.ohr_times!=0)
			{
				usr_timer_del(use_OHR.ohr_times);
				use_OHR.ohr_times = 0;
			}
			break;

		default:
			break;
	}
}

void cardiotachemoterInt(void)
{
	ohr_4hz_event();
	//use_OHR.changecurflg =OHRL_IrqDatatProcess(&use_OHR.OHR_fifo,&use_OHR.ACC_fifo);

}

void use_OHR_init(void)
{
	use_OHR.ohr_times = 0;
	use_OHR.op_status = OHR_STATUS_IDLE;
}
void  cardiotachometer_poweron(void)
{
	cardioPinCfgint();
	use_OHR_init();
	//OHR_I2C_init();
	log_info("--->gsensor iic open\n");
	//qlio_ext_interrupt_init();

	//return;
	

	OHR_POWER_OFF();
	sport_data_mem.clearSportData = clear_colarie_data;
	sport_data_mem.closeHeartModule = HRS_exit;
	sport_data_mem.openHeartModule = start_ohr_module;
	
	ohr_drv_init();
	OHRL_init((uint32_t*)encryptbuf,0,0,0,OhrRawbuf);
	Initial_Pedometer(5,1);

	sport_data_mem.stride_length = 6000; // x100 in cm
	log_info("--->gsensor spi??? open\n");

	
	//Initial_Pedometer(PCB_Z_UP_X_WRIST,1);
	//OHRL_init(use_OHR.block_id,ELECTRIC_CUR_AUTO,0,0,use_OHR.ohrlogbuff);
	
	
	ql_lsm6d_drive_init();

	use_OHR.ohr_times = usr_timer_add(NULL,OhrProStatus,250,1);//250MS
	
	log_info("--->gsensor open end\n");

}
void cardiotachometer_poweroff(void)
{
	if(use_OHR.ohr_times!=0)
	{
		usr_timer_del(use_OHR.ohr_times);
		use_OHR.ohr_times = 0;
	}

	if(use_OHR.ohr_1hztimes!=0)
	{
		usr_timer_del(use_OHR.ohr_1hztimes);
		use_OHR.ohr_1hztimes = 0;
	}
	log_info("--->gsensor iic close\n");
	OHR_I2C_uninit();
	cardioPinCfgUnint();
	qlio_ext_interrupt_unint();
	use_OHR_init();
}

#endif



