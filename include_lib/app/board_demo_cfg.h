#ifndef CONFIG_BOARD_DEMO_CFG_H
#define CONFIG_BOARD_DEMO_CFG_H

#include "board_demo_global_build_cfg.h"



#define CONFIG_SDFILE_ENABLE

//*********************************************************************************//
//                                 配置开始                                        //
//*********************************************************************************//
#define ENABLE_THIS_MOUDLE					1
#define DISABLE_THIS_MOUDLE					0

#define ENABLE								1
#define DISABLE								0

#define NO_CONFIG_PORT						(-1)

#define DEMO_SD_OK 0
#define USER_USB_OK 0

//*********************************************************************************//
//                                  app 配置                                       //
//*********************************************************************************//
#define TCFG_APP_BT_EN			            1
#define TCFG_APP_MUSIC_EN			        1
#define TCFG_APP_LINEIN_EN					1
#define TCFG_APP_FM_EN					    0
#define TCFG_APP_PC_EN					    1
#define TCFG_APP_RTC_EN					    0
#define TCFG_APP_RECORD_EN				    0
#define TCFG_APP_SPDIF_EN                   0
#define TCFG_APP_FM_EMITTER_EN              0
#define TCFG_APP_BC_MIC_EN					0


//*********************************************************************************//
//                                 UART配置                                        //
//*********************************************************************************//
#define TCFG_UART0_ENABLE					ENABLE_THIS_MOUDLE                     //串口打印模块使能
#if USER_USB_OK
#define TCFG_UART0_RX_PORT					NO_CONFIG_PORT                         //串口接收脚配置（用于打印可以选择NO_CONFIG_PORT）
#define TCFG_UART0_TX_PORT  				IO_PORTA_07                            //串口发送脚配置
#else
#define TCFG_UART0_RX_PORT					NO_CONFIG_PORT                         //串口接收脚配置（用于打印可以选择NO_CONFIG_PORT）
#define TCFG_UART0_TX_PORT  				IO_PORT_DM                            //串口发送脚配置
#endif
#define TCFG_UART0_BAUDRATE  				115200                                 //串口波特率配置

//*********************************************************************************//
//                                 IIC配置                                        //
//*********************************************************************************//
/*软件IIC设置*/
#if DEMO_SD_OK
#define TCFG_SW_I2C0_CLK_PORT               IO_PORTG_04//IO_PORTB_04                             //软件IIC  CLK脚选择
#define TCFG_SW_I2C0_DAT_PORT               IO_PORTG_05//IO_PORTB_05                             //软件IIC  DAT脚选择
#else
#define TCFG_SW_I2C0_CLK_PORT               IO_PORTC_04//IO_PORTB_04                             //软件IIC  CLK脚选择
#define TCFG_SW_I2C0_DAT_PORT               IO_PORTC_05//IO_PORTB_05                             //软件IIC  DAT脚选择
#endif
#define TCFG_SW_I2C0_DELAY_CNT              75                                      //IIC延时参数，影响通讯时钟频率

/*硬件IIC端口选择*/
#define TCFG_HW_I2C0_CLK_PORT               NO_CONFIG_PORT//IO_PORTC_04
#define TCFG_HW_I2C0_DAT_PORT               NO_CONFIG_PORT//IO_PORTC_05
#define TCFG_HW_I2C0_CLK                    100000                                  //硬件IIC波特率

//*********************************************************************************//
//                                 硬件SPI 配置                                        //
//*********************************************************************************//
#define	TCFG_HW_SPI1_ENABLE		DISABLE_THIS_MOUDLE//ENABLE_THIS_MOUDLE
#define TCFG_HW_SPI1_PORT_CLK   IO_PORTA_05//IO_PORTA_07//
#define TCFG_HW_SPI1_PORT_DO    IO_PORTA_06//IO_PORTA_08//
#define TCFG_HW_SPI1_PORT_DI    IO_PORTA_04//NO_CONFIG_PORT//IO_PORTA_09//
#define TCFG_HW_SPI1_BAUD		5000000L//24000000L
#define TCFG_HW_SPI1_MODE		SPI_MODE_BIDIR_1BIT
#define TCFG_HW_SPI1_ROLE		SPI_ROLE_MASTER
#define TCFG_HW_SPI1_CS         IO_PORTA_10   //added by helena for spi

#define	TCFG_HW_SPI2_ENABLE		ENABLE_THIS_MOUDLE
#define TCFG_HW_SPI2_PORT_CLK   IO_PORTA_05 //NO_CONFIG_PORT
#define TCFG_HW_SPI2_PORT_DO    IO_PORTA_04//NO_CONFIG_PORT
#define TCFG_HW_SPI2_PORT_DI    IO_PORTA_06//NO_CONFIG_PORT
#define TCFG_HW_SPI2_BAUD		2000000L//1000000L//
#define TCFG_HW_SPI2_MODE		SPI_MODE_BIDIR_1BIT
#define TCFG_HW_SPI2_ROLE		SPI_ROLE_MASTER
#define TCFG_HW_SPI2_CS         IO_PORTA_10   //added by helena for spi

//*********************************************************************************//
//                                 USB 配置                                        //
//*********************************************************************************//
#if USER_USB_OK
#define TCFG_PC_ENABLE						TCFG_APP_PC_EN//PC模块使能
#define TCFG_UDISK_ENABLE					ENABLE_THIS_MOUDLE//U盘模块使能
#else
#define TCFG_PC_ENABLE						0//PC模块使能
#define TCFG_UDISK_ENABLE					DISABLE_THIS_MOUDLE//U盘模块使能

#endif
#define TCFG_HOST_AUDIO_ENABLE				DISABLE_THIS_MOUDLE
#define TCFG_HID_HOST_ENABLE				DISABLE_THIS_MOUDLE
#define TCFG_OTG_USB_DEV_EN                 BIT(0)//USB0 = BIT(0)  USB1 = BIT(1)
#define TCFG_PC_DUAL_MIC                    DISABLE_THIS_MOUDLE

#include "usb_std_class_def.h"

#undef  USB_DEVICE_CLASS_CONFIG
#define USB_DEVICE_CLASS_CONFIG 		    TCFG_PC_DUAL_MIC ? (MIC_CLASS | SPEAKER_CLASS) : (MASSSTORAGE_CLASS|SPEAKER_CLASS|MIC_CLASS|HID_CLASS)

//*********************************************************************************//
//                                 FLASH 配置                                      //
//*********************************************************************************//
#define FLASH_INSIDE_REC_ENABLE             DISABLE_THIS_MOUDLE			// 内置flash录音
#define TCFG_NORFLASH_SFC_DEV_ENABLE 		DISABLE_THIS_MOUDLE
#define TCFG_NORFLASH_DEV_ENABLE		    DISABLE_THIS_MOUDLE
#define TCFG_NANDFLASH_DEV_ENABLE		    DISABLE_THIS_MOUDLE
#define TCFG_FLASH_DEV_SPI_HW_NUM			2// 1: SPI1    2: SPI2
#define TCFG_FLASH_DEV_SPI_CS_PORT	    	IO_PORTB_07

#if  TCFG_NORFLASH_DEV_ENABLE
#define TCFG_NOR_FAT                    0//ENABLE
#define TCFG_NOR_FS                     0//ENABLE
#define TCFG_NOR_REC                    1//ENABLE
#else
#define TCFG_NOR_FAT                    0//ENABLE
#define TCFG_NOR_FS                     0//ENABLE
#define TCFG_NOR_REC                    0//ENABLE
#endif

#define TCFG_VIRFAT_FLASH_ENABLE  			DISABLE_THIS_MOUDLE

#define TCFG_EX_FLASH_EN                    DISABLE_THIS_MOUDLE
// 使能UI
#define TCFG_EX_FLASH_POWER_IO				IO_PORTC_08 // 外置flash电源脚
#define CONFIG_EX_FLASH_POWER_IO			PC08 // NULL // 外置flash电源脚。不接IO时填NULL或者屏蔽掉该宏定义
// 不使能UI
// #define TCFG_EX_FLASH_POWER_IO				NO_CONFIG_PORT//IO_PORTC_08 // 外置flash电源脚
// #define CONFIG_EX_FLASH_POWER_IO		    NULL//	PC08 // NULL // 外置flash电源脚。不接IO时填NULL或者屏蔽掉该宏定义


//*********************************************************************************//
//                                  SD 配置                                        //
//*********************************************************************************//
#define TCFG_SD0_ENABLE						ENABLE_THIS_MOUDLE                    //SD0模块使能
#define TCFG_SD0_DAT_MODE					1               //线数设置，1：一线模式  4：四线模式
#define TCFG_SD0_DET_MODE					SD_CMD_DECT//SD_CLK_DECT     //SD卡检测方式
#define TCFG_SD0_DET_IO                     NO_CONFIG_PORT//IO_PORTB_03     //当检测方式为IO检测可用
#define TCFG_SD0_DET_IO_LEVEL               0               //当检测方式为IO检测可用,0：低电平检测到卡。 1：高电平(外部电源)检测到卡。 2：高电平(SD卡电源)检测到卡。
#define TCFG_SD0_CLK						(3000000 * 4L)  //SD卡时钟频率设置
#if DEMO_SD_OK
#define TCFG_SD0_PORT_CMD					IO_PORTC_04//
#define TCFG_SD0_PORT_CLK					IO_PORTC_05//
#define TCFG_SD0_PORT_DA0					IO_PORTC_03//

