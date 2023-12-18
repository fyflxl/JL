#ifndef _APP_BROADCASE_H
#define _APP_BROADCASE_H

#include "typedef.h"

enum {
    BROADCAST_APP_MODE_ENTER,
    BROADCAST_APP_MODE_EXIT,
    BROADCAST_A2DP_START,
    BROADCAST_A2DP_STOP,
    BROADCAST_PHONE_START,
    BROADCAST_PHONE_STOP,
    BROADCAST_EDR_DISCONN,
    BROADCAST_MUSIC_START,
    BROADCAST_MUSIC_STOP,
};

enum {
    BROADCAST_SYNC_VOL,
    BROADCAST_SYNC_SOFT_OFF,
};

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播开关切换
 *
 * @return -1:切换失败，0:切换成功
 */
/* ----------------------------------------------------------------------------*/
int app_broadcast_switch(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播开启情况下，不同场景的处理流程
 *
 * @return -1:无需处理，0:处理事件但不拦截后续流程，1:处理事件并拦截后续流程
 */
/* ----------------------------------------------------------------------------*/
int app_broadcast_deal(int switch_mode);

/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前是否在退出模式的状态
 *
 * @return 1；是，0：否
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_app_mode_exit_flag(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 更新广播同步状态的数据
 *
 * @param type:更新项
 * @param value:更新值
 *
 * @return -1:fail，0:success
 */
/* ----------------------------------------------------------------------------*/
int update_broadcast_sync_data(u8 type, int value);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播资源初始化
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_init(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播资源卸载
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_uninit(void);

void app_broadcast_enter_pair(u8 role);

void app_broadcast_exit_pair(u8 role);

#endif

