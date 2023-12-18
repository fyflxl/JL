#include "app_config.h"



#include "system/includes.h"
#include "media/includes.h"
#include "asm/sdmmc.h"
#include "asm/chargestore.h"
#include "asm/charge.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "audio_config.h"
#include "gSensor/gSensor_manage.h"
#include "key_event_deal.h"
#include "user_cfg.h"
#include "ui/ui_api.h"
#include "dev_manager.h"
#include "asm/spi.h"
#include "ui_manage.h"
#include "spi/nor_fs.h"
#include "fs/virfat_flash.h"
/* #include "norflash_sfc.h" */
#include "norflash.h"
#include "asm/power/power_port.h"
#include "usb/otg.h"
#include "linein/linein_dev.h"
#include "fm/fm_manage.h"
#include "audio_link.h"
#include "io_led.h"
#include "app_main.h"

#define LOG_TAG_CONST       BOARD
#define LOG_TAG             "[BOARD]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

void board_power_init(void);

/*各个状态下默认的闪灯方式和提示音设置，如果USER_CFG中设置了USE_CONFIG_STATUS_SETTING为1，则会从配置文件读取对应的配置来填充改结构体*/
STATUS_CONFIG status_config = {
    //灯状态设置
    .led = {
        .charge_start  = PWM_LED1_ON,
        .charge_full   = PWM_LED0_ON,
        .power_on      = PWM_LED0_ON,
        .power_off     = PWM_LED1_FLASH_THREE,
        .lowpower      = PWM_LED1_SLOW_FLASH,
        .max_vol       = PWM_LED_NULL,
        .phone_in      = PWM_LED_NULL,
        .phone_out     = PWM_LED_NULL,
        .phone_activ   = PWM_LED_NULL,
        .bt_init_ok    = PWM_LED0_LED1_SLOW_FLASH,
        .bt_connect_ok = PWM_LED0_ONE_FLASH_5S,
        .bt_disconnect = PWM_LED0_LED1_FAST_FLASH,
        .tws_connect_ok = PWM_LED0_LED1_FAST_FLASH,
        .tws_disconnect = PWM_LED0_LED1_SLOW_FLASH,
    },
    //提示音设置
    .tone = {
        .charge_start  = IDEX_TONE_NONE,
        .charge_full   = IDEX_TONE_NONE,
        .power_on      = IDEX_TONE_POWER_ON,
        .power_off     = IDEX_TONE_POWER_OFF,
        .lowpower      = IDEX_TONE_LOW_POWER,
        .max_vol       = IDEX_TONE_MAX_VOL,
        .phone_in      = IDEX_TONE_NONE,
        .phone_out     = IDEX_TONE_NONE,
        .phone_activ   = IDEX_TONE_NONE,
        .bt_init_ok    = IDEX_TONE_BT_MODE,
        .bt_connect_ok = IDEX_TONE_BT_CONN,
        .bt_disconnect = IDEX_TONE_BT_DISCONN,
        .tws_connect_ok   = IDEX_TONE_TWS_CONN,
        .tws_disconnect   = IDEX_TONE_TWS_DISCONN,
    }
};

#define __this (&status_config)


// *INDENT-OFF*
/************************** UART config****************************/
#if TCFG_UART0_ENABLE
UART0_PLATFORM_DATA_BEGIN(uart0_data)
    .tx_pin = TCFG_UART0_TX_PORT,                             //串口打印TX引脚选择
    .rx_pin = TCFG_UART0_RX_PORT,                             //串口打印RX引脚选择
    .baudrate = TCFG_UART0_BAUDRATE,                          //串口波特率

    .flags = UART_DEBUG,                                      //串口用来打印需要把改参数设置为UART_DEBUG
UART0_PLATFORM_DATA_END()
#endif //TCFG_UART0_ENABLE


/************************** CHARGE config****************************/
#if TCFG_CHARGE_ENABLE
CHARGE_PLATFORM_DATA_BEGIN(charge_data)
    .charge_en              = TCFG_CHARGE_ENABLE,              //内置充电使能
    .charge_poweron_en      = TCFG_CHARGE_POWERON_ENABLE,      //是否支持充电开机
    .charge_full_V          = TCFG_CHARGE_FULL_V,              //充电截止电压
    .charge_full_mA			= TCFG_CHARGE_FULL_MA,             //充电截止电流
    .charge_mA				= TCFG_CHARGE_MA,                  //恒流充电电流
    .charge_trickle_mA		= TCFG_CHARGE_TRICKLE_MA,          //涓流充电电流
/*ldo5v拔出过滤值，过滤时间 = (filter*2 + 20)ms,ldoin<0.6V且时间大于过滤时间才认为拔出
 对于充满直接从5V掉到0V的充电仓，该值必须设置成0，对于充满由5V先掉到0V之后再升压到xV的
 充电仓，需要根据实际情况设置该值大小*/
	.ldo5v_off_filter		= 100,
    .ldo5v_on_filter        = 50,
    .ldo5v_keep_filter      = 220,
    .ldo5v_pulldown_lvl     = CHARGE_PULLDOWN_200K,            //下拉电阻档位选择
    .ldo5v_pulldown_keep    = 1,
//1、对于自动升压充电舱,若充电舱需要更大的负载才能检测到插入时，请将该变量置1,并且根据需求配置下拉电阻档位
//2、对于按键升压,并且是通过上拉电阻去提供维持电压的舱,请将该变量设置1,并且根据舱的上拉配置下拉需要的电阻挡位
//3、对于常5V的舱,可将改变量设为0,省功耗
//4、为LDOIN防止被误触发唤醒,可设置为200k下拉
	.ldo5v_pulldown_en		= 1,
CHARGE_PLATFORM_DATA_END()
#endif//TCFG_CHARGE_ENABLE

/************************** chargestore config****************************/
#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE
CHARGESTORE_PLATFORM_DATA_BEGIN(chargestore_data)
    .io_port                = TCFG_CHARGESTORE_PORT,
CHARGESTORE_PLATFORM_DATA_END()
#endif

/************************** ADC ****************************/
#if TCFG_AUDIO_ADC_ENABLE