#else
#define TCFG_SD0_PORT_CMD					IO_PORTG_01//IO_PORTC_04//
#define TCFG_SD0_PORT_CLK					IO_PORTG_02//IO_PORTC_05//
#define TCFG_SD0_PORT_DA0					IO_PORTG_00//IO_PORTC_03//
#endif
#define TCFG_SD0_PORT_DA1					NO_CONFIG_PORT  //当选择4线模式时要用
#define TCFG_SD0_PORT_DA2					NO_CONFIG_PORT
#define TCFG_SD0_PORT_DA3					NO_CONFIG_PORT
#define TCFG_SD0_POWER                      IO_PORTE_06//sd card power by helena added2023 0709


#define TCFG_SD0_SD1_USE_THE_SAME_HW	    DISABLE_THIS_MOUDLE//SD1与SD0共一个硬件模块，分时复用

#if TCFG_SD0_SD1_USE_THE_SAME_HW
#define TCFG_SD1_ENABLE						1               //SD1模块使能
#else
#define TCFG_SD1_ENABLE						0
#endif
#define TCFG_SD1_DAT_MODE					1               //线数设置，1：一线模式  4：四线模式
#define TCFG_SD1_DET_MODE					SD_CLK_DECT     //SD卡检测方式
#define TCFG_SD1_DET_IO                     NO_CONFIG_PORT//IO_PORTB_03     //当检测方式为IO检测可用
#define TCFG_SD1_DET_IO_LEVEL               0               //当检测方式为IO检测可用,0：低电平检测到卡。 1：高电平(外部电源)检测到卡。 2：高电平(SD卡电源)检测到卡。
#define TCFG_SD1_CLK						(3000000 * 4L)  //SD卡时钟频率设置
#define TCFG_SD1_PORT_CMD					IO_PORTG_01//IO_PORTC_04
#define TCFG_SD1_PORT_CLK					IO_PORTG_02//IO_PORTC_05
#define TCFG_SD1_PORT_DA0					IO_PORTG_00//IO_PORTC_03
#define TCFG_SD1_PORT_DA1					NO_CONFIG_PORT  //当选择4线模式时要用
#define TCFG_SD1_PORT_DA2					NO_CONFIG_PORT
#define TCFG_SD1_PORT_DA3					NO_CONFIG_PORT

#define TCFG_SD_ALWAY_ONLINE_ENABLE         ENABLE//DISABLE
#define TCFG_KEEP_CARD_AT_ACTIVE_STATUS	    ENABLE	// 保持卡活跃状态。会增加功耗

//*********************************************************************************//
//                                 设备异步读 配置                                 //
//*********************************************************************************//

#define TCFG_DEVICE_BULK_READ_ASYNC_ENABLE

//*********************************************************************************//
//                                 key 配置                                        //
//*********************************************************************************//
#define KEY_IO_NUM_MAX						6
#define KEY_AD_NUM_MAX						10
#define KEY_IR_NUM_MAX						21
#define KEY_TOUCH_NUM_MAX					6
#define KEY_RDEC_NUM_MAX                    6
#define KEY_CTMU_TOUCH_NUM_MAX				6

#define MULT_KEY_ENABLE						DISABLE 		//是否使能组合按键消息, 使能后需要配置组合按键映射表

#define TCFG_KEY_TONE_EN					DISABLE 		// 按键提示音。

#define TCFG_CHIP_RESET_PIN					IO_PORTB_09 // 长按复位
#define TCFG_CHIP_RESET_LEVEL				0 // 0-低电平复位；1-高电平复位
#define TCFG_CHIP_RESET_TIME				8 // 复位时间1 2 4 8 16 单位为秒

//*********************************************************************************//
//                                 iokey 配置                                      //
//*********************************************************************************//
#define TCFG_IOKEY_ENABLE					ENABLE_THIS_MOUDLE//DISABLE_THIS_MOUDLE //是否使能IO按键

#define TCFG_IOKEY_POWER_CONNECT_WAY		ONE_PORT_TO_LOW    //按键一端接低电平一端接IO
#define TCFG_IOKEY_POWER_ONE_PORT			IO_PORTB_09        //IO按键端口

#define TCFG_IOKEY_PREV_CONNECT_WAY			ONE_PORT_TO_LOW  //按键一端接低电平一端接IO
#define TCFG_IOKEY_PREV_ONE_PORT			IO_PORTC_02

#define TCFG_IOKEY_NEXT_CONNECT_WAY 		ONE_PORT_TO_LOW  //按键一端接低电平一端接IO
#define TCFG_IOKEY_NEXT_ONE_PORT			IO_PORTC_06

//*********************************************************************************//
//                                 adkey 配置                                      //
//*********************************************************************************//
#define TCFG_ADKEY_ENABLE                   DISABLE_THIS_MOUDLE//ENABLE_THIS_MOUDLE  //是否使能AD按键
#define TCFG_ADKEY_PORT                     IO_PORTB_01         //AD按键端口(需要注意选择的IO口是否支持AD功能)
/*AD通道选择，需要和AD按键的端口相对应:
    AD_CH_PA1    AD_CH_PA3    AD_CH_PA4    AD_CH_PA5
    AD_CH_PA9    AD_CH_PA1    AD_CH_PB1    AD_CH_PB4
    AD_CH_PB6    AD_CH_PB7    AD_CH_DP     AD_CH_DM
    AD_CH_PB2
*/
#define TCFG_ADKEY_AD_CHANNEL               AD_CH_PB1
#define TCFG_ADKEY_EXTERN_UP_ENABLE         ENABLE_THIS_MOUDLE //是否使用外部上拉

#if TCFG_ADKEY_EXTERN_UP_ENABLE
#define R_UP    220                 //22K，外部上拉阻值在此自行设置
#else
#define R_UP    100                 //10K，内部上拉默认10K
#endif

//必须从小到大填电阻，没有则同VDDIO,填0x3ffL
#define TCFG_ADKEY_AD0      (0)                                 //0R
#define TCFG_ADKEY_AD1      (0x3ffL * 30   / (30   + R_UP))     //3k
#define TCFG_ADKEY_AD2      (0x3ffL * 62   / (62   + R_UP))     //6.2k
#define TCFG_ADKEY_AD3      (0x3ffL * 91   / (91   + R_UP))     //9.1k
#define TCFG_ADKEY_AD4      (0x3ffL * 150  / (150  + R_UP))     //15k
#define TCFG_ADKEY_AD5      (0x3ffL * 240  / (240  + R_UP))     //24k
#define TCFG_ADKEY_AD6      (0x3ffL * 330  / (330  + R_UP))     //33k
#define TCFG_ADKEY_AD7      (0x3ffL * 510  / (510  + R_UP))     //51k
#define TCFG_ADKEY_AD8      (0x3ffL * 1000 / (1000 + R_UP))     //100k
#define TCFG_ADKEY_AD9      (0x3ffL * 2200 / (2200 + R_UP))     //220k
#define TCFG_ADKEY_VDDIO    (0x3ffL)

#define TCFG_ADKEY_VOLTAGE0 ((TCFG_ADKEY_AD0 + TCFG_ADKEY_AD1) / 2)
#define TCFG_ADKEY_VOLTAGE1 ((TCFG_ADKEY_AD1 + TCFG_ADKEY_AD2) / 2)
#define TCFG_ADKEY_VOLTAGE2 ((TCFG_ADKEY_AD2 + TCFG_ADKEY_AD3) / 2)
#define TCFG_ADKEY_VOLTAGE3 ((TCFG_ADKEY_AD3 + TCFG_ADKEY_AD4) / 2)
#define TCFG_ADKEY_VOLTAGE4 ((TCFG_ADKEY_AD4 + TCFG_ADKEY_AD5) / 2)
#define TCFG_ADKEY_VOLTAGE5 ((TCFG_ADKEY_AD5 + TCFG_ADKEY_AD6) / 2)
#define TCFG_ADKEY_VOLTAGE6 ((TCFG_ADKEY_AD6 + TCFG_ADKEY_AD7) / 2)
#define TCFG_ADKEY_VOLTAGE7 ((TCFG_ADKEY_AD7 + TCFG_ADKEY_AD8) / 2)
#define TCFG_ADKEY_VOLTAGE8 ((TCFG_ADKEY_AD8 + TCFG_ADKEY_AD9) / 2)
#define TCFG_ADKEY_VOLTAGE9 ((TCFG_ADKEY_AD9 + TCFG_ADKEY_VDDIO) / 2)

