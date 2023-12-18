/****************************************************************/
/* Copyright:(c)JIELI  2011-2022 All Rights Reserved.           */
/****************************************************************/
/*
>file name : broadcast_audio_sync.c
>create time : Fri 10 Jun 2022 04:01:52 PM CST
*****************************************************************/
#include "system/includes.h"
#include "broadcast_audio_sync.h"
#include "media/drift_compensation.h"
#include "media/delay_compensation.h"
#include "media/audio_syncts.h"
#include "broadcast_api.h"

#define DMA_INPUT_TO_TASK_MSG           1

#define BC_AUDIO_SYNC_MSG_DMA           0
#define BC_AUDIO_SYNC_MSG_STOP          1

DEFINE_ATOMIC(task_used);

struct broadcast_tx_sync_context {
    u8 remain;
    u8 new_frame;
    u8 dma_start;
    int sample_rate;
    void *drift_compensation;
    void *delay_compensation;
    void *syncts;
    struct broadcast_audio_stream stream;
    u32 tick_time;
    s16 *dma_data;
    spinlock_t lock;
    void *resume_priv;
    void (*block_resume)(void *resume_priv);
};

void broadcast_audio_sync_update_source_fmt(void *bcsync, int sample_rate, u8 channel_num)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)bcsync;
    if (!ctx) {
        return;
    }
    ctx->sample_rate = sample_rate;
    ctx->stream.nch = channel_num;
    ctx->stream.sample_rate = sample_rate;
    audio_syncts_update_sample_rate(ctx->syncts, ctx->sample_rate);
}

void broadcast_audio_sync_set_resume_callback(void *bcsync, void *priv, void (*resume)(void *priv))
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)bcsync;
    if (!ctx) {
        return;
    }
    ctx->resume_priv = priv;
    ctx->block_resume = resume;

}

void broadcast_audio_sync_Kick_start(void *bcsync)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)bcsync;
    if (!ctx) {
        return;
    }

    if (ctx->block_resume) {
        ctx->block_resume(ctx->resume_priv);
    }
}


static int audio_input_drift_compensation_init(struct broadcast_tx_sync_context *ctx)
{
    if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
        struct pcm_sample_params params;
        params.sample_rate = ctx->stream.sample_rate;
        params.unit = CLOCK_TIME_US;
        params.factor = 1;
        params.period = 1000000; //1秒
        params.nch = ctx->stream.nch;
        ctx->drift_compensation = audio_drift_compensation_open(&params);
    }

    return 0;
}

static void audio_input_drift_compensation_free(struct broadcast_tx_sync_context *ctx)
{
    if (ctx->drift_compensation) {
        audio_drift_compensation_close(ctx->drift_compensation);
        ctx->drift_compensation = NULL;
    }
}

static int audio_stream_syncts_data_handler(void *priv, void *data, int len)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)priv;

    return ctx->stream.output(ctx->stream.priv, data, len);
}

static int audio_stream_syncts_init(struct broadcast_tx_sync_context *ctx)
{
    struct audio_syncts_params params;

    params.network = AUDIO_NETWORK_LOCAL;
    params.pcm_device = PCM_TRANSMIT;
    params.factor = 1;
    params.nch = ctx->stream.nch;
    params.rin_sample_rate = ctx->stream.sample_rate;
    params.rout_sample_rate = ctx->stream.broadcast_sample_rate;
    params.priv = ctx;
    params.output = audio_stream_syncts_data_handler;
    return audio_syncts_open(&ctx->syncts, &params);
}

static void audio_stream_syncts_free(struct broadcast_tx_sync_context *ctx)
{
    if (ctx->syncts) {
        audio_syncts_close(ctx->syncts);
        ctx->syncts = NULL;
    }
}

/***********************************************************
 * broadcast delay compensation init
 * description  : 广播延时控制器的初始
 * arguments    : ctx   -   广播音频同步的总数据结构
 * return       : 0     -   sucess,
 *                非0   -   failed
 * notes        : 应用于广播方案的延时控制策略
 ***********************************************************/