#ifndef TCFG_AUDIO_MIC0_BIAS_EN
#define TCFG_AUDIO_MIC0_BIAS_EN             0
#endif/*TCFG_AUDIO_MIC0_BIAS_EN*/
#ifndef TCFG_AUDIO_MIC1_BIAS_EN
#define TCFG_AUDIO_MIC1_BIAS_EN             0
#endif/*TCFG_AUDIO_MIC1_BIAS_EN*/
#ifndef TCFG_AUDIO_MIC2_BIAS_EN
#define TCFG_AUDIO_MIC2_BIAS_EN             0
#endif/*TCFG_AUDIO_MIC2_BIAS_EN*/
#ifndef TCFG_AUDIO_MIC3_BIAS_EN
#define TCFG_AUDIO_MIC3_BIAS_EN             0
#endif/*TCFG_AUDIO_MIC3_BIAS_EN*/
#ifndef TCFG_AUDIO_MIC_LDO_EN
#define TCFG_AUDIO_MIC_LDO_EN               0
#endif/*TCFG_AUDIO_MIC_LDO_EN*/

struct adc_platform_data adc_data = {
	/* .mic_channel    = TCFG_AUDIO_ADC_MIC_CHA,                   //MIC通道选择，对于693x，MIC只有一个通道，固定选择右声道 */
/*MIC LDO电流档位设置：
    0:0.625ua    1:1.25ua    2:1.875ua    3:2.5ua*/
	.mic_ldo_isel   = TCFG_AUDIO_ADC_LD0_SEL,
/*mic_mode 工作模式定义
    #define AUDIO_MIC_CAP_MODE          0   //单端隔直电容模式
    #define AUDIO_MIC_CAP_DIFF_MODE     1   //差分隔直电容模式
    #define AUDIO_MIC_CAPLESS_MODE      2   //单端省电容模式
*/
    .mic_mode = TCFG_AUDIO_MIC_MODE,
    .mic1_mode = TCFG_AUDIO_MIC1_MODE,
    .mic2_mode = TCFG_AUDIO_MIC2_MODE,
    .mic3_mode = TCFG_AUDIO_MIC3_MODE,

    .mic_bias_inside = TCFG_AUDIO_MIC0_BIAS_EN,
    .mic1_bias_inside = TCFG_AUDIO_MIC1_BIAS_EN,
    .mic2_bias_inside = TCFG_AUDIO_MIC2_BIAS_EN,
    .mic3_bias_inside = TCFG_AUDIO_MIC3_BIAS_EN,


//MIC免电容方案需要设置，影响MIC的偏置电压
/*MIC免电容方案需要设置，影响MIC的偏置电压
    0b0001~0b1001 : 0.5k ~ 4.5k step = 0.5k
    0b1010~0b1111 : 5k ~ 10k step = 1k */
    .mic_bias_res   = 4,
    .mic1_bias_res   = 4,
    .mic2_bias_res   = 4,
    .mic3_bias_res   = 4,
/*MIC LDO电压档位设置,也会影响MIC的偏置电压
    0:1.3v  1:1.4v  2:1.5v  3:2.0v
    4:2.2v  5:2.4v  6:2.6v  7:2.8v */
	.mic_ldo_vsel  = 5,
    .mic_ldo_pwr   = TCFG_AUDIO_MIC_LDO_EN,
 //mic的去直流dcc寄存器配置值,可配0到15,数值越大,其高通转折点越低
    .mic_dcc       = 8,
};
#endif

/* struct audio_pf_data audio_pf_d = { */
/* #if TCFG_AUDIO_DAC_ENABLE */
    /* .adc_pf_data = &adc_data, */
/* #endif */

/* #if TCFG_AUDIO_ADC_ENABLE */
    /* .dac_pf_data = &dac_data, */
/* #endif */
/* }; */

/* struct audio_platform_data audio_data = { */
    /* .private_data = (void *) &audio_pf_d, */
/* }; */


/************************** IO KEY ****************************/
#if TCFG_IOKEY_ENABLE
const struct iokey_port iokey_list[] = {
    {
        .connect_way = TCFG_IOKEY_POWER_CONNECT_WAY,          //IO按键的连接方式
        .key_type.one_io.port = TCFG_IOKEY_POWER_ONE_PORT,    //IO按键对应的引脚
        .key_value = 0,                                       //按键值
    },

    {
        .connect_way = TCFG_IOKEY_PREV_CONNECT_WAY,
        .key_type.one_io.port = TCFG_IOKEY_PREV_ONE_PORT,
        .key_value = 1,
    },

    {
        .connect_way = TCFG_IOKEY_NEXT_CONNECT_WAY,
        .key_type.one_io.port = TCFG_IOKEY_NEXT_ONE_PORT,
        .key_value = 2,
    },
};
const struct iokey_platform_data iokey_data = {
    .enable = TCFG_IOKEY_ENABLE,                              //是否使能IO按键
    .num = ARRAY_SIZE(iokey_list),                            //IO按键的个数
    .port = iokey_list,                                       //IO按键参数表
};

#if MULT_KEY_ENABLE
//组合按键消息映射表
//配置注意事项:单个按键按键值需要按照顺序编号,如power:0, prev:1, next:2
//bit_value = BIT(0) | BIT(1) 指按键值为0和按键值为1的两个按键被同时按下,
//remap_value = 3指当这两个按键被同时按下后重新映射的按键值;
const struct key_remap iokey_remap_table[] = {
	{.bit_value = BIT(0) | BIT(1), .remap_value = 3},
	{.bit_value = BIT(0) | BIT(2), .remap_value = 4},
	{.bit_value = BIT(1) | BIT(2), .remap_value = 5},
};

const struct key_remap_data iokey_remap_data = {
	.remap_num = ARRAY_SIZE(iokey_remap_table),
	.table = iokey_remap_table,
};
#endif

#endif

/************************** PLCNT TOUCH_KEY ****************************/
#if TCFG_TOUCH_KEY_ENABLE
const const struct touch_key_port touch_key_list[] = {
    {
	    .press_delta    = TCFG_TOUCH_KEY0_PRESS_DELTA,
        .port           = TCFG_TOUCH_KEY0_PORT,
        .key_value      = TCFG_TOUCH_KEY0_VALUE,
    },
    {
	    .press_delta    = TCFG_TOUCH_KEY1_PRESS_DELTA,
	    .port           = TCFG_TOUCH_KEY1_PORT,
        .key_value      = TCFG_TOUCH_KEY1_VALUE,
    },
};