#define TCFG_ADKEY_VALUE0                   0
#define TCFG_ADKEY_VALUE1                   1
#define TCFG_ADKEY_VALUE2                   2
#define TCFG_ADKEY_VALUE3                   3
#define TCFG_ADKEY_VALUE4                   4
#define TCFG_ADKEY_VALUE5                   5
#define TCFG_ADKEY_VALUE6                   6
#define TCFG_ADKEY_VALUE7                   7
#define TCFG_ADKEY_VALUE8                   8
#define TCFG_ADKEY_VALUE9                   9

//*********************************************************************************//
//                                 irkey 配置                                      //
//*********************************************************************************//
#define TCFG_IRKEY_ENABLE                   DISABLE_THIS_MOUDLE//是否使能AD按键
#define TCFG_IRKEY_PORT                     IO_PORTC_08        //IR按键端口

//*********************************************************************************//
//                              tocuh key 配置                                     //
//*********************************************************************************//
#define TCFG_TOUCH_KEY_ENABLE               DISABLE_THIS_MOUDLE             //是否使能plcnt触摸按键
//key0配置
#define TCFG_TOUCH_KEY0_PRESS_DELTA	   	    100//变化阈值，当触摸产生的变化量达到该阈值，则判断被按下，每个按键可能不一样，可先在驱动里加到打印，再反估阈值
#define TCFG_TOUCH_KEY0_PORT 				IO_PORTB_06 //触摸按键key0 IO配置
#define TCFG_TOUCH_KEY0_VALUE 				0x12 		//触摸按键key0 按键值
//key1配置
#define TCFG_TOUCH_KEY1_PRESS_DELTA	   	    100//变化阈值，当触摸产生的变化量达到该阈值，则判断被按下，每个按键可能不一样，可先在驱动里加到打印，再反估阈值
#define TCFG_TOUCH_KEY1_PORT 				IO_PORTB_07 //触摸按键key1 IO配置
#define TCFG_TOUCH_KEY1_VALUE 				0x34        //触摸按键key1 按键值

//*********************************************************************************//
//                                 rdec_key 配置                                      //
//*********************************************************************************//
#define TCFG_RDEC_KEY_ENABLE					DISABLE_THIS_MOUDLE //是否使能RDEC按键
//RDEC0配置
#define TCFG_RDEC0_ECODE1_PORT					IO_PORTA_03
#define TCFG_RDEC0_ECODE2_PORT					IO_PORTA_04
#define TCFG_RDEC0_KEY0_VALUE 				 	0
#define TCFG_RDEC0_KEY1_VALUE 				 	1

//RDEC1配置
#define TCFG_RDEC1_ECODE1_PORT					IO_PORTA_05
#define TCFG_RDEC1_ECODE2_PORT					IO_PORTA_06
#define TCFG_RDEC1_KEY0_VALUE 				 	2
#define TCFG_RDEC1_KEY1_VALUE 				 	3

//RDEC2配置
#define TCFG_RDEC2_ECODE1_PORT					IO_PORTB_00
#define TCFG_RDEC2_ECODE2_PORT					IO_PORTB_01
#define TCFG_RDEC2_KEY0_VALUE 				 	4
#define TCFG_RDEC2_KEY1_VALUE 				 	5

//*********************************************************************************//
//                                 Audio配置                                       //
//*********************************************************************************//

/*PA*/
#define TCFG_AUDIO_DAC_PA_PORT				NO_CONFIG_PORT //SHDN helena for amp mute
#define TCFG_AUDIO_DAC_PA_PORT1				IO_PORTG_03//NO_CONFIG_PORT SHDN helena for amp mute

//>>>>>Audio_DAC配置
#define TCFG_AUDIO_DAC_ENABLE				ENABLE_THIS_MOUDLE

#define TCFG_SUPPORT_MIC_CAPLESS			DISABLE_THIS_MOUDLE//	ENABLE_THIS_MOUDLE
#define TCFG_MIC_CAPLESS_ENABLE				ENABLE_THIS_MOUDLE//DISABLE_THIS_MOUDLE//ENABLE_THIS_MOUDLE
#define TCFG_MIC1_CAPLESS_ENABLE			DISABLE_THIS_MOUDLE//ENABLE_THIS_MOUDLE
/*
DAC硬件上的连接方式,可选的配置：
    DAC_OUTPUT_MONO_L               左声道
    DAC_OUTPUT_MONO_R               右声道
    DAC_OUTPUT_LR                   立体声
    DAC_OUTPUT_MONO_LR_DIFF         左右差分

    TCFG_AUDIO_DAC_CONNECT_MODE 决定方案需要使用多少个DAC声道
    TCFG_AUDIO_DAC_MODE 决定DAC是差分输出模式还是单端输出模式

    注意:
    1、选择 DAC_OUTPUT_MONO_LR_DIFF DAC要选单端输出，选差分N端也会打开导致功耗大。
*/
#define TCFG_AUDIO_DAC_CONNECT_MODE         DAC_OUTPUT_LR

/*
         DAC模式选择
#define DAC_MODE_L_DIFF          (0)  // 低压差分模式   , 适用于低功率差分耳机  , 输出幅度 0~2Vpp
#define DAC_MODE_H1_DIFF         (1)  // 高压1档差分模式, 适用于高功率差分耳机  , 输出幅度 0~3Vpp  , VDDIO >= 3.0V
#define DAC_MODE_H1_SINGLE       (2)  // 高压1档单端模式, 适用于高功率单端PA音箱, 输出幅度 0~1.5Vpp, VDDIO >= 3.0V
#define DAC_MODE_H2_DIFF         (3)  // 高压2档差分模式, 适用于高功率差分PA音箱, 输出幅度 0~5Vpp  , VDDIO >= 3.3V
#define DAC_MODE_H2_SINGLE       (4)  // 高压2档单端模式, 适用于高功率单端PA音箱, 输出幅度 0~2.5Vpp, VDDIO >= 3.3V
*/
#define TCFG_AUDIO_DAC_MODE                 DAC_MODE_H1_DIFF//DAC_MODE_H2_SINGLE

/*
 *系统音量类型选择
 *软件数字音量是指纯软件对声音进行运算后得到的
 *硬件数字音量是指dac内部数字模块对声音进行运算后输出
 *选择软件数字音量或者软件数字音量会固定模拟音量，
 *固定的模拟音量值由 MAX_ANA_VOL 决定
 */
#define VOL_TYPE_DIGITAL		0	//软件数字音量
#define VOL_TYPE_AD				2	//联合音量(模拟数字混合调节 暂不支持)
#define VOL_TYPE_DIGITAL_HW		3  	//硬件数字音量
#define VOL_TYPE_DIGGROUP		4	//独立通道软件数字音量
#define SYS_VOL_TYPE            VOL_TYPE_DIGITAL_HW
//每个解码通道都开启数字音量管理,音量类型为VOL_TYPE_DIGGROUP时要使能
#define SYS_DIGVOL_GROUP_EN     DISABLE

#if  (SYS_VOL_TYPE == VOL_TYPE_DIGGROUP)
#undef SYS_DIGVOL_GROUP_EN
#define SYS_DIGVOL_GROUP_EN     ENABLE
#endif
/*
 *通话的时候使用数字音量
 *0：通话使用和SYS_VOL_TYPE一样的音量调节类型
 *1：通话使用数字音量调节，更加平滑
 */
#define TCFG_CALL_USE_DIGITAL_VOLUME		0

/*
 *PC模式左右声道音量独立设置功能
 */
#define PC_VOL_INDEPENDENT_EN                  0
//>>>>>Audio_ADC配置
#define TCFG_AUDIO_ADC_ENABLE				ENABLE_THIS_MOUDLE
/* MIC通道配置:可配置MIC0 MIC1 MIC2 MIC3
 * 同时使能多个MIC，直接或上对应通道即可，比如同时使能MIC0和MIC1:
 * #define TCFG_AUDIO_ADC_MIC_CHA			(AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1)
 */
#define TCFG_AUDIO_ADC_MIC_CHA				AUDIO_ADC_MIC_3//(TCFG_PC_DUAL_MIC ? (AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1) :AUDIO_ADC_MIC_0)
/*MIC LDO电流档位设置：0:0.625ua    1:1.25ua    2:1.875ua    3:2.5ua*/
#define TCFG_AUDIO_ADC_LD0_SEL				2

#define TCFG_AUDIO_DUAL_MIC_ENABLE			DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_NOISE_GATE				DISABLE_THIS_MOUDLE

/*MIC模式配置:单端隔直电容模式/差分隔直电容模式/单端省电容模式*/
#define TCFG_AUDIO_MIC_MODE                 AUDIO_MIC_CAP_DIFF_MODE//AUDIO_MIC_CAP_MODE
#define TCFG_AUDIO_MIC1_MODE                AUDIO_MIC_CAP_MODE
#define TCFG_AUDIO_MIC2_MODE                AUDIO_MIC_CAP_MODE
#define TCFG_AUDIO_MIC3_MODE                AUDIO_MIC_CAP_DIFF_MODE


 #define TCFG_USE_OTHER_MIC_BIAS            DISABLE_THIS_MOUDLE
