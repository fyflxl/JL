/* COPYRIGHT NOTICE
 * 文件名称 ：ui_pushScreen_manager.c
 * 简    介 ：UI框架推屏管理层
 * 功    能 ：
 * 			输入控制：LCD屏幕驱动，板级SPI配置
 * 			出输控制：硬件SPI推TFT彩屏，硬件SPI推OLED点阵屏，IMD推TFT彩屏
 *
 * 			输入作用：
 * 				LCD屏幕驱动：LCD初始化代码，LCD特别控制参数和方法
 * 				板级SPI配置：SPI模块选择，控制IO配置，SPI模块配置参数，LCD类型
 * 			输出根据：
 * 				lcd->type 判断为TFT彩屏或OLED屏，选择推点阵屏或彩屏
 * 				cpu	判断推TFT彩屏时使用硬件SPI还是IMD（具有IMD模块的芯片默认使用IMD，否则默认使用硬件SPI）
 *
 * 			说明：
 * 				点阵屏只能用SPI驱动推
 * 				TFT彩屏使用硬件SPI或IMD模块，可通过CPU宏来控制
 *
 * 作    者 ：zhuhaifang
 * 创建时间 ：2022/05/10 10:26
 */

#include "app_config.h"

#if (TCFG_UI_ENABLE)

#include "includes.h"
#include "ui/ui_api.h"
#include "system/includes.h"
#include "system/timer.h"
#include "asm/spi.h"
#include "clock_cfg.h"
#include "asm/mcpwm.h"
#include "ui/res_config.h"
#include "asm/imd.h"
#include "ui/buffer_manager.h"
#include "ui/ui_sys_param.h"


#define LOG_TAG_CONST       UI
#define LOG_TAG     		"[UI-PUSH]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CHAR_ENABLE
#include "debug.h"


#if 0
/**********************************************************
 * 通过CPU判断有没有硬件推屏接口
 * 如果CPU有IMD模块，则默认使用IMD模块推屏
 * 如果CPU没有IMD模块，则默认使用硬件SPI推屏
 ***********************************************************/
#if ((defined CONFIG_CPU_BR27) || (defined CONFIG_CPU_BR28))
#define ui_draw				imd_draw
#define ui_write_cmd		imd_write_cmd
#define ui_set_area			imd_set_draw_area
#define ui_clear_screen		imd_clear_screen

#else
#define ui_draw				spi_lcd_draw
#define ui_write_cmd		spi_lcd_write_cmd
#define ui_set_area			spi_lcd_set_draw_area
#define ui_clear_screen		spi_lcd_clear_screen

#endif
#endif



/* 推屏功能测试，将根据指定规则进行刷屏测试 */
static void push_screen_test(void);
#define LCD_DRV_TEST()		//push_screen_test()


static u8  backlight_status = 0;
static u8  lcd_sleep_in     = 0;
static volatile u8 is_lcd_busy = 0;
static struct lcd_platform_data *lcd_dat = NULL;
struct pwm_platform_data lcd_pwm_p_data;


// 推屏管理模块私有参数，读写命令、数据需要根据不同屏幕配置，因此需根据屏幕类型设置
static struct ui_push_screen_var {
    int lcd_type;
    struct _lcd_drive *lcd;
    struct imd_param  *param;

    void (*write_cmd)();
    void (*read_cmd)();
};
static struct ui_push_screen_var push_screen = {0};
#define	__this	(&push_screen)


// EN 控制
void lcd_en_ctrl(u8 val)
{
    if (lcd_dat->pin_en == NO_CONFIG_PORT) {
        return;
    }
    gpio_set_die(lcd_dat->pin_en, 1);
    gpio_direction_output(lcd_dat->pin_en, val);
}

// BL 控制
void lcd_bl_ctrl(u8 val)
{
    if (lcd_dat->pin_bl == NO_CONFIG_PORT) {
        return;
    }
    gpio_set_die(lcd_dat->pin_bl, 1);
    gpio_direction_output(lcd_dat->pin_bl, !!val);
}

