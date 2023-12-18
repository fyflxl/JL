/* #define __LOG_LEVEL  __LOG_WARN */
/*#define __LOG_LEVEL  __LOG_DEBUG*/
/*#define __LOG_LEVEL  __LOG_VERBOSE*/



#include "generic/gpio.h"
#include "asm/includes.h"
//#include "asm/pwm_led.h"
#include "asm/power/p11.h"
#include "asm/power/p33.h"
#include "system/timer.h"
#include "app_config.h"
#include "io_led.h"
#include "ui_manage.h"

//#include "classic/tws_api.h"
/* io led blink */
#define __LOG_LEVEL  __LOG_INFO

#if TCFG_IOLED_ENABLE

//#define IO_LED_TEST_MODE 		//LED模块测试函数
#define IO_LED_DEBUG_ENABLE
#ifdef IO_LED_DEBUG_ENABLE
#define log_info(fmt, ...)    printf("[IO_LED_IF] "fmt, ##__VA_ARGS__)

#define led_debug(fmt, ...) 	printf("[IO_LED] "fmt, ##__VA_ARGS__)
#define led_err(fmt, ...) 		printf("[IO_LED_ERR] "fmt, ##__VA_ARGS__)
#else
#define led_debug(...)
#define led_err(...)
#endif

//////////////////////////////////////////////////////////////
struct io_platform_led_data io_led_data;

/*
 * IO使能注意, 否则有可能会出现在显示效果前会出现闪烁
 * 1)设置PU, PD为1;
 * 2)设置DIR, OUT为1;
 * 3)最后设置为方向为输出;
 */

static void io_led_pin_set_enable(u8 gpio)
{
    gpio_set_output_value(gpio, 0);
}
static void io_led_pin_set_disable(u8 gpio)
{
    gpio_set_output_value(gpio, 1);
}
static void io_led_pin_init(u8 gpio)
{
	gpio_set_pull_up(gpio, 1);
    gpio_set_pull_down(gpio, 0);
    gpio_set_die(gpio, 1);
	gpio_set_direction(gpio, 0);
	io_led_pin_set_disable(gpio);
}
void del_io_led_time_id(u8 id)
{
	if(io_led_data.timeid[id]!=0)
	{
		usr_timer_del(io_led_data.timeid[id]);
		//log_info("--->del time1 %x,id:%x,c:%x \n",id,io_led_data.timeid[id],io_led_data.modeinf[id].count);
		io_led_data.timeid[id] = LED_TIME_ID_NULL;
		//log_info("--->del time2 %x,id:%x,c:%x \n",id,io_led_data.timeid[id],io_led_data.modeinf[id].count);
	}
}