const struct touch_key_platform_data touch_key_data = {
    .num = ARRAY_SIZE(touch_key_list),
    .port_list = touch_key_list,
};
#endif  /* #if TCFG_TOUCH_KEY_ENABLE */

/************************** AD KEY ****************************/
#if TCFG_ADKEY_ENABLE
const struct adkey_platform_data adkey_data = {
    .enable = TCFG_ADKEY_ENABLE,                              //AD按键使能
    .adkey_pin = TCFG_ADKEY_PORT,                             //AD按键对应引脚
    .ad_channel = TCFG_ADKEY_AD_CHANNEL,                      //AD通道值
    .extern_up_en = TCFG_ADKEY_EXTERN_UP_ENABLE,              //是否使用外接上拉电阻
    .ad_value = {                                             //根据电阻算出来的电压值
        TCFG_ADKEY_VOLTAGE0,
        TCFG_ADKEY_VOLTAGE1,
        TCFG_ADKEY_VOLTAGE2,
        TCFG_ADKEY_VOLTAGE3,
        TCFG_ADKEY_VOLTAGE4,
        TCFG_ADKEY_VOLTAGE5,
        TCFG_ADKEY_VOLTAGE6,
        TCFG_ADKEY_VOLTAGE7,
        TCFG_ADKEY_VOLTAGE8,
        TCFG_ADKEY_VOLTAGE9,
    },
    .key_value = {                                             //AD按键各个按键的键值
        TCFG_ADKEY_VALUE0,
        TCFG_ADKEY_VALUE1,
        TCFG_ADKEY_VALUE2,
        TCFG_ADKEY_VALUE3,
        TCFG_ADKEY_VALUE4,
        TCFG_ADKEY_VALUE5,
        TCFG_ADKEY_VALUE6,
        TCFG_ADKEY_VALUE7,
        TCFG_ADKEY_VALUE8,
        TCFG_ADKEY_VALUE9,
    },
};
#endif

/************************** IR KEY ****************************/
#if TCFG_IRKEY_ENABLE
const struct irkey_platform_data irkey_data = {
	    .enable = TCFG_IRKEY_ENABLE,                              //IR按键使能
	    .port = TCFG_IRKEY_PORT,                                       //IR按键口
};
#endif

/************************** RDEC_KEY ****************************/
#if TCFG_RDEC_KEY_ENABLE
const struct rdec_device rdeckey_list[] = {
    {
        .index = RDEC0 ,
        .sin_port0 = TCFG_RDEC0_ECODE1_PORT,
        .sin_port1 = TCFG_RDEC0_ECODE2_PORT,
        .key_value0 = TCFG_RDEC0_KEY0_VALUE | BIT(7),
        .key_value1 = TCFG_RDEC0_KEY1_VALUE | BIT(7),
    },

    {
        .index = RDEC1 ,
        .sin_port0 = TCFG_RDEC1_ECODE1_PORT,
        .sin_port1 = TCFG_RDEC1_ECODE2_PORT,
        .key_value0 = TCFG_RDEC1_KEY0_VALUE | BIT(7),
        .key_value1 = TCFG_RDEC1_KEY1_VALUE | BIT(7),
    },

    {
        .index = RDEC2 ,
        .sin_port0 = TCFG_RDEC2_ECODE1_PORT,
        .sin_port1 = TCFG_RDEC2_ECODE2_PORT,
        .key_value0 = TCFG_RDEC2_KEY0_VALUE | BIT(7),
        .key_value1 = TCFG_RDEC2_KEY1_VALUE | BIT(7),
    },


};
const struct rdec_platform_data  rdec_key_data = {
    .enable = 1, //TCFG_RDEC_KEY_ENABLE,                              //是否使能RDEC按键
    .num = ARRAY_SIZE(rdeckey_list),                            //RDEC按键的个数
    .rdec = rdeckey_list,                                       //RDEC按键参数表
};
#endif

/************************** IIS config ****************************/
#if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_IIS)
ALINK_PARM alink0_platform_data = {
    .module = ALINK0,
    .mclk_io = TCFG_IIS_MCLK_IO,
    .sclk_io = TCFG_SCLK_IO,
    .lrclk_io = TCFG_LRCLK_IO,
    .ch_cfg[0].data_io = TCFG_DATA0_IO,
    .mode = ALINK_MD_IIS,
    .role = TCFG_IIS_MODE,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = 4 * 1024,
    .sample_rate = TCFG_IIS_OUTPUT_SR,
    .buf_mode = ALINK_BUF_CIRCLE,
};
#endif

#if TCFG_AUDIO_INPUT_IIS
ALINK_PARM alink0_rx_platform_data = {
    .module = ALINK0,
    .mclk_io = TCFG_IIS_INPUT_MCLK_IO,
    .sclk_io = TCFG_INPUT_SCLK_IO,
    .lrclk_io = TCFG_INPUT_LRCLK_IO,
    .ch_cfg[1].data_io = TCFG_INPUT_DATA1_IO,
    .mode = ALINK_MD_IIS,
    .role = TCFG_IIS_MODE,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = ALNK_BUF_POINTS_NUM * 2 * 2 ,
    .sample_rate = TCFG_IIS_INPUT_SR,
    .buf_mode = ALINK_BUF_CIRCLE,
};
#endif
/************************** PWM_LED ****************************/
#if TCFG_PWMLED_ENABLE
LED_PLATFORM_DATA_BEGIN(pwm_led_data)
	.io_mode = TCFG_PWMLED_IOMODE,              //推灯模式设置:支持单个IO推两个灯和两个IO推两个灯
	.io_cfg.one_io.pin = TCFG_PWMLED_PIN,       //单个IO推两个灯的IO口配置
LED_PLATFORM_DATA_END()
#endif

#if TCFG_UI_ENABLE


#if TCFG_UI_LED1888_ENABLE
LED7_PLATFORM_DATA_BEGIN(led7_data)
	.pin_type = LED7_PIN7,
	.pin_cfg.pin7.pin[0] = IO_PORTA_11,
	.pin_cfg.pin7.pin[1] = IO_PORTA_10,
	.pin_cfg.pin7.pin[2] = IO_PORTA_09,
	.pin_cfg.pin7.pin[3] = IO_PORTA_08,
	.pin_cfg.pin7.pin[4] = IO_PORTA_07,
	.pin_cfg.pin7.pin[5] = IO_PORTA_06,
	.pin_cfg.pin7.pin[6] = (u8)-1,
