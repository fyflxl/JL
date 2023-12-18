
/****************************************************************/
/* Copyright:(c)JIELI  2011-2022 All Rights Reserved.           */
/****************************************************************/
/*
>file name : drift_compensation.h
>create time : Mon 13 Jun 2022 05:32:59 PM CST
*****************************************************************/
#ifndef _DRIFT_COMPENSATION_H_
#define _DRIFT_COMPENSATION_H_

#include "typedef.h"
#define CLOCK_TIME_US       0   /*微秒时钟单位*/
#define CLOCK_TIME_MS       1   /*毫秒时钟单位*/

struct pcm_sample_params {
    u8 unit;            /*时间单位*/
    u8 nch;             /*采样通道数*/
    s16 factor;         /*放大系数，默认可填0*/
    int sample_rate;    /*初始采样率*/
    int period;         /*漂移检测周期，和时间单位对应*/
};

/***********************************************************
 *      audio drift compensation open
 * description  : 打开音频采样漂移补偿
 * arguments    : params        -   漂移检测相关的初始化配置
 * return       : NULL      -   failed,
 *                非NULL    -   private_data
 * notes        : None.
 ***********************************************************/
void *audio_drift_compensation_open(struct pcm_sample_params *params);

/***********************************************************
 *      audio drift samples to reference clock
 * description  : 音频采样相对于参考时钟的漂移检测
 * arguments    : s             -   漂移补偿私有结构
 *                len           -   单次采样长度(bytes)
 *                time          -   采样时刻的时钟
 * return       : 漂移的采样率
 * notes        : None.
 ***********************************************************/
int audio_drift_samples_to_reference_clock(void *s, int len, u32 time);

/***********************************************************
 *      audio drift compensation sample rate
 * description  : 直接获取漂移采样率
 * arguments    : s             -   漂移补偿私有结构
 * return       : 漂移的采样率
 * notes        : 无需传递采样参数.
 ***********************************************************/
int audio_drift_compensation_sample_rate(void *s);

/***********************************************************
 *      audio drift compensation close
 * description  : 关闭音频采样漂移补偿
 * arguments    : s             -   漂移补偿私有结构
 * return       : None.
 * notes        : None.
 ***********************************************************/
void audio_drift_compensation_close(void *s);

#endif
