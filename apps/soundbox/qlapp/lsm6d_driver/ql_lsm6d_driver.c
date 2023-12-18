
/***********************************************************************
*                               include files
************************************************************************
*/
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "system/includes.h"


#include "system/timer.h"
#include "gsensor.h"

//#include "data_management.h"
#include "ql_lsm6d_driver.h"
#include "ql_lsm6d_reg.h"
//#include "nrf_log.h"
#include "Ohrdrive.h"
//#include "main.h"
//#include "ble_nus.h"
//#include "ble_srv_common.h"
//#include "custome_ble_data_handler.h"

/***********************************************************************
*                            macro define
************************************************************************
*/
//#define	WRITE_DEVADDR           (0xd4)
//#define	READ_DEVADDR            (0xd5)

#define LSM6DSL_DEBUG_ENABLE
#ifdef LSM6DSL_DEBUG_ENABLE
#define log_info(fmt, ...)    printf("[6DDRV] "fmt, ##__VA_ARGS__)
#endif


/*****************6D SENSOR***********************/



//-----------------ACC&GYRO-------------------------
#define ADD_GYROSCOPE
//-------------------------------------------------
#define POWER_LEN	4
const uint8_t xPower[POWER_LEN]={1,2,4,8};
#define FIFO_LEN_MAX		(18*xPower[POWER_LEN-1])	//13+5=18 reserve 5;26*2/4*xPower[POWER_LEN-1]		//4hz
//=================================================================

//bool lis3d_enable = true;
//unsigned char my_x_send;//用来生产测试传输xyz数据的
//unsigned char my_y_send;
//unsigned char my_z_send;

 //unsigned char FX_raw[3];
 //unsigned char FY_raw[3];
 //unsigned char FZ_raw[3];
 
//int8_t xyz_buf[XYZ_BUF_LEN];

uint8_t rawdata[2048]={0};
uint16_t rawlen=0;
uint8_t raw_buf[2048]={0};
uint16_t raw_size=0;
uint8_t *rdp = raw_buf;
uint8_t sequ = 0;
uint8_t odr_setting = 0;//set out data freq clk
uint8_t xl_index=0;
#define lsm6d_delayms(x) udelay(1000*x)
#define lsm6d_delayus(x) udelay(x)

#define read_byte(x)             spi_recv_byte(SPI2, x)
#define write_byte(x)           spi_send_byte(SPI2, x)
#define dma_read(x, y)          spi_dma_recv(SPI2, x, y)
#define dma_write(x, y)         spi_dma_send(SPI2, x, y)
#define set_width(x)            spi_set_bit_mode(SPI2, x)

void lsm6dsl_cs_init(void)
{
	gpio_set_pull_up(TCFG_HW_SPI2_CS, 1);
    gpio_set_die(TCFG_HW_SPI2_CS, 1);
    gpio_set_direction(TCFG_HW_SPI2_CS, 0);
    gpio_write(TCFG_HW_SPI2_CS, 1);
}
void lsm6dsl_cs_uninit(void)
{
	gpio_set_pull_up(TCFG_HW_SPI2_CS, 0);
    gpio_set_die(TCFG_HW_SPI2_CS, 1);
    gpio_set_direction(TCFG_HW_SPI2_CS, 1);
    //gpio_write(TCFG_HW_SPI2_CS, 0);
}

void lsm6dsl_cs_low(void)
{
	gpio_write(TCFG_HW_SPI2_CS,0);
}
void lsm6dsl_cs_high(void)
{
	gpio_write(TCFG_HW_SPI2_CS,1);
}
/*************************************************************************   
**	function name:	lsm6d_write_regs 
**	description:	                
**	input para:		 	
**	return:					                                         
**************************************************************************/

uint8_t lsm6d_write_regs(uint8_t reg, uint8_t *bufp,uint16_t len)
{
	#if 1
	uint8_t buf[16] = {0};
	clr_wdt();
    buf[0] = reg;
    memcpy(&buf[1], bufp, len);
	lsm6dsl_cs_low();
	lsm6d_delayus(5);
	#if 0
	for(uint16_t i = 0;i<len+1;i++)
	{
		write_byte(buf[i]);
		log_info("-%x,%x\n",i,buf[i]);
	}
	#endif
	#if 1
	if(dma_write(buf,len+1)<0)
	{
		log_info("--->gsensor writer error 2\n");
	}
	#endif
	lsm6d_delayus(5);
	lsm6dsl_cs_high();
	//log_info("--->spi%x,%x,%x\n",reg,len,bufp[0]);
	return TRUE;
	#else
	uint8_t flag = 0;
	lsm6dsl_cs_low();
	if(write_byte(reg)==0)
	{
		dma_write(bufp,len);
		flag = 1;
	}
	else
	{
		log_info("--->gsensor writer error 2\n");
	}
	lsm6dsl_cs_high();
	return flag;
	#endif
}

