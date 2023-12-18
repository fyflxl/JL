/*
 * Copyright 2020 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#include "system/includes.h"
#include "unisound/ual_aik_config.h"
#include "unisound/ual_aik_event.h"
#include "unisound/ual_aik_id.h"
#include "unisound/ual_aik_mode.h"
#include "unisound/ual_aik.h"

#include "unisound/grammar.h"
#include "unisound/model_183k.h"
//#include "unisound/audio.h"
#include "key_event_deal.h"
#include "avctp_user.h"

#if TCFG_UNISOUND_ENABLE

#define LOG_TAG_CONST       UNISOUND
#define LOG_TAG             "[UNISOUND]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define UNI_MIC_NUM          (2)
#define UNI_AUDIO_CHS        (UNI_MIC_NUM + 1)

/* AIS100 配置，应用可结合自身的配置管理模块来设计结构体 */
typedef struct Ais100Cfg {
    SlaveConfig slave;
    KwsConfig kws;      // asr 模块配置
    SspConfig ssp;      // ssp 模块配置
} Ais100Cfg;

int fd = 0;
FILE *out_file = NULL;
static void SspUploader(int32_t index, int32_t doa, void *data, int32_t len)
{
    // printf("index: %d---\n", index);
    // write(fd, data, len);
    // TRACE(0, "Get ssp data:\n");
    // if(out_file){
    //   fwrite(out_file, data, len);
    //   printf("write len: %d\n", len);
    // }
    return;
}

/* 识别词阈值表 */
static KwsFaValue global_cmd_fa[] = {{0.02, 11.8}, {0.05, 10.85}, {0.1, 10.05}, {0.2, 0.23}, {0.4, -1.5}};
static KwsFaTable global_cmd_table = {
    .nums = sizeof(global_cmd_fa) / sizeof(global_cmd_fa[0]),
    .value = global_cmd_fa
};
/* 定制命令词条（当词表中个别词性能不佳时，可以通过该列表单独调整） */
static KwsCustomCmd global_cmd_custom[] = {{.cmd = "上一首", .fa = 0.2},
    {.cmd = "下一首", .fa = 0.2},
    {.cmd = "暂停播放", .fa = 0.2},
    {.cmd = "播放音乐", .fa = 0.2},
    {.cmd = "增加音量", .fa = 0.2},
    {.cmd = "降低音量", .fa = 0.2}
};
/* 识别功能配置 */
static KwsCmdCfg global_cmd_cfg = {
    .table = &global_cmd_table,
    .fa = 0.2,     // 通用的 FA 一般配置为每十小时 2 次
    .timeout = 10, // 识别超时时间，单位秒，每 10 秒无结果会上报一个 timeout，应用如果不需要超时可以忽略 timeout 事件
    .custom_nums = sizeof(global_cmd_custom) / sizeof(global_cmd_custom[0]),
    .cmd_custom = &global_cmd_custom[0]
};

long uni_get_ms(void)
{
    u32 msec = 0;
    msec = timer_get_ms();
    return msec;
}

static int vad_flag = 0;
static float anker_mic_position[] = {0.0, 0.0, 0.01854f, 0.0};
// static float anker_voice_direction = 170.0f;
static float anker_voice_direction = 0.0f;

// 识别功能配置
static Ais100Cfg global_ais_config = {
    .slave = {
        .chs = UNI_AUDIO_CHS
    },

    /* 降噪模块配置 */
    .ssp = {  							// SSP 相关配置不需要用户修改
        .total_chs = UNI_AUDIO_CHS,     // 输入的总通道数
        .mode = 14,                     // SSP 工作模式
        .mic_orders = {0, 1},           // Mic 数据顺序
        .ref_orders = {2},              // Ref 数据顺序
        .data_uploader = SspUploader
    },// 数据回调，获取降噪后数据

    /* KWS 模块配置 */
    .kws = {
        .am = (char *)acoutstic_model, // 声学模型数据，model_183k.h
        .grammar = (char *)grammar,    // 语法模型数据，grammar.h
        .auto_clear_when_start = 1,    // 启动时重置超时计数
        .auto_clear_when_result = 1,   // 有结果时重置超时计数
        .shared_size = 0,              // 共享内存大小，不需要则配为 0
        .shared_buffer = NULL,         // 共享内存地址指针
        .wkr_cfg = NULL,               // 唤醒功能配置
        .cmd_cfg = &global_cmd_cfg
    }
};

