#ifndef __IMD_H__
#define __IMD_H__

#include "generic/typedef.h"
#include "asm/imd_spi.h"
#include "asm/imd_mcu.h"
#include "asm/imd_rgb.h"


#define AT_UI_RAM             AT(.ui_ram)


extern const int ENABLE_JL_UI_FRAME;
extern const int ENABLE_PSRAM_UI_FRAME;
extern u8 curr_index;


enum {
    PIN_NONE,
    PIN_SYNC0_PA4,
    PIN_SYNC1_PA5,
    PIN_SYNC2_PA6,
};

enum {
    SYNC0_SEL_NONE,
    SYNC0_SEL_RGB_SYNC0,
    SYNC0_SEL_PAP_WR,
    SYNC0_SEL_PAP_RD,
};

enum {
    SYNC1_SEL_NONE,
    SYNC1_SEL_RGB_SYNC1,
    SYNC1_SEL_PAP_WR,
    SYNC1_SEL_PAP_RD,
};

enum {
    SYNC2_SEL_NONE,
    SYNC2_SEL_RGB_SYNC2,
    SYNC2_SEL_PAP_WR,
    SYNC2_SEL_PAP_RD,
};


#define FORMAT_RGB565 1//0//1P2T
#define FORMAT_RGB666 2//1//1P3T
#define FORMAT_RGB888 0//2//1P3T


// IMD驱动类型定义
typedef enum {
    IMD_DRV_SPI,
    IMD_DRV_MCU,
    IMD_DRV_RGB,
    IMD_DRV_MAX,
} IMD_DRV_TYPE;



// IMD draw模式定义
enum {
    LCD_COLOR_MODE,
    LCD_DATA_MODE,
};


// spi mcu rgb模式的接口定义
typedef struct imd_driver {
    void (*init)(void *priv);
    void (*write)(u8, u8 *, u8);
    void (*read)(u8, u8 *, u8);
    void (*set_draw_area)(int, int, int, int);
    void (*draw)();
    int (*isr)();
} imd_drive_cfg;



// imd私有变量定义
struct imd_variable {
    u8 imd_pnd;		// imd中断标志
    u8 imd_busy;	// imd忙碌标志
    u8 te_ext;		// te中断标志
    u8 clock_init;	// imd时钟初始化标志
    u8 sfr_save;	// 寄存器保存标志
    u32  imd_sfr[17];
    void (*dc_ctrl)(u8 val);
    void (*cs_ctrl)(u8 val);
    int (*te_stat)();
    struct imd_param *param;
};



// imd输入数据格式
enum OUTPUT_FORMAT {
    OUTPUT_FORMAT_RGB888,
    OUTPUT_FORMAT_RGB565,
};



// lcd配置结构体
struct imd_param {
    // 显示区域相关
    int scr_x;
    int scr_y;
    int scr_w;
    int scr_h;

    // lcd配置
    int lcd_width;
    int lcd_height;

    // 驱动类型
    IMD_DRV_TYPE drv_type;

    // 显存配置
    int buffer_num;
    int buffer_size;

    // 帧率配置
    int fps;

    // imd模块的输入，与imb模块一起用时，也是imb模块的输出
    int in_width;
    int in_height;
    int in_format;
    int in_stride;

    // debug模式
    int debug_mode_en;
    int debug_mode_color;

    // te功能
    int te_en;		//TE信号使能
    int te_port;	// te的io

    // 子模块配置参数
    struct imd_spi_config_def spi;	// SPI屏
    struct imd_mcu_config_def pap;	// MCU屏
    struct imd_rgb_config_def rgb;	// RGB屏
};



void imd_set_size(int width, int height);
void imd_set_stride(int stride);

#define imd_set_buf_size(width, height, stride) \
{ \
	imd_set_size(width, height); \
	imd_set_stride(stride); \
}


int  imd_init(struct imd_param *param);

void imd_clock_init(unsigned int imd_target_clk_kHz);

void imd_write_cmd(u8 cmd, u8 *buf, u8 len);

void imd_read_cmd(u8 cmd, u8 *buf, u8 len);

void imd_set_draw_area(int xstart, int xend, int ystart, int yend);

void imd_draw(u8 mode, u32 priv);

void imd_full_clear(u32 color);

void imd_fill_rect(int draw_mode, u32 buf_or_rgb888, int x, int w, int y, int h);


#endif