/*************************************************************************   
**  function name:  lsm6d_read_regs 
**  description:					  
**  input para:		  
**  return:														   
**************************************************************************/
uint8_t lsm6d_read_regs( uint8_t reg, uint8_t *bufp,uint16_t len,bool is_addr_inc)
{

#if 1
	int err;
	//lsm6d_delayus(500);
	clr_wdt();
	lsm6dsl_cs_low();
	lsm6d_delayus(5);
	//log_info("--->gsensor read %x,len %x\n",reg,len);
	if(write_byte(reg|LSM6DSL_ADD_READ)==0)
	{
		for(uint16_t i = 0;i<len;i++)
		{
			bufp[i]=read_byte(&err);
			clr_wdt();
		}
		//dma_read(bufp, len);
		#if 0
		//flag = 1;
		//log_info("--->spi read %x,len%x\n",reg,len);
		for(uint8_t i = 0;i<len;i++)
		{
		  log_info(">-%x,%x",i,bufp[i]);
		}
		#endif
	}
	else
	{
		log_info("--->spi writer error\n");
	}
	lsm6d_delayus(5);
	lsm6dsl_cs_high();

	return TRUE;
#else
	u8 i;
	uint8_t flag =0;
	//int err;
	lsm6dsl_cs_low();
	//lsm6dsl_delay(BOOT_TIME);
	log_info("--->gsensor writer %x\n",reg);
	if(write_byte(reg|LSM6DSL_ADD_READ)==0)
	{
		dma_read(bufp, len);
		flag = 1;
		log_info("--->gsensor read %x\n",reg);
		for(i = 0;i<len;i++)
		{
		  log_info("-%x,%x",i,bufp[i]);
		}
	}
	else
	{
		log_info("--->gsensor writer error\n");
		flag = 0;//bad
	}
	//lsm6dsl_delay(BOOT_TIME);
	lsm6dsl_cs_high();

	return flag;
#endif
}