LED7_PLATFORM_DATA_END()

const struct ui_devices_cfg ui_cfg_data = {
	.type = LED_7,
	.private_data = (void *)&led7_data,
};
#endif /* #if TCFG_UI_LED1888_ENABLE */


#if TCFG_UI_LCD_SEG3X9_ENABLE
LCD_SEG3X9_PLATFORM_DATA_BEGIN(lcd_seg3x9_data)
	.vlcd = LCD_SEG3X9_VOLTAGE_3_3V,
	.bias = LCD_SEG3X9_BIAS_1_3,
	.pin_cfg.pin_com[0] = IO_PORTC_05,
	.pin_cfg.pin_com[1] = IO_PORTC_04,
	.pin_cfg.pin_com[2] = IO_PORTC_03,

	.pin_cfg.pin_seg[0] = IO_PORTA_00,
	.pin_cfg.pin_seg[1] = IO_PORTA_01,
	.pin_cfg.pin_seg[2] = IO_PORTA_02,
	.pin_cfg.pin_seg[3] = IO_PORTA_03,
	.pin_cfg.pin_seg[4] = IO_PORTA_04,
	.pin_cfg.pin_seg[5] = IO_PORTA_07,
	.pin_cfg.pin_seg[6] = IO_PORTA_12,
	.pin_cfg.pin_seg[7] = IO_PORTA_10,
	.pin_cfg.pin_seg[8] = IO_PORTA_09,
LCD_SEG3X9_PLATFORM_DATA_END()

const struct ui_devices_cfg ui_cfg_data = {
	.type = LCD_SEG3X9,
	.private_data = (void *)&lcd_seg3x9_data,
};

#endif /* #if TCFG_UI_LCD_SEG3X9_ENABLE */

#if (TCFG_UI_ENABLE && ( TCFG_LCD_OLED_ENABLE))
LCD_SPI_PLATFORM_DATA_BEGIN(lcd_spi_data)

#if (TCFG_LCD_OLED_ENABLE)
    .pin_reset	= IO_PORTA_05,
    .pin_cs		= IO_PORTA_06,
    .pin_bl		= NO_CONFIG_PORT,
    .pin_dc		= IO_PORTA_03,
    .pin_en		= NO_CONFIG_PORT,
    .pin_te		= NO_CONFIG_PORT,
#endif

#if (TCFG_TFT_LCD_DEV_SPI_HW_NUM == 1)
    .spi_cfg	= SPI1,
    .spi_pdata  = &spi1_p_data,
#elif (TCFG_TFT_LCD_DEV_SPI_HW_NUM == 2)
    .spi_cfg	= SPI2,
    .spi_pdata  = &spi2_p_data,
#endif
LCD_SPI__PLATFORM_DATA_END()

const struct ui_devices_cfg ui_cfg_data = {

#if (TCFG_LCD_OLED_ENABLE)
	.type = DOT_LCD,	// 点阵屏
#endif
	.private_data = (void *)&lcd_spi_data,
};
#endif

#endif /*TCFG_UI_ENABLE*/


const struct soft_iic_config soft_iic_cfg[] = {
    //iic0 data
    {
        .scl = TCFG_SW_I2C0_CLK_PORT,                   //IIC CLK脚
        .sda = TCFG_SW_I2C0_DAT_PORT,                   //IIC DAT脚
        .delay = TCFG_SW_I2C0_DELAY_CNT,                //软件IIC延时参数，影响通讯时钟频率
        .io_pu = 1,                                     //是否打开上拉电阻，如果外部电路没有焊接上拉电阻需要置1
    },
#if 0
    //iic1 data
    {
        .scl = IO_PORTA_05,
        .sda = IO_PORTA_06,
        .delay = 50,
        .io_pu = 1,
    },
#endif
};

u32 soft_iic_real_delay[] = {
    //iic0 data
    TCFG_SW_I2C0_DELAY_CNT,
#if 0
    //iic1 data
    50,
#endif
};


const struct hw_iic_config hw_iic_cfg[] = {
    //iic0 data
    {
        .port = {
            TCFG_HW_I2C0_CLK_PORT,
            TCFG_HW_I2C0_DAT_PORT,
        },
        .baudrate = TCFG_HW_I2C0_CLK,      //IIC通讯波特率
        .hdrive = 0,                       //是否打开IO口强驱
        .io_filter = 1,                    //是否打开滤波器（去纹波）
        .io_pu = 1,                        //是否打开上拉电阻，如果外部电路没有焊接上拉电阻需要置1
    },
};

#if	TCFG_HW_SPI1_ENABLE
const struct spi_platform_data spi1_p_data = {
    .port = {
        TCFG_HW_SPI1_PORT_CLK,
        TCFG_HW_SPI1_PORT_DO,
        TCFG_HW_SPI1_PORT_DI,
    },
	.mode = TCFG_HW_SPI1_MODE,
	.clk  = TCFG_HW_SPI1_BAUD,
	.role = TCFG_HW_SPI1_ROLE,
};
#endif

#if	TCFG_HW_SPI2_ENABLE
const struct spi_platform_data spi2_p_data = {
    .port = {
        TCFG_HW_SPI2_PORT_CLK,
        TCFG_HW_SPI2_PORT_DO,
        TCFG_HW_SPI2_PORT_DI,
    },
	.mode = TCFG_HW_SPI2_MODE,
	.clk  = TCFG_HW_SPI2_BAUD,
	.role = TCFG_HW_SPI2_ROLE,
};
#endif

#if TCFG_NORFLASH_DEV_ENABLE
#if TCFG_NOR_FAT
NORFLASH_DEV_PLATFORM_DATA_BEGIN(norflash_fat_dev_data)
    .spi_hw_num     = TCFG_FLASH_DEV_SPI_HW_NUM,
    .spi_cs_port    = TCFG_FLASH_DEV_SPI_CS_PORT,
#if (TCFG_FLASH_DEV_SPI_HW_NUM == 1)
    .spi_pdata      = &spi1_p_data,
