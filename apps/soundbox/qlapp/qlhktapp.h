
#ifndef _QLHKT_APP_H_
#define _QLHKT_APP_H_
#include "system/includes.h"
#include "app_config.h"



#define HTK_VOL_DEFAULT 3
#define HTK_VOX_DEFAULT 3
#define HTK_FUN_SQ_DEFAULT 5
#define HTK_FUN_MICLVL_DEFAULT 4
#define HTK_FUN_TOT_DEFAULT 0
#define HTK_FUN_SCRAMLVL_DEFAULT 0
#define HTK_FUN_COMP_DEFULT 0

#define HTK_FREQ_MAX 56000
#define HTK_FREQ_MIN 0


#define     getBit(x,y)         (x) &= (1L << (y))

#define HTK_MESSAGE_MAX_LEN 60
//102
typedef struct
{
	u8 len;
	u8 message[HTK_MESSAGE_MAX_LEN];
}shtkmessage;//暂时不用这个功能
typedef struct
{
	u16 RFV;/* = (freq-400.00000)mhz/1.25khz,max = 0xDAC0(=>470.00000mhz) */
	u16 TFV;
	u16 RXCT;/* 67.8,rxct = 0x7806*/
	u16 TXCT;
	u8 setbit0;/*  bit3-2:flag1;bit1-0:flag */
	u8 sendbitflag;/* bit0-6:HTK_CMD_GRP;HTK_CMD_SAVE;HTK_CMD_VER;HTK_CMD_VOL;HTK_CMD_VOX;HTK_CMD_FUN;HTK_CMD_MESS*/
	u8 ver[20];
	
}shtkparam;

typedef struct
{
	u8 setbit;/*bit 0 :adjust freq start=0 ,bit 1:adjusting flag*/
	u8 istrans;
	u16 adjtime;
	u16 delay2s;//for send data
	u16 delaycon;
}shtkstatus;

#define MAX_SIZE_HKT_LEN 22
typedef	struct{
	u8 start;														
	u8 end;
	u16 analytime;
	u8 data[MAX_SIZE_HKT_LEN];
	unsigned start_flag	:1;												
	unsigned end_flag	:1;
	unsigned gettimes:2;
	unsigned noused:4;
}DATA_HKT_RX;

void HKTtest(void);
void Htk_repeatsend(void *priv);
void Htk_getcomAnaly(u8 * buff,u8 len);
void IncHktparm(void);
void DecHktparm(void);
void HktFirstInit(void);
void htk_getDataToCommun(u8* data,u8 len);
u8 getHktAdjStat(void);
void HktTrans(void);
void HktReceive(void);
void HktTest3(void);
void HktCommuninit(void);
void hkt_power_init(void);
void app_CommunDataAnaly(void *param);
void HKTtest2(void);



#endif


