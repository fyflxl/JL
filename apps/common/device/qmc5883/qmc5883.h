#ifndef _QMC5883_H
#define _QMC5883_H

#include "typedef.h"
#include "system/includes.h"
#include "asm/iic_hw.h"
#include "asm/iic_soft.h"
#include "asm/timer.h"
#include "asm/cpu.h"
#include "app_config.h"
#include "asm/clock.h"
#include "generic/typedef.h"

#define TCFG_QMC5883_DEV_ENABLE 1
#define TCFG_QMC5883_USE_IIC_TYPE 0


#if TCFG_QMC5883_DEV_ENABLE

#define QMC5883_DEV_ADDRESS_W   0x1a
#define QMC5883_DEV_ADDRESS_R   0x1b

#define QMC5883_OUT_DATA_X_LSB  0x00
#define QMC5883_OUT_DATA_X_MSB  0x01
#define QMC5883_OUT_DATA_Y_LSB  0x02
#define QMC5883_OUT_DATA_Y_MSB  0x03
#define QMC5883_OUT_DATA_Z_LSB  0x04
#define QMC5883_OUT_DATA_Z_MSB  0x05
#define QMC5883_STATUS          0x06
#define QMC5883_TEMPER_DATA_LSB 0x07
#define QMC5883_TEMPER_DATA_MSB 0x08
#define QMC5883_CONTR1          0x09
#define QMC5883_CONTR2          0x0a
#define QMC5883_SET_PERIOD      0x0b
#define QMC5883_RESERVED        0x0c
#define QMC5883_CHIP_ID         0x0d
enum qmc5883_mode { //control reg1: bit0,bit1
    QMC5883_MODE_STANDBY,
    QMC5883_MODE_CONTINUOUS
};
enum {//control reg1: bit2,bit3
    QMC5883_ODR_10HZ,
    QMC5883_ODR_50HZ,
    QMC5883_ODR_100HZ,
    QMC5883_ODR_200HZ,
};
enum {//control reg1: bit4,bit5
    QMC5883_RNG_2G,
    QMC5883_RNG_8G
};
enum {//control reg1: bit6,bit7
    QMC5883_OSR_512,
    QMC5883_OSR_256,
    QMC5883_OSR_128,
    QMC5883_OSR_64,
};




struct _qmc5883_dev_platform_data {
    u8 iic_hdl;
    u8 iic_delay;          //这个延时并非影响iic的时钟频率，而是2Byte数据之间的延时
};
struct qmc5883_data {
    s16 x;
    s16 y;
    s16 z;
    double angle_xy;
    double angle_xz;
    double angle_yz;
};

#define QMC5883_PLATFORM_DATA_BEGIN(data) \
        static const struct _qmc5883_dev_platform_data data = {

#define QMC5883_PLATFORM_DATA_END() \
            };



bool qmc5883_reset();
bool qmc5883_init(void *priv);
void qmc5883_mode_change(enum qmc5883_mode mode);
void get_qmc5883_data(struct qmc5883_data *qmc5883_data);

// void qmc5883_test(void);

#endif
#endif


