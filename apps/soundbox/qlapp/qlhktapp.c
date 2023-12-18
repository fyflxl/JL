#include "app_config.h"
#include "asm/cpu.h"
#include "generic/typedef.h"

#include "system/includes.h"
#include "asm/uart_dev.h"
#include "uartPcmSender.h"
#include "qlhktapp.h"
#include "app_main.h"
#include "ui_manage.h"
#define LOG_TAG             "[APP_HKT]"
#define LOG_INFO_ENABLE
#define PRINTF(format, ...)         printf(format, ## __VA_ARGS__)
#ifdef LOG_INFO_ENABLE
#define log_info(format, ...)       PRINTF("[Info] : [APP_HKT]" format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif
DATA_HKT_RX hkt_rx;

#define HKT_HEAD               "AT"
//设置模块工作参数
#define HKT_SET_PARM           "+DMOGRP:"
//设置模块省电模式
#define HKT_SET_SAVEBAT        "+DMOSAV:"
//获得模块版本号
#define HKT_GET_VER            "+DMOVER:"
//设置模块音量
#define HKT_SET_VOL            "+DMOVOL:"
//设置模块声控级别
#define HKT_SET_VOX            "+DMOVOX:"
//设置模块功能
#define HKT_SET_FUN            "+DMOFUN:"
//发送和接收模块短信息
#define HKT_SET_MESSAGE        "+DMOMES:"
#define HKT_SEND_CMD_LEN       7
#define HKT_GET_CMD_LEN        8

typedef enum
{
	HTK_CMD_GRP,
	HTK_CMD_SAVE,
	HTK_CMD_VER,
	HTK_CMD_VOL,
	HTK_CMD_VOX,
	HTK_CMD_FUN,
	HTK_CMD_MESS,
	HTK_CMD_END

}HTK_SEND_INDEX;

//设置模块工作参数"+DMOGRP:"
//parm0:400mhz=0  ... 470mhz=55999,step = 1.25k,parm0%5==0或者parm0%2==0有效
//parm1:400mhz=0  ... 470mhz=55999,step = 1.25kparm0%5==0或者parm0%2==0有效
shtkparam ghtkinf;
shtkstatus ghtkstat;

u8 hextostr(u8 valu)
{
	valu = valu&0x0f;
	if(valu>0x09)
		valu = 'A'+(valu-0xa);
	else
		valu = '0'+valu;
	return valu;
}
void Htk_freqtostr(u8*buf,u16 rfv)
{
	/*freq = 450.02500mhz
	rfv = (450.02500-400.00000)mhz/1250hz= 4002(0xfa2)
	4----------------------------4
	rfv = 4002
	rfv / 8000 = 0---------------0
	rfv % 8000 = 4002
	rfv / 800 = 5----------------5
	rfv % 800 = 2
	rfv / 80  = 0----------------0
	.----------------------------.
	rfv % 80  = 2
	rfv / 8   = 0----------------0
	rfv % 8   = 2
	rfv * 125 = 250
	rfv / 100 = 2----------------2
	rfv % 100 = 50
	rfv / 10  = 5----------------5
	rfv % 10  = 0
	rfv       = 0----------------0
	'4050.0250'
	*/
	buf[0] = '4';
	buf[1] = '0' + (u8)(rfv/8000);//10mhz
	rfv = rfv%8000;
	buf[2] = '0' + (u8)(rfv/800);//1mhz
	buf[3] = '.' ;
	rfv = rfv % 800;
	buf[4] = '0' + (u8)(rfv/80);//0.1mkhz
	rfv = rfv % 80;
	buf[5] = '0' + (u8)(rfv/8);//0.01mhz
	rfv = rfv % 8;
	rfv = rfv*125;/* 0.00001mhz */
	buf[6] = '0' + (u8)(rfv/100);//0.001mhz
	rfv = rfv%100;
	buf[7] = '0' + (u8)(rfv/10);//0.0001mhz
	rfv = rfv%10;
	buf[8] = '0' + (u8)(rfv);//0.00001mhz
}
void Htk_bcdtostr(u8*buf,u16 val)
{
	buf[0] =(u8)((val&0xff00)>>8);
	buf[2] = (u8)(val&0x00ff);

}
u8 tmpdata[HTK_MESSAGE_MAX_LEN];

