/* COPYRIGHT NOTICE
 * 文件名称 ：spi_oled_drive.c
 * 简    介 ：使用硬件SPI驱动OLED点阵屏
 * 功    能 ：
 * 			这里只提供使用硬件SPI封装的读写数据接口，对于
 * 			IO操作配置、背光控制、LCD复位等均不在这里控制
 * 			注意：本文件API仅在使能TCFG_LCD_OLED_ENABLE宏时有效
 * 作    者 ：zhuhaifang
 * 创建时间 ：2022/05/17 16:03
 */

#include "app_config.h"


#if (TCFG_LCD_OLED_ENABLE)

#include "asm/spi.h"
#include "ui/ui_api.h"
#include "includes.h"

/* 选择SPIx模块作为推屏通道，0、1、2 */
/* 但SPI0通常作为外部falsh使用，一般不用SPI0 */
#define SPI_MODULE_CHOOSE   TCFG_TFT_LCD_DEV_SPI_HW_NUM


/* 中断使能，一般推屏不需要 */
#ifndef LCD_SPI_INTERRUPT_ENABLE
#define LCD_SPI_INTERRUPT_ENABLE 0
#endif


#if LCD_SPI_INTERRUPT_ENABLE
#if SPI_MODULE_CHOOSE	== 0
#define IRQ_SPI_IDX     IRQ_SPI0_IDX
#elif SPI_MODULE_CHOOSE == 1
#define IRQ_SPI_IDX     IRQ_SPI1_IDX
#elif SPI_MODULE_CHOOSE == 2
#define IRQ_SPI_IDX     IRQ_SPI2_IDX
#else
#error  "error! SPI_MODULE_CHOOSE defien error!"
#endif
#endif

#define OLED_CMD  0
#define OLED_DATA 1


static struct spi_oled_cfg_var {
    int  spi_dev;	// spi设备号
    struct lcd_platform_data *priv;
    void (*cs_ctrl)(u8);	// CS脚控制
    void (*dc_ctrl)(u8);	// DC脚控制
    int (*te_stat)();		// TE信号状态
};
static struct spi_oled_cfg_var spi_cfg = {0};
#define __this		(&spi_cfg)



#if LCD_SPI_INTERRUPT_ENABLE
static void spi_oled_isr()
{
    if (spi_get_pending(__this->spi_dev)) {
        spi_set_ie(__this->spi_dev, 0);
    }
}
#endif


/* API NOTES
 * 名    称 ：void spi_oled_set_ctrl_pin_func(void (*dc_ctrl)(u8), void (*cs_ctrl)(u8), int (*te_stat)())
 * 功    能 ：设置SPI的DC，CS和TE状态脚控制函数
 * 参    数 ：dc_ctrl DC脚控制函数，cs_ctrl CS脚控制函数，te_stat TE信号状态
 * 返 回 值 ：void
 */
void spi_oled_set_ctrl_pin_func(void (*dc_ctrl)(u8), void (*cs_ctrl)(u8), int (*te_stat)())
{
    __this->cs_ctrl = cs_ctrl;
    __this->dc_ctrl = dc_ctrl;
    __this->te_stat = te_stat;
}


/* API NOTES
 * 名    称 ：void spi_oled_init(struct lcd_platform_data *spi)
 * 功    能 ：SPI 模块初始化，打开SPI模块，注册中断
 * 参    数 ：*spi spi配置
 * 返 回 值 ：void
 */
void spi_oled_init(struct lcd_platform_data *spi)
{
    int err = 0;
    __this->spi_dev = spi->spi_cfg;		// SPI设备
    __this->priv = spi;

    err = spi_open(__this->spi_dev);
    if (err < 0) {
        printf("ERROR, open spi device faild\n");
        return;
    }

#if LCD_SPI_INTERRUPT_ENABLE
    // 配置中断优先级，中断函数
    /* spi_set_ie(spi_cfg, 1); */
    request_irq(IRQ_SPI_IDX, 3, spi_oled_isr, 0);
#endif
}


static int spi_oled_send_byte(u8 byte)
{
    int ret = 0;
#if 0
    spi_send_byte_for_isr(__this->spi_dev, byte);
    asm("csync");
    /* spi_dma_wait_finish(); */
#else
    ret = spi_send_byte(__this->spi_dev, byte);
#endif
    return ret;
}

