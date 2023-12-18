#include "qmc5883.h"
#include "math.h"

#if(TCFG_QMC5883_DEV_ENABLE == 1)

#undef LOG_TAG_CONST
#define LOG_TAG     "[qmc5883]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

#if TCFG_QMC5883_USE_IIC_TYPE
#define iic_init(iic)                       hw_iic_uninit(iic)
#define iic_uninit(iic)                     hw_iic_uninit(iic)
#define iic_start(iic)                      hw_iic_start(iic)
#define iic_stop(iic)                       hw_iic_stop(iic)
#define iic_tx_byte(iic, byte)              hw_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               hw_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         hw_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        hw_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    hw_iic_suspend(iic)
#define iic_resume(iic)                     hw_iic_resume(iic)
#else
#define iic_init(iic)                       soft_iic_init(iic)
#define iic_uninit(iic)                     soft_iic_uninit(iic)
#define iic_start(iic)                      soft_iic_start(iic)
#define iic_stop(iic)                       soft_iic_stop(iic)
#define iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    soft_iic_suspend(iic)
#define iic_resume(iic)                     soft_iic_resume(iic)
#endif
extern void delay_2ms(int cnt);//fm

static struct _qmc5883_dev_platform_data *qmc5883_iic_info;

static u8 qmc5883_read_buf(u8 register_address, u8 *data, u8 data_len)
{
    if (!data_len) {
        return 0;
    }
    u8 ret = 0;
    iic_start(qmc5883_iic_info->iic_hdl);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, QMC5883_DEV_ADDRESS_W);
    if (!ret) {
        log_error("qmc5883 read fail1!\n");
        goto __qmc5883_r_end;
    }
    delay_2ms(qmc5883_iic_info->iic_delay);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, register_address);
    if (!ret) {
        log_error("qmc5883 read fail2!\n");
        goto __qmc5883_r_end;
    }
    delay_2ms(qmc5883_iic_info->iic_delay);
    iic_start(qmc5883_iic_info->iic_hdl);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, QMC5883_DEV_ADDRESS_R);
    if (!ret) {
        log_error("qmc5883 read fail3!\n");
        goto __qmc5883_r_end;
    }
    u8 i;
    for (i = 0; i < (data_len - 1); i ++) {
        *(data + i) = iic_rx_byte(qmc5883_iic_info->iic_hdl, 1);
        delay_2ms(qmc5883_iic_info->iic_delay);
    }
    *(data + i) = iic_rx_byte(qmc5883_iic_info->iic_hdl, 0);
    ret = data_len;
__qmc5883_r_end:
    iic_stop(qmc5883_iic_info->iic_hdl);
    return ret;
}

static u8 qmc5883_write_byte(u8 register_address, u8 data)
{
    u8 ret;
    iic_start(qmc5883_iic_info->iic_hdl);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, QMC5883_DEV_ADDRESS_W);
    if (!ret) {
        log_error("qmc5883 write fail1!\n");
        goto __qmc5883_w_end;
    }
    delay_2ms(qmc5883_iic_info->iic_delay);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, register_address);
    if (!ret) {
        log_error("qmc5883 write fail2!\n");
        goto __qmc5883_w_end;
    }
    delay_2ms(qmc5883_iic_info->iic_delay);
    ret = iic_tx_byte(qmc5883_iic_info->iic_hdl, data);
    if (!ret) {
        log_error("qmc5883 write fail3!\n");
        goto __qmc5883_w_end;
    }
__qmc5883_w_end:
    iic_stop(qmc5883_iic_info->iic_hdl);
    return ret;
}
//Reset the qmc5883
//return:0:error;  1:ok
bool qmc5883_reset()
{
    return qmc5883_write_byte(QMC5883_CONTR2, 0x80);
}