#elif (TCFG_FLASH_DEV_SPI_HW_NUM == 2)
    .spi_pdata      = &spi2_p_data,
#endif
    .start_addr     = 0,
    .size           = 1*1024*1024,
NORFLASH_DEV_PLATFORM_DATA_END()
#endif

#if TCFG_NOR_FS
NORFLASH_DEV_PLATFORM_DATA_BEGIN(norflash_norfs_dev_data)
    .spi_hw_num     = TCFG_FLASH_DEV_SPI_HW_NUM,
    .spi_cs_port    = TCFG_FLASH_DEV_SPI_CS_PORT,
#if (TCFG_FLASH_DEV_SPI_HW_NUM == 1)
    .spi_pdata      = &spi1_p_data,
#elif (TCFG_FLASH_DEV_SPI_HW_NUM == 2)
    .spi_pdata      = &spi2_p_data,
#endif
    /* .start_addr     = 1*1024*1024, */
    .start_addr     = 0,
    .size           = 4*1024*1024,
NORFLASH_DEV_PLATFORM_DATA_END()
#endif

#if TCFG_NOR_REC
NORFLASH_DEV_PLATFORM_DATA_BEGIN(norflash_norfs_rec_dev_data)
    .spi_hw_num     = TCFG_FLASH_DEV_SPI_HW_NUM,
    .spi_cs_port    = TCFG_FLASH_DEV_SPI_CS_PORT,
#if (TCFG_FLASH_DEV_SPI_HW_NUM == 1)
    .spi_pdata      = &spi1_p_data,
#elif (TCFG_FLASH_DEV_SPI_HW_NUM == 2)
    .spi_pdata      = &spi2_p_data,
#endif
    .start_addr     = 2*1024*1024,
    .size           = 2*1024*1024,
NORFLASH_DEV_PLATFORM_DATA_END()
#endif

#endif /*TCFG_NORFLASH_DEV_ENABLE*/



#if TCFG_NORFLASH_SFC_DEV_ENABLE
SFC_SPI_PLATFORM_DATA_BEGIN(sfc_spi_data)
	.spi_hw_index    = 1,
    .sfc_data_width  = SFC_DATA_WIDTH_4,
    .sfc_read_mode   = SFC_RD_OUTPUT,
    .sfc_encry       = 0, //是否加密
    .sfc_clk_div     = 0, //时钟分频: sfc_fre = sys_clk / div;
    .unencry_start_addr = CONFIG_EXTERN_FLASH_SIZE - CONFIG_EXTERN_USER_VM_FLASH_SIZE,//不加密区域
    .unencry_size  = CONFIG_EXTERN_USER_VM_FLASH_SIZE,
SFC_SPI_PLATFORM_DATA_END()

NORFLASH_SFC_DEV_PLATFORM_DATA_BEGIN(norflash_sfc_dev_data)
    .sfc_spi_pdata     = &sfc_spi_data,
    .start_addr     = 0,
    .size           = CONFIG_EXTERN_FLASH_SIZE - CONFIG_EXTERN_USER_VM_FLASH_SIZE,
NORFLASH_SFC_DEV_PLATFORM_DATA_END()
#endif /* #if TCFG_NORFLASH_SFC_DEV_ENABLE */


#if TCFG_NOR_VM
NORFLASH_SFC_DEV_PLATFORM_DATA_BEGIN(norflash_norfs_vm_dev_data)
    .sfc_spi_pdata     = &sfc_spi_data,
    .start_addr     = CONFIG_EXTERN_FLASH_SIZE - CONFIG_EXTERN_USER_VM_FLASH_SIZE,
    .size           = CONFIG_EXTERN_USER_VM_FLASH_SIZE,
NORFLASH_SFC_DEV_PLATFORM_DATA_END()
#endif



#if TCFG_SD0_ENABLE
SD0_PLATFORM_DATA_BEGIN(sd0_data)
	.port = {
        TCFG_SD0_PORT_CMD,
        TCFG_SD0_PORT_CLK,
        TCFG_SD0_PORT_DA0,
        TCFG_SD0_PORT_DA1,
        TCFG_SD0_PORT_DA2,
        TCFG_SD0_PORT_DA3,
    },
	.data_width             = TCFG_SD0_DAT_MODE,
	.speed                  = TCFG_SD0_CLK,
	.detect_mode            = TCFG_SD0_DET_MODE,
	.priority				= 3,

#if (TCFG_SD0_DET_MODE == SD_IO_DECT)
    .detect_io              = TCFG_SD0_DET_IO,
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_io_detect,
	.power                  = sd_set_power,
	/* .power                  = NULL, */
#elif (TCFG_SD0_DET_MODE == SD_CLK_DECT)
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_clk_detect,
	.power                  = sd_set_power,
	/* .power                  = NULL, */
#else
    .detect_func            = sdmmc_cmd_detect,
    .power                  = NULL,
#endif

SD0_PLATFORM_DATA_END()
#endif /* #if TCFG_SD0_ENABLE */

#if TCFG_SD1_ENABLE
SD1_PLATFORM_DATA_BEGIN(sd1_data)
	.port = {
        TCFG_SD1_PORT_CMD,
        TCFG_SD1_PORT_CLK,
        TCFG_SD1_PORT_DA0,
        TCFG_SD1_PORT_DA1,
        TCFG_SD1_PORT_DA2,
        TCFG_SD1_PORT_DA3,
    },
	.data_width             = TCFG_SD1_DAT_MODE,
	.speed                  = TCFG_SD1_CLK,
	.detect_mode            = TCFG_SD1_DET_MODE,
	.priority				= 3,

#if (TCFG_SD1_DET_MODE == SD_IO_DECT)
    .detect_io              = TCFG_SD1_DET_IO,
    .detect_io_level        = TCFG_SD1_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_io_detect,
	.power                  = NULL,
#elif (TCFG_SD1_DET_MODE == SD_CLK_DECT)
    .detect_io_level        = TCFG_SD1_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_clk_detect,
	.power                  = NULL,
#else
    .detect_func            = sdmmc_cmd_detect,
    .power                  = NULL,
#endif

SD1_PLATFORM_DATA_END()
#endif