static int audio_stream_delay_compensation_init(struct broadcast_tx_sync_context *ctx)
{
    if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
        ctx->delay_compensation = audio_delay_compensation_open(ctx->stream.delay_time, ctx->sample_rate, PERIOD_DELAY_MODE);
    } else if (ctx->stream.source == BROADCAST_SOURCE_BT_BLOCK) {
        ctx->delay_compensation = audio_delay_compensation_open(ctx->stream.delay_time, ctx->sample_rate, AVERAGE_DELAY_MODE);
    }

    return 0;
}

static void audio_stream_delay_compensation_free(struct broadcast_tx_sync_context *ctx)
{
    if (ctx->delay_compensation) {
        audio_delay_compensation_close(ctx->delay_compensation);
        ctx->delay_compensation = NULL;
    }
}

static int broadcast_audio_sync_data_handler(void *_ctx, void *data, int len, u32 time)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)_ctx;
    int sample_rate = ctx->sample_rate;
    int sample_rate_offset = 0;

    /*如果需要漂移补偿，则在此进行漂移补偿后的采样率获取*/
    if (ctx->drift_compensation) {
        sample_rate = audio_drift_samples_to_reference_clock(ctx->drift_compensation, len, time);
    }

    /*延时补偿*/
    if (ctx->delay_compensation) {
        int buffered_time_us = 0;
        int delay_time = 0;
        if (ctx->stream.source == BROADCAST_SOURCE_DMA && ctx->tick_time) {
            spin_lock(&ctx->lock);
            buffered_time_us = ((len >> 1) / ctx->stream.nch * 1000000) / ctx->sample_rate + sound_buffered_between_syncts_and_device(ctx->syncts, 1);
            delay_time = (buffered_time_us + (ctx->tick_time - time)) / 1000;
            if (ctx->tick_time && __builtin_abs(delay_time) < (0x1ffffff / 1000)) { //需要考虑时钟溢出的情况
                sample_rate_offset = audio_delay_compensation_detect(ctx->delay_compensation, delay_time);
                sample_rate += sample_rate_offset;
                if (ctx->sample_rate != sample_rate) {
                    /*printf("<%d, %d, %d, %d>\n", delay_time, buffered_time_us, ctx->tick_time, time);*/
                }
            }
            spin_unlock(&ctx->lock);
        } else if (ctx->stream.source == BROADCAST_SOURCE_BT_BLOCK) {
            if (ctx->new_frame) {
                buffered_time_us = time * 1000 + sound_buffered_between_syncts_and_device(ctx->syncts, 1);
                delay_time = (buffered_time_us - (ctx->stream.clock_time(ctx->stream.clock_data, CURRENT_TIME) - ctx->tick_time)) / 1000;
                if (ctx->tick_time && __builtin_abs(delay_time) < (0x1ffffff / 1000)) { //需要考虑时钟溢出的情况
                    sample_rate_offset = audio_delay_compensation_detect(ctx->delay_compensation, delay_time);
                    if (sample_rate_offset) {
                        sample_rate = ctx->stream.sample_rate + sample_rate_offset;
                    }
                }
            }
        }
    }

    /*变采样*/
    if (ctx->syncts) {
        if (ctx->sample_rate != sample_rate) {
            /*printf("---->>> sample rate : %d, %d-----\n", ctx->sample_rate, sample_rate);*/
            audio_syncts_update_sample_rate(ctx->syncts, sample_rate);
            ctx->sample_rate = sample_rate;
        }
        if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
            int remain_len = len;
            int wlen = 0;
            while (remain_len) {
                wlen = audio_syncts_frame_filter(ctx->syncts, data, remain_len);
                if (wlen == 0) {
                    break;
                }
                data = (u8 *)data + wlen;
                remain_len -= wlen;
            }
            audio_syncts_push_data_out(ctx->syncts);
        } else {
            int wlen = 0;
            wlen = audio_syncts_frame_filter(ctx->syncts, data, len);
            if (wlen < len) {
                audio_syncts_trigger_resume(ctx->syncts, ctx->resume_priv, ctx->block_resume);
            }
            return wlen;
        }
    }

    return len;
}

