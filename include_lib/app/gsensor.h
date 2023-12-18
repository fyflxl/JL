
#ifndef _GSENSOR_H
#define _GSENSOR_H
#include "printf.h"
#include "cpu.h"
#include "timer.h"
#include "app_config.h"
#include "event.h"
#include "system/includes.h"

#include "device/device.h"
#include "ioctl_cmds.h"
#include "asm/spi.h"
#include "printf.h"
#include "gpio.h"
#include "device_drive.h"
#include "malloc.h"

int gsensor_init(void);
void lsm6dsl_self_test(void);

#endif

