#ifndef IO_LED_H
#define IO_LED_H

#include "typedef.h"


typedef enum  {
    IO_LED_MODE_START,

	IO_LED_ALL_ON,              	//mode1: 全亮
    IO_LED_ALL_OFF,         		//mode2: 全灭
    

    IO_BLE_LED_ON,             		//mode3: 蓝BLE亮
    IO_BLE_LED_OFF,            		//mode4: 蓝BLE灭
    IO_BLE_LED_SLOW_FLASH,          //mode5: 蓝BLE慢闪
    IO_BLE_LED_FAST_FLASH,          //mode6: 蓝BLE快闪
    IO_BLE_LED_BREATHE,				//mode7: 蓝灯呼吸灯模式
    IO_BLE_LED_FLASH_3S_OFF,  	    //mode8: 蓝BLE灯3秒连闪关闭

    IO_GREEN_LED_ON,             	//mode9:  GREEN亮
    IO_GREEN_LED_OFF,            	//mode10: GREEN灭
    IO_GREEN_LED_SLOW_FLASH,        //mode11: GREEN慢闪
    IO_GREEN_LED_FAST_FLASH,        //mode12: GREEN快闪
    IO_GREEN_LED_BREATHE,           //mode13: 绿灯呼吸灯模式
    IO_GREEN_LED_FLASH_3S_OFF,      //mode14: GREEN灯3秒连闪关闭
    
	IO_CHANGE_LED_ON,             	//mode15: 红CHANGE亮
    IO_CHANGE_LED_OFF,            	//mode16: 红CHANGE灭
    IO_CHANGE_LED_SLOW_FLASH,       //mode17: 红CHANGE慢闪
    IO_CHANGE_LED_FAST_FLASH,       //mode18: 红CHANGE快闪
    IO_CHANGE_LED_BREATHE,			//mode19: 红灯呼吸灯模式
    IO_CHANGE_LED_FLASH_3S_OFF,     //mode20: 红CHANGE灯3秒连闪关闭

    IO_ALL_LED_SLOW_FLASH,          //mode21: ALL慢闪
    IO_ALL_LED_FAST_FLASH,          //mode22: ALL快闪
    IO_ALL_LED_BREATHE,			    //mode23: ALL灯呼吸灯模式
    IO_ALL_LED_FLASH_3S_OFF,        //mode24: ALL灯3秒连闪关闭

    IO_LED_NULL = 0xFF,
}io_led_mode;

typedef enum{
	LED_SLOW_FLASH,
	LED_FAST_FLASH,
	LED_BREAK_FLASH,
	LED_3S_FLASH,
	LED_NO_FLASH
}io_led_flash_mode;
typedef struct{
	io_led_flash_mode mode;
	u8 count;
}io_led_mode_inf;
#define LED_IO_BLE 0
#define LED_IO_GREEN 1
#define LED_IO_CHANGE 2
#define LED_IO_ALL 3
#define LED_TIME_ID_NULL 0X0

struct io_platform_led_data {
    u8 pin[LED_IO_ALL];
	io_led_mode_inf modeinf[LED_IO_ALL+1];
	u16 timeid[LED_IO_ALL+1];
};

#define SLOW_FLASH_MAX_COUNT 4
#define FAST_FLASH_MAX_COUNT 2
#define BREAK_FLASH_MAX_COUNT 2
#define FLASH_3S_MAX_COUNT 11
#define SLOW_FLASH_TIME 500
#define FAST_FLASH_TIME 250
#define BREAK_FLASH_TIME 500
#define FLASH_3S_SHORT_TIME 250
#define FLASH_3S_LONG_TIME 500
#define FALSH_1S_SHORT_TIME 80


void io_led_init(void);
void io_led_test(void);
void ui_update_status(u8 status);

#endif



