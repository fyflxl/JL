/*
** 包含board的头文件，确定baord里面开关的屏驱宏
*/
#include "app_config.h"


/*
** 驱动代码的宏开关
*/
//<<<[4-fire屏 240x240]>>>//
#if TCFG_OLED_SPI_SSD1306_ENABLE



/*
** 包含imd头文件，屏驱相关的变量和结构体都定义在imd.h
** 包含imb头文件，imb输出格式定义在imb.h
*/
#include "asm/imd.h"
#include "includes.h"
#include "ui/ui_api.h"



#define SCR_X 0
#define SCR_Y 0
#define SCR_W 128
#define SCR_H 64
#define LCD_W 128
#define LCD_H 64
#define LCD_BLOCK_W 128
#define LCD_BLOCK_H 64
#define BUF_NUM 2

/*
** 初始化代码
*/
static const u8 lcd_cmd_t[] ALIGNED(4) = {
    _BEGIN_, REGFLAG_DELAY, 200, _END_,	// delay 120ms
    _BEGIN_, 0xAE, _END_,
    _BEGIN_, 0X00, _END_,
    _BEGIN_, 0X10, _END_,
    _BEGIN_, 0X40, _END_,
    _BEGIN_, 0X81, _END_,
    _BEGIN_, 0XCF, _END_,
    _BEGIN_, 0XA1, _END_,
    _BEGIN_, 0XC8, _END_,
    _BEGIN_, 0XA6, _END_,
    _BEGIN_, 0XA8, _END_,
    _BEGIN_, 0X3F, _END_,
    _BEGIN_, 0XD3, _END_,
    _BEGIN_, 0X00, _END_,
    _BEGIN_, 0XD5, _END_,
    _BEGIN_, 0X80, _END_,
    _BEGIN_, 0XD9, _END_,
    _BEGIN_, 0XF1, _END_,
    _BEGIN_, 0XDA, _END_,
    _BEGIN_, 0X12, _END_,
    _BEGIN_, 0XDB, _END_,
    _BEGIN_, 0X40, _END_,
    _BEGIN_, 0X20, _END_,
    /* _BEGIN_, 0X02, _END_, */
    _BEGIN_, 0X00, _END_,
    _BEGIN_, 0X8D, _END_,
    _BEGIN_, 0X14, _END_,
    _BEGIN_, 0XA4, _END_,
    _BEGIN_, 0XA6, _END_,
    _BEGIN_, 0XAF, _END_,
    _BEGIN_, 0XAF, _END_,
    _BEGIN_, REGFLAG_DELAY, 200, _END_,
};



// 配置参数
struct imd_param param_t = {
    .scr_x    = SCR_X,
    .scr_y	  = SCR_Y,
    .scr_w	  = SCR_W,
    .scr_h	  = SCR_H,

    // imb配置相关
    .in_width  = SCR_W,
    .in_height = SCR_H,
    .in_format = LCD_COLOR_MONO,

    // 屏幕相关
    .lcd_width  = LCD_W,
    .lcd_height = LCD_H,
    .drv_type	= IMD_DRV_SPI,

    .buffer_num  = BUF_NUM,
    .buffer_size = LCD_BLOCK_W * LCD_BLOCK_H / 8 * 2,
    .fps = 60,

    .spi = {
        .spi_mode	= SPI_IF_MODE(LCD_DRIVE_CONFIG),
        .pixel_type = PIXEL_TYPE(LCD_DRIVE_CONFIG),
        .out_format = OUT_FORMAT(LCD_DRIVE_CONFIG),
        .port		= SPI_PORTA,
        .spi_dat_mode = SPI_MODE_UNIDIR,
    },

    .debug_mode_en = false,
    .debug_mode_color = 0x0000ff,

    .te_en = false,

};


/*
** lcd背光控制
** 考虑到手表应用lcd背光控制需要更灵活自由，可能需要pwm调光，随时亮灭等
** 因此内部不操作lcd背光，全部由外部自行控制
*/
#if 0
static int lcd_backlight_ctrl(u8 onoff)
{
    JL_PORTC->DIR &= ~BIT(8);
    if (onoff) {
        JL_PORTC->OUT |= BIT(8);
    } else {
        JL_PORTC->OUT &= ~BIT(8);
    }
    return 0;
}
#endif


/*
** 设置lcd进入睡眠
*/
static void lcd_entersleep(void)
{
#if (TCFG_OLED_SPI_SSD1306_ENABLE == 0)
    imd_write_cmd(0x28, NULL, 0);
    imd_write_cmd(0x10, NULL, 0);
#endif
    delay_2ms(120 / 2);	// delay 120ms
}

/*
** 设置lcd退出睡眠
*/
static void lcd_exitsleep(void)
{
#if (TCFG_OLED_SPI_SSD1306_ENABLE == 0)
    imd_write_cmd(0x11, NULL, 0);
    delay_2ms(120 / 2);	// delay 120ms
    imd_write_cmd(0x29, NULL, 0);
#endif
}




REGISTER_LCD_DEVICE() = {
    .logo = "ssd1306",
    .row_addr_align		= 1,
    .column_addr_align	= 1,

    .lcd_cmd = (void *) &lcd_cmd_t,
    .cmd_cnt = sizeof(lcd_cmd_t) / sizeof(lcd_cmd_t[0]),
    .param   = (void *) &param_t,

    .init			= NULL,
    .reset			= NULL,
    .backlight_ctrl = NULL,
    .entersleep     = lcd_entersleep,
    .exitsleep      = lcd_exitsleep,
};


#endif