void Htk_sentcom(HTK_SEND_INDEX type)
{
	u8 temp,len;

	switch(type)
	{
		case HTK_CMD_GRP:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_PARM,7);
			tmpdata[9] = '=';
			Htk_freqtostr(&tmpdata[10],ghtkinf.RFV);
			tmpdata[19] =',';
			Htk_freqtostr(&tmpdata[20],ghtkinf.TFV);
			tmpdata[29] =',';
			Htk_bcdtostr(&tmpdata[30],ghtkinf.RXCT);
			tmpdata[32] =',';
			Htk_bcdtostr(&tmpdata[35],ghtkinf.TXCT);
			tmpdata[35] = ',';
			temp = ghtkinf.setbit0 &0x03;/* flag */
			tmpdata[36] = '0'+temp;
			tmpdata[37] = ',';
			temp = ((ghtkinf.setbit0 & 0x0c)>>2);
			tmpdata[38] = '0'+temp;
			len = 39;
			//setBit(ghtkinf.sendbitflag, HTK_CMD_GRP);
			break;
		case HTK_CMD_SAVE:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_SAVEBAT,7);
			tmpdata[9] = '=';
			temp = (ghtkinf.setbit0 & 0x10)>>4;
			tmpdata[10] = '0'+temp;
			len = 11;
			//setBit(ghtkinf.sendbitflag, HTK_CMD_SAVE);
			break;
		case HTK_CMD_VER:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_GET_VER,7);
			len = 9;
			//setBit(ghtkinf.sendbitflag, HTK_CMD_VER);
			break;
		case HTK_CMD_VOL:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_VOL,7);
			tmpdata[9] = '=';
			tmpdata[10] = '0'+HTK_VOL_DEFAULT;
			len = 11;
			//setBit(ghtkinf.sendbitflag, HTK_CMD_VOL);
			break;
		#if 0
		case HTK_CMD_VOX:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_VOX,7);
			tmpdata[9] = '=';
			tmpdata[10] = '0'+HTK_VOL_DEFAULT;
			len = 11;
			setBit(ghtkinf.sendbitflag, HTK_CMD_VOX);
			break;
		case HTK_CMD_FUN:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_FUN,7);
			tmpdata[9] = '=';
			tmpdata[10] = '0'+HTK_FUN_SQ_DEFAULT;
			tmpdata[11] = ',';
			tmpdata[12] = '0'+HTK_FUN_MICLVL_DEFAULT;
			tmpdata[13] = ',';
			tmpdata[14] = '0'+HTK_FUN_TOT_DEFAULT;
			tmpdata[15] = ',';
			tmpdata[16] = '0'+HTK_FUN_SCRAMLVL_DEFAULT;
			tmpdata[17] = ',';
			tmpdata[18] = '0'+HTK_FUN_COMP_DEFULT;
			len = 19;
			//setBit(ghtkinf.sendbitflag, HTK_CMD_FUN);
			break;
		#endif
		case HTK_CMD_MESS:
			memcpy(tmpdata,HKT_HEAD,2);
			memcpy(&tmpdata[2],HKT_SET_MESSAGE,7);
			tmpdata[9]='=';
			sendOhrDataToHkt(&tmpdata[10]);
			len = tmpdata[10]+10;

		break;
		default:
			return;
	}
	tmpdata[++len] = 0x0d;
	tmpdata[++len] = 0x0a;
	tmpdata[++len] = 0x0;
	len++;
	log_info("sencom%d, \n",len);
	//for(temp = 0;temp <len;temp++)
	for(temp = 0;temp <2;temp++)
	{
		clr_wdt();
		printf(" %d %x,",temp,tmpdata[temp]);
	}

	
	#ifdef AUDIO_PCM_DEBUG
	uartSendData(tmpdata,len);
	#endif
	if(hkt_rx.analytime==0)
		hkt_rx.analytime =  usr_timer_add(NULL, app_CommunDataAnaly, 100, 1);
	//if(user_var.hkttime==0)
	//	user_var.hkttime =usr_timer_add(NULL, Htk_repeatsend, 1000, 0);//usr_timer 1000ms
}

