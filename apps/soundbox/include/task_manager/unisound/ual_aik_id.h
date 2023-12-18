/*
 * Copyright 2019 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#ifndef UAL_AIK_ID_H_
#define UAL_AIK_ID_H_

#include "app_config.h"

#if TCFG_UNISOUND_ENABLE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WARNING: 修改该文件后必须同步修改 AikId.java! */
/*
 * AIK type id 格式定义
 * 32          24         16                        0
 * |<--------->|<--------->|<---------------------->|
 *   MAIN_TYPE    SUB_TYPE        COMMAND  ID
 */

/* List of main type */
#define AIK_MAIN_ENGINE ((uint8_t)0x10)
#define AIK_MAIN_DEV ((uint8_t)0x20)

/* List of sub type */
#define AIK_SUBTYPE_START ((uint8_t)0x00)
#define AIK_SUBTYPE_AUDIO ((uint8_t)0x01)
#define AIK_SUBTYPE_SSP ((uint8_t)0x02)
#define AIK_SUBTYPE_KWS ((uint8_t)0x03)
#define AIK_SUBTYPE_KIT ((uint8_t)0x04)
#define AIK_SUBTYPE_SLAVE ((uint8_t)0x05)
#define AIK_SUBTYPE_END ((uint8_t)0xFF)

#define AIK_GET_MODULE_BY_ID(id) (((id)&0x00FF0000) >> 16)

#define AIK_ID_NULL 0x0
/* ID of KIT */
#define AIK_ID_KIT \
	(((AIK_MAIN_DEV & 0xFF) << 24) | (AIK_SUBTYPE_KIT & 0xFF) << 16)
#define AIK_ID_SLAVE \
	(((AIK_MAIN_DEV & 0xFF) << 24) | (AIK_SUBTYPE_SLAVE & 0xFF) << 16)
#define AIK_ID_SLAVE_SET_CONFIG (AIK_ID_SLAVE | (uint16_t)0x01)

/* ID of Audio */
#define AIK_ID_AUDIO \
	(((AIK_MAIN_DEV & 0xFF) << 24) | (AIK_SUBTYPE_AUDIO & 0xFF) << 16)
/* 设置Audio模块配置，args类型：AudioConfig (ual-aik-config.h) */
#define AIK_ID_AUDIO_SET_CONFIG (AIK_ID_AUDIO | (uint16_t)0x01)

/* ID of SSP */
#define AIK_ID_SSP \
	(((AIK_MAIN_ENGINE & 0xFF) << 24) | ((AIK_SUBTYPE_SSP & 0xFF) << 16))
/* 设置SSP模块配置，args类型：SspConfig (ual-aik-config.h) */
#define AIK_ID_SSP_SET_CONFIG (AIK_ID_SSP | (uint16_t)0x01)
/* 设置BSS状态，仅限内部使用，用户不用关心 */
#define AIK_ID_SSP_SET_BSS_VOICE (AIK_ID_SSP | (uint16_t)0x02)
/* 设置AEC非使能，args类型：int */
#define AIK_ID_SSP_SET_AEC_DISABLE (AIK_ID_SSP | (uint16_t)0x03)
/* 低功耗vad检测，args类型：int 1开启，0关闭 */
#define AIK_ID_SSP_SET_LP_VAD_ON (AIK_ID_SSP | (uint16_t)0x04)

/* ID of KWS */
#define AIK_ID_KWS \
	(((AIK_MAIN_ENGINE & 0xFF) << 24) | ((AIK_SUBTYPE_KWS & 0xFF) << 16))
/* 设置KWS模块配置，args类型：KwsConfig (ual-aik-config.h) */
#define AIK_ID_KWS_SET_CONFIG (AIK_ID_KWS | (uint16_t)0x0001)
/* 设置工作模式为唤醒， args类型：NULL */
#define AIK_ID_KWS_SET_WAKEUP (AIK_ID_KWS | (uint16_t)0x0002)
/* 设置工作模式为识别， args类型：NULL */
#define AIK_ID_KWS_SET_COMMAND (AIK_ID_KWS | (uint16_t)0x0003)
/* 设置超时时间， args类型：int */
#define AIK_ID_KWS_SET_TIMEOUT (AIK_ID_KWS | (uint16_t)0x0004)
/* 清除超时时间 */
#define AIK_ID_KWS_CLEAR_TICKS (AIK_ID_KWS | (uint16_t)0x0005)
/* 更新语法，args类型：char* */
#define AIK_ID_KWS_UPDATE_GRAMMAR (AIK_ID_KWS | (uint16_t)0x0006)
/* 设置工作模式为学习， args类型：NULL */
#define AIK_ID_KWS_SET_STUDY_MODE (AIK_ID_KWS | (uint16_t)0x0007)
/* 获取学习模式的临时语法, args类型：UalAikKwsGetTmpGrammarArgs
 * (ual-aik-args.h)*/
#define AIK_ID_KWS_GET_STUDY_GRAMMAR (AIK_ID_KWS | (uint16_t)0x0008)
/* 学习模式比较临时语法相似度，args类型：UalAikKwsGetGrammarSimilarityArgs,
 * (ual-aik-args.h)*/
#define AIK_ID_KWS_GET_GRAMMAR_COMPARE (AIK_ID_KWS | (uint16_t)0x0009)
/* 学习默斯和合并grammar，args类型：UalAikKwsGetGrammarMergedArgs,
 * (ual-aik-args.h)*/
#define AIK_ID_KWS_GET_GRAMMAR_MERGE (AIK_ID_KWS | (uint16_t)0x000A)

#ifdef __cplusplus
}
#endif

#endif  // TCFG_VOICE_BOX_ENABLE

#endif  // UAL_AIK_ID_H_