/*************************************************************************   
**	function name:	lsm6d_who_am_i 
**	description:	                
**	input para:		 	
**	return:					                                          
**************************************************************************/
uint8_t lsm6d_who_am_i(void)      
{
    uint8_t dat=0;    
     
    lsm6d_read_regs(0x0F, &dat, 1, false);
	//lis3d_enable=true;
    if (dat == 0x6a)
	{
		log_info("who_am_i_6d,OK = 0x%02x",dat);
      	return TRUE;
	}
    else
    { 
    	log_info("who_am_i_6d,BAD= 0x%02x",dat);
      	return FALSE;
    }
}
//----------------------------------------------------
void set_lsm6d_odr(uint8_t odr)
{
	uint8_t dat;
	uint8_t odr_xl=0;
	uint8_t odr_gy=0;
	
	switch(odr)
	{
		case 0: //26hz
				odr_xl = LSM6DSL_XL_ODR_26Hz<<4;
				odr_gy = LSM6DSL_GY_ODR_26Hz<<4;
			break;
		case 1: //52hz
				odr_xl = LSM6DSL_XL_ODR_52Hz<<4;
				odr_gy = LSM6DSL_GY_ODR_52Hz<<4;
			break;
		case 2: //104hz
				odr_xl = LSM6DSL_XL_ODR_104Hz<<4;
				odr_gy = LSM6DSL_GY_ODR_104Hz<<4;
			break;
		case 3: //208hz
				odr_xl = LSM6DSL_XL_ODR_208Hz<<4;
				odr_gy = LSM6DSL_GY_ODR_208Hz<<4;
			break;
	}
	
	lsm6d_read_regs(LSM6DSL_CTRL1_XL, &dat, 1,false);
	dat &= 0x0f;	
	dat |= odr_xl;
	lsm6d_write_regs(LSM6DSL_CTRL1_XL, &dat, 1);
	
	lsm6d_read_regs(LSM6DSL_CTRL1_XL, &dat, 1,false);
	log_info("ctrl1_xl: %x",dat);
	
#ifdef ADD_GYROSCOPE
	lsm6d_read_regs(LSM6DSL_CTRL2_G, &dat, 1,false);
	dat &= 0x0f;
	dat |= odr_gy;
	
	lsm6d_write_regs(LSM6DSL_CTRL2_G, &dat, 1);
#endif
}
//--------------------------------------------------------
void set_batching_data_rate(uint8_t bdr)
{
	uint8_t dat=0;
	uint8_t odr_fifo=0;
	
	switch(bdr)
	{
		case 0:
			odr_fifo = LSM6DSL_FIFO_26Hz;
			break;
		case 1:
			odr_fifo = LSM6DSL_FIFO_52Hz;
			break;
		case 2:
			odr_fifo = LSM6DSL_FIFO_104Hz;
			break;
		case 3:
			odr_fifo = LSM6DSL_FIFO_208Hz;
			break;
	}
	
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL5, &dat, 1,false);
	dat &= 0x87;
	dat |= (odr_fifo<<3);

	log_info("FIFO_ODR_dat: %x",dat);
	lsm6d_write_regs(LSM6DSL_FIFO_CTRL5, &dat, 1);
	
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL5, &dat, 1,false);
	log_info("FIFO_ODR_CTRL5: %x",dat);
	
}
#if 0
void set_batching_data_rate(uint8_t bdr)
{
	uint8_t dat=0;
	uint8_t bdr_xl=0;
	uint8_t bdr_gy=0;
	
	switch(bdr)
	{
		case 0:
			bdr_xl = LSM6DSO_XL_BATCHED_AT_26Hz;
			bdr_gy = LSM6DSO_GY_BATCHED_AT_26Hz<<4;
			break;
		case 1:
			bdr_xl = LSM6DSO_XL_BATCHED_AT_52Hz;
			bdr_gy = LSM6DSO_GY_BATCHED_AT_52Hz<<4;
			break;
		case 2:
			bdr_xl = LSM6DSO_XL_BATCHED_AT_104Hz;
			bdr_gy = LSM6DSO_GY_BATCHED_AT_104Hz<<4;
			break;
		case 3:
			bdr_xl = LSM6DSO_XL_BATCHED_AT_208Hz;
			bdr_gy = LSM6DSO_GY_BATCHED_AT_208Hz<<4;
			break;
	}
	
	lsm6d_read_regs(LSM6DSO_FIFO_CTRL3, &dat, 1,false);
	dat &= 0xf0;
	dat |= bdr_xl;	
	lsm6d_write_regs(LSM6DSO_FIFO_CTRL3, &dat, 1);
	
#ifdef ADD_GYROSCOPE
	lsm6d_read_regs(LSM6DSO_FIFO_CTRL3, &dat, 1,false);
	dat &= 0x0f;
	dat |= bdr_gy;
	lsm6d_write_regs(LSM6DSO_FIFO_CTRL3, &dat, 1);
#endif
	
}
#endif