/************************** otg data****************************/
#if TCFG_OTG_MODE
struct otg_dev_data otg_data = {
    .usb_dev_en = TCFG_OTG_USB_DEV_EN,
	.slave_online_cnt = TCFG_OTG_SLAVE_ONLINE_CNT,
	.slave_offline_cnt = TCFG_OTG_SLAVE_OFFLINE_CNT,
	.host_online_cnt = TCFG_OTG_HOST_ONLINE_CNT,
	.host_offline_cnt = TCFG_OTG_HOST_OFFLINE_CNT,
#if TCFG_USB_CDC_BACKGROUND_RUN
	.detect_mode = TCFG_OTG_MODE | OTG_SLAVE_MODE,
#else
	.detect_mode = TCFG_OTG_MODE,
#endif
	.detect_time_interval = TCFG_OTG_DET_INTERVAL,
};
#endif




/************************** gsensor ****************************/
#if TCFG_GSENSOR_ENABLE
#if TCFG_DA230_EN
GSENSOR_PLATFORM_DATA_BEGIN(gSensor_data)
    .iic = 0,
    .gSensor_name = "da230",
    .gSensor_int_io = IO_PORTB_08,
GSENSOR_PLATFORM_DATA_END();
#endif      //end if DA230_EN
#endif      //end if CONFIG_GSENSOR_ENABLE

/************************** FM ****************************/
#if TCFG_FM_ENABLE
FM_DEV_PLATFORM_DATA_BEGIN(fm_dev_data)
	.iic_hdl = 0,
	.iic_delay = 50,
FM_DEV_PLATFORM_DATA_END();
#endif


/************************** linein KEY ****************************/
#if TCFG_LINEIN_ENABLE
struct linein_dev_data linein_data = {
	.enable = TCFG_LINEIN_ENABLE,
	.port = TCFG_LINEIN_CHECK_PORT,
	.up = TCFG_LINEIN_PORT_UP_ENABLE,
	.down = TCFG_LINEIN_PORT_DOWN_ENABLE,
	.ad_channel = TCFG_LINEIN_AD_CHANNEL,
	.ad_vol = TCFG_LINEIN_VOLTAGE,
};
#endif

/************************** rtc ****************************/
#if TCFG_RTC_ENABLE
const struct sys_time def_sys_time = {  //初始一下当前时间
    .year = 2020,
    .month = 1,
    .day = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};
const struct sys_time def_alarm = {     //初始一下目标时间，即闹钟时间
    .year = 2050,
    .month = 1,
    .day = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};

/* extern void alarm_isr_user_cbfun(u8 index); */
RTC_DEV_PLATFORM_DATA_BEGIN(rtc_data)
    .default_sys_time = &def_sys_time,
    .default_alarm = &def_alarm,
	.clk_sel = RTC_CLK_RES_SEL,
	.cbfun = NULL,                      //闹钟中断的回调函数,用户自行定义
    /* .cbfun = alarm_isr_user_cbfun, */
RTC_DEV_PLATFORM_DATA_END()

#endif


extern const struct device_operations mass_storage_ops;

REGISTER_DEVICES(device_table) = {
    /* { "audio", &audio_dev_ops, (void *) &audio_data }, */

#if TCFG_VIRFAT_FLASH_ENABLE
	{ "virfat_flash", 	&virfat_flash_dev_ops, 	(void *)"res_nor"},
#endif

#if TCFG_CHARGE_ENABLE
    { "charge", &charge_dev_ops, (void *)&charge_data },
#endif
#if TCFG_SD0_ENABLE
	{ "sd0", 	&sd_dev_ops, 	(void *) &sd0_data},
#endif
#if TCFG_SD1_ENABLE
	{ "sd1", 	&sd_dev_ops, 	(void *) &sd1_data},
#endif

#if TCFG_LINEIN_ENABLE
    { "linein",  &linein_dev_ops, (void *)&linein_data},
#endif
#if TCFG_OTG_MODE
    { "otg",     &usb_dev_ops, (void *) &otg_data},
#endif
#if TCFG_UDISK_ENABLE
    { "udisk0",   &mass_storage_ops , NULL},
#endif
#if TCFG_RTC_ENABLE
#if TCFG_USE_VIRTUAL_RTC
    { "rtc",   &rtc_simulate_ops , (void *)&rtc_data},
#else
    { "rtc",   &rtc_dev_ops , (void *)&rtc_data},
#endif
#endif

/* #if TCFG_NORFLASH_DEV_ENABLE */
    /* { "norflash",   &norflash_dev_ops , (void *)&norflash_dev_data}, */
/* #endif */
/* #if TCFG_NOR_FS_ENABLE */
    /* { "nor_ui",   &norfs_dev_ops , (void *)&norfs_dev_data}, */
/* #endif */

#if TCFG_NORFLASH_DEV_ENABLE
#if TCFG_NOR_FAT
    { "fat_nor",   &norflash_dev_ops , (void *)&norflash_fat_dev_data},
#endif

#if TCFG_NOR_FS
    { "res_nor",   &norfs_dev_ops , (void *)&norflash_norfs_dev_data},
#endif

#if TCFG_NOR_REC
    {"rec_nor",   &norfs_dev_ops , (void *)&norflash_norfs_rec_dev_data},
#endif
#endif /*TCFG_NORFLASH_DEV_ENABLE*/

#if TCFG_NORFLASH_SFC_DEV_ENABLE
#if TCFG_NOR_FS
    { "res_nor",   &norflash_sfc_fs_dev_ops, (void *)&norflash_sfc_dev_data},
#endif
#endif /*TCFG_NORFLASH_SFC_DEV_ENABLE*/

#if TCFG_NANDFLASH_DEV_ENABLE
	{"nand_flash",   &nandflash_dev_ops , (void *)&nandflash_dev_data},
#endif

#if TCFG_NOR_VM
    {"ui_vm",   &norflash_sfc_fs_dev_ops , (void *)&norflash_norfs_vm_dev_data},
#endif

};