// 选择哪个mic 作为省电容mic来预校准, 必须先把要设置的mic的TCFG_AUDIO_MIC_MODE设成AUDIO_MIC_CAPLESS_MODE
#define TCFG_CAPLESS_MIC_CHANNEL            AUDIO_ADC_MIC_0

/*
 *>>MIC电源管理:根据具体方案，选择对应的mic供电方式
 *(1)如果是多种方式混合，则将对应的供电方式或起来即可，比如(MIC_PWR_FROM_GPIO | MIC_PWR_FROM_MIC_BIAS)
 *(2)如果使用固定电源供电(比如dacvdd)，则配置成DISABLE_THIS_MOUDLE
 */
#define MIC_PWR_FROM_GPIO       (1UL << 0)  //使用普通IO输出供电
#define MIC_PWR_FROM_MIC_BIAS   (1UL << 1)  //使用内部mic_ldo供电(有上拉电阻可配)
#define MIC_PWR_FROM_MIC_LDO    (1UL << 2)  //使用内部mic_ldo供电
//配置MIC电源
#define TCFG_AUDIO_MIC_PWR_CTL              MIC_PWR_FROM_MIC_LDO

//使用普通IO输出供电:不用的port配置成NO_CONFIG_PORT
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_GPIO)
#define TCFG_AUDIO_MIC_PWR_PORT             NO_CONFIG_PORT//IO_PORTC_02
#define TCFG_AUDIO_MIC1_PWR_PORT            NO_CONFIG_PORT
#define TCFG_AUDIO_MIC2_PWR_PORT            NO_CONFIG_PORT
#endif/*MIC_PWR_FROM_GPIO*/

//使用内部mic_ldo供电(有上拉电阻可配)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_BIAS)
#define TCFG_AUDIO_MIC0_BIAS_EN             ENABLE_THIS_MOUDLE/*Port:PA2*/
#define TCFG_AUDIO_MIC1_BIAS_EN             ENABLE_THIS_MOUDLE/*Port:PA4*/
#define TCFG_AUDIO_MIC2_BIAS_EN             ENABLE_THIS_MOUDLE/*Port:PG7*/
#define TCFG_AUDIO_MIC3_BIAS_EN             ENABLE_THIS_MOUDLE/*Port:PG5*/
#endif/*MIC_PWR_FROM_MIC_BIAS*/

//使用内部mic_ldo供电(Port:PA0)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_LDO)
#define TCFG_AUDIO_MIC_LDO_EN               ENABLE_THIS_MOUDLE
#endif/*MIC_PWR_FROM_MIC_LDO*/
/*>>MIC电源管理配置结束*/

//第三方清晰语音开发使能
#define TCFG_CVP_DEVELOP_ENABLE             DISABLE_THIS_MOUDLE

/*通话降噪模式配置*/
#define CVP_ANS_MODE	0	/*传统经典降噪*/
#define CVP_DNS_MODE	1	/*神经网络降噪*/
#define TCFG_AUDIO_CVP_NS_MODE				CVP_ANS_MODE

/*通话CVP 测试使能，配合量产设备测试MIC频响和算法性能*/
#define TCFG_AUDIO_CVP_DUT_ENABLE			DISABLE_THIS_MOUDLE

// AUTOMUTE
#define AUDIO_OUTPUT_AUTOMUTE   			DISABLE_THIS_MOUDLE



/*Audio数据导出配置:支持BT_SPP\SD\UART载体导出*/
#define AUDIO_DATA_EXPORT_USE_SD	1
#define AUDIO_DATA_EXPORT_USE_SPP 	2
#define AUDIO_DATA_EXPORT_USE_UART 	3
#define TCFG_AUDIO_DATA_EXPORT_ENABLE		DISABLE_THIS_MOUDLE

/*通话清晰语音处理数据导出*/
 #define AUDIO_PCM_DEBUG

/*通话参数在线调试*/
#define TCFG_AEC_TOOL_ONLINE_ENABLE         DISABLE_THIS_MOUDLE

/*多mic写卡功能，用于调试*/
#define TCFG_MULTIPLE_MIC_REC_ENABLE        DISABLE_THIS_MOUDLE

//*********************************************************************************//
//                                 Audio VAD                                       //
//*********************************************************************************//
#define TCFG_AUDIO_VAD_ENABLE 				DISABLE_THIS_MOUDLE   //ENABLE_THIS_MOUDLE


//AudioEffects代码链接管理
#define AUDIO_EFFECTS_DRC_AT_RAM			1
#define AUDIO_EFFECTS_REVERB_AT_RAM			0
#define AUDIO_EFFECTS_ECHO_AT_RAM			0
#define AUDIO_EFFECTS_AFC_AT_RAM			1
#define AUDIO_EFFECTS_EQ_AT_RAM				1
#define AUDIO_EFFECTS_NOISEGATE_AT_RAM		0
#define AUDIO_EFFECTS_MIC_EFFECT_AT_RAM		0
#define AUDIO_EFFECTS_MIC_STREAM_AT_RAM		0
#define AUDIO_EFFECTS_DEBUG_AT_RAM			0
#define AUDIO_EFFECTS_DYNAMIC_EQ_AT_RAM     0
#define AUDIO_EFFECTS_VBASS_AT_RAM          0
#define AUDIO_STREAM_AT_RAM                 0
//*********************************************************************************//
//                                  充电舱配置                                     //
//   充电舱/蓝牙测试盒/ANC测试三者为同级关系,开启任一功能都会初始化PP0通信接口     //
//*********************************************************************************//
#define TCFG_CHARGESTORE_ENABLE				DISABLE_THIS_MOUDLE         //是否支持智能充电舱
#define TCFG_TEST_BOX_ENABLE			    DISABLE_THIS_MOUDLE         //是否支持蓝牙测试盒
#define TCFG_ANC_BOX_ENABLE			        DISABLE_THIS_MOUDLE         //是否支持ANC测试盒
#define TCFG_CHARGESTORE_PORT				IO_PORTP_00                 //耳机通讯的IO口

//*********************************************************************************//
//                                  充电参数配置                                   //
//*********************************************************************************//
//是否支持芯片内置充电
#define TCFG_CHARGE_ENABLE					ENABLE_THIS_MOUDLE//DISABLE_THIS_MOUDLE//ENABLE_THIS_MOUDLE
//是否支持开机充电
#define TCFG_CHARGE_POWERON_ENABLE			ENABLE//DISABLE
//是否支持拔出充电自动开机功能
#define TCFG_CHARGE_OFF_POWERON_NE			DISABLE
/*充电截止电压可选配置*/
#define TCFG_CHARGE_FULL_V					CHARGE_FULL_V_4534//CHARGE_FULL_V_4199//
/*充电截止电流可选配置*/
#define TCFG_CHARGE_FULL_MA					CHARGE_FULL_mA_10
/*恒流充电电流可选配置*/
#define TCFG_CHARGE_MA						CHARGE_mA_200
/*涓流充电电流配置*/
#define TCFG_CHARGE_TRICKLE_MA              CHARGE_mA_20

#if (TCFG_PC_ENABLE && TCFG_CHARGE_ENABLE && !TCFG_CHARGE_POWERON_ENABLE)
#define TCFG_USB_PORT_CHARGE                ENABLE
#endif

//*********************************************************************************//
//                                  LED 配置                                       //
//*********************************************************************************//
#define TCFG_PWMLED_ENABLE					DISABLE_THIS_MOUDLE			//是否支持PMW LED推灯模块
#define TCFG_PWMLED_IOMODE					LED_ONE_IO_MODE				//LED模式，单IO还是两个IO推灯
#define TCFG_PWMLED_PIN						IO_PORTB_06					//LED使用的IO口

#if TCFG_PWMLED_ENABLE
#else
#define TCFG_IOLED_ENABLE                  ENABLE_THIS_MOUDLE
#define TCFG_IOLED_BLE                     IO_PORTB_03
#define TCFG_IOLED_GREEN                   IO_PORTA_11
#define TCFG_IOLED_CHANGE                  IO_PORTC_00
#define TCFG_IOLED_NUM                     3
#endif

//*********************************************************************************//
//                                  UI 配置                                        //
//*********************************************************************************//
#define TCFG_UI_ENABLE 						DISABLE_THIS_MOUDLE//UI总开关

/* 选择UI风格 */
// #define CONFIG_UI_STYLE                     STYLE_JL_WTACH
#define CONFIG_UI_STYLE                     STYLE_JL_SOUNDBOX

/* 选择推屏用的SPI设备号 */
#define TCFG_TFT_LCD_DEV_SPI_HW_NUM			1// 1: SPI1    2: SPI2 配置lcd选择的spi口

/* 选择屏幕类型 */
#define TCFG_LCD_OLED_ENABLE				ENABLE_THIS_MOUDLE	// oled黑白点阵屏


/* 选择具体屏幕驱动 */
#define TCFG_OLED_SPI_SSD1306_ENABLE		ENABLE_THIS_MOUDLE