void io_led_init(void)
{
	u8 i;
	io_led_data.pin[LED_IO_BLE] = TCFG_IOLED_BLE;
	io_led_data.pin[LED_IO_GREEN] = TCFG_IOLED_GREEN;
	io_led_data.pin[LED_IO_CHANGE] = TCFG_IOLED_CHANGE;
	for(i = 0;i<LED_IO_ALL;i++)
	{
		io_led_pin_init(io_led_data.pin[i]);
		io_led_data.timeid[i] = LED_TIME_ID_NULL;
		io_led_data.modeinf[i].count= 0;
		io_led_data.modeinf[i].mode = LED_NO_FLASH;
	}
}
void io_led_flash(u8 id,u8 count,u8 maxcount,u8 falshcount)
{
	if(count >= maxcount)
	{
		if(LED_IO_ALL == id)
		{
			io_led_pin_set_enable(io_led_data.pin[LED_IO_BLE]);
			io_led_pin_set_enable(io_led_data.pin[LED_IO_GREEN]);
			io_led_pin_set_enable(io_led_data.pin[LED_IO_CHANGE]);
		}
		else
			io_led_pin_set_enable(io_led_data.pin[id]);
		count = maxcount;
	}
	else if(count == falshcount)
	{
		if(LED_IO_ALL == id)
		{
			io_led_pin_set_disable(io_led_data.pin[LED_IO_BLE]);
			io_led_pin_set_disable(io_led_data.pin[LED_IO_GREEN]);
			io_led_pin_set_disable(io_led_data.pin[LED_IO_CHANGE]);
		}
		else
			io_led_pin_set_disable(io_led_data.pin[id]);
	}
	if(count>0)count--;
	if(count == 0)count = maxcount;
	io_led_data.modeinf[id].count = count;
}
void io_led_tick(u8 id)
{
	u8 mode = io_led_data.modeinf[id].mode;
	u8 count = io_led_data.modeinf[id].count;
	/* 切记，调用结束后要关下对应的灯 */
	switch(mode)
	{
		case LED_SLOW_FLASH:
			io_led_flash(id,count,SLOW_FLASH_MAX_COUNT,SLOW_FLASH_MAX_COUNT-1);
			break;
		case LED_FAST_FLASH:
			io_led_flash(id,count,FAST_FLASH_MAX_COUNT,FAST_FLASH_MAX_COUNT-1);
			break;
		case LED_BREAK_FLASH:
			io_led_flash(id,count,BREAK_FLASH_MAX_COUNT,BREAK_FLASH_MAX_COUNT-1);
			break;
		case LED_3S_FLASH:
			if(count >= FLASH_3S_MAX_COUNT)
			{
				count = FLASH_3S_MAX_COUNT;
			}
			if((count%2)==0)
			{
				io_led_pin_set_enable(io_led_data.pin[id]);
				log_info("--->io_led_tick1 c:%x \n",count);
			}
			else 
			{
				io_led_pin_set_disable(io_led_data.pin[id]);
				log_info("--->io_led_tick0 c:%x \n",count);
			}
			if(count%4==0)
			{
				usr_timer_modify(io_led_data.timeid[id],FLASH_3S_LONG_TIME);
			}
			else if(count%4 ==1)
			{
				usr_timer_modify(io_led_data.timeid[id],FLASH_3S_SHORT_TIME);
			}
			if(count>0)count--;
			if(count == 0)del_io_led_time_id(id);;
			io_led_data.modeinf[id].count = count;
			break;
		default:
			break;
	}
	wdt_clear();
	//log_info("--->io_led_tick m:%x,id:%x,c:%x \n",mode,io_led_data.timeid[id],io_led_data.modeinf[id].count);
	

}
void io_change_led_tick(void *priv)
{
	io_led_tick(LED_IO_CHANGE);
}
void io_green_led_tick(void *priv)
{
	io_led_tick(LED_IO_GREEN);
}
void io_ble_led_tick(void *priv)
{
	io_led_tick(LED_IO_BLE);
}