/************************** LOW POWER config ****************************/
const struct low_power_param power_param = {
    .config         = TCFG_LOWPOWER_LOWPOWER_SEL,          //0：sniff时芯片不进入低功耗  1：sniff时芯片进入powerdown
    .btosc_hz       = TCFG_CLOCK_OSC_HZ,                   //外接晶振频率
    .delay_us       = TCFG_CLOCK_SYS_HZ / 1000000L,        //提供给低功耗模块的延时(不需要需修改)
    .btosc_disable  = TCFG_LOWPOWER_BTOSC_DISABLE,         //进入低功耗时BTOSC是否保持
    .vddiom_lev     = TCFG_LOWPOWER_VDDIOM_LEVEL,          //强VDDIO等级,可选：2.0V  2.2V  2.4V  2.6V  2.8V  3.0V  3.2V  3.6V
    .osc_type       = TCFG_LOWPOWER_OSC_TYPE,

#if TCFG_RTC_ENABLE
    .rtc_clk        = RTC_CLK_RES_SEL,
#endif

#if (TCFG_LOWPOWER_RAM_SIZE == 4)
    .mem_lowpower_con = MEM_PWR_RAM0_RAM3_CON | MEM_PWR_RAM4_RAM5_CON | MEM_PWR_RAM6_RAM7_CON,
#elif (TCFG_LOWPOWER_RAM_SIZE == 3)
    .mem_lowpower_con = MEM_PWR_RAM0_RAM3_CON | MEM_PWR_RAM4_RAM5_CON,
#elif (TCFG_LOWPOWER_RAM_SIZE == 2)
    .mem_lowpower_con = MEM_PWR_RAM0_RAM3_CON,
#elif (TCFG_LOWPOWER_RAM_SIZE == 1)
#error "BR28 RAM0~RAM3 IS 256K"
#endif
	.flash_pg = TCFG_EX_FLASH_POWER_IO,
};


/************************** PWR config ****************************/
struct port_wakeup port0 = {
    .pullup_down_enable = ENABLE,                            //配置I/O 内部上下拉是否使能
    .edge               = FALLING_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿
    .iomap              = TCFG_IOKEY_POWER_ONE_PORT,                       //唤醒口选择
};

#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
struct port_wakeup port1 = {
    .pullup_down_enable = DISABLE,                            //配置I/O 内部上下拉是否使能
    .edge               = FALLING_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿
    .iomap              = TCFG_CHARGESTORE_PORT,             //唤醒口选择
};
#endif
#if TCFG_CHARGE_ENABLE
struct port_wakeup charge_port = {
    .edge               = RISING_EDGE,                       //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_CHGFL_DET,                      //唤醒口选择
};

struct port_wakeup vbat_port = {
    .edge               = BOTH_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_VBTCH_DET,                      //唤醒口选择
};

struct port_wakeup ldoin_port = {
    .edge               = BOTH_EDGE,                      //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter             = PORT_FLT_16ms,
    .iomap              = IO_LDOIN_DET,                      //唤醒口选择
};
#endif

const struct wakeup_param wk_param = {
/* #if (!TCFG_LP_TOUCH_KEY_ENABLE) */
	.port[1] = &port0,
/* #endif */
#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
	.port[2] = &port1,
#endif

#if TCFG_CHARGE_ENABLE
    .aport[0] = &charge_port,
    .aport[1] = &vbat_port,
    .aport[2] = &ldoin_port,
#endif
};

void gSensor_wkupup_disable(void)
{
    log_info("gSensor wkup disable\n");
    power_wakeup_index_enable(1, 0);
}

void gSensor_wkupup_enable(void)
{
    log_info("gSensor wkup enable\n");
    power_wakeup_index_enable(1, 1);
}

static void key_wakeup_disable()
{
    power_wakeup_index_enable(1, 0);
}
static void key_wakeup_enable()
{
    power_wakeup_index_enable(1, 1);
}

void debug_uart_init(const struct uart_platform_data *data)
{
#if TCFG_UART0_ENABLE
    if (data) {
        uart_init(data);
    } else {
        uart_init(&uart0_data);
    }
#endif
}


STATUS *get_led_config(void)
{
    return &(__this->led);
}

STATUS *get_tone_config(void)
{
    return &(__this->tone);
}

u8 get_sys_default_vol(void)
{
    return 21;
}


u8 get_power_on_status(void)
{
#if TCFG_IOKEY_ENABLE
    struct iokey_port *power_io_list = NULL;
    power_io_list = iokey_data.port;

    if (iokey_data.enable) {
        if (gpio_read(power_io_list->key_type.one_io.port) == power_io_list->connect_way){
            return 1;
        }
    }
#endif

#if TCFG_ADKEY_ENABLE
    if (adkey_data.enable) {
        if (adc_get_value(adkey_data.ad_channel) < 10) {
            return 1;
        }
    }
#endif

    return 0;
}

static void board_devices_init(void)
{
#if TCFG_PWMLED_ENABLE
    /* pwm_led_init(&pwm_led_data); */
    ui_pwm_led_init(&pwm_led_data);
#elif TCFG_IOLED_ENABLE
	io_led_init();
#endif

#if (TCFG_IOKEY_ENABLE || TCFG_ADKEY_ENABLE || TCFG_IRKEY_ENABLE || TCFG_RDEC_KEY_ENABLE ||  TCFG_CTMU_TOUCH_KEY_ENABLE)
	key_driver_init();
#endif

#if TCFG_UART_KEY_ENABLE
	extern int uart_key_init(void);
	uart_key_init();
#endif /* #if TCFG_UART_KEY_ENABLE */

#if TCFG_GSENSOR_ENABLE
    gravity_sensor_init(&gSensor_data);
#endif      //end if CONFIG_GSENSOR_ENABLE

#if TCFG_UI_ENABLE
	UI_INIT(&ui_cfg_data);
#endif /* #if TCFG_UI_ENABLE */

#if  TCFG_APP_FM_EMITTER_EN
 	fm_emitter_manage_init(930);
#endif

}