/* 选择IMD模块的SPI驱动模式 */
#define LCD_DRIVE_CONFIG					SPI_4WIRE_RGB565_1T8B
// #define LCD_DRIVE_CONFIG					QSPI_RGB565_SUBMODE0_1T8B



//*********************************************************************************//
//                                  时钟配置                                       //
//*********************************************************************************//
#define TCFG_CLOCK_SYS_SRC					SYS_CLOCK_INPUT_PLL_BT_OSC   //系统时钟源选择
//#define TCFG_CLOCK_SYS_HZ					12000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					16000000                     //系统时钟设置
#define TCFG_CLOCK_SYS_HZ					24000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					32000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					48000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					54000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					64000000                     //系统时钟设置
#define TCFG_CLOCK_OSC_HZ					24000000                     //外界晶振频率设置
#define TCFG_CLOCK_MODE                     CLOCK_MODE_ADAPTIVE

// 240Mhz: 160 120 96 80 48...
// 192Mhz: 192 128 96 64 48...
#define TCFG_CLOCK_PLL_240MHZ				0	// PLL跑240Mhz

//*********************************************************************************//
//                                  低功耗配置                                     //
//*********************************************************************************//
#define TCFG_LOWPOWER_POWER_SEL				PWR_LDO15          //电源模式设置，可选DCDC和LDO
#define TCFG_LOWPOWER_BTOSC_DISABLE			0                   //低功耗模式下BTOSC是否保持
#define TCFG_LOWPOWER_LOWPOWER_SEL			0                   //芯片是否进入powerdown
#define TCFG_LOWPOWER_VDDIOM_LEVEL			VDDIOM_VOL_34V
#define TCFG_LOWPOWER_OSC_TYPE              OSC_TYPE_LRC
#define TCFG_LOWPOWER_RAM_SIZE              0

//*********************************************************************************//
//                                  EQ配置                                         //
//*********************************************************************************//
#define TCFG_EQ_ENABLE                      1     //支持EQ功能,EQ总使能
// #if TCFG_EQ_ENABLE
#define TCFG_BT_MUSIC_EQ_ENABLE             1     //支持蓝牙音乐EQ
#define TCFG_PHONE_EQ_ENABLE                1     //支持通话近端EQ
#define TCFG_AEC_DCCS_EQ_ENABLE           	1     // AEC DCCS
#define TCFG_AEC_UL_EQ_ENABLE           	1     // AEC UL
#define TCFG_MUSIC_MODE_EQ_ENABLE           1     //支持音乐模式EQ
#define TCFG_BROADCAST_MODE_EQ_ENABLE       1     //支持广播模式EQ
#define TCFG_LINEIN_MODE_EQ_ENABLE          1     //支持linein近端EQ
#define TCFG_FM_MODE_EQ_ENABLE              0     //支持fm模式EQ
#define TCFG_SPDIF_MODE_EQ_ENABLE           0     //支持SPDIF模式EQ
#define TCFG_PC_MODE_EQ_ENABLE              1     //支持pc模式EQ
#define TCFG_AUDIO_OUT_EQ_ENABLE			0 	  //高低音EQ


#define EQ_SECTION_MAX                      10	  //EQ段数
#define TCFG_DYNAMIC_EQ_ENABLE              0     //动态EQ使能，接在EQ后，需输入32bit位宽数据

/*省电容mic通过eq模块实现去直流滤波*/
#if (TCFG_SUPPORT_MIC_CAPLESS && (TCFG_MIC_CAPLESS_ENABLE || TCFG_MIC1_CAPLESS_ENABLE))
#if ((TCFG_EQ_ENABLE == 0) || (TCFG_AEC_DCCS_EQ_ENABLE == 0))
#error "MicCapless enable,Please enable TCFG_EQ_ENABLE and TCFG_AEC_DCCS_EQ_ENABLE"
#endif
#endif

#define TCFG_DRC_ENABLE						1	  //DRC 总使能
#define TCFG_AUDIO_MDRC_ENABLE              0     //多带DRC使能  0:关闭多带DRC，  1：使能多带DRC  2：使能多带DRC 并且 多带DRC后再做一次全带的drc


#define TCFG_BT_MUSIC_DRC_ENABLE            1     //支持蓝牙音乐DRC
#define TCFG_MUSIC_MODE_DRC_ENABLE          1     //支持音乐模式DRC
#define TCFG_BROADCAST_MODE_DRC_ENABLE      1     //支持广播模式DRC
#define TCFG_LINEIN_MODE_DRC_ENABLE         1     //支持LINEIN模式DRC
#define TCFG_FM_MODE_DRC_ENABLE             0     //支持FM模式DRC
#define TCFG_SPDIF_MODE_DRC_ENABLE          0     //支持SPDIF模式DRC
#define TCFG_PC_MODE_DRC_ENABLE             1     //支持PC模式DRC
#define TCFG_AUDIO_OUT_DRC_ENABLE			0 	  //高低音EQ后的DRC
#define TCFG_PHONE_DRC_ENABLE               0     //通话下行drc



#define LINEIN_MODE_SOLE_EQ_EN              0  //linein模式是否需要独立的音效

//*********************************************************************************//
//                          新音箱配置工具 && 调音工具                             //
//*********************************************************************************//
#define TCFG_CFG_TOOL_ENABLE				DISABLE		  	//是否支持音箱在线配置工具
#define TCFG_EFFECT_TOOL_ENABLE				DISABLE		  	//是否支持在线音效调试,使能该项还需使能EQ总使能TCFG_EQ_ENABL,
#define TCFG_NULL_COMM						0				//不支持通信
#define TCFG_UART_COMM						1				//串口通信
#define TCFG_USB_COMM						2				//USB通信
#if (TCFG_CFG_TOOL_ENABLE || TCFG_EFFECT_TOOL_ENABLE)
#define TCFG_COMM_TYPE						TCFG_USB_COMM	//通信方式选择
#else
#define TCFG_COMM_TYPE						TCFG_NULL_COMM
#endif
#define TCFG_ONLINE_TX_PORT					IO_PORT_DP      //UART模式调试TX口选择
#define TCFG_ONLINE_RX_PORT					IO_PORT_DM      //UART模式调试RX口选择
#define TCFG_ONLINE_ENABLE                  (TCFG_EFFECT_TOOL_ENABLE)    //是否支持音效在线调试功能

/***********************************非用户配置区***********************************/
#if (TCFG_CFG_TOOL_ENABLE || TCFG_ONLINE_ENABLE)
#if TCFG_COMM_TYPE == TCFG_UART_COMM
#undef TCFG_UDISK_ENABLE
#define TCFG_UDISK_ENABLE 					0
#undef TCFG_SD0_PORTS
#define TCFG_SD0_PORTS 					   'B'
#endif
#endif

#include "usb_std_class_def.h"
#if (TCFG_CFG_TOOL_ENABLE || TCFG_EFFECT_TOOL_ENABLE)
#if (TCFG_COMM_TYPE == TCFG_USB_COMM)
#define TCFG_USB_CDC_BACKGROUND_RUN         ENABLE
#if (TCFG_USB_CDC_BACKGROUND_RUN && (!TCFG_PC_ENABLE))
#define TCFG_OTG_USB_DEV_EN                 0
#endif
#endif
#if (TCFG_COMM_TYPE == TCFG_UART_COMM)
#define TCFG_USB_CDC_BACKGROUND_RUN         DISABLE
#endif
#endif
/**********************************************************************************/

//*********************************************************************************//
//                                  混响配置                                   //
//*********************************************************************************//
#define TCFG_MIC_EFFECT_ENABLE       	DISABLE_THIS_MOUDLE
#define TCFG_MIC_EFFECT_START_DIR    	DISABLE//开机直接启动混响

//混响效果配置
#define MIC_EFFECT_REVERB		1 //混响
#define MIC_EFFECT_ECHO         2 //回声
#define MIC_EFFECT_REVERB_ECHO  3 //混响+回声
#define MIC_EFFECT_MEGAPHONE    4 //扩音器/大声公
#define TCFG_MIC_EFFECT_SEL          	MIC_EFFECT_MEGAPHONE//MIC_EFFECT_REVERB//MIC_EFFECT_REVERB_ECHO//MIC_EFFECT_ECHO

//MIC变声模块使能配置
#define TCFG_MIC_VOICE_CHANGER_ENABLE 	DISABLE
#define TCFG_MIC_AUTOTUNE_ENABLE        DISABLE//电音模块使能
#define  TCFG_MIC_BASS_AND_TREBLE_ENABLE     DISABLE//混响高低音
/***********************************非用户配置区***********************************/
#if TCFG_MIC_EFFECT_ENABLE
#undef EQ_SECTION_MAX
#define EQ_SECTION_MAX 25
#ifdef TCFG_AEC_ENABLE
#undef TCFG_AEC_ENABLE
#define TCFG_AEC_ENABLE 0
#endif//TCFG_AEC_ENABLE

#if TCFG_DYNAMIC_EQ_ENABLE
#undef TCFG_DYNAMIC_EQ_ENABLE
#define TCFG_DYNAMIC_EQ_ENABLE 0
#endif

