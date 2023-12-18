#ifndef __IMD_SPI_H__
#define __IMD_SPI_H__


// <<<[lcd接口配置]>>>
#define SPI_MODE  (0<<4)
#define DSPI_MODE (1<<4)
#define QSPI_MODE (2<<4)


#define SPI_WIRE3  0
#define SPI_WIRE4  1


#define QSPI_SUBMODE0 0//0x02
#define QSPI_SUBMODE1 1//0x32
#define QSPI_SUBMODE2 2//0x12


#define PIXEL_1P1T (0<<5)
#define PIXEL_1P2T (1<<5)
#define PIXEL_1P3T (2<<5)
#define PIXEL_2P3T (3<<5)


#define PIXEL_1T2B  1
#define PIXEL_1T6B  5
#define PIXEL_1T8B  7
#define PIXEL_1T9B  8
#define PIXEL_1T12B 11
#define PIXEL_1T16B 15
#define PIXEL_1T18B 17
#define PIXEL_1T24B 23

#define SPI_PORTA 0
#define SPI_PORTB 1

#define SPI_MODE_UNIDIR  0//半双工，d0分时发送接收
#define SPI_MODE_BIDIR   1//全双工，d0发送、d1接收


#define IMD_SPI_PND_IE()	SFR(JL_IMD->IMDSPI_CON0, 29, 1, 1)
#define IMD_SPI_PND_DIS()	SFR(JL_IMD->IMDSPI_CON0, 29, 1, 0)


// imd spi 配置参数
struct imd_spi_config_def {
    int port;
    int spi_mode;
    int spi_dat_mode;
    int pixel_type;
    int out_format;

    void (*cs_ctrl)(u8);	// cs脚控制函数
    void (*dc_ctrl)(u8);	// dc脚控制函数
};



void imd_spi_init(void *priv);

void imd_spi_write_cmd(u8 cmd, u8 *buf, u8 len);

void imd_spi_read_cmd(u8 cmd, u8 *buf, u8 len);

void imd_spi_set_area(int xstart, int xend, int ystart, int yend);

void imd_spi_draw();

int  imd_spi_isr();



#endif