void Htk_repeatsend(void *priv)
{
	u8 i,flag = 0;
	for(i = HTK_CMD_GRP;i<HTK_CMD_MESS;i++)
	{
		if(getBit(ghtkinf.sendbitflag, i))
		{
			Htk_sentcom(i);
			flag = 1;
		}
	}
	if(flag == 0)
	{
		if(user_var.hkttime!=0)
		{
			usr_timer_del(user_var.hkttime);
			user_var.hkttime =0;
		}
	}
}

#define MIN_HKT_PACKET_SIZE 7
uint8_t testcount = 0;
void Htk_getcomAnaly(u8 * buff,u8 len)
{
	#if 1//0
	if(0==memcmp(buff, HKT_SET_PARM, MIN_HKT_PACKET_SIZE))
	{
		printf("+DMOGRP:%s",buff);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_GRP);
		return;
	}
	if(0==memcmp(buff, HKT_SET_SAVEBAT,MIN_HKT_PACKET_SIZE))
	{
		printf(HKT_SET_SAVEBAT);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_SAVE);
		return;
	}
	#endif
	if(0==memcmp(buff, HKT_GET_VER, MIN_HKT_PACKET_SIZE))
	{
		buff[len] = 0;
		printf("get ver:%s",buff);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_VER);
		return;
	}
	if(0==memcmp(buff,HKT_SET_MESSAGE,MIN_HKT_PACKET_SIZE))
	{
		u8 i = 0;
		testcount++;
		printf("+DMOMES:%x,%d\n",buff[MIN_HKT_PACKET_SIZE+1],testcount);
		#if 0
		for(i = MIN_HKT_PACKET_SIZE;i<len;i++)
		{
			printf("%x",buff[i]);
		}
		printf("\n");
		#endif
	}
	#if 1
	if(0==memcmp(buff, HKT_SET_VOL, MIN_HKT_PACKET_SIZE))
	{
		printf(HKT_SET_VOL);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_VOL);
		return;
	}
	if(0==memcmp(buff, HKT_SET_VOX, MIN_HKT_PACKET_SIZE))
	{
		printf(HKT_SET_VOX);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_VOX);
		return;
	}
	if(0==memcmp(buff, HKT_SET_FUN, MIN_HKT_PACKET_SIZE))
	{
		printf(HKT_SET_FUN);
		clrBit(ghtkinf.sendbitflag, HTK_CMD_FUN);
		return;
	}
	#endif
}
void setHktAdjStat(void* priv)
{
	if(ghtkstat.setbit&0x01==0)
	{
		ghtkstat.setbit|=0x01;
		ui_update_status(STATUS_HTK_ADJ_FREQ);
		if(ghtkstat.adjtime == 0)
			ghtkstat.adjtime = usr_timer_add(NULL, setHktAdjStat, 5000, 0);//5s
	}
	if(ghtkstat.setbit &0x02)
	{
		ghtkstat.setbit &=0xfd;//clear bit 1
		return;
	}
	else
	{
		del_io_led_time_id(ghtkstat.adjtime);
		ghtkstat.adjtime = 0;
		ghtkstat.setbit &=0xfc;//clear bit 0-1
		ui_update_status(STATUS_HTK_PLAY_FREQ);
	}
}
u8 getHktAdjStat(void)
{
	if(ghtkstat.setbit&0x03)
		return true;
	else
		return false;
}
#if 1


//#define INFO_PRINT_DEBUG
#define COM_BAD_DATA 0XFF
void MemClrU8(u8* dec,u8 len)
{
	u8 i;
	for(i = 0;i<len;i++)
	{
		dec[i]= 0;
	}
}
void MemCpyU8(u8* dec,u8* src,u8 len)
{
	u8 i;

	for(i=0;i<len;i++)
	{
		dec[i]=src[i];
	}
	return ;
}
void htk_getDataToCommun(u8* data,u8 len)
{
	u8 i;
	for(i = 0;i<len;i++)
	{
    	hkt_rx.data[hkt_rx.end++]= data[i];
		if( hkt_rx.end >= MAX_SIZE_HKT_LEN )
		{
			hkt_rx.end = 0;
			hkt_rx.end_flag ^= 1;
		}
	}
	//hkt_rx.gettimes = 2;//200ms后处理接收数据
}