void lsm6d_fifo_mode_set(uint8_t fifo_mode)
{
	uint8_t dat=0;
	
	log_info("fifo mode: %d",fifo_mode);	//0=disable;6=fifo stream
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL5, &dat, 1,false);
	dat &= 0xf8;
	dat |= fifo_mode;
	lsm6d_write_regs(LSM6DSL_FIFO_CTRL5, &dat, 1);
	
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL5, &dat, 1,false);
	log_info("fifo_ctrl5: %x",dat);
}
/*************************************************************************   
**	function name:	lsm6d_config
**	input para:		 	
**	return:					                                         
**************************************************************************/
void lsm6d_config(void)
{
	uint8_t dat;
		//-----------Restore default configuration---------
	lsm6d_read_regs(LSM6DSL_CTRL3_C, &dat, 1,false);
	dat |= 1;	//sw reset
	lsm6d_write_regs(LSM6DSL_CTRL3_C, &dat, 1);
	do{
		lsm6d_read_regs(LSM6DSL_CTRL3_C, &dat, 1,false);
	}while(dat&0x01);//wait reset finshed
	log_info("CTRL3_C reset %x\n",dat);
	//delay_2ms(300);
	//lsm6d_who_am_i();
	//-------------Enable Block Data Update------------
	lsm6d_read_regs(LSM6DSL_CTRL3_C, &dat, 1 ,false);
	dat |= 0x40;	//bdu
	lsm6d_write_regs(LSM6DSL_CTRL3_C, &dat, 1);// block data no updated, until MSB and LSB have been read
	////////////?
	lsm6d_read_regs(LSM6DSL_CTRL3_C, &dat, 1,false);
	log_info("CTRL3_C %x\n",dat);
	///////////////?
	//------------Set full scale------------------------
	lsm6d_read_regs(LSM6DSL_CTRL1_XL, &dat, 1,false);
	dat &= 0xf3;
	dat |= LSM6DSL_8g<<2;
	lsm6d_write_regs(LSM6DSL_CTRL1_XL, &dat, 1);//8g
	////////////?
	lsm6d_read_regs(LSM6DSL_CTRL1_XL, &dat, 1,false);
	log_info("CTRL1_XL %x\n",dat);
	///////////////?
	
#ifdef ADD_GYROSCOPE
	lsm6d_read_regs(LSM6DSL_CTRL2_G, &dat, 1,false);
	dat &= 0xf3;
	//dat |= LSM6DSL_1000dps<<2;
	dat |= LSM6DSL_1000dps<<1;
	lsm6d_write_regs(LSM6DSL_CTRL2_G, &dat, 1);
	////////////?
	lsm6d_read_regs(LSM6DSL_CTRL2_G, &dat, 1,false);
	log_info("CTRL2_G %x\n",dat);
	///////////////?
#endif
	//------------------------------------------------
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL3, &dat, 1,false);
	dat &= 0xc0;
	dat |= 0x12;//////0x09;
	lsm6d_write_regs(LSM6DSL_FIFO_CTRL3, &dat, 1);
	////////////?
	lsm6d_read_regs(LSM6DSL_FIFO_CTRL3, &dat, 1,false);
	log_info("CTRL3 %x\n",dat);
	///////////////?
	
	//----------FIFO Watermark------------------------
//	lsm6d_read_regs(LSM6DSL_FIFO_CTRL1, &dat, 1,false);
//	dat = 10*12;
//	lsm6d_write_regs(LSM6DSL_FIFO_CTRL1, &dat, 1);
	//----------FIFO Batching data rate---------------
	set_batching_data_rate(odr_setting);
	//-?????/-------------disable fifo----------------------
	lsm6d_fifo_mode_set(LSM6DSL_FIFO_MODE);//LSM6DSL_BYPASS_MODE);
	//------------- Set Output Data Rate---------------
	set_lsm6d_odr(odr_setting);

}
/*************************************************************************   
**	function name:	lsm3dh_init 
**	description:	                
**	input para:		 	
**	return:					                                         
**************************************************************************/
void lsm6d_init(void)
{
    	
		if(lsm6d_who_am_i()==TRUE)
		//if(lis3d_enable)
		{
			lsm6d_config();
		}
		else 
		{
			//nrf_gpio_cfg_input(LSM6D_MISO_PIN, NRF_GPIO_PIN_PULLUP);
			log_info("who_am_i bad!");
		}
}
/*************************************************************************   
**	function name:	lis3dh_powerdown 
**	description:	                
**	input para:		 	
**	return:					                                         
**************************************************************************/
void lis3dh_powerdown(void)
{
    uint8_t dat;
		
	lsm6d_fifo_mode_set(LSM6DSL_BYPASS_MODE);		//clear fifo buffer
	//------------- Set Output Data Rate---------------
	lsm6d_read_regs(LSM6DSL_CTRL1_XL, &dat, 1,false);
	dat &= 0x0f;
	dat |= LSM6DSL_XL_ODR_OFF<<4;
	lsm6d_write_regs(LSM6DSL_CTRL1_XL, &dat, 1);

	lsm6d_read_regs(LSM6DSL_CTRL2_G, &dat, 1,false);
	dat &= 0x0f;
	dat |= LSM6DSL_GY_ODR_OFF<<4;
	lsm6d_write_regs(LSM6DSL_CTRL2_G, &dat, 1);

	lsm6d_delayus(500);
	lsm6dsl_cs_uninit();
	//2// 1: SPI1    2: SPI2
	spi_close(SPI2);
	
}
/*************************************************************************   
**	function name:	lis3dh_lowpower_odr10hz 
**	description:	                
**	input para:		 	
**	return:					                                         
**************************************************************************/
void lis3dh_lowpower_odr10hz(void)
{
	lsm6d_fifo_mode_set(LSM6DSL_BYPASS_MODE);	//clear fifo buffer
	set_lsm6d_odr(odr_setting);
	set_batching_data_rate(odr_setting);
	lsm6d_fifo_mode_set(LSM6DSL_STREAM_MODE);
}
/*************************************************************************   
**	function name:	lis3dh_lowpower_odr200hz 
**	description:	                
**	input para:		 	
**	return:					                                         
**************************************************************************/
void lis3dh_lowpower_odr200hz(void)
{
//    uint8_t dat;
	set_lsm6d_odr(odr_setting);
		
}

