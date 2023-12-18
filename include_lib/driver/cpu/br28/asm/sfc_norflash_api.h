#ifndef __SFC_NORFLASH_API_H__
#define __SFC_NORFLASH_API_H__

#include "typedef.h"

int norflash_init(const struct dev_node *node, void *arg);
int norflash_open(const char *name, struct device **device, void *arg);
int norflash_read(struct device *device, void *buf, u32 len, u32 offset);
int norflash_write(struct device *device, void *buf, u32 len, u32 offset);
int norflash_ioctl(struct device *device, u32 cmd, u32 arg);

/* ---------------------------------------------------------------------------- */
/**
 * @brief sfc加锁
 *
 * @param index: 0: sfc0, 1: sfc1
 * @note: 该接口不能在中断中调用,且调用前不能关中断
 */
/* ---------------------------------------------------------------------------- */
void sfc_protect_lock(u8 index);

/* ---------------------------------------------------------------------------- */
/**
 * @brief sfc解锁
 *
 * @param index: 0: sfc0, 1: sfc1
 */
/* ---------------------------------------------------------------------------- */
void sfc_protect_release(u8 index);

//SFC Norflash自定义IOCTL命令:
#define IOCTL_SFCN_CMD_DEFINE(cmd) 					(('S' << 24) | ('F' << 16) | ('N' << 8) | (cmd << 0))

#define IOCTL_SFC_NORFLASH_READ_NO_ENC             	IOCTL_SFCN_CMD_DEFINE(1)
#define IOCTL_SFC_NORFLASH_WRITE_NO_ENC           	IOCTL_SFCN_CMD_DEFINE(2)

struct sfc_no_enc_wr {
    u8 *buf;
    u32 addr;
    u32 len;
};

int sfc_spi_read_no_enc(u32 addr, void *buf, u32 len);

void sfc_spi_write_enc_set(u8 enc);

#endif