void ClearHTKRx(void)
{
    hkt_rx.start= 0x00;
    hkt_rx.end = 0x00;
    hkt_rx.start_flag = 0x00;
    hkt_rx.end_flag = 0x00;
	hkt_rx.analytime = 0;
	hkt_rx.gettimes = 0;
    MemClrU8(hkt_rx.data,MAX_SIZE_HKT_LEN);
}
void HktRxPtrInc( u16 n )
{
	hkt_rx.start+=n;
	if( hkt_rx.start >= MAX_SIZE_HKT_LEN )
	{
		hkt_rx.start -= MAX_SIZE_HKT_LEN;
		hkt_rx.start_flag ^= 1;
	}
}
u8 AdjustHktRxPtr( u8 n )
{
	if( n >= MAX_SIZE_HKT_LEN )
	{
		n -= MAX_SIZE_HKT_LEN;
	}
	return	n;
}

void app_CommunDataAnaly(void *param)
{

		u8	n,count,i;
		u8	buff[MAX_SIZE_HKT_LEN];
		if(hkt_rx.gettimes!=0)
		{
			hkt_rx.gettimes--;
			return;
		}
		#if 0
		if(hkt_rx.analytime !=0)
		{
			usr_timer_del(hkt_rx.analytime);
			hkt_rx.analytime = 0;
		}
#endif

		/*
		 * Get data from MCU_Rx buffer
		 */
		while( 1 )
		{
			/*
			 * get Rx data count
			 * 2 type
			 * MPU_Rx.end>MPU_Rx.start and MPU_Rx.start>MPU_Rx.end
			 */

			if( hkt_rx.start_flag == hkt_rx.end_flag )//start>=end
			{
				count = hkt_rx.end - hkt_rx.start;
			}
			else
			{
				count = MAX_SIZE_HKT_LEN - hkt_rx.start + hkt_rx.end;
			}



			/*
			 * If data count < command min length continue wait
			 */
			if(count < MIN_HKT_PACKET_SIZE)
				break ;
		#ifdef INFO_PRINT_DEBUG
			/* print test */
			log_info("\r\nCO:get_info start:%x",count);
			for( i = 0; i < count; i++ )
			{
				//if((i%10)==0)NS_LOG_INFO("\r\n");
				log_info(",%x ",hkt_rx.data[AdjustHktRxPtr(hkt_rx.start+i)]);
			}
			log_info("\r\n");
		#endif
			/*
			 * Get data start position
			 */
			n = COM_BAD_DATA;
			for( i = 0; i < count; i++ )
			{
				if(( hkt_rx.data[AdjustHktRxPtr(hkt_rx.start)]=='+'))
				{
					n = hkt_rx.start;
					break;
				}
				HktRxPtrInc(1);
				count--;
				if(count < MIN_HKT_PACKET_SIZE)
					break ;
			}
			//NS_LOG_INFO("\r\nfind %x\r\n",n);
			if( n == COM_BAD_DATA )
			{
				//NS_LOG_INFO("\r\nbadA\r\n");
				break;						// not get data start position
			}

			/*
			 * get availability buffer to buff[]
			 */
			if((hkt_rx.start + count) <= MAX_SIZE_HKT_LEN)
				MemCpyU8( &buff[0], &hkt_rx.data[AdjustHktRxPtr( hkt_rx.start )],count);
			else
			{
				i = MAX_SIZE_HKT_LEN - hkt_rx.start;
				MemCpyU8( &buff[0], &hkt_rx.data[AdjustHktRxPtr( hkt_rx.start )], i );
				MemCpyU8( &buff[i], &hkt_rx.data[0], count-i );
			}
			buff[count] = 0;
			Htk_getcomAnaly(buff,count+1);

		#ifdef INFO_FOR_PRINT_TEST
			NS_LOG_INFO("\r\nble OK \r\n");
		#endif
			/*
			 * If data OK,mpu rx pointer + packet_length
			 * Analyse next command packet
			 */
			HktRxPtrInc(count);

			break;
		}

}