void read_6d_sensor_data(void)
{
		uint8_t tempu[20]={0};
		int16_t sensor_data[6]={0};
		uint8_t i=0;
		uint8_t j=0;
		
		lsm6d_read_regs(LSM6DSL_OUTX_L_G , tempu, 12,false);//gy + acc	
		for(i = 0;i<6;i++)
		{
			sensor_data[i] = (int16_t)((tempu[j+1]<<8) | tempu[j]);//+32768;
			j+=2;
		}	
		
		//log_info("gyr:%d %d %d",sensor_data[0],sensor_data[1],sensor_data[2]);
		log_info("acc:%d %d %d",sensor_data[3],sensor_data[4],sensor_data[5]);
}

//------------------------------------------
void GetACCdata(typ_fifodata * fifo)
{
		uint8_t buf[10*6];
		uint8_t tmp[2]={0};
		uint16_t length = 0;
		uint16_t xl_len=0;
		uint16_t gy_len=0;
		
		uint8_t xl_buf[100*6]={0};
		uint8_t gy_buf[100*6]={0};
		
		lsm6d_read_regs(LSM6DSL_FIFO_STATUS1,tmp,2,false);
		length = ((tmp[1] & 0x07)<<8) + tmp[0];
		log_info("-----------fifo_len: %d----------",length);
		length /= 6;
		
		if(length>FIFO_LEN_MAX/2)
		{	
			lsm6d_fifo_mode_set(LSM6DSL_BYPASS_MODE);		//clear fifo buffer
			log_info("Clear fifo");
			length = 0;
			lsm6d_fifo_mode_set(LSM6DSL_STREAM_MODE);
		}
		
		rawlen = 0;
		
		//if(tmp[1]&0x80)
		{
				while(length-->0)
				{
					clr_wdt();
					//---------------Gyro-----------------------------------
					lsm6d_read_regs(LSM6DSL_FIFO_DATA_OUT_L,buf,6,false);	//read gyro raw data
					memcpy(&gy_buf[gy_len],buf,6);
					gy_len += 6;
					memcpy((rawdata+rawlen),buf,6);
					rawlen+=6;
					//log_info("gy_len: %d,[0]=%d,[1]=%d,%d,%d,%d,%d",gy_len,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
					clr_wdt();
					//---------------Acce------------------------------------
					lsm6d_read_regs(LSM6DSL_FIFO_DATA_OUT_L,buf,6,false);	//read acc raw data
					if((xl_index%(xPower[odr_setting]))==0)
					{
							memcpy(&xl_buf[xl_len],buf,6);
							xl_len += 6;
					}
					memcpy((rawdata+rawlen),buf,6);
					rawlen+=6;
					//log_info("xl_len: %d,[0]=%d,[1]=%d,%d,%d,%d,%d",xl_len,buf[0]<<8|buf[1],buf[2],buf[3],buf[4],buf[5]);
					xl_index++;			//all ways increase until overflow
				}
		}
		//-------copy raw data to send buf------------
		#if 0
		if(raw_size == 0)
		{
			memcpy(raw_buf,rawdata,rawlen);
			raw_size = rawlen;
			rdp = raw_buf;
//			sequ = 0;
		}
		#endif
		//------------------------------------------------------------

		memcpy(fifo->data.bytes,xl_buf, xl_len); 
        //log_info("length = %d\r\n",length);
    	fifo->datalen = xl_len;
		log_info("xl_len: %d",xl_len);
		log_info("gy_len: %d",gy_len);
		//log_info("totalstep:%d",PedoSummary.TotalSteps_ul);
		log_info("x= %d y %d z %d\r\n",fifo->data.words[0],fifo->data.words[1],fifo->data.words[2]);
		//=============================================================
	   //Mem.Day.Steps = PedoSummary.TotalSteps_ul;
	
     //setPage4Info(heartValue);
}

