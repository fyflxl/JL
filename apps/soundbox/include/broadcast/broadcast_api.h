/*********************************************************************************************
    *   Filename        : broadcast.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-07-07 14:37

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#ifndef _BROADCASE_API_H
#define _BROADCASE_API_H

#include "app_config.h"
#include "typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
#define SET_SDU_PERIOD_MS   5
#define SET_MTL_TIME        0
#else
#if (JLA_CODING_BIT_RATE > 96000)
#define SET_SDU_PERIOD_MS   20

/*! \MTL配置 */
#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
#define SET_MTL_TIME        0
#else
#define SET_MTL_TIME        (SET_SDU_PERIOD_MS * 1)
#endif /*TCFG_LIVE_AUDIO_LOW_LATENCY_EN*/

#else
#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
#define SET_SDU_PERIOD_MS   5
#define SET_MTL_TIME        0
#else
#define SET_SDU_PERIOD_MS   20

/*! \MTL配置 */
#define SET_MTL_TIME        (SET_SDU_PERIOD_MS * 2)
#endif /*TCFG_LIVE_AUDIO_LOW_LATENCY_EN*/

#endif
#endif

/*! 延时配置 */
/* BIS_SYNC_DELAY + MTL + INTERVAL */
#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
#define BROADCAST_TX_DELAY_TIME      (SET_SDU_PERIOD_MS + SET_MTL_TIME + 4 + (JLA_CODING_CHANNEL / 2))
#else
#define BROADCAST_TX_DELAY_TIME      (SET_SDU_PERIOD_MS + SET_MTL_TIME + 10)
#endif /*TCFG_LIVE_AUDIO_LOW_LATENCY_EN*/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief 广播角色枚举 */
enum {
    BROADCAST_ROLE_UNKNOW,
    BROADCAST_ROLE_TRANSMITTER,
    BROADCAST_ROLE_RECEIVER,
};

//time 类型
enum {
    CURRENT_TIME,
    PACKET_RX_TIME,
};

/*! \brief 状态同步的参数 */
struct broadcast_sync_info {
    u8 volume;
    u8 softoff;
};

/*! \brief 广播编码模块参数配置结构体 */
struct broadcast_enc_params {
    u8 nch;
    u32 coding_type;
    int sample_rate;
    int frame_size;
    int bit_rate;
};

/*! \brief 广播同步模块参数配置结构体 */
struct broadcast_sync_params {
    u8 source;      /*!<  */
    int input_sample_rate;
    int output_sample_rate;
    int nch;
    int delay_time;
};

/*! \brief 广播解码模块参数配置结构体 */
struct broadcast_dec_params {
    u8 nch;
    u32 coding_type;
    int sample_rate;
    int frame_size;
    int bit_rate;
};

/*! \brief 广播同步模块结构体 */
struct broadcast_sync_hdl {
    struct list_head entry; /*!< 同步模块链表项，用于多同步模块管理 */
    u8 big_hdl;         /*!< big句柄，用于big控制接口 */
    u8 channel_num;
    u16 seqn;
    int sample_rate;
    int broadcast_sample_rate;
    u32 tx_sync_time;
    OS_SEM sem;
    void *bcsync;
    void *broadcast_hdl;
};

/*! \brief 广播参数接口结构体，用于广播运行流程，实体由外部流程定义 */
struct broadcast_params_interface  {
    struct broadcast_sync_hdl *sync_hdl;
    struct broadcast_sync_params sync_params;
    struct broadcast_dec_params dec_params;
    struct broadcast_enc_params enc_params;
    int (*start_original)(void);    /*!< 开启原来音频流程 */
    void (*stop_original)(void);    /*!< 停止原来音频流程 */
    int (*start)(void); /*!< 开启广播音频流程 */
    void (*stop)(void); /*!< 停止广播音频流程 */
    u8(*get_status)(void);  /*!< 获取当前音频播放状态 */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/


/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* ***************************************************************************/
/**
 * @brief open broadcast as transmitter
 *
 * @return err:-1, success:available_big_hdl
 */
/* *****************************************************************************/
int broadcast_transmitter(void);

/* ***************************************************************************/
/**
 * @brief open broadcast as receiver
 *
 * @return err:-1, success:available_big_hdl
 */
/* *****************************************************************************/
int broadcast_receiver(void);

/* ***************************************************************************/
/**
 * @brief close broadcast function
 *
 * @param big_hdl:need closed of big_hdl
 */
/* *****************************************************************************/
void broadcast_close(u8 big_hdl);

/* --------------------------------------------------------------------------*/
/**
 * @brief get current broadcast role
 *
 * @return broadcast role
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_role(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief register params and interface of hdl
 *
 * @param ops:params and interface of hdl
 */
/* ----------------------------------------------------------------------------*/
void broadcast_params_interface_register(struct broadcast_params_interface *ops);

/* --------------------------------------------------------------------------*/
/**
 * @brief init broadcast params
 */
/* ----------------------------------------------------------------------------*/
void broadcast_init(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief uninit broadcast params
 */
/* ----------------------------------------------------------------------------*/
void broadcast_uninit(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 设置需要同步的状态数据
 *
 * @param big_hdl:big句柄
 * @param data:数据buffer
 * @param length:数据长度
 *
 * @return -1:fail，0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_set_sync_data(u8 big_hdl, void *data, size_t length);

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化同步的状态数据的内容
 *
 * @param data:用来同步的数据
 */
/* ----------------------------------------------------------------------------*/
void broadcast_sync_data_init(struct broadcast_sync_info *data);

int broadcast_enter_pair(u8 role);

int broadcast_exit_pair(u8 role);

void app_broadcast_config();

#ifdef __cplusplus
};
#endif

#endif //_BROADCASE_API_H