#endif
void StopTrans(void)
{
	//HktReceive();
	gpio_set_output_value(TCFG_IO_HKT_PTT, 1);//hkt Ptt
	log_info("--rec");
}
void ContinueTrans2(void* proc)
{
	if(ghtkstat.istrans)
	{
		gpio_set_output_value(TCFG_IO_HKT_PTT, 0);//hkt Ptt
		log_info("++tran");
	}
}
void ContinueTrans(void* proc)
{
	
	HKTtest2();
	//Htk_sentcom(HTK_CMD_MESS);
	if (!ghtkstat.delaycon) {
			usr_timeout_del(ghtkstat.delaycon);
	}
    ghtkstat.delaycon = usr_timeout_add(NULL, ContinueTrans2, 20, 1);//30,35,40,50,100ms ok,
}
void SendHktMess2sPro(void* proc)
{
	static uint8_t tmp = 0;
	tmp++;
	
	//if(IssendOhrDataflag())
	{
		
		StopTrans();
		//
		//Htk_sentcom(HTK_CMD_MESS);
		log_info("#%d",tmp);
		HKTtest2();
		#if 1
		if (!ghtkstat.delaycon) {
			usr_timeout_del(ghtkstat.delaycon);
		}
        ghtkstat.delaycon = usr_timeout_add(NULL, ContinueTrans2, 70, 1);//60-110ms//30,35,40,50,100ms ok,
        #endif

	}
}
void HktCommuninit(void)
{
	ClearHTKRx();

}

void hkt_power_init(void)
{
	//hkt配置 start
	gpio_set_pull_up(TCFG_IO_HKT_POWER, 1);
    gpio_set_pull_down(TCFG_IO_HKT_POWER, 0);
    gpio_set_die(TCFG_IO_HKT_POWER, 1);
	gpio_set_direction(TCFG_IO_HKT_POWER, 0);
	gpio_set_output_value(TCFG_IO_HKT_POWER, 1);//hkt POWER ON

	gpio_set_pull_up(TCFG_IO_HKT_PTT, 1);
    gpio_set_pull_down(TCFG_IO_HKT_PTT, 0);
    gpio_set_die(TCFG_IO_HKT_PTT, 1);
	gpio_set_direction(TCFG_IO_HKT_PTT, 0);
	gpio_set_output_value(TCFG_IO_HKT_PTT, 1);//hkt Ptt

	gpio_set_pull_up(TCFG_IO_HKT_PD, 1);
    gpio_set_pull_down(TCFG_IO_HKT_PD, 0);
    gpio_set_die(TCFG_IO_HKT_PD, 1);
	gpio_set_direction(TCFG_IO_HKT_PD, 0);
	gpio_set_output_value(TCFG_IO_HKT_PD, 1);//hkt Pd sleep=0
	//HKT 配置 end

	
}
void hkt_power_off(void)
{
	//hkt配置 start
	gpio_set_pull_up(TCFG_IO_HKT_POWER, 1);
    gpio_set_pull_down(TCFG_IO_HKT_POWER, 0);
    gpio_set_die(TCFG_IO_HKT_POWER, 1);
	gpio_set_direction(TCFG_IO_HKT_POWER, 0);
	gpio_set_output_value(TCFG_IO_HKT_POWER, 0);//hkt POWER ON

	gpio_set_pull_up(TCFG_IO_HKT_PTT, 1);
    gpio_set_pull_down(TCFG_IO_HKT_PTT, 0);
    gpio_set_die(TCFG_IO_HKT_PTT, 1);
	gpio_set_direction(TCFG_IO_HKT_PTT, 0);
	gpio_set_output_value(TCFG_IO_HKT_PTT, 1);//hkt Ptt

	gpio_set_pull_up(TCFG_IO_HKT_PD, 1);
    gpio_set_pull_down(TCFG_IO_HKT_PD, 0);
    gpio_set_die(TCFG_IO_HKT_PD, 1);
	gpio_set_direction(TCFG_IO_HKT_PD, 0);
	gpio_set_output_value(TCFG_IO_HKT_PD, 0);//hkt Pd sleep=0
	//HKT 配置 end
	if(ghtkstat.delay2s != 0)
	{
		usr_timer_del(ghtkstat.delay2s);
		ghtkstat.delay2s=0;
	}
	if(ghtkstat.adjtime != 0)
	{
		usr_timer_del(ghtkstat.adjtime);
		ghtkstat.adjtime=0;
	}
	if(ghtkstat.delaycon!=0)
	{
		usr_timer_del(ghtkstat.delaycon);
		ghtkstat.delaycon = 0;
	}
	HktCommuninit();

}