/*----------------------------------------------------------------------------*/
/**@brief    语音识别结果回调函数
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void AisEventCb(AikEvent event, void *args)
{
    static int vad_start, vad_end;
    switch (event) {
    case AIK_EVENT_KWS_COMMAND: {
        AikEventKwsArgs *kws = (AikEventKwsArgs *)args;
        if (strncmp("播放音乐", kws->word, sizeof("播放音乐")) == 0) {
            if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
                app_task_put_key_msg(KEY_MUSIC_PP, 0); //msg:key event ；value:key_value
            }
        } else if (strncmp("暂停播放", kws->word, sizeof("暂停播放")) == 0) {
            if (a2dp_get_status() != BT_MUSIC_STATUS_SUSPENDING) {
                app_task_put_key_msg(KEY_MUSIC_PP, 0); //msg:key event ；value:key_value
            }
        } else if (strncmp("增加音量", kws->word, sizeof("增加音量")) == 0) {
            app_task_put_key_msg(KEY_VOL_UP, 0); //msg:key event ；value:key_value
        } else if (strncmp("降低音量", kws->word, sizeof("降低音量")) == 0) {
            app_task_put_key_msg(KEY_VOL_DOWN, 0); //msg:key event ；value:key_value
        } else if (strncmp("下一首", kws->word, sizeof("下一首")) == 0) {
            app_task_put_key_msg(KEY_MUSIC_NEXT, 0); //msg:key event ；value:key_value
        } else if (strncmp("上一首", kws->word, sizeof("上一首")) == 0) {
            app_task_put_key_msg(KEY_MUSIC_PREV, 0); //msg:key event ；value:key_value
        } else {
            log_info("Not This Command!\n");
        }

        log_info("Get a ASR result:%s, score:%f, start[%.2f s] to  end[%.2f s]\n", kws->word, kws->score, kws->start_ms / 1000.0f, kws->end_ms / 1000.0f);
    }
    break;
    case AIK_EVENT_NONE:
        log_info("AIK_EVENT_NONE\n");
        break;
    case  AIK_EVENT_START:
        log_info("AIK_EVENT_START\n");
        break;
    case  AIK_EVENT_STOP:
        log_info("AIK_EVENT_STOP\n");
        break;
    case  AIK_EVENT_EXIT:
        log_info("AIK_EVENT_EXIT\n");
        break;
    case  AIK_EVENT_KWS_WAKEUP:
        log_info("AIK_EVENT_KWS_WAKEUP\n");
        break;
    case  AIK_EVENT_KWS_TIMEOUT:
        log_info("AIK_EVENT_KWS_TIMEOUT\n");
        break;
    case  AIK_EVENT_HEARTBEAT:
        log_info("AIK_EVENT_HEARTBEAT\n");
        break;
    case  AIK_EVENT_LP_VAD_VOICE:
        log_info("AIK_EVENT_LP_VAD_VOICE\n");
        break;
    default:
        log_info("Nothing\n");
        break;
    }
    return;
}

/*----------------------------------------------------------------------------*/
/**@brief    语音识别相关初始化
   @param
   @return   0:成功， 1:失败
   @note
*/
/*----------------------------------------------------------------------------*/
int uniAsrInit(void)
{
    int err = 0;

    //配置打印等级
    UalAikSetLogLevel(3);

    //配置为从机模式
    UalAikSet(AIK_ID_SLAVE_SET_CONFIG, &global_ais_config.slave);
    //配置SPP模块
    UalAikSet(AIK_ID_SSP_SET_CONFIG, &global_ais_config.ssp);
    //配置KWS模块
    UalAikSet(AIK_ID_KWS_SET_CONFIG, &global_ais_config.kws);

    //注册识别完成的回调函数
    err = UalAikInit(AisEventCb);
    if (err == 0) {
        //配置工作模式为识别模式
        UalAikSet(AIK_ID_KWS_SET_COMMAND, NULL);
        //启动语音识别，识别模式为SSP+KWS(降噪+关键词)
        UalAikStart(AIK_MODE_SSP_KWS);
    }
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    mic数据流识别函数
   @param
   @return
   @note	 识别mic采集到的数据
*/
/*----------------------------------------------------------------------------*/
int uniAsrProcess(void *data, int size)
{
    UalAikUpdate(data, size);
}

/*----------------------------------------------------------------------------*/
/**@brief    关闭语音识别
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
int uniAsrUninit(void)
{
    UalAikStop(AIK_MODE_SSP_KWS);
    UalAikRelease();
}
#endif  //TCFG_VOICE_BOX_ENABLE