// CS 控制
static void spi_cs_ctrl(u8 val)
{
    if (lcd_dat->pin_cs == NO_CONFIG_PORT) {
        return;
    }
    gpio_set_die(lcd_dat->pin_cs, 1);
    gpio_direction_output(lcd_dat->pin_cs, val);
}

// DC 控制
static void spi_dc_ctrl(u8 val)
{
    if (lcd_dat->pin_dc == NO_CONFIG_PORT) {
        return;
    }
    gpio_set_die(lcd_dat->pin_dc, 1);
    gpio_direction_output(lcd_dat->pin_dc, val);
}

// TE 控制
static int spi_te_stat()
{
    if (lcd_dat->pin_te == NO_CONFIG_PORT) {
        return -1;
    }
    gpio_set_pull_up(lcd_dat->pin_te, 1);
    gpio_set_pull_down(lcd_dat->pin_te, 0);
    gpio_set_die(lcd_dat->pin_te, 1);
    gpio_direction_input(lcd_dat->pin_te);

    return gpio_read(lcd_dat->pin_te);
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD DEVICE RESET
 *
 * Description: LCD 设备复位
 *
 * Arguments  : none
 *
 * Returns    : none
 *
 * Notes      : 1、判断是否在屏驱有重新定义LCD复位函数，
 * 					是使用屏驱上定义的LCD复位函数，
 * 					否使用GPIO控制板级配置的LCD复位IO
 *********************************************************************************************************
 */

static void lcd_reset()
{
    if (__this->lcd->reset) {
        __this->lcd->reset();
    } else {
        gpio_set_die(lcd_dat->pin_reset, 1);
        gpio_direction_output(lcd_dat->pin_reset, 1);
        os_time_dly(10);
        gpio_direction_output(lcd_dat->pin_reset, 0);
        os_time_dly(10);
        gpio_direction_output(lcd_dat->pin_reset, 1);
        os_time_dly(10);
    }
}



/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD BACKLIGHT CONTROL
 *
 * Description: LCD 背光控制
 *
 * Arguments  : on LCD 背光开关标志，0为关，其它值为开
 *
 * Returns    : none
 *
 * Notes      : 1、判断是否在屏驱有重新定义背光控制函数，
 * 					是使用屏驱上定义的背光控制函数，
 * 					否使用GPIO控制板级配置的背光IO
 *
 *              2、配置背光状态标志，打开为true，关闭为false
 *********************************************************************************************************
 */

void lcd_mcpwm_init()
{
#if (TCFG_BACKLIGHT_PWM_MODE == 2)
    extern void mcpwm_init(struct pwm_platform_data * arg);
    lcd_pwm_p_data.pwm_aligned_mode = pwm_edge_aligned;         //边沿对齐
    lcd_pwm_p_data.pwm_ch_num = pwm_ch0;                        //通道
    lcd_pwm_p_data.frequency = 10000;                           //Hz
    lcd_pwm_p_data.duty = 10000;                                //占空比
    lcd_pwm_p_data.h_pin = lcd_dat->pin_bl;                     //任意引脚
    lcd_pwm_p_data.l_pin = -1;                                  //任意引脚,不需要就填-1
    lcd_pwm_p_data.complementary_en = 1;                        //两个引脚的波形, 0: 同步,  1: 互补，互补波形的占空比体现在H引脚上
    mcpwm_init(&lcd_pwm_p_data);
#endif
}

int lcd_drv_backlight_ctrl(u8 on)
{
    if (__this->lcd->backlight_ctrl) {
        __this->lcd->backlight_ctrl(on);
    } else if (lcd_dat->pin_bl != -1) {
#if (TCFG_BACKLIGHT_PWM_MODE == 0)
        gpio_direction_output(lcd_dat->pin_bl, on);
#elif (TCFG_BACKLIGHT_PWM_MODE == 1)
        //注意：duty不能大于prd，并且prd和duty是非标准非线性的，建议用示波器看着来调
        extern int pwm_led_output_clk(u8 gpio, u8 prd, u8 duty);
        if (on) {
            extern int get_light_level();
            u32 light_level = (get_light_level() + 1) * 2;
            if (light_level > 10) {
                light_level = 10;
            }
            pwm_led_output_clk(lcd_dat->pin_bl, 10, light_level);
        } else {
            pwm_led_output_clk(lcd_dat->pin_bl, 10, 0);
        }
#elif (TCFG_BACKLIGHT_PWM_MODE == 2)
        if (on) {
            extern int get_light_level();
            u32 pwm_duty = (get_light_level() + 1) * 2 * 1000;
            if (pwm_duty > 10000) {
                pwm_duty = 10000;
            }
            mcpwm_set_duty(lcd_pwm_p_data.pwm_ch_num, pwm_duty);
        } else {
            mcpwm_set_duty(lcd_pwm_p_data.pwm_ch_num, 0);
        }
#endif
    } else {
        backlight_status = false;
        return -1;
    }

    if (on) {
        backlight_status = true;
    } else {
        backlight_status = false;
    }

    return 0;
}


struct lcd_platform_data *lcd_get_platform_data()
{
    return lcd_dat;
}



static int find_begin(u8 *begin, u8 *end, int pos)
{
    int i;
    u8 *p = &begin[pos];
    while ((p + 3) < end) {
        if ((p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]) == BEGIN_FLAG) {
            return (&p[4] - begin);
        }
        p++;
    }

    return -1;
}