#if TCFG_AUDIO_MDRC_ENABLE
#undef TCFG_AUDIO_MDRC_ENABLE
#define TCFG_AUDIO_MDRC_ENABLE 0
#endif
#if !TCFG_EQ_ENABLE
#undef TCFG_EQ_ENABLE
#define TCFG_EQ_ENABLE 1//混响必开eq
#endif
#if !TCFG_DRC_ENABLE
#undef TCFG_DRC_ENABLE
#define TCFG_DRC_ENABLE 1//混响必开drc
#endif
#endif
/**********************************************************************************/
#define TCFG_REVERB_SAMPLERATE_DEFUAL (44100)
#define MIC_EFFECT_SAMPLERATE			(44100L)

#if TCFG_MIC_EFFECT_ENABLE
#undef MIC_SamplingFrequency
#define     MIC_SamplingFrequency         1
#undef MIC_AUDIO_RATE
#define     MIC_AUDIO_RATE              MIC_EFFECT_SAMPLERATE
#undef  SPK_AUDIO_RATE
#define SPK_AUDIO_RATE                  TCFG_REVERB_SAMPLERATE_DEFUAL
#endif


/*********配置MIC_EFFECT_CONFIG宏定义即可********************************/
#define TCFG_USB_MIC_ECHO_ENABLE           DISABLE //不能与TCFG_MIC_EFFECT_ENABLE同时打开
#define TCFG_USB_MIC_DATA_FROM_MICEFFECT   DISABLE //要确保开usbmic前已经开启混响



//*********************************************************************************//
//                                  g-sensor配置                                   //
//*********************************************************************************//


#if	TCFG_HW_SPI2_ENABLE
#define TCFG_LSM6DSL_EN                           1
#else
#define TCFG_GSENSOR_ENABLE                       0     //gSensor使能
#define TCFG_DA230_EN                             0
#define TCFG_SC7A20_EN                            0
#define TCFG_STK8321_EN                           0
#define TCFG_IRSENSOR_ENABLE                      0
#define TCFG_JSA1221_ENABLE                       0
#define TCFG_GSENOR_USER_IIC_TYPE                 0     //0:软件IIC  1:硬件IIC
#define TCFG_LSM6DSL_EN                           0
#endif
#define TCFG_OHRMO_EN                             1   //added by helena for cardiotachometer
//*********************************************************************************//
//                                  系统配置                                         //
//*********************************************************************************//
#define TCFG_AUTO_SHUT_DOWN_TIME		          0   //没有蓝牙连接自动关机时间
#define TCFG_SYS_LVD_EN						      1   //电量检测使能
#define TCFG_POWER_ON_NEED_KEY				      0	  //慎重选择，*****是否需要按按键开机配置
#define TWFG_APP_POWERON_IGNORE_DEV         	  4000//上电忽略挂载设备，0时不忽略，非0则n毫秒忽略

//*********************************************************************************//
//                                  蓝牙配置                                       //
//*********************************************************************************//
#define TCFG_USER_TWS_ENABLE                	  0   //tws功能使能
#define TCFG_USER_BLE_ENABLE                	  1   //BLE功能使能
#define TCFG_USER_BT_CLASSIC_ENABLE         	  1   //经典蓝牙功能使能
#define TCFG_BT_SUPPORT_AAC                 	  1   //AAC格式支持
#define TCFG_USER_EMITTER_ENABLE           	 	  0   //emitter功能使能
#define TCFG_BT_SNIFF_ENABLE                	  1   //bt sniff 功能使能

#define USER_SUPPORT_PROFILE_SPP    0
#define USER_SUPPORT_PROFILE_HFP    1
#define USER_SUPPORT_PROFILE_A2DP   1
#define USER_SUPPORT_PROFILE_AVCTP  1
#define USER_SUPPORT_PROFILE_HID    1
#define USER_SUPPORT_PROFILE_PNP    1
#define USER_SUPPORT_PROFILE_PBAP   0


#define TCFG_VIRTUAL_FAST_CONNECT_FOR_EMITTER      0

#if TCFG_USER_TWS_ENABLE
#define TCFG_BD_NUM						    1   //连接设备个数配置
#define TCFG_AUTO_STOP_PAGE_SCAN_TIME       0   //配置一拖二第一台连接后自动关闭PAGE SCAN的时间(单位分钟)
#define TCFG_USER_ESCO_SLAVE_MUTE           1   //对箱通话slave出声音
#else
#define TCFG_BD_NUM						    1   //连接设备个数配置
#define TCFG_AUTO_STOP_PAGE_SCAN_TIME       0 //配置一拖二第一台连接后自动关闭PAGE SCAN的时间(单位分钟)
#define TCFG_USER_ESCO_SLAVE_MUTE           0   //对箱通话slave出声音
#endif

#define BT_INBAND_RINGTONE                  0   //是否播放手机自带来电铃声
#define BT_PHONE_NUMBER                     1   //是否播放来电报号
#define BT_SYNC_PHONE_RING                  0   //是否TWS同步播放来电铃声
#define BT_SUPPORT_DISPLAY_BAT              1   //是否使能电量检测
#define BT_SUPPORT_MUSIC_VOL_SYNC           1   //是否使能音量同步

#define TCFG_BLUETOOTH_BACK_MODE			1	//后台模式

#if (TCFG_USER_TWS_ENABLE && TCFG_BLUETOOTH_BACK_MODE) && (TCFG_BT_SNIFF_ENABLE==0) && defined(CONFIG_LOCAL_TWS_ENABLE)
#define TCFG_DEC2TWS_ENABLE					1
#define TCFG_PCM_ENC2TWS_ENABLE				1
#define TCFG_TONE2TWS_ENABLE				1
#else
#define TCFG_DEC2TWS_ENABLE					0
#define TCFG_PCM_ENC2TWS_ENABLE				0
#define TCFG_TONE2TWS_ENABLE				0
#endif

//#define TWS_PHONE_LONG_TIME_DISCONNECTED

/*spp数据导出配置*/
#if (TCFG_AUDIO_DATA_EXPORT_ENABLE == AUDIO_DATA_EXPORT_USE_SPP)
#undef TCFG_USER_TWS_ENABLE
#undef TCFG_USER_BLE_ENABLE
#undef TCFG_BD_NUM
#undef USER_SUPPORT_PROFILE_SPP
#undef USER_SUPPORT_PROFILE_A2DP
#define TCFG_USER_TWS_ENABLE        0//spp数据导出，关闭tws
#define TCFG_USER_BLE_ENABLE        0//spp数据导出，关闭ble
#define TCFG_BD_NUM					1//连接设备个数配置
#define USER_SUPPORT_PROFILE_SPP	1
#define USER_SUPPORT_PROFILE_A2DP   0
#define APP_ONLINE_DEBUG            1//通过spp导出数据
#else
#define APP_ONLINE_DEBUG            0//在线APP调试,发布默认不开
#endif/*TCFG_AUDIO_DATA_EXPORT_ENABLE*/

/*以下功能需要打开SPP和在线调试功能
 *1、回音消除参数在线调试
 *2、通话CVP_DUT 产测模式
 *3、ANC工具蓝牙spp调试
 */
#if (TCFG_AEC_TOOL_ONLINE_ENABLE || TCFG_AUDIO_CVP_DUT_ENABLE)
#undef USER_SUPPORT_PROFILE_SPP
#undef APP_ONLINE_DEBUG
#define USER_SUPPORT_PROFILE_SPP	1
#define APP_ONLINE_DEBUG            1
#endif/*TCFG_AEC_TOOL_ONLINE_ENABLE*/

#if (APP_ONLINE_DEBUG && !USER_SUPPORT_PROFILE_SPP)
#error "NEED ENABLE USER_SUPPORT_PROFILE_SPP!!!"
#endif

#define TCFG_BROADCAST_ENABLE       DISABLE_THIS_MOUDLE
/* le audio big controller mode control */
#if TCFG_BROADCAST_ENABLE
#define LEA_BIG_CTRLER_TX_EN        1
#define LEA_BIG_CTRLER_RX_EN        1
#define BROADCAST_DATA_SYNC_EN      1//广播状态同步
#define BROADCAST_LOCAL_DEC_EN      1
#define BROADCAST_RECEIVER_CLOSE_EDR_CONN   0//广播从机关闭经典蓝牙连接
#else
#define LEA_BIG_CTRLER_TX_EN        0
#define LEA_BIG_CTRLER_RX_EN        0
#define BROADCAST_DATA_SYNC_EN      0//广播状态同步
#define BROADCAST_LOCAL_DEC_EN      0
#define BROADCAST_RECEIVER_CLOSE_EDR_CONN   0//广播从机关闭经典蓝牙连接
#endif

//*********************************************************************************//
//                                  AI配置                                       //
//*********************************************************************************//
#define CONFIG_APP_BT_ENABLE // AI功能、流程总开关