static u8 spi_oled_recv_byte()
{
    int err;
    int ret;
#if 0
    ret = spi_recv_byte_for_isr(__this->spi_dev);
    asm("csync");
    /* spi_dma_wait_finish(); */
#else
    ret = spi_recv_byte(__this->spi_dev, &err);
#endif
    return ret;
}


/* API NOTES
 * 名    称 ：void spi_oled_wr_byte(u8 dat, u8 cmd)
 * 功    能 ：向ssd1306写入一个字节
 * 参    数 ：dat 要写入的数据/命令，cmd 数据/命令标志
 * 返 回 值 ：void
 */
static void spi_oled_wr_byte(u8 dat, u8 cmd)
{
    __this->cs_ctrl(0);
    if (cmd == OLED_CMD) {
        __this->dc_ctrl(0);
    } else {
        __this->dc_ctrl(1);
    }
    spi_oled_send_byte(dat);
    __this->cs_ctrl(1);
}


/* API NOTES
 * 名    称 ：static u8 spi_oled_rd_byte()
 * 功    能 ：从ssd1306读取一个字节
 * 参    数 ：void
 * 返 回 值 ：读取到的字节
 */
static u8 spi_oled_rd_byte()
{
    u8 dat;
    __this->cs_ctrl(0);
    __this->dc_ctrl(1);
    dat = spi_oled_recv_byte();
    __this->cs_ctrl(1);
    return dat;
}


/* API NOTES
 * 名    称 ：void spi_oled_write_cmd(u8 cmd, u8 *buf, u8 len)
 * 功    能 ：spi 写oled参数
 * 参    数 ：cmd 命令地址，*buf 参数buf，len 参数数量
 * 返 回 值 ：void
 */
void spi_oled_write_cmd(u8 cmd, u8 *buf, u8 len)
{
    spi_oled_wr_byte(cmd, OLED_CMD);
    __this->cs_ctrl(0);
    __this->dc_ctrl(1);
    if (buf && len > 0) {
        spi_dma_send(__this->spi_dev, buf, len);
    }
    __this->cs_ctrl(1);
}


/* API NOTES
 * 名    称 ：void spi_oled_read_cmd(u8 cmd, u8 *buf, u8 len)
 * 功    能 ：spi 读oled参数
 * 参    数 ：cmd 待读命令，*buf 缓存buf，len 读取长度
 * 返 回 值 ：void
 */
void spi_oled_read_cmd(u8 cmd, u8 *buf, u8 len)
{
    spi_oled_wr_byte(cmd, OLED_CMD);
    __this->cs_ctrl(0);
    __this->dc_ctrl(1);
    if (buf && len > 0) {
        spi_dma_recv(__this->spi_dev, buf, len);
    }
    __this->cs_ctrl(1);
}


/* API NOTES
 * 名    称 ：void spi_oled_display_on()
 * 功    能 ：开启oled显示
 * 参    数 ：void
 * 返 回 值 ：void
 */
void spi_oled_display_on()
{
    spi_oled_wr_byte(0X8D, OLED_CMD); //SET DCDC命令
    spi_oled_wr_byte(0X14, OLED_CMD); //DCDC ON
    spi_oled_wr_byte(0XAF, OLED_CMD); //DISPLAY ON
}


/* API NOTES
 * 名    称 ：void spi_oled_display_off()
 * 功    能 ：关闭oled显示
 * 参    数 ：void
 * 返 回 值 ：void
 */
void spi_oled_display_off()
{
    spi_oled_wr_byte(0X8D, OLED_CMD); //SET DCDC命令
    spi_oled_wr_byte(0X10, OLED_CMD); //DCDC OFF
    spi_oled_wr_byte(0XAE, OLED_CMD); //DISPLAY OFF
}


/* API NOTES
 * 名    称 ：void spi_oled_clear_screen(u32 color)
 * 功    能 ：oled 清空屏幕
 * 参    数 ：color 清屏颜色（oeld点阵屏只有黑“0x00”和白“0xff”两种颜色）
 * 返 回 值 ：void
 */