static int find_end(u8 *begin, u8 *end, int pos)
{
    u8 *p = &begin[pos];
    while ((p + 3) < end) {
        if ((p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]) == END_FLAG) {
            return (&p[0] - begin);
        }
        p++;
    }

    return -1;
}


void lcd_drv_cmd_list(u8 *cmd_list, int cmd_cnt)
{
    int i;
    int k;
    int cnt;
    u16 *p16;
    u8 *p8;

    u8 *temp = NULL;
    u16 temp_len = 5 * 64;
    u16 len;
    temp = (u8 *)malloc(temp_len);

    for (i = 0; i < cmd_cnt;) {
        p16 = (u16 *)&cmd_list[i];
        int begin = find_begin(cmd_list, &cmd_list[cmd_cnt], i);
        if ((begin != -1)) {
            int end = find_end(cmd_list, &cmd_list[cmd_cnt], begin);
            if (end != -1) {
                p8 = (u8 *)&cmd_list[begin];
                u8 *param = &p8[1];
                u8 addr = p8[0];
                u8 cnt = end - begin - 1;
                if (((p8[0] << 24) | (p8[1] << 16) | (p8[2] << 8) | p8[3]) == REGFLAG_DELAY_FLAG) {
                    os_time_dly(p8[4] / 10);
                    /* printf("delay %d ms\n", p8[4]); */
                }  else {
                    len = sprintf((char *)temp, "send : 0x%02x(%d), ", addr, cnt);
                    for (k = 0; k < cnt; k++) {
                        len += sprintf((char *)&temp[len], "0x%02x, ", param[k]);
                        if (len > (temp_len - 10)) {
                            len += sprintf((char *)&temp[len], "...");
                            break;
                        }
                    }
                    len += sprintf((char *)&temp[len], "\n");
                    /* printf("cmd:%s", temp); */
#if 0
                    // 根据LCD类型选择发送命令的API
                    if (__this->lcd_type == TFT_LCD) {
                        imd_write_cmd(addr, param, cnt);
                    } else if (__this->lcd_type == DOT_LCD) {
                        spi_oled_write_cmd(addr, param, cnt);
                    }
#endif
#if (TCFG_SPI_LCD_ENABLE)
                    imd_write_cmd(addr, param, cnt);
#elif (TCFG_LCD_OLED_ENABLE)
                    spi_oled_write_cmd(addr, param, cnt);
#endif
                }
                i = end + 4;
            }
        }
    }
    free(temp);

#if 0
    u8 buf[5];
    printf("lcd_spi_read:\n");
    imd_read_cmd(0x0a, buf, 1);
    imd_read_cmd(0x52, buf, 1);
    imd_read_cmd(0x54, buf, 1);
    imd_read_cmd(0x0a, buf, 1);
    imd_read_cmd(0x59, buf, 1);
    imd_read_cmd(0x64, buf, 1);
    imd_read_cmd(0xa1, buf, 5);
    imd_read_cmd(0x0a, buf, 1);
    imd_read_cmd(0xa8, buf, 5);
    imd_read_cmd(0xaa, buf, 1);
    imd_read_cmd(0x0a, buf, 1);
    imd_read_cmd(0xaf, buf, 1);
    imd_read_cmd(0x0a, buf, 1);
#endif
}