void io_all_led_tick(void *priv)
{
	u8 mode = io_led_data.modeinf[LED_IO_ALL].mode;
	u8 count = io_led_data.modeinf[LED_IO_ALL].count;
	wdt_clear();
	/* 切记，调用结束后要关下对应的灯 */
	switch(mode)
	{
		case LED_SLOW_FLASH:
			io_led_flash(LED_IO_ALL,count,SLOW_FLASH_MAX_COUNT,SLOW_FLASH_MAX_COUNT-1);
			break;
		case LED_FAST_FLASH:
			io_led_flash(LED_IO_ALL,count,FAST_FLASH_MAX_COUNT,FAST_FLASH_MAX_COUNT-1);
			break;
		case LED_BREAK_FLASH:
			io_led_flash(LED_IO_ALL,count,BREAK_FLASH_MAX_COUNT,BREAK_FLASH_MAX_COUNT-1);
			break;
		case LED_3S_FLASH:
			if(count >= FLASH_3S_MAX_COUNT)
			{
				count = FLASH_3S_MAX_COUNT;
			}
			if((count%2)==0)
			{
				io_led_pin_set_enable(io_led_data.pin[LED_IO_BLE]);
				io_led_pin_set_enable(io_led_data.pin[LED_IO_GREEN]);
				io_led_pin_set_enable(io_led_data.pin[LED_IO_CHANGE]);
				log_info("--->io_led_tick1 c:%x \n",count);
			}
			else 
			{
				io_led_pin_set_disable(io_led_data.pin[LED_IO_BLE]);
				io_led_pin_set_disable(io_led_data.pin[LED_IO_GREEN]);
				io_led_pin_set_disable(io_led_data.pin[LED_IO_CHANGE]);
				log_info("--->io_led_tick0 c:%x \n",count);
			}
			if(count%4==0)
			{
				usr_timer_modify(io_led_data.timeid[LED_IO_ALL],FLASH_3S_LONG_TIME);
			}
			else if(count%4 ==1)
			{
				usr_timer_modify(io_led_data.timeid[LED_IO_ALL],FLASH_3S_SHORT_TIME);
			}
			if(count>0)count--;
			if(count == 0)del_io_led_time_id(LED_IO_ALL);;
			io_led_data.modeinf[LED_IO_ALL].count = count;
			break;
		default:
			break;
	}
	//log_info("--->io_led_tick m:%x,id:%x,c:%x \n",mode,io_led_data.timeid[id],io_led_data.modeinf[id].count);
	
}
void set_other_led_off(void *priv)
{
	u8 i;
	for(i = 0;i<LED_IO_ALL;i++)
	{
		if(io_led_data.timeid[i]==0)
		{
			del_io_led_time_id(i);
			io_led_pin_set_disable(io_led_data.pin[i]);
			io_led_data.modeinf[i].count= 0;
			io_led_data.modeinf[i].mode = LED_NO_FLASH;
		}//for other work
	}
	del_io_led_time_id(LED_IO_ALL);
}

void set_all_led_off(void)
{
	u8 i;
	for(i = 0;i<LED_IO_ALL;i++)
	{
		del_io_led_time_id(i);
		io_led_pin_set_disable(io_led_data.pin[i]);
		io_led_data.modeinf[i].count= 0;
		io_led_data.modeinf[i].mode = LED_NO_FLASH;
	}
	del_io_led_time_id(LED_IO_ALL);
}

void set_all_led_on(void)
{
	u8 i;
	for(i = 0;i<LED_IO_ALL;i++)
	{
		del_io_led_time_id(i);
		io_led_pin_set_enable(io_led_data.pin[i]);
		
		io_led_data.modeinf[i].count= 0;
		io_led_data.modeinf[i].mode = LED_NO_FLASH;
	}
	del_io_led_time_id(LED_IO_ALL);
}
void clear_led_flash_modinf(u8 id)
{
	io_led_data.modeinf[id].count= 0;
	io_led_data.modeinf[id].mode = LED_NO_FLASH;
}
void set_led_slow_flash(u8 id)
{
	if(id>LED_IO_CHANGE)
	{
		log_info("--->err1 \n");
		return;
	}
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_SLOW_FLASH;
	io_led_data.modeinf[id].count = SLOW_FLASH_MAX_COUNT;
	switch(id)
	{
		case LED_IO_BLE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_ble_led_tick, SLOW_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_CHANGE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_change_led_tick, SLOW_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_GREEN:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_green_led_tick, SLOW_FLASH_TIME, 0);//usr_timer
			break;
		default:
			return;
	}
	log_info("--->set_led_slow_flash \n");
}	
void set_led_fast_flash(u8 id)
{
	if(id>LED_IO_CHANGE)
	{
		log_info("--->err2 \n");
		return;
	}
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_FAST_FLASH;
	io_led_data.modeinf[id].count = FAST_FLASH_MAX_COUNT;
	switch(id)
	{
		case LED_IO_BLE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_ble_led_tick, FAST_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_CHANGE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_change_led_tick, FAST_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_GREEN:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_green_led_tick, FAST_FLASH_TIME, 0);//usr_timer
			break;
		default:
			return;
	}
	log_info("--->set_led_fast_flash \n");
}
void set_led_break_flash(u8 id)
{
	if(id>LED_IO_CHANGE)
	{
		log_info("--->err3 \n");
		return;
	}
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_BREAK_FLASH;
	io_led_data.modeinf[id].count = BREAK_FLASH_MAX_COUNT;
	switch(id)
	{
		case LED_IO_BLE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_ble_led_tick, BREAK_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_CHANGE:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_change_led_tick, BREAK_FLASH_TIME, 0);//usr_timer
			break;
		case LED_IO_GREEN:
			io_led_data.timeid[id]=usr_timer_add(NULL, io_green_led_tick, BREAK_FLASH_TIME, 0);//usr_timer
			break;
		default:
			return;
	}
	log_info("--->set_led_break_flash \n");
}
void set_led_3s_flash(u8 id)
{
	if(id>LED_IO_CHANGE)
	{
		log_info("--->err4 \n");
		return;
	}
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_3S_FLASH;
	io_led_data.modeinf[id].count = FLASH_3S_MAX_COUNT;
	io_led_data.timeid[id]=usr_timer_add(NULL, io_led_tick, FLASH_3S_SHORT_TIME, 0);//usr_timer //4.25
	log_info("--->set_led_3s_flash \n");
}