static void bc_audio_sync_task(void *arg)
{
    int msg[16];
    int res;
    u8 pend_taskq = 1;

    while (1) {
        res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));

        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case BC_AUDIO_SYNC_MSG_DMA:
                broadcast_audio_sync_data_handler((void *)msg[2], (void *)msg[3], msg[4], (u32)msg[5]);
                break;
            case BC_AUDIO_SYNC_MSG_STOP:
                os_sem_post((OS_SEM *)msg[2]);
                break;
            }
        }
    }
}

void *broadcast_audio_sync_open(struct broadcast_audio_stream *stream)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)zalloc(sizeof(struct broadcast_tx_sync_context));

    memcpy(&ctx->stream, stream, sizeof(struct broadcast_audio_stream));

    ctx->sample_rate = ctx->stream.sample_rate;

    audio_input_drift_compensation_init(ctx);

    audio_stream_delay_compensation_init(ctx);

    audio_stream_syncts_init(ctx);

    spin_lock_init(&ctx->lock);

    if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
        if (atomic_add_return(1, &task_used) == 1) {
            ctx->dma_data = NULL;
            int err = task_create(bc_audio_sync_task, NULL, ctx->stream.task);
            if (err) {
                printf("!!broadcast up audio sync task create failed.\n");
            }
        }
    }
    return ctx;
}

void broadcast_audio_sync_dump_info(void *basync)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)basync;

    printf("buffered time : %dus\n", sound_buffered_between_syncts_and_device(ctx->syncts, 1));
}

int broadcast_audio_sync_dma_input(void *basync, void *data, int len)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)basync;

    u32 time = 0;
    if (!ctx->dma_start) {
        return 0;
    }
    if (!ctx->dma_data) {
        ctx->dma_data = zalloc(len);
    }
    ASSERT(ctx->dma_data, "%s,%d, malloc err !!!\n", __func__, __LINE__);
    memcpy(ctx->dma_data, data, len);
    if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
        //TODO
        spin_lock(&ctx->lock);
        time = ctx->stream.clock_time(ctx->stream.clock_data, CURRENT_TIME);
        spin_unlock(&ctx->lock);
    }

#if DMA_INPUT_TO_TASK_MSG
    int ret = os_taskq_post_msg(ctx->stream.task, 5, BC_AUDIO_SYNC_MSG_DMA, ctx, ctx->dma_data, len, time);
    if (ret) {
        r_printf("dma input error\n");
    }
#else

#endif
    return len;
}

void broadcast_audio_sync_trigger_start(void *basync)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)basync;

    ctx->dma_start = 1;
}

void broadcast_audio_update_tx_num(void *basync, int frames)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)basync;

    if (ctx->syncts) {
        spin_lock(&ctx->lock);
        sound_pcm_update_frame_num(ctx->syncts, frames);
        ctx->tick_time = ctx->stream.clock_time(ctx->stream.clock_data, PACKET_RX_TIME);
        spin_unlock(&ctx->lock);
    }
}

int broadcast_audio_sync_block_input(void *basync, void *data, int len)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)basync;
    int buffer_delay = 0;

    if (ctx->stream.source == BROADCAST_SOURCE_BT_BLOCK) {
        buffer_delay = ctx->stream.buffered_time(ctx->stream.private_input);
        ctx->new_frame = buffer_delay >= 0 ? 1 : 0;
    }
    int wlen = broadcast_audio_sync_data_handler(ctx, data, len, buffer_delay);
    return wlen;
}

void broadcast_audio_sync_close(void *priv)
{
    struct broadcast_tx_sync_context *ctx = (struct broadcast_tx_sync_context *)priv;

    if (ctx->stream.source == BROADCAST_SOURCE_DMA) {
        if (atomic_sub_return(1, &task_used) == 0) {
            OS_SEM *sem = (OS_SEM *)malloc(sizeof(OS_SEM));
            os_sem_create(sem, 0);
            os_taskq_post_msg(ctx->stream.task, 2, BC_AUDIO_SYNC_MSG_STOP, (int)sem);
            os_sem_pend(sem, 0);
            free(sem);
            task_kill(ctx->stream.task);
            if (ctx->dma_data) {
                free(ctx->dma_data);
                ctx->dma_data = NULL;
            }
        }
    }

    audio_input_drift_compensation_free(ctx);
    audio_stream_delay_compensation_free(ctx);
    audio_stream_syncts_free(ctx);
    if (ctx) {
        free(ctx);
    }
}

