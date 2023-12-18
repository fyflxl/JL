/*
 * Copyright 2020 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#ifndef UAL_AIK_MODE_H_
#define UAL_AIK_MODE_H_

#include "app_config.h"

#if TCFG_UNISOUND_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(4)
/*
 * WARNING: 修改该文件后必须同步修改 AikMode.java,
 * 新增 mode 放最后，数值 +1, 废弃的 mode 不能删除，保证各个版本取值的一致性
 */
typedef enum AikMode {
    AIK_MODE_NULL = 0,
    /* 离线唤醒 + 识别 */
    AIK_MODE_KWS = 1,
    /* 降噪模式 */
    AIK_MODE_SSP = 2,
    /* SSP + KWS LP */
    AIK_MODE_SSP_KWS = 3,

    /* TODO: 持续更新新模式 */
    AIK_MODE_MAX
} AikMode;
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif  // TCFG_VOICE_BOX_ENABLE

#endif  // UAL_AIK_MODE_H_