void set_all_led_slow_flash(u8 id)
{
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_SLOW_FLASH;
	io_led_data.modeinf[id].count = SLOW_FLASH_MAX_COUNT;
	io_led_data.timeid[id]=usr_timer_add(NULL, io_all_led_tick, SLOW_FLASH_TIME, 0);//usr_timer
	log_info("--->set_led_slow_flash \n");
}
void set_all_led_fast_flash(u8 id)
{
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_FAST_FLASH;
	io_led_data.modeinf[id].count = FAST_FLASH_MAX_COUNT;
	io_led_data.timeid[id]=usr_timer_add(NULL, io_all_led_tick, FAST_FLASH_TIME, 0);//usr_timer
	log_info("--->set_led_fast_flash \n");
}
void set_all_led_break_flash(u8 id)
{
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_BREAK_FLASH;
	io_led_data.modeinf[id].count = BREAK_FLASH_MAX_COUNT;
	io_led_data.timeid[id]=usr_timer_add(NULL, io_all_led_tick, BREAK_FLASH_TIME, 0);//usr_timer
	log_info("--->set_led_break_flash \n");
}
void set_all_led_3s_flash(u8 id)
{
	del_io_led_time_id(id);
	io_led_data.modeinf[id].mode = LED_3S_FLASH;
	io_led_data.modeinf[id].count = FLASH_3S_MAX_COUNT;
	io_led_data.timeid[id]=usr_timer_add(NULL, io_all_led_tick, FLASH_3S_SHORT_TIME, 0);//usr_timer //4.25
	log_info("--->set_led_3s_flash \n");
}


void _io_led_on(u8 id)
{
	del_io_led_time_id(id);
	io_led_pin_set_enable(io_led_data.pin[id]);
	clear_led_flash_modinf(id);
}
void _io_led_off(u8 id)
{
	del_io_led_time_id(id);
	io_led_pin_set_disable(io_led_data.pin[id]);
	clear_led_flash_modinf(id);
}