//------------------------------------------
void GetACCdata1(typ_fifodata * fifo)
{
#if 0
		uint8_t buf[10*6];
		uint8_t xl_buf[100*6]={0};
		uint8_t gy_buf[100*6]={0};
		uint16_t xl_len=0;
		uint16_t gy_len=0;
    uint16_t length = 0;
		uint8_t tmp[2]={0};
		uint8_t tag_sensor=0;

		lsm6d_read_regs(LSM6DSL_FIFO_STATUS1,tmp,2,false);
		//lsm6d_read_regs(LSM6DSO_FIFO_STATUS2,(tmp+1),1,false);
		//log_info("f1: 0x%2x",tmp[0]);
		//log_info("f2: 0x%2x",tmp[1]);
		
		length = ((tmp[1] & 0x03)<<8) + tmp[0];
		log_info("-----------fifo_len: %d----------",length);
		
		if(length>FIFO_LEN_MAX)
		{	
			lsm6d_fifo_mode_set(LSM6DSL_BYPASS_MODE);		//clear fifo buffer
			log_info("Clear fifo");
			length = 0;
			lsm6d_fifo_mode_set(LSM6DSL_STREAM_MODE);
		}
		rawlen = 0;
#if 0		
		while(length--)
		{
			lsm6d_read_regs(LSM6DSO_FIFO_DATA_OUT_TAG,&tag_sensor,1,false);
			tag_sensor >>= 3;
			//log_info("tag_sensor: %d",tag_sensor);
			switch(tag_sensor)//tag_sensor
			{
				case LSM6DSO_XL_NC_TAG:
					lsm6d_read_regs(LSM6DSO_FIFO_DATA_OUT_X_L,buf,6,false);	//read acc raw data
					if(xl_index%(xPower[odr_setting])==0)
					{
							memcpy(&xl_buf[xl_len],buf,6);
							xl_len += 6;
					}
					memcpy((rawdata+rawlen),buf,6);
					rawlen+=6;
					//log_info("xl_len: %d",xl_len);
					xl_index++;			//all ways increase until overflow
					break;
				case LSM6DSO_GYRO_NC_TAG:
					lsm6d_read_regs(LSM6DSO_FIFO_DATA_OUT_X_L,buf,6,false);		//read gyro raw data
					memcpy(&gy_buf[gy_len],buf,6);
					gy_len += 6;
					memcpy((rawdata+rawlen),buf,6);
					rawlen+=6;
					//log_info("gy_len: %d",gy_len);
					break;
				
				default: break;
			}
		}
#endif
		//-------copy raw data to send buf------------
		if(raw_size == 0)
		{
			memcpy(raw_buf,rawdata,rawlen);
			raw_size = rawlen;
			rdp = raw_buf;
//			sequ = 0;
		}
		//------------------------------------------------------------

		memcpy(fifo->data.bytes,xl_buf, xl_len); 
//        my_printf("length = %d\r\n",length);
    fifo->datalen = xl_len;
		log_info("xl_len: %d",xl_len);
		log_info("gy_len: %d",gy_len);
		//=============================================================
	   Mem.Day.Steps = PedoSummary.TotalSteps_ul;
	
     setPage4Info(heartValue);
#endif
}
//-------------------------------------------------------
void ble_raw_data_lsm6d_notify(void)
{
	#if 0
		uint8_t send_len=144;
		uint8_t send_buf[150];
	
		if(isEnblaedNotify==false)
			return ;
		
		if(raw_size)
		{
				if(raw_size>=144)
					send_len = 144;
				else
					send_len = raw_size;
				
				raw_size -= send_len;
				
				send_buf[0] = 0xff;
				send_buf[1] = 4+send_len+1;	//1 is sequence
				send_buf[2] = RAW_DATA_6D_NOTIFY;	//cmd
				send_buf[3] = sequ++;	
				
				memcpy((send_buf+4),rdp,send_len);
				
				send_buf[send_buf[1]-1] = get_checksum(send_buf,(send_buf[1]-1));
				ble_updateData(send_buf,(send_buf[1]));
				rdp = raw_buf+send_len;
		}
		#endif
}



void ql_lsm6d_drive_init(void)
{
	log_info("--->gsensor open\n");
	lsm6dsl_cs_init();
	//2// 1: SPI1    2: SPI2
	spi_open(SPI2);
	odr_setting =0;

}