extern void cfg_file_parse(u8 idx);
void board_init()
{
    board_power_init();

    //adc_vbg_init();
    adc_init();
    cfg_file_parse(0);
    /* devices_init(); */

#if TCFG_CHARGE_ENABLE
    extern int charge_init(const struct dev_node *node, void *arg);
    charge_init(NULL, (void *)&charge_data);
#else
	CHARGE_EN(0);
    CHGGO_EN(0);
    gpio_longpress_pin1_reset_config(IO_LDOIN_DET, 0, 0);
#endif

#if (TCFG_CHARGE_ENABLE && !TCFG_CHARGE_POWERON_ENABLE && TCFG_USB_PORT_CHARGE)
    gpio_longpress_pin1_reset_config(IO_LDOIN_DET, 0, 0);
#endif
    /* if (!get_charge_online_flag()) { */
        /* check_power_on_voltage(); */
    /* } */
	power_on_port_init();//added by helena 

#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
	sdpg_config(4);
#endif

#if TCFG_FM_ENABLE
	fm_dev_init(&fm_dev_data);
#endif

#if TCFG_NOR_REC
    nor_fs_ops_init();
#endif

#if TCFG_NOR_FS
    init_norsdfile_hdl();
#endif

#if FLASH_INSIDE_REC_ENABLE
    sdfile_rec_ops_init();
#endif

	dev_manager_init();
    dev_manager_set_valid_by_logo("res_nor", 0);///将设备设置为无效设备

	board_devices_init();

    extern void temp_pll_trim_init(void);
    temp_pll_trim_init();


#if TCFG_CHARGE_ENABLE
    if(get_charge_online_flag())
#else
    if (0)
#endif
	{
    	power_set_mode(PWR_LDO15);
	}else{
    	power_set_mode(TCFG_LOWPOWER_POWER_SEL);
	}

 #if TCFG_UART0_ENABLE
    if (uart0_data.rx_pin < IO_MAX_NUM) {
        gpio_set_die(uart0_data.rx_pin, 1);
    }
#endif

#if TCFG_AUDIO_VAD_ENABLE
	u32 audio_vad_init(void);
	audio_vad_init();
#endif /* #if TCFG_AUDIO_VAD_ENABLE */




#if TCFG_RTC_ENABLE
    alarm_init();
#endif

}

/*进软关机之前默认将IO口都设置成高阻状态，需要保留原来状态的请修改该函数*/
extern void dac_power_off(void);
void board_set_soft_poweroff(void)
{
#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
	sdpg_config(0);
#endif

	// 外置flash
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
	soff_gpio_protect(TCFG_EX_FLASH_POWER_IO);
#endif

	//power按键
#if TCFG_ADKEY_ENABLE
    soff_gpio_protect(TCFG_ADKEY_PORT);
#endif

#if TCFG_IOKEY_ENABLE
    soff_gpio_protect(TCFG_IOKEY_POWER_ONE_PORT);
#endif

#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
    power_wakeup_index_enable(2, 0);
#endif

	board_set_soft_poweroff_common(NULL);

	dac_power_off();
	power_wakeup_index_enable(1,1);
}

#if TCFG_SD_ALWAY_ONLINE_ENABLE

extern int sdx_dev_entry_lowpower(const char *sdx_name);

static int sdx_entry_lowpower(int param)
{
	/* putchar('s'); */
#if TCFG_SD0_ENABLE
	sdx_dev_entry_lowpower("sd0");
#endif /* #if TCFG_SD1_ENABLE */
#if TCFG_SD1_ENABLE
	sdx_dev_entry_lowpower("sd1");
#endif /* #if TCFG_SD1_ENABLE */
	return 0;
}
static void sdx_sleep_callback(void)
{
    int argv[3];

    argv[0] = (int)sdx_entry_lowpower;
    argv[1] = 1;
    /* argv[2] = ; */

    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
	if (ret) {
		printf("post ret:%d \n", ret);
	}
}
#endif /*#if TCFG_SD_ALWAY_ONLINE_ENABLE*/

void sleep_exit_callback(u32 usec)
{
	sleep_exit_callback_common(NULL);

	putchar('>');

}
void sleep_enter_callback(u8  step)
{
    /* 此函数禁止添加打印 */
    if (step == 1) {
        putchar('<');
    } else {
		sleep_enter_callback_common(NULL);
    }
}

static void wl_audio_clk_on(void)
{
    JL_WL_AUD->CON0 = 1;
    JL_ASS->CLK_CON |= BIT(0);//audio时钟
}

static void port_wakeup_callback(u8 index, u8 gpio)
{
    log_info("%s:%d,%d",__FUNCTION__,index,gpio);

    switch (index) {
#if (TCFG_TEST_BOX_ENABLE || TCFG_CHARGESTORE_ENABLE || TCFG_ANC_BOX_ENABLE)
        case 2:
            extern void chargestore_ldo5v_fall_deal(void);
            chargestore_ldo5v_fall_deal();
            break;
#endif
    }
}

static void aport_wakeup_callback(u8 index, u8 gpio, u8 edge)
{
    log_info("%s:%d,%d",__FUNCTION__,index,gpio);
#if TCFG_CHARGE_ENABLE
    switch (gpio) {
        case IO_CHGFL_DET://charge port
            charge_wakeup_isr();
            break;
        case IO_VBTCH_DET://vbat port
        case IO_LDOIN_DET://ldoin port
            ldoin_wakeup_isr();

            break;
    }
#endif
}

void board_power_init(void)
{
    log_info("Power init : %s", __FILE__);

#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
	// ext flash power
    log_info("ex flash Power on \n");
	gpio_direction_output(TCFG_EX_FLASH_POWER_IO, 1);
	gpio_set_hd(TCFG_EX_FLASH_POWER_IO, 1);
	gpio_set_hd0(TCFG_EX_FLASH_POWER_IO, 1);
	/* os_time_dly(5); */
#endif /* #if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT) */

	JL_WL_AUD->CON0 = 1;
    power_init(&power_param);

    power_set_callback(TCFG_LOWPOWER_LOWPOWER_SEL, sleep_enter_callback, sleep_exit_callback, board_set_soft_poweroff);

    wl_audio_clk_on();

	power_wakeup_init(&wk_param);
	power_awakeup_set_callback(aport_wakeup_callback);
	power_wakeup_set_callback(port_wakeup_callback);

#ifdef AUDIO_PCM_DEBUG

		uartSendInit();

#endif

#if USER_UART_UPDATE_ENABLE
        {
#include "uart_update.h"
            uart_update_cfg  update_cfg = {
                .rx = IO_PORTA_02,
                .tx = IO_PORTA_03,
                .output_channel = CH1_UT1_TX,
                .input_channel = INPUT_CH0
            };
            uart_update_init(&update_cfg);
        }
#endif
}

extern __attribute__((weak)) unsigned RAM1_SIZE ;

__attribute__((noinline)) u32 check_ram1_size(void)
{
    return &RAM1_SIZE;
}