void io_led_display(io_led_mode mode)
{
	log_info("%s,***>>>%d\n",__func__,mode);
	switch(mode)
	{
		case IO_LED_MODE_START: 
			io_led_init();
			break;
		case IO_LED_ALL_OFF:				//mode1: 全灭 	
			set_all_led_off();
			break;
		case IO_LED_ALL_ON: 				//mode2: 全亮	
			set_all_led_on();
			break;
		case IO_BLE_LED_ON: 				//mode3: BLE亮
			_io_led_on(LED_IO_BLE);
			break;
		case IO_BLE_LED_OFF:				//mode4: BLE灭
			_io_led_off(LED_IO_BLE);
			break;
		case IO_BLE_LED_SLOW_FLASH: 		//mode5: BLE慢闪 
			set_led_slow_flash(LED_IO_BLE);
			break;
		case IO_BLE_LED_FAST_FLASH: 		//mode6: BLE快闪 
			set_led_fast_flash(LED_IO_BLE);
			break;
		case IO_BLE_LED_BREATHE:			//mode7: 蓝灯呼吸灯模式
			set_led_break_flash(LED_IO_BLE);
			break;
		case IO_BLE_LED_FLASH_3S_OFF:	    //mode8: BLE灯3秒连闪两下，关闭
			set_led_3s_flash(LED_IO_BLE);
			break;
		case IO_GREEN_LED_ON:				//mode9:GREEN亮	
			_io_led_on(LED_IO_GREEN);
			break;
		case IO_GREEN_LED_OFF:				//mode10: GREEN灭
			_io_led_off(LED_IO_GREEN);
			break;
		case IO_GREEN_LED_SLOW_FLASH:		//mode11: GREEN慢闪	
			set_led_slow_flash(LED_IO_GREEN);
			break;
		case IO_GREEN_LED_FAST_FLASH:		//mode12: GREEN快闪	
			set_led_fast_flash(LED_IO_GREEN);
			break;
		case IO_GREEN_LED_BREATHE:          //mode13: 绿灯呼吸灯模式
    		set_led_break_flash(LED_IO_GREEN);
    		break;
		case IO_GREEN_LED_FLASH_3S_OFF:	    //mode14: GREEN灯3秒连闪两下，关闭	 
			set_led_3s_flash(LED_IO_GREEN);
			break;
		case IO_CHANGE_LED_ON:				//mode15: CHANGE亮 
			_io_led_on(LED_IO_CHANGE);
			break;
		case IO_CHANGE_LED_OFF: 			//mode16: CHANGE灭 
			_io_led_off(LED_IO_CHANGE);
			break;
		case IO_CHANGE_LED_SLOW_FLASH:		//mode17: CHANGE慢闪 		
			set_led_slow_flash(LED_IO_CHANGE);
			break;
		case IO_CHANGE_LED_FAST_FLASH:		//mode18: CHANGE快闪 
			set_led_fast_flash(LED_IO_CHANGE);
			break;
		case IO_CHANGE_LED_BREATHE:			//mode19: 红灯呼吸灯模式
        	set_led_break_flash(LED_IO_CHANGE);
        	break;
		case IO_CHANGE_LED_FLASH_3S_OFF:    //mode20: CHANGE灯3秒连闪两下，关闭		
			set_led_3s_flash(LED_IO_CHANGE);
			break;	
	    case IO_ALL_LED_SLOW_FLASH:         //mode21: WHITE慢闪
	    	set_all_led_slow_flash(LED_IO_ALL);
	    	break;
	    case IO_ALL_LED_FAST_FLASH:         //mode22: WHITE快闪
	    	set_all_led_fast_flash(LED_IO_ALL);
	    	break;
	    case IO_ALL_LED_BREATHE:			//mode23: WHITE灯呼吸灯模式
	    	set_all_led_break_flash(LED_IO_ALL);
	    	break;
	    case IO_ALL_LED_FLASH_3S_OFF:       //mode24: WHITE灯3秒连闪关闭
	    	set_all_led_3s_flash(LED_IO_ALL);
	    	break;
		case IO_LED_NULL:										
			break;
																															 

	}
}
void io_led_power_off(void)
{
	
	set_all_led_off();
}

