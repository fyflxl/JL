/****************************************************************/
/* Copyright:(c)JIELI  2011-2022 All Rights Reserved.           */
/****************************************************************/
/*
>file name : delay_compensation.h
>create time : Mon 13 Jun 2022 05:33:17 PM CST
>description :
    1)一个实时流的传输/播放系统中，延时是保证数据流稳定的基本要素
      之一;
    2)delay compensation提供延迟统计后的比较，并基于参考采样率给定
      延时补偿的采样率偏移值，来满足调用者实现通过变采样达到补偿延
      时的目的。
*****************************************************************/
#ifndef _DELAY_COMPENSATION_H_
#define _DELAY_COMPENSATION_H_

#define PERIOD_DELAY_MODE       0   /*定时周期采样的延时统计方式*/
#define AVERAGE_DELAY_MODE      1   /*不定周期实时流的均值延时统计方式*/

/***********************************************************
 *      audio delay compensation open
 * description  : 延时补偿初始化设置
 * arguments    : reference_time    -   目标参考延时
 *                sample_rate       -   参考采样率
 *                mode              -   延时的统计方式
 * return       : NULL      -   failed,
 *                非NULL    -   private_data
 * notes        : None.
 ***********************************************************/
void *audio_delay_compensation_open(int reference_time, int sample_rate, int mode);

/***********************************************************
 *      audio delay compensation detect
 * description  : 根据当前延时检测偏移
 * arguments    : dc            -   延时补偿私有结构
 *                delay_time    -   当前延时
 * return       : 基于采样率需要补偿的采样率偏移
 * notes        : None.
 ***********************************************************/
int audio_delay_compensation_detect(void *dc, int delay_time);

/***********************************************************
 *      audio delay compensation close
 * description  : 关闭延时补偿检测
 * arguments    : dc            -   延时补偿私有结构
 * return       : None.
 * notes        : None.
 ***********************************************************/
void audio_delay_compensation_close(void *dc);


#endif