void IncHktparm(void)
{
	//HKTtest2();
	#if 1
	u16 freq = ghtkinf.RFV;
	u8 i;
	for(i = 0;i<10;i++)
	{
		freq++;
		if((freq%2==0)||(freq%5==0))
		{
			break;
		}
	}
	if(freq >HTK_FREQ_MAX)freq = HTK_FREQ_MAX;
	ghtkinf.RFV = freq;
	ghtkinf.TFV = freq;
	log_info(" INC : feq%x \n",freq);
	Htk_sentcom(HTK_CMD_GRP);
	ghtkstat.setbit &=0xfd;
	ghtkstat.setbit |=0x02;
	setHktAdjStat(NULL);
	#endif
}
void DecHktparm(void)
{
	u16 freq = ghtkinf.RFV;
	u8 i;
	for(i = 0;i<10;i++)
	{
		if(freq==HTK_FREQ_MIN)break;
		freq--;
		if((freq%2==0)||(freq%5==0))
		{
			break;
		}
	}
	ghtkinf.RFV = freq;
	ghtkinf.TFV = freq;
	log_info(" DEC : feq%x \n",freq);
	Htk_sentcom(HTK_CMD_GRP);
	ghtkstat.setbit |=0x02;
	setHktAdjStat(NULL);

}
void HktTrans(void)
{
	log_info("HKT trans\n");
	//gpio_set_pull_up(TCFG_IO_HKT_PTT, 1);
    //gpio_set_pull_down(TCFG_IO_HKT_PTT, 0);
    //gpio_set_die(TCFG_IO_HKT_PTT, 1);
	//gpio_set_direction(TCFG_IO_HKT_PTT, 0);
	gpio_set_output_value(TCFG_IO_HKT_PTT, 0);//hkt Ptt
	ghtkstat.istrans = 1;
}
void HktReceive(void)
{
	log_info("HKT recieve\n");
	//gpio_set_pull_up(TCFG_IO_HKT_PTT, 1);
    //gpio_set_pull_down(TCFG_IO_HKT_PTT, 0);
    //gpio_set_die(TCFG_IO_HKT_PTT, 1);
	//gpio_set_direction(TCFG_IO_HKT_PTT, 0);
	gpio_set_output_value(TCFG_IO_HKT_PTT, 1);//hkt Ptt
	ghtkstat.istrans = 0;
}
void HktTest3(void)
{
	static u8 testflag = 0;
	if(testflag == 0)
	{
		HktTrans();
		testflag = 1;
	}
	else
	{
		HktReceive();
		testflag = 0;
	}
}
void HktFirstInit(void)
{
	ghtkinf.RFV = 0;
	ghtkinf.TFV = 0;
	ghtkinf.RXCT = 0x7006;
	ghtkinf.TXCT = 0x7006;
	ghtkinf.sendbitflag = 0x0;
	ghtkinf.setbit0 = 0;
	ghtkstat.adjtime = 0;
	ghtkstat.setbit = 0;
	ghtkstat.delay2s = 0;
	ghtkstat.delaycon = 0;
	ghtkstat.istrans = 0;

}
void HKTtestGRP(void)
{
	u8 i,len,tmpdata[60];
	log_info("uart send: \n");
	memcpy(tmpdata,HKT_HEAD,2);
	memcpy(&tmpdata[2],"+DMOGRP",7);
	memcpy(&tmpdata[9],"=450.02500,450.02500",20);
	tmpdata[29]=',';
	tmpdata[30]=0xff;
	tmpdata[31]=0xff;
	tmpdata[32]=',';
	tmpdata[33]=0xff;
	tmpdata[34]=0xff;
	tmpdata[35]=',';
	tmpdata[36]='0';
	tmpdata[37]=',';
	tmpdata[38]='0';
	tmpdata[39]=0x0d;
	tmpdata[40]= 0x0a;
	tmpdata[41]= 0x00;
	len = 42;

	//log_info("uart send: %s\n", tmpdata);
	#if 1
	for (int i = 0; i < len; i++) {
		printf("*%d- %x,", i,tmpdata[i]);

         }
	#endif
	#ifdef AUDIO_PCM_DEBUG
	uartSendData(tmpdata,len);
	#endif

}
void HKTtestSAV(void)
{
	u8 i,tmpdata[60];
	//log_info("uart send1: \n");
	memcpy(tmpdata,HKT_HEAD,2);
	memcpy(&tmpdata[2],"+DMOSAV",7);
	memcpy(&tmpdata[9],"=1",2);
	tmpdata[11]=0x0d;
	tmpdata[12]= 0x0a;
	tmpdata[13]= 0x00;
	log_info("uart send: %s\n", tmpdata);
	//for (int i = 0; i < 12; i++) {
    //log_info("*%d- %x", i,tmpdata[i]);}
	#ifdef AUDIO_PCM_DEBUG
	uartSendData(tmpdata,13);
	#endif

}
void HKTtestVOL(void)
{
	u8 i,tmpdata[60];
	//log_info("uart send1: \n");
	memcpy(tmpdata,HKT_HEAD,2);
	memcpy(&tmpdata[2],"+DMOVOL",7);
	memcpy(&tmpdata[9],"=8",2);
	tmpdata[11]=0x0d;
	tmpdata[12]= 0x0a;
	tmpdata[13]= 0x00;
	log_info("uart send: %s\n", tmpdata);
	//for (int i = 0; i < 12; i++) {
    //log_info("*%d- %x", i,tmpdata[i]);}
	#ifdef AUDIO_PCM_DEBUG
	uartSendData(tmpdata,13);
	#endif

}
void HKTtest2(void)
{
	static u8 tesflag=2;//0
	u8 i,len;//,tmpdata[60];
	if(tesflag <= 0)//2
	{
		tesflag ++;
		log_info("send +DMOVER\n");
		memcpy(tmpdata,HKT_HEAD,2);
		memcpy(&tmpdata[2],"+DMOVER",7);
		tmpdata[9]=0x0d;
		tmpdata[10]= 0x0a;
		tmpdata[11]= 0x00;
		//log_info("uart send: %s\n", tmpdata);
		for (int i = 0; i < 12; i++) {
	    printf("*%d-%x,", i,tmpdata[i]);}
		printf(" end\n");
		#ifdef AUDIO_PCM_DEBUG
		uartSendData(tmpdata,11);
		#endif
	}
	#if 0
	else
	{
		//if(tesflag++ >4)
			tesflag = 0;
		HKTtestGRP();   //OK
		//HKTtestSAV();  //OK
		//HKTtestVOL();  //OK
	}
	#else
	else
	{

		//tesflag = 0;
		log_info("send +DMOMES\n");
		memcpy(tmpdata,HKT_HEAD,2);
		memcpy(&tmpdata[2],HKT_SET_MESSAGE,7);
		tmpdata[9]='=';
		len = 10;//40//0x9;
		tmpdata[10]=len;
		for(i = 0;i<len;i++)
			tmpdata[11+i] = '0'+(i%10);//i
		len = 11+i;
		tmpdata[len] = 0x0d;
		tmpdata[++len] = 0x0a;
		tmpdata[++len] = 0x0;
		//for (i = 0; i < len; i++) {
		for (i = 0; i < 2; i++) {
	    printf("*%d-%x,", i,tmpdata[i]);}
		printf(" end\n");
		#ifdef AUDIO_PCM_DEBUG
		uartSendData(tmpdata,len);
		#endif
	}
	#endif
	if(hkt_rx.analytime==0)
		hkt_rx.analytime =  usr_timer_add(NULL, app_CommunDataAnaly, 100, 1);


}

void HKTtest(void)
{
	//Htk_sentcom(HTK_CMD_MESS);
	//SendHktMess2sPro(NULL);
	//HKTtest2();
	if(ghtkstat.delay2s == 0)
		ghtkstat.delay2s = usr_timer_add(NULL, SendHktMess2sPro, 5000, 1);//5s

}