void ui_update_status(u8 status)
{
	static u8 oldstatus = STATUS_NULL;
	if(oldstatus!=status)
	{
		log_info("%s,***%d---->%d\n",__func__,oldstatus,status);
		oldstatus = status;
		
	}
	else
	{
		return;
	}
	switch (status) {
	case STATUS_POWERON:
		io_led_display(IO_LED_ALL_ON);
		io_led_data.timeid[LED_IO_ALL]=usr_timeout_add(NULL, set_other_led_off, 1000, 0);//1s
    case STATUS_POWEROFF:
        //log_info("[STATUS_POWEROFF]\n");
		io_led_display(IO_LED_ALL_OFF);
        break;

    case STATUS_BT_INIT_OK:
	case STATUS_BT_DISCONN:
		io_led_display(IO_CHANGE_LED_OFF);
        //log_info("[STATUS_BT_INIT_OK]\n");
        io_led_display(IO_BLE_LED_FAST_FLASH);
        break;

    case STATUS_BT_CONN:
        //log_info("[STATUS_BT_CONN]\n");
        io_led_display(IO_BLE_LED_SLOW_FLASH);
        break;

    
    case STATUS_POWERON_LOWPOWER:
        //log_info("[STATUS_POWERON_LOWPOWER]\n");
        io_led_display(IO_CHANGE_LED_FAST_FLASH);
        break;

    case STATUS_MUSIC_MODE:
        //log_info("[STATUS_MUSIC_MODE]\n");
        
        io_led_display(IO_GREEN_LED_SLOW_FLASH);
        break;
	case STATUS_LINEIN_MODE:
		io_led_display(IO_GREEN_LED_OFF);
		break;
	case STATUS_HTK_ADJ_FREQ:
		io_led_display(IO_LED_ALL_ON);
		break;
    case STATUS_HTK_PLAY_FREQ:
		io_led_display(IO_ALL_LED_SLOW_FLASH);
		break;
    case STATUS_HTK_EXIT:
		set_other_led_off(NULL);
		break;
    case STATUS_CHARGE_START:
		io_led_display(IO_CHANGE_LED_ON);
		break;
    case STATUS_CHARGE_FULL:
		io_led_display(IO_CHANGE_LED_OFF);
		io_led_display(IO_GREEN_LED_ON);
		break;
	case STATUS_LOWPOWER:
		io_led_display(IO_CHANGE_LED_SLOW_FLASH);
		break;
    case STATUS_CHARGE_CLOSE:
		set_other_led_off(NULL);
		break;

    default:
        break;
    }

    return;

}



void io_led_test(void)
{
	static io_led_mode flag1 = IO_LED_MODE_START;
	static  u16 timeid;
	if(flag1 == IO_LED_MODE_START)
	{
		io_led_display(IO_BLE_LED_ON);
		timeid = usr_timer_add(NULL, io_led_test, 5000, 0);//usr_timer定时扫描增加接口5000
		log_info("--->io_led_test1  %d\n",flag1);
		flag1=IO_BLE_LED_ON +1;
	}
	else
	{
		io_led_display(flag1);
		log_info("--->io_led_test  %d\n",flag1);
		if(IO_CHANGE_LED_FLASH_3S_OFF ==flag1)
		{
			flag1 = IO_LED_ALL_OFF;
			usr_timer_del(timeid);
			set_all_led_off();
		}
		else
			flag1++;
	}
	
	#if 0
    static const char *dis_mode[] = {
        "ALL off",
        "ALL on",

        "BLUE on",
        "BLUE off",
        "BLUE slow flash",
        "BLUE fast flash",
        "BLUE double flash 5s",
        "BLUE one flash 5s",

        "RED on",
        "RED off",
        "RED slow flash",
        "RED fast flash",
        "RED double flash 5s",
        "RED one flash 5s",

        "RED & BLUE fast falsh",
        "RED & BLUE slow",
        "BLUE  breathe",
        "RED breathe",
        "RED & BLUE breathe",
    };
    /* static u8 index = C_BLED_BREATHE; */
    /* static u8 index = PWM_LED_ALL_OFF; */
    //static u8 index = PWM_LED_ALL_ON;
    static u8 index = PWM_LED0_BREATHE;
    //static u8 index = PWM_LED1_ON;

    led_debug("\n\nDISPPLAY MODE : %d-%s", index, dis_mode[index - 1]);

    pwm_led_mode_set(index++);
    pwm_led_register_irq(_pwm_led_test_isr);

    if (index >= PWM_LED_MODE_END) {
        index = PWM_LED_ALL_OFF;
    }
	#endif
}

#endif