///注意：以下功能配置只选其一
#ifdef CONFIG_APP_BT_ENABLE
#define	   RCSP_MODE			     RCSP_MODE_SOUNDBOX
#define    TRANS_DATA_EN             0
#define	   ANCS_CLIENT_EN			 0
#define	   BLE_CLIENT_EN 			 0
#define	   AI_APP_PROTOCOL 			 0
#define    TUYA_DEMO_EN              0
#else
#define    RCSP_MODE				 RCSP_MODE_OFF
#define    TRANS_DATA_EN             0
#define	   ANCS_CLIENT_EN			 0
#define	   BLE_CLIENT_EN 			 0
#define    AI_APP_PROTOCOL           0
#define    TUYA_DEMO_EN              0
#endif

///注意：对应的第三方应用平台是否需要支持语音上报， 如果需要请使能BT_MIC_EN
//对应的语音编码参数在mic_rec.c中定义
#define BT_MIC_EN                  	 1

//*********************************************************************************//
//                           编解码格式配置(CodecFormats Config)                   //
//*********************************************************************************//
//解码格式使能配置(Decode Format Config)
#define TCFG_DEC_MP3_ENABLE                 ENABLE
#define TCFG_DEC_WTGV2_ENABLE				ENABLE
#define TCFG_DEC_G729_ENABLE                ENABLE
#define TCFG_DEC_WMA_ENABLE					ENABLE
#define TCFG_DEC_WAV_ENABLE					ENABLE
#define TCFG_DEC_FLAC_ENABLE				DISABLE
#define TCFG_DEC_APE_ENABLE					DISABLE
#define TCFG_DEC_M4A_ENABLE					ENABLE
#define TCFG_DEC_ALAC_ENABLE				ENABLE
#define TCFG_DEC_AMR_ENABLE					ENABLE
#define TCFG_DEC_DTS_ENABLE					ENABLE
#define TCFG_DEC_G726_ENABLE			    DISABLE
#define TCFG_DEC_MIDI_ENABLE			    DISABLE
#define TCFG_DEC_MTY_ENABLE					DISABLE
#define TCFG_DEC_SBC_ENABLE					ENABLE
#define TCFG_DEC_PCM_ENABLE					ENABLE
#define TCFG_DEC_CVSD_ENABLE				ENABLE
#define TCFG_DEC_SPEEX_ENABLE               DISABLE
#define TCFG_DEC_OPUS_ENABLE                DISABLE
#define TCFG_DEC_JLA_ENABLE                 ENABLE

//编码格式使能配置(Encode Format Config)
#define TCFG_ENC_CVSD_ENABLE                ENABLE
#define TCFG_ENC_MSBC_ENABLE                ENABLE
#define TCFG_ENC_MP3_ENABLE                 ENABLE
#define TCFG_ENC_ADPCM_ENABLE               DISABLE
#define TCFG_ENC_PCM_ENABLE                 DISABLE
#define TCFG_ENC_SBC_ENABLE                 DISABLE
#define TCFG_ENC_OPUS_ENABLE                DISABLE
#define TCFG_ENC_SPEEX_ENABLE               DISABLE
#define TCFG_ENC_JLA_ENABLE				    ENABLE
#define TCFG_ENC_AMR_ENABLE				    DISABLE	// 固定单声道8000采样

//JLA 编解码参数配置
#if TCFG_ENC_JLA_ENABLE
#define JLA_CODING_SAMPLERATE  48000 	//JLA 编码的采样率 广播暂不支持44100;
#define JLA_CODING_FRAME_LEN   100  	//一帧编码的pcm时间长,只支持25，50，100,一帧编码的pcm时间长,单位ms  广播暂不支持修改;
#define JLA_CODING_CHANNEL     2  		//JLA 的通道数 其他参数不变，实际单声道的码率是双声道码率的 2 倍;
#if (JLA_CODING_CHANNEL == 2)
#define JLA_CODING_BIT_RATE    96000  		//JLA 的码率 广播时 若码率大于96K,需要修改每次传输数据大小为 1 帧编码长度;
#else
#define JLA_CODING_BIT_RATE    64000  		//JLA 的码率 广播时 若码率大于96K,需要修改每次传输数据大小为 1 帧编码长度;
#endif
#endif/*TCFG_ENC_JLA_ENABLE*/

/*实时音频低延时使能，BIG和CIG两种模式均可使用*/
#define TCFG_LIVE_AUDIO_LOW_LATENCY_EN      0

#if TCFG_LIVE_AUDIO_LOW_LATENCY_EN
#if (TCFG_ENC_JLA_ENABLE && (JLA_CODING_FRAME_LEN > 50))
#error "Low latency live audio need use JLA_CODING_FRAME_LEN 50! You can comment out this code if you don't want the lowest latency!"
#endif
#endif

//提示音叠加播放配置
#define TCFG_WAV_TONE_MIX_ENABLE			DISABLE	//wav提示音叠加播放

#define TCFG_AUDIO_DEC_OUT_TASK           0   //解码独立任务输出
//*********************************************************************************//
//                           音乐播放配置(Music Playback Config)                   //
//*********************************************************************************//
/*ID3信息获取配置*/
#define TCFG_DEC_ID3_V1_ENABLE				DISABLE
#define TCFG_DEC_ID3_V2_ENABLE				DISABLE

/*加密文件播放配置*/
#define TCFG_DEC_DECRYPT_ENABLE	            DISABLE			//使能配置
#define TCFG_DEC_DECRYPT_KEY	            (0x12345678) 	//密钥配置

/*变速变调配置*/
#define TCFG_SPEED_PITCH_ENABLE             DISABLE

//*********************************************************************************//
//                                  IIS 配置                                       //
//*********************************************************************************//

#define TCFG_AUDIO_INPUT_IIS                DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_OUTPUT_IIS               DISABLE_THIS_MOUDLE

#if TCFG_AUDIO_OUTPUT_IIS
#define TCFG_IIS_MODE                       (0)   //  0:master  1:slave
#define TCFG_IIS_MCLK_IO                    IO_PORTA_05
#define TCFG_SCLK_IO                        IO_PORTA_06
#define TCFG_LRCLK_IO                       IO_PORTA_07
#define TCFG_DATA0_IO                       IO_PORTA_08
#define TCFG_IIS_OUTPUT_SR                  44100
#endif

#if TCFG_AUDIO_INPUT_IIS
#define TCFG_IIS_MODE                       (0)   //  0:master  1:slave
#define TCFG_IIS_INPUT_MCLK_IO              IO_PORTA_12
#define TCFG_INPUT_SCLK_IO                  IO_PORTA_13
#define TCFG_INPUT_LRCLK_IO                 IO_PORTA_14
#define TCFG_INPUT_DATA1_IO                 IO_PORTA_11
#define TCFG_IIS_INPUT_SR                   16000
#endif/*TCFG_AUDIO_INPUT_IIS*/

// OUTPUT WAY
#if TCFG_AUDIO_OUTPUT_IIS
#define AUDIO_OUT_WAY_TYPE 				    AUDIO_WAY_TYPE_IIS
#else
#define AUDIO_OUT_WAY_TYPE 					AUDIO_WAY_TYPE_DAC
#endif

//*********************************************************************************//
//                                  REC 配置                                       //
//*********************************************************************************//
#define RECORDER_MIX_EN						DISABLE//混合录音使能, 需要录制例如蓝牙、FM、 LINEIN才开
#define TCFG_RECORD_FOLDER_DEV_ENABLE       DISABLE//音乐播放录音区分使能
#define RECORDER_MIX_BT_PHONE_EN			DISABLE//电话录音使能


//*********************************************************************************//
//                                  linein配置                                     //
//*********************************************************************************//
#define TCFG_LINEIN_ENABLE					TCFG_APP_LINEIN_EN		// linein使能
#define TCFG_LINEIN_LR_CH					AUDIO_LIN1L_CH//AUDIO_LIN0_LR
#define TCFG_LINEIN_DETECT_ENABLE           DISABLE//ENABLE
#define TCFG_LINEIN_CHECK_PORT				NO_CONFIG_PORT//IO_PORTA_02				// linein检测IO
#define TCFG_LINEIN_PORT_UP_ENABLE        	1						// 检测IO上拉使能
#define TCFG_LINEIN_PORT_DOWN_ENABLE       	0						// 检测IO下拉使能
#define TCFG_LINEIN_AD_CHANNEL             	NO_CONFIG_PORT			// 检测IO是否使用AD检测
#define TCFG_LINEIN_VOLTAGE                	0						// AD检测时的阀值

/*Linein输入方式配置*/
#define LINEIN_INPUT_WAY_ANALOG      0	// 模拟通路
#define LINEIN_INPUT_WAY_ADC         1	// 数字通路
#define LINEIN_INPUT_WAY_DAC         2  // 数字通路（DAC）
#define TCFG_LINEIN_INPUT_WAY               LINEIN_INPUT_WAY_ADC	//AC701N仅支持数字通路（ADC通路)