/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD DEVICE INIT
 *
 * Description: LCD 设备初始化
 *
 * Arguments  : *p 板级配置的 LCD SPI 信息
 *
 * Returns    : 0 初始化成功
 * 				-1 初始化失败
 *
 * Notes      : 1、判断是否在板级文件配置SPI，是继续，否进入断言，
 *
 *              2、配置SPI可操作IO给IMD操作
 *
 *              3、LCD设备复位
 *
 *              4、SPI模块初始化，IMD模块初始化
 *********************************************************************************************************
 */

int lcd_drv_init(void *p)
{
    log_debug("lcd_drv_init ...\n");
    int err = 0;
    struct ui_devices_cfg *cfg = (struct ui_devices_cfg *)p;
    __this->lcd_type	= cfg->type;	// 保存LCD屏幕类型(TFT_LCD/DOT_LCD)
    __this->lcd			= &lcd_drive;	// 获取LCD驱动配置
    __this->param		= __this->lcd->param;	// 获取LCD参数配置
    lcd_dat = (struct lcd_platform_data *)cfg->private_data;
    ASSERT(lcd_dat, "Error! spi io not config");

    log_debug("spi pin rest:%d, cs:%d, dc:%d, en:%d, spi:%d\n", \
              lcd_dat->pin_reset, lcd_dat->pin_cs, lcd_dat->pin_dc, lcd_dat->pin_en, lcd_dat->spi_cfg);

    /* 如果有使能IO，设置使能IO输出高电平 */
    if (lcd_dat->pin_en != -1) {
        gpio_direction_output(lcd_dat->pin_en, 1);
    }

    /* printf("location [[%s : %s : %d]]\n", __FILE__, __FUNCTION__, __LINE__); */
    lcd_reset(); /* lcd复位 */

#if (TCFG_SPI_LCD_ENABLE)
    imd_set_ctrl_pin_func(spi_dc_ctrl, spi_cs_ctrl, spi_te_stat);
    imd_init(__this->param);
#elif (TCFG_LCD_OLED_ENABLE) /* oled屏推屏测试 */
    spi_oled_set_ctrl_pin_func(spi_dc_ctrl, spi_cs_ctrl, spi_te_stat);
    spi_oled_init(lcd_dat);
#else
#error "unknow lcd type!..."
#endif

#if 0
    if (__this->lcd_type == TFT_LCD) {
        // 初始化TFT_LCD彩屏
        imd_set_ctrl_pin_func(spi_dc_ctrl, spi_cs_ctrl, spi_te_stat);
        imd_init(__this->param);

    } else if (__this->lcd_type == DOT_LCD) {
        // 初始化DOT_LCD点阵屏
        spi_oled_set_ctrl_pin_func(spi_dc_ctrl, spi_cs_ctrl, spi_te_stat);
        spi_oled_init(lcd_dat);

    } else {
        printf("unknow lcd type!\n");
        return -1;
    }
#endif

    /* SPI发送屏幕初始化代码 */
    lcd_drv_cmd_list(__this->lcd->lcd_cmd, __this->lcd->cmd_cnt);

    lcd_mcpwm_init();
    LCD_DRV_TEST();


    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       GET LCD DEVICE INFO
 *
 * Description: 获取 LCD 设备信息
 *
 * Arguments  : *info LCD 设备信息缓存结构体，根据结构体内容赋值即可
 *
 * Returns    : 0 获取成功
 * 				-1 获取失败
 *
 * Notes      : 1、根据参数结构体的内容，将LCD对应信息赋值给结构体元素
 *********************************************************************************************************
 */

static int lcd_drv_get_screen_info(struct lcd_info *info)
{
    /* imb的宽高 */
    info->width = __this->param->in_width;
    info->height = __this->param->in_height;

    /* imb的输出格式 */
    info->color_format = __this->param->in_format;//OUTPUT_FORMAT_RGB565;
    if (info->color_format == OUTPUT_FORMAT_RGB565) {
        info->stride = (info->width * 2 + 3) / 4 * 4;
    } else if (info->color_format == OUTPUT_FORMAT_RGB888) {
        info->stride = (info->width * 3 + 3) / 4 * 4;
    }

    /* 屏幕类型 */
    info->interface = __this->param->drv_type;

    /* 对齐 */
    info->col_align = __this->lcd->column_addr_align;
    info->row_align = __this->lcd->row_addr_align;

    /* 背光状态 */
    info->bl_status = backlight_status;
    info->buf_num = __this->param->buffer_num;

    ASSERT(info->col_align, " = 0, lcd driver column address align error, default value is 1");
    ASSERT(info->row_align, " = 0, lcd driver row address align error, default value is 1");

    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       MALLOC DISPLAY BUFFER
 *
 * Description: 申请 LCD 显存 buffer
 *
 * Arguments  : **buf 保存显存buffer指针
 * 				*size 保存显存buffer大小
 *
 * Returns    : 0 成功
 * 				-1 失败
 *
 * Notes      : 1、根据LCD驱动中配置的显存大小和数量申请显存BUFFER
 *
 *				2、将显存buffer指针赋值给参数**buf，显存buffer大小赋值给参数*size
 *
 *				注意：buffer默认是lock状态，此时不能推屏，需由UI框架获取并写入数据后才能推屏
 *********************************************************************************************************
 */
static int lcd_drv_buffer_malloc(u8 **buf, u32 *size)
{
    int buf_size = (__this->param->buffer_size + 3) / 4 * 4;	// 把buffer大小做四字节对齐

#if UI_USED_DOUBLE_BUFFER
    *buf = (u8 *)malloc(buf_size * __this->param->buffer_num);
#else
    *buf = (u8 *)malloc(buf_size * __this->param->buffer_num);
#endif

    if (!buf) {
        // 如果buffer申请失败
        *buf = NULL;
        *size = 0;
        return -1;
    }
    *size = buf_size * __this->param->buffer_num;

    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       FREE DISPLAY BUFFER
 *
 * Description: 释放 LCD 显存 buffer
 *
 * Arguments  : *buf 显存buffer指针
 *
 * Returns    : 0 成功
 * 				-1 失败
 *
 * Notes      : 1、使用memory API 释放显存buffer
 *********************************************************************************************************
 */
static int lcd_drv_buffer_free(u8 *buf)
{
    if (buf) {
        printf("lcd_buffer_free : 0x%x\n", buf);
        free(buf);
        buf = NULL;
    }
    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD DRAW BUFFER
 *
 * Description: 把显存 buf 推送到屏幕
 *
 * Arguments  : *buf 显存buffer指针
 * 				len 显存buffer的数据量
 * 				wait 是否等待
 *
 * Returns    : 0 成功
 * 				-1 失败
 *
 * Notes      : 1、使用 IMD 模块将显存buffer推给屏幕
 *********************************************************************************************************
 */
static int lcd_drv_draw(u8 *buf, u32 len, u8 wait)
{
    /* put_buf(buf, len); */
#if (TCFG_SPI_LCD_ENABLE)
    imd_draw(LCD_DATA_MODE, (u32)buf);

#elif (TCFG_LCD_OLED_ENABLE)
    spi_oled_draw_page(buf);

#endif
    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD DRAW PAGE
 *
 * Description: 将UI页面buffer推给屏幕
 *
 * Arguments  : *buf 页面缓存buffer指针
 *
 * Returns    : 0 成功
 * 				-1 失败
 *
 * Notes      :
 *********************************************************************************************************
 */
#if (TCFG_LCD_OLED_ENABLE)
static int lcd_drv_draw_page(u8 *buf, u8 page_star, u8 page_len)
{
#if (TCFG_OLED_SPI_SSD1306_ENABLE == 0)
    u8 page;
    u8 *page_buf = buf;
    for (page = page_star; page < page_star + page_len; page++) {
        imd_write_cmd(0xb0 + page, NULL, 0);
        imd_write_cmd(0x00, NULL, 0);
        imd_write_cmd(0x10, page_buf, 128);
        page_buf += 128;
    }
#endif
    return 0;
}
#endif



/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       GET LCD BACKLIGHT STATUS
 *
 * Description: 获取 LCD 背光状态
 *
 * Arguments  : none
 *
 * Returns    : 0 背光熄灭
 * 				1 背光点亮
 *
 * Notes      :
 *********************************************************************************************************
 */
int lcd_backlight_status()
{
    return !!backlight_status;
}


int lcd_sleep_status()
{
    return lcd_sleep_in;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       LCD SLEEP CONTROL
 *
 * Description: LCD 休眠控制
 *
 * Arguments  : enter 是否进入休眠，true 进入休眠，false 退出休眠
 *
 * Returns    : 0 成功
 * 				-1 失败
 *
 * Notes      : 1、判断 LCD 是否正在使用，是等待使用结束，否进入下一步
 *
 * 				2、enter是否进入休眠，是使用LCD休眠函数进入休眠状态，否使用LCD退出休眠函数退出休眠状态
 *
 * 				3、lcd_sleep_in 记录LCD的休眠状态
 *********************************************************************************************************
 */

int lcd_sleep_ctrl(u8 enter)
{
    if ((!!enter) == lcd_sleep_in) {
        return -1;
    }
    while (is_lcd_busy);
    is_lcd_busy = 0x11;

    if (enter) {
        if (__this->lcd->entersleep) {
            __this->lcd->entersleep();
            lcd_sleep_in = true;
        }
    } else {
        if (__this->lcd->exitsleep) {
            __this->lcd->exitsleep();
            lcd_sleep_in = false;
        }
    }

    is_lcd_busy = 0;
    return 0;
}


/*$PAGE*/
/*
 *********************************************************************************************************
 *                                       GET LCD DRIVE HANDLER
 *
 * Description: 获取LCD驱动句柄
 *
 * Arguments  : none
 *
 * Returns    : struct lcd_interface* LCD驱动接口句柄
 *
 * Notes      : 1、从LCD接口列表中找到LCD接口句柄并返回
 *********************************************************************************************************
 */

struct lcd_interface *lcd_get_hdl()
{
    struct lcd_interface *p;

    ASSERT(lcd_interface_begin != lcd_interface_end, "don't find lcd interface!");
    for (p = lcd_interface_begin; p < lcd_interface_end; p++) {
        return p;
    }
    return NULL;
}


void lcd_drv_set_draw_area(u16 xs, u16 xe, u16 ys, u16 ye)
{
#if (TCFG_SPI_LCD_ENABLE)
    imd_set_draw_area(xs, xe, ys, ye);

#elif (TCFG_LCD_OLED_ENABLE)
    spi_oled_set_draw_area(xs, xe, ys, ye);

#endif
}


static void lcd_drv_clear_screen(u32 color)
{
#if (TCFG_SPI_LCD_ENABLE)
    /* imd_full_clear(color); */
    imd_fill_rect(LCD_COLOR_MODE, color, 0, __this->param->lcd_width - 1, 0, __this->param->lcd_height - 1);

#elif (TCFG_LCD_OLED_ENABLE)
    spi_oled_clear_screen(color);

#else
#error "undefined lce type!"
#endif
}



REGISTER_LCD_INTERFACE(lcd) = {
    .init				= lcd_drv_init,
    .draw				= lcd_drv_draw,
    .get_screen_info	= lcd_drv_get_screen_info,
    .buffer_malloc		= lcd_drv_buffer_malloc,
    .buffer_free		= lcd_drv_buffer_free,
    .backlight_ctrl		= lcd_drv_backlight_ctrl,
    .set_draw_area		= lcd_drv_set_draw_area,
    .clear_screen		= lcd_drv_clear_screen,

#if (TCFG_LCD_OLED_ENABLE)
    .draw_page			= lcd_drv_draw_page,
#endif
};


static u8 lcd_idle_query(void)
{
    return !is_lcd_busy;
}

REGISTER_LP_TARGET(lcd_lp_target) = {
    .name = "lcd",
    .is_idle = lcd_idle_query,
};





/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/*                                                                */
/*                     以下为推屏功能测试代码                     */
/*                                                                */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#if (TCFG_SPI_LCD_ENABLE)	// 仅TFT LCD需要RGB色填充

#define COLOR_MODE_RGB888	3
#define COLOR_MODE_RGB565	2
#define COLOR_MODE_RGB666	1
static void fill_buffer_test(u8 *buf, int w, int h, u32 color_888, u8 color_mode)
{
    /* 根据颜色模式，将颜色填充到buf中 */
    int pos = 0;
    int size = 0;
    u8 r, g, b;
    u8 bit1, bit2;
    u32 color;

    /* 从rgb888中获取r, g, b颜色分量 */
    r = (color_888 >> 16) & 0xff;
    g = (color_888 >> 8) & 0xff;
    b = (color_888 >> 0) & 0xff;

    /* 计算buffer大小 */
    if (color_mode == COLOR_MODE_RGB888) {
        size = w * h * 3;
        for (pos = 0; pos < size; pos += 3) {
            buf[pos + 0] = r;
            buf[pos + 1] = g;
            buf[pos + 2] = b;
        }
    } else if (color_mode == COLOR_MODE_RGB565) {
        size = w * h * 2;
        color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        bit1 = (color >> 8) & 0xff;
        bit2 = (color >> 0) & 0xff;
        for (pos = 0; pos < size; pos += 2) {
            buf[pos + 0] = bit1;
            buf[pos + 1] = bit2;
        }
    } else if (color_mode == COLOR_MODE_RGB666) {
        size = w * h * 3;
        color = ((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2);
        r = (color_888 >> 16) & 0xff;
        g = (color_888 >> 8) & 0xff;
        b = (color_888 >> 0) & 0xff;
        for (pos = 0; pos < size; pos += 3) {
            buf[pos + 0] = r;
            buf[pos + 1] = g;
            buf[pos + 2] = b;
        }
    } else {
        printf("%s, %d: error!, unknow color mode!", __FUNCTION__, __LINE__);
    }
}
#endif


static void push_screen_test(void)
{
#define	INTERVAL_TIME		100	// 刷屏测试间隔时间
    extern void wdt_clr();

#if (TCFG_SPI_LCD_ENABLE)
    u32 color_tab[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff, 0xffffff, 0x000000};
    int color_num = sizeof(color_tab) / sizeof(color_tab[0]);
    static u8 i = 0;

    while (1) {
        log_debug("clear color:0x%x\n", color_tab[i]);
        lcd_drv_clear_screen(color_tab[i]);

        if (++i >= color_num) {
            i = 0;
        }
        os_time_dly(INTERVAL_TIME);
        wdt_clr();
    }

#elif (TCFG_LCD_OLED_ENABLE) /* oled屏推屏测试 */
    spi_oled_test();

#endif
}

#endif



