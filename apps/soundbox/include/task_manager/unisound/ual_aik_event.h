/*
 * Copyright 2020 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#ifndef UAL_AIK_EVENT_H_
#define UAL_AIK_EVENT_H_

#include "app_config.h"

#if TCFG_UNISOUND_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(4)
/* WARNING: 修改该文件后必须同步修改 jni/AikEvent.java! */
typedef enum AikEvent {
    AIK_EVENT_NONE = 0,
    AIK_EVENT_START,
    AIK_EVENT_STOP,
    AIK_EVENT_EXIT,
    /* 离线唤醒结果，args：type of AikEventKwsArgs */
    AIK_EVENT_KWS_WAKEUP = 10,
    /* 离线识别结果 */
    AIK_EVENT_KWS_COMMAND,
    /* 离线识别超时，args：NULL */
    AIK_EVENT_KWS_TIMEOUT,
    /* 心跳，Master 模式上报，2s 一次 */
    AIK_EVENT_HEARTBEAT,
    /* 学习事件结果 */
    AIK_EVENT_KWS_STUDY,
    /* 低功耗 VAD 检测到人声事件 */
    AIK_EVENT_LP_VAD_VOICE,
    /* 测试模式 */
    AIK_EVENT_TEST_END = 999,
    /* END */
    AIK_EVENT_END
} AikEvent;

/* JNI 接口会将 args 转换成 cJSON */
typedef struct AikEventKwsArgs {
    const char *word;
    int start_ms;
    int end_ms;
    int kws_index;
    int is_oneshot;
    double score;
} AikEventKwsArgs;

typedef struct AikEventVadArgs {
    int status;     // 0 表示无人声，1 表示有人声
    int timestamp;  // 单位为 ms，init 之后处理的录音长度
} AikEventVadArgs;
#pragma pack()

#ifdef __cplusplus
}
#endif

#endif  // TCFG_VOICE_BOX_ENABLE

#endif  // UAL_AIK_EVENT_H_