void spi_oled_clear_screen(u32 color)
{
    u8 buf[128];
    memset(&buf, (color & 0xff), 128);
    for (int i = 0; i < 8; i++) {
        spi_oled_wr_byte(0xb0 + i, OLED_CMD);
        spi_oled_wr_byte(0x00, OLED_CMD);
        spi_oled_write_cmd(0x10, &buf, 128);
    }
}


/* API NOTES
 * 名    称 ：void spi_oled_set_draw_area(u16 xs, u16 xe, u16 ys, u16 ye)
 * 功    能 ：oled 设置刷新区域
 * 参    数 ：xs X起始坐标，xe X结束坐标，ys Y起始坐标，ye Y结束坐标
 * 返 回 值 ：void
 */
void spi_oled_set_draw_area(u16 xs, u16 xe, u16 ys, u16 ye)
{
    spi_oled_write_cmd(0xb0 + (ys / 8) % 8, NULL, 0);
    spi_oled_write_cmd(0x00 + xs & 0x0f, NULL, 0);
    spi_oled_write_cmd(0x10 + (xs >> 4) & 0x0f, NULL, 0);
}


/* API NOTES
 * 名    称 ：void spi_oled_draw_buf(u8 *buf)
 * 功    能 ：oled 刷新一帧buf(全屏buf)
 * 参    数 ：*buf 待刷新buf地址
 * 返 回 值 ：void
 */
void spi_oled_draw_page(u8 *buf)
{
    /* u8 test[128]; */
    u8 page;
    u8 *page_buf = buf;
    for (page = 0; page < 8; page ++) {
        /* spi_oled_wr_byte(0xb0 + page, OLED_CMD); */
        /* spi_oled_wr_byte(((0 & 0xf0) >> 4) | 0x10, OLED_CMD); */
        /* spi_oled_write_cmd((0 & 0x0f) | 0x01, page_buf, 128); */
#if 1
        spi_oled_wr_byte(0xb0 + page, OLED_CMD);
        spi_oled_wr_byte(0x00, OLED_CMD);
        /* if (page % 2) { */
        /* memset(&test[0], 0xff, 128); */
        /* } else { */
        /* memset(&test[0], 0x00, 128); */
        /* } */
        /* spi_oled_write_cmd(0x10, &test[0], 128); */
        spi_oled_write_cmd(0x10, page_buf, 128);
        page_buf += 128;
#endif
    }
}


/* API NOTES
 * 名    称 ：void spi_oled_test()
 * 功    能 ：oled 测试函数，刷屏特征：白 --> 黑 --> 特征buf --> buf循环移动
 * 参    数 ：void
 * 返 回 值 ：void
 */
void spi_oled_test()
{
#define	INTERVAL_TIME		100	// 刷屏测试间隔时间
    extern void wdt_clr();

    spi_oled_clear_screen(0xff);
    os_time_dly(INTERVAL_TIME);

    spi_oled_clear_screen(0x00);
    os_time_dly(INTERVAL_TIME);

    /* 刷指定内容 */
    static u8 cnt = 0;
    u8 word[] = {
        0x80, 0x80, 0x9f, 0x80, 0x80, 0x80, 0x80, 0xff,
        0xff, 0x80, 0x80, 0x9d, 0x95, 0x95, 0x97, 0x80,
        0x00, 0x54, 0x54, 0x54, 0x7c, 0x00, 0x00, 0xff,
        0xff, 0x00, 0x00, 0x3c, 0x20, 0xfc, 0x20, 0x20
    };

    while (1) {
        spi_oled_write_cmd(0xb0 + cnt, NULL, 0);
        spi_oled_write_cmd(0x00, NULL, 0);
        spi_oled_write_cmd(0x10, &word[0], 16);

        spi_oled_write_cmd(0xb1 + cnt, NULL, 0);
        spi_oled_write_cmd(0x00, NULL, 0);
        spi_oled_write_cmd(0x10, &word[16], 16);

        if ((cnt += 2) > 6) {
            cnt = 0;
        }
        /* printf("%s...", __FUNCTION__); */
        os_time_dly(INTERVAL_TIME);
        spi_oled_clear_screen(0x00);
        wdt_clr();
    }
}

#endif