#define TCFG_LINEIN_MULTIPLEX_WITH_FM		DISABLE 				// linein 脚与 FM 脚复用
#define TCFG_LINEIN_MULTIPLEX_WITH_SD		DISABLE 				// linein 检测与 SD cmd 复用
#define TCFG_LINEIN_SD_PORT		            0// 0:sd0 1:sd1     //选择复用的sd



//*********************************************************************************//
//                                  fm 配置                                     //
//*********************************************************************************//
#define TCFG_FM_ENABLE							TCFG_APP_FM_EN // fm 使能
#define TCFG_FM_INSIDE_ENABLE					DISABLE //AC701N不支持内置FM
#define TCFG_FM_RDA5807_ENABLE					ENABLE //需要使能软件IIC模块
#define TCFG_FM_BK1080_ENABLE					DISABLE //需要使能软件IIC模块
#define TCFG_FM_QN8035_ENABLE					DISABLE //需要使能软件IIC模块

#define TCFG_FMIN_LR_CH				        	AUDIO_LIN0_LR
#define TCFG_FM_INPUT_WAY                       LINEIN_INPUT_WAY_ADC	//AC701N仅支持数字通路（ADC通路)

#if (TCFG_FM_INSIDE_ENABLE && TCFG_FM_ENABLE)
#if (((TCFG_USER_TWS_ENABLE) || (RECORDER_MIX_EN) || (TCFG_MIC_EFFECT_ENABLE)))
#define TCFG_CODE_RUN_RAM_FM_MODE 				DISABLE_THIS_MOUDLE  	//FM模式 代码跑ram
#else
#define TCFG_CODE_RUN_RAM_FM_MODE 				ENABLE_THIS_MOUDLE  	//FM模式 代码跑ram
#endif
#else
#define TCFG_CODE_RUN_RAM_FM_MODE 				DISABLE_THIS_MOUDLE 	//FM模式 代码跑ram
#endif /*(TCFG_FM_INSIDE_ENABLE && TCFG_FM_ENABLE)*/

#if (TCFG_CODE_RUN_RAM_FM_MODE && TCFG_UI_ENABLE)
#undef TCFG_LED7_RUN_RAM
#define TCFG_LED7_RUN_RAM 					ENABLE_THIS_MOUDLE 	//led7跑ram 不屏蔽中断(需要占据2k附近ram)
#endif /*(TCFG_CODE_RUN_RAM_FM_MODE && TCFG_UI_ENABLE)*/


//*********************************************************************************//
//                                  rtc 配置                                     //
//*********************************************************************************//
#define TCFG_RTC_ENABLE						TCFG_APP_RTC_EN

#define TCFG_USE_VIRTUAL_RTC                DISABLE    //假时钟

//*********************************************************************************//
//                                  fat 文件系统配置                                       //
//*********************************************************************************//
#define CONFIG_FATFS_ENABLE					ENABLE
#define TCFG_LFN_EN                         1

//*********************************************************************************//
//                                 电源切换配置                                    //
//*********************************************************************************//

#define CONFIG_PHONE_CALL_USE_LDO15	    0


//*********************************************************************************//
//                         音效配置(AudioEffects Config)
//*********************************************************************************//
/* 音效配置-人声消除(AudioEffects-VocalRemove)*/
#define AUDIO_VOCAL_REMOVE_EN       0

/*音效配置-等响度(AudioEffects-EqualLoudness)*/
//注1：等响度 开启后，需要固定模拟音量,调节软件数字音量
//注2：等响度使用EQ实现，同个数据流中，若打开等响度，请开EQ总使能，关闭其他EQ,例如蓝牙模式EQ
#define AUDIO_EQUALLOUDNESS_CONFIG  0	//等响度, 不支持四声道

/*音效配置-环绕声(AudioEffects-Surround)*/
#define AUDIO_SURROUND_CONFIG     	0	//3D环绕

/*音效配置-虚拟低音(AudioEffects-VBass)*/
#define AUDIO_VBASS_CONFIG        	0	//虚拟低音,虚拟低音不支持四声道

#define AUDIO_SPECTRUM_CONFIG     	0  //频响能量值获取接口
#define AUDIO_MIDI_CTRL_CONFIG    	0  //midi电子琴接口使能

#define BASS_AND_TREBLE_ENABLE            0//旋钮调eq调节接口使能(高音、低音eq/drc使能, 使能后，相关参数有效果文件配置)
#if BASS_AND_TREBLE_ENABLE
#if TCFG_AUDIO_OUT_EQ_ENABLE == 0
#undef TCFG_AUDIO_OUT_EQ_ENABLE
#define TCFG_AUDIO_OUT_EQ_ENABLE 1
#endif
#if TCFG_AUDIO_OUT_DRC_ENABLE == 0
#undef TCFG_AUDIO_OUT_DRC_ENABLE
#define TCFG_AUDIO_OUT_DRC_ENABLE 1
#endif
#if TCFG_MIC_BASS_AND_TREBLE_ENABLE == 0
#undef TCFG_MIC_BASS_AND_TREBLE_ENABLE
#define TCFG_MIC_BASS_AND_TREBLE_ENABLE 1
#endif

#endif


//*********************************************************************************//
//                                 无线2.X声道配置
//                                 2.1  情景一：1个全频音箱(从机做LR) + 一个低音炮(主机做低音)
//                                 2.1  情景二：两个全频音箱(从机1做L,从机2做R) + 一个低音炮(主机做低音)
//*********************************************************************************//
#define WIRELESS_SOUND_TRACK_2_P_X_ENABLE   0//无线2.1声道使能
#define SOUND_TRACK_2_P_X_CH_CONFIG        WIRELESS_SOUND_TRACK_2_P_X_ENABLE //2.X声道总使能

#if SOUND_TRACK_2_P_X_CH_CONFIG

#define BIT(n)                       (1UL << (n))
#define TWO_POINT_ONE_CH             BIT(0)  //2.1声道
#define TWO_POINT_TWO_CH             BIT(1)  //2.2声道
#define TWO_POINT_ZERO_CH            BIT(2)  //2.0声道
#define WIRELESS_SCENE_ONE           BIT(3)  //无线2.1声道，场景1
#define WIRELESS_SCENE_TWO           BIT(4)  //无线2.1声道，场景2
#define TWO_POINT_X_CH               WIRELESS_SCENE_TWO// WIRELESS_SCENE_ONE //选配xx

/*无线2.x声道下相应模块使能*/
#if (TWO_POINT_X_CH == WIRELESS_SCENE_ONE) || (TWO_POINT_X_CH == WIRELESS_SCENE_TWO)
#define TCFG_EQ_DIVIDE_ENABLE               1
#define TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE 1
#define MUSIC_EXT_EQ_AFTER_DRC              1
#define MUSIC_EXT_EQ2_AFTER_DRC             1
#if (TWO_POINT_X_CH == WIRELESS_SCENE_TWO)
#undef  AUDIO_SURROUND_CONFIG
#define AUDIO_SURROUND_CONFIG     	        1
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
#define AUDIO_SURROUND_LEFT_OR_RIGHT     	        0  //立体声时、配置环绕声声道，0:左左   1:右右
#endif/*(TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)*/
#endif/*(TWO_POINT_X_CH == WIRELESS_SCENE_TWO)*/
#endif/*(TWO_POINT_X_CH == WIRELESS_SCENE_ONE) || (TWO_POINT_X_CH == WIRELESS_SCENE_TWO)*/


#endif/*SOUND_TRACK_2_P_X_CH_CONFIG*/

#if WIRELESS_SOUND_TRACK_2_P_X_ENABLE || TCFG_DYNAMIC_EQ_ENABLE
#undef  EQ_SECTION_MAX
#define EQ_SECTION_MAX                      20	  //EQ段数
#if TCFG_DYNAMIC_EQ_ENABLE
#define TCFG_BROADCAST_MODE_LAST_DRC_ENABLE 0     //广播音箱动态eq后添加个drc限幅处理，默认关闭(跑不过来)
#endif/*TCFG_DYNAMIC_EQ_ENABLE*/
#endif/*WIRELESS_SOUND_TRACK_2_P_X_ENABLE || TCFG_DYNAMIC_EQ_ENABLE*/

/**************************other IO by helena *******************************/
#define TCFG_IO_SHIP_EN       IO_PORTB_01
#define TCFG_IO_OHR_DVAIL     IO_PORTC_03
#define TCFG_IO_AMP_POWER     IO_PORTC_01
#define TCFG_IO_HKT_POWER     IO_PORTC_08
#define TCFG_IO_HKT_PTT       IO_PORTA_09
#define TCFG_IO_HKT_SQ        IO_PORTA_08
#define TCFG_IO_HKT_VOX       IO_PORTA_07
#define TCFG_IO_HKT_PD        IO_PORTA_03
#define TCFG_IO_OHR_VLED      IO_PORTA_02
#define TCFG_IO_OHR_3V3       IO_PORTA_01
#define TCFG_IO_6D_INT        IO_PORTG_07










//*********************************************************************************//
//                                 配置结束                                        //
//*********************************************************************************//

#endif //CONFIG_BOARD_DEMO_CFG_H

