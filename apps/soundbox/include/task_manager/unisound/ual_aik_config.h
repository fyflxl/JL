/*
 * Copyright 2020 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#ifndef UAL_AIK_CONFIG_H_
#define UAL_AIK_CONFIG_H_

#include "app_config.h"

#if TCFG_UNISOUND_ENABLE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(4)
/* Slave 模块配置（AIS 无线程方案） */
typedef struct SlaveConfig {
    int32_t chs;  // 输入数据的通道数
} SlaveConfig;
/* Audio 模块配置 */
/*
 * @Description: 获取录音接口，要求 16k，16bit,
 *               如果没有读到足够的数据，会重新调用读取
 * @Input params: data, 录音数据缓冲
 *                len, 需要读取的数据
 * @Output params:
 * @Return: 读到的数据大小
 */
typedef int32_t (*AudioDataGet)(void *data, int32_t len);
typedef struct AudioConfig {
    int32_t stack_size;
    int32_t priority;
    int32_t chs;
    AudioDataGet audio_get;
} AudioConfig;

/* SSP 模块配置项 */
#define SSP_MAX_MIC_NUM 16
#define SSP_MAX_REF_NUM 4
typedef void (*SspDataUploader)(int32_t index, int32_t doa, void *data,
                                int32_t len);
typedef struct SspConfig {
    int32_t total_chs;
    int32_t mode;
    int8_t mic_orders[SSP_MAX_MIC_NUM]; /* Mic 通道顺序 */
    int8_t ref_orders[SSP_MAX_REF_NUM]; /* Echo 通道顺序 */
    SspDataUploader data_uploader;
} SspConfig;

/* KWS 模块配置项 */
/* KWS FA 阈值对照表，取值参考引擎发布报告 */
typedef struct KwsFaValue {
    double fa;
    double threshold;
} KwsFaValue;
typedef struct KwsFaTable {
    int32_t nums;
    KwsFaValue *value;
} KwsFaTable;
/* 定制唤醒词及性能配置 */
typedef struct KwsCustomWuw {
    const char *wuw;       // 定制词条
    double fa;             // 唤醒 fa
    double fa_second_wkr;  // 二次唤醒 fa
    double fa_sleep;       // 深度唤醒 fa
} KwsCustomWuw;
/* 唤醒词配置 */
typedef struct KwsWkrCfg {
    KwsFaTable *table;           // 阈值表
    double fa;                   // 通用 fa
    double fa_second_wkr;        // 通用二次唤醒 fa
    double fa_sleep;             // 通用深度唤醒 fa
    int32_t time_to_sleep;       // 深度睡眠等待时间
    int32_t time_second_wakeup;  // 二次唤醒等待时间
    int32_t custom_nums;         // 定制词表数量
    KwsCustomWuw *wuw_custom;    // 定制词表
} KwsWkrCfg;
/* 定制命令词表 */
typedef struct KwsCustomCmd {
    const char *cmd;  // 命令词条
    double fa;        // 定制 fa
} KwsCustomCmd;
/* 命令词配置 */
typedef struct KwsCmdCfg {
    KwsFaTable *table;         // 阈值表
    double fa;                 // 通用 fa
    int32_t timeout;           // 超时时间，单位秒
    int32_t custom_nums;       // 定制词表数
    KwsCustomCmd *cmd_custom;  // 定制词表
} KwsCmdCfg;

/* KWS 模块配置主体 */
typedef struct KwsConfig {
    int32_t stack_size;              // 线程栈大小
    int32_t priority;                // 线程优先级
    char *am;                        // 声学模型数据
    char *grammar;                   // 语法模型数据
    int32_t auto_clear_when_start;   // 启动时自动清空计数
    int32_t auto_clear_when_result;  // 出结果时自动清空计数
    int32_t shared_size;             // 共享内存大小，默认为 0
    void *shared_buffer;  // 需要共享内存时配置，默认为 NULL
    KwsWkrCfg *wkr_cfg;   // 配置唤醒词表及阈值
    KwsCmdCfg *cmd_cfg;   // 配置识别命令词表及阈值
} KwsConfig;
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif  // TCFG_VOICE_BOX_ENABLE

#endif  // UAL_AIK_CONFIG_H_
