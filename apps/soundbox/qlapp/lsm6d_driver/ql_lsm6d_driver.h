#ifndef _QL_LSM6D_DRIVER_H__
#define _QL_LSM6D_DRIVER_H__

#include "app_config.h"
#include "system/includes.h"

#define LSM6DSL_ADD_READ 0X80 

#define XYZ_BUF_LEN  48
#define LSM6D_FULL_SCALE	8

//extern unsigned char my_x_send;//用来生产测试传输xyz数据的
//extern unsigned char my_y_send;
//extern unsigned char my_z_send;

//extern unsigned char FX_raw[3];
//extern unsigned char FY_raw[3];
//extern unsigned char FZ_raw[3];
//extern int8_t xyz_buf[XYZ_BUF_LEN];
//extern bool lis3d_enable;


extern uint8_t odr_setting;

uint8_t lsm6d_who_am_i(void);
void lsm6d_init(void);
void read_6d_sensor_data(void);
void lis3dh_powerdown(void);

void set_batching_data_rate(uint8_t bdr);
void set_lsm6d_odr(uint8_t odr);
void ble_raw_data_lsm6d_notify(void);

void ql_lsm6d_drive_init(void);


#endif