bool qmc5883_init(void *priv)
{
    if (priv == NULL) {
        log_info("qmc5883 init fail(no priv)\n");
        return false;
    }
    u8 temp_data = 0;
    qmc5883_iic_info = (struct _qmc5883_dev_platform_data *)priv;
    qmc5883_write_byte(0x20, 0x40);
    qmc5883_write_byte(0x21, 0x01);
    qmc5883_write_byte(QMC5883_SET_PERIOD, 0x01);
    temp_data = QMC5883_OSR_512 << 6 | QMC5883_RNG_2G << 4 | QMC5883_ODR_100HZ << 2 | QMC5883_MODE_CONTINUOUS;
    qmc5883_write_byte(QMC5883_CONTR1, temp_data);
    return true;
}
void qmc5883_mode_change(enum qmc5883_mode mode)
{
    u8 temp_data = 0;
    qmc5883_read_buf(QMC5883_CONTR1, &temp_data, 1);
    if ((temp_data & 0x03) == mode) {
        return;
    }
    temp_data &= 0xfc;
    temp_data |= mode;
    qmc5883_write_byte(QMC5883_CONTR1, temp_data);
}
void get_qmc5883_data(struct qmc5883_data *qmc5883_data)
{
    u8 xyz_data[6] = {0};
    u8 temp_data = 0;
    qmc5883_read_buf(QMC5883_STATUS, &temp_data, 1);
    if (temp_data == 0x01 || temp_data == 0x05) {
        /* log_info("new data is ok"); */
        qmc5883_read_buf(QMC5883_OUT_DATA_X_LSB, xyz_data, 6);
        qmc5883_data->x = xyz_data[1] << 8 | xyz_data[0];
        qmc5883_data->y = xyz_data[3] << 8 | xyz_data[2];
        qmc5883_data->z = xyz_data[5] << 8 | xyz_data[4];
        /* printf("x=%6d,y=%6d,z=%6d      ", qmc5883_data->x, qmc5883_data->y, qmc5883_data->z); */

        if (qmc5883_data->x > 0x7fff) {
            qmc5883_data->x -= 0xffff;
        }
        if (qmc5883_data->y > 0x7fff) {
            qmc5883_data->y -= 0xffff;
        }
        if (qmc5883_data->z > 0x7fff) {
            qmc5883_data->z -= 0xffff;
        }
        qmc5883_data->angle_xy = atan2((double)qmc5883_data->y, (double)qmc5883_data->x) * (180 / 3.14159265) + 180; //计算XY平面角度
        qmc5883_data->angle_xz = atan2((double)qmc5883_data->z, (double)qmc5883_data->x) * (180 / 3.14159265) + 180; //计算XZ平面角度
        qmc5883_data->angle_yz = atan2((double)qmc5883_data->z, (double)qmc5883_data->y) * (180 / 3.14159265) + 180; //计算YZ平面角度
        /* printf("Angle_XY=%3d.%d,Angle_XZ=%3d.%d,Angle_YZ=%3d.%d\n",(s32)qmc5883_data->angle_xy,(s32)(qmc5883_data->angle_xy*10)%10,(s32)qmc5883_data->angle_xz,(s32)(qmc5883_data->angle_xz*10)%10,(s32)qmc5883_data->angle_yz,(s32)(qmc5883_data->angle_yz*10)%10); */
    } else {
        log_error("qmc5883 no data. StatusReg:%x", temp_data);
    }
}



/*************************test*************************/
#if 0
static struct _qmc5883_dev_platform_data qmc5883_iic_info_test = {
    .iic_hdl = 0,
    .iic_delay = 0
};
void qmc5883_test(void)
{
    printf("\n**************qmc5883_test**********\n");
    iic_init(qmc5883_iic_info_test.iic_hdl);
    /**初始化QMC5883***/
    if (qmc5883_init(&qmc5883_iic_info_test)) {
        log_info("qmc5883 init ok!***************************\n");
    } else {
        log_error("qmc5883 init fail!***************************\n");
    }

    u8 qmc5883_id;
    qmc5883_read_buf(QMC5883_CHIP_ID, &qmc5883_id, 1);
    printf("qmc5883_id=%x\n", qmc5883_id);//qmc5883 id:0xff

    struct qmc5883_data read_qmc5883_data;
    while (1) {
        get_qmc5883_data(&read_qmc5883_data);
        printf("x=%6d,y=%6d,z=%6d      ", read_qmc5883_data.x, read_qmc5883_data.y, read_qmc5883_data.z);
        printf("Angle_XY=%3d.%d,Angle_XZ=%3d.%d,Angle_YZ=%3d.%d\n", (s32)read_qmc5883_data.angle_xy, (s32)(read_qmc5883_data.angle_xy * 10) % 10, (s32)read_qmc5883_data.angle_xz, (s32)(read_qmc5883_data.angle_xz * 10) % 10, (s32)read_qmc5883_data.angle_yz, (s32)(read_qmc5883_data.angle_yz * 10) % 10);

        os_time_dly(100);
        wdt_clear();
    }
}


#endif
#endif




