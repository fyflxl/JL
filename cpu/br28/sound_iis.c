/*****************************************************************
>file name : sound_pcm_iis.c
>create time : Tue 15 Mar 2022 09:00:17 AM CST
*****************************************************************/
#define LOG_TAG     "[SND-IIS]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
/* #define LOG_DUMP_ENABLE */
#include "debug.h"
#include "sound/sound.h"
#include "audio_link.h"
#include "asm/iis.h"

struct sound_iis_context {
    struct sound_pcm_hw_params hw_params;
    struct sound_pcm_map_status status;
    struct sound_pcm_map_fifo   fifo;           /* DAC fifo映射 */
    ALINK_PARM *pd;
    void *hw;
    void *hw_ch;
    u8 alink_ch;
    u8 hw_start;
    u32 sw_ptr;
    u32 hw_ptr;
    struct list_head stream_list;
    OS_MUTEX mutex;
};

#define SOUND_IIS_STREAM_RUNNING(ctx)       (ctx->status.ref_count > 0)

#define substream_to_iis_context(substream)     ((struct sound_iis_context *)substream->private_data)

#define ALINK_FIFO_BUFFERED_FRAMES(hw, ch)         (alink_get_dma_len(hw) - alink_get_shn(ch) - 1)

/*
*********************************************************************
*       sound iis dma fifo sync
* Description: IIS的DMA FIFO与软件的数据同步
* Arguments  : substream     - PCM的子数据流结构
*			   flag          - DMA数据同步的标志，R/W或者其他扩展
* Return	 : 无.
* Note(s)    : 对于复用循环buffer方式的DMA，必须要实现该数据同步才能
*              完成更多多通道复用的情况.
*********************************************************************
*/
static void sound_iis_dma_fifo_sync(struct sound_pcm_substream *substream, u8 flag)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);
    struct sound_pcm_substream *substr1;

    if (flag & DMA_SYNC_R) {
        u32 read_ptr = audio_cfifo_get_hw_ptr(&iis->fifo.cfifo);
        iis->hw_ptr = iis->sw_ptr - ALINK_FIFO_BUFFERED_FRAMES(iis->hw, iis->hw_ch);
        u32 read_frames = iis->hw_ptr - read_ptr;
        list_for_each_entry(substr1, &iis->stream_list, entry) {
            substr1->runtime->hw_ptr += read_frames;
        }
        audio_cfifo_read_update(&iis->fifo.cfifo, read_frames);
    }

    if (flag & DMA_SYNC_W) {
        u32 write_ptr = audio_cfifo_get_sw_ptr(&iis->fifo.cfifo);
        u32 write_frames = write_ptr - iis->sw_ptr;
        alink_set_shn(iis->hw_ch, write_frames);
        if (!iis->hw_start) {
            alink_start(iis->hw);
            iis->hw_start = 1;
        }
        list_for_each_entry(substr1, &iis->stream_list, entry) {
            substr1->runtime->sw_ptr += write_frames;
        }
        iis->sw_ptr += write_frames;
        /*ASSERT(iis->sw_ptr % JL_AUDIO->DAC_LEN == JL_AUDIO->DAC_SWP, " %d, %d\n", iis->sw_ptr, JL_AUDIO->DAC_SWP);*/
    }

}

/*
*********************************************************************
*       sound iis tx irq handler
* Description: IIS的DMA 发送私有中断服务函数
* Arguments  : buf      - DMA发送中断的采样地址(循环buffer下无意义)
*			   len      - DMA中断即将采样的长度(循环buffer下无意义)
*			   priv     - 已注册的私有数据
* Return	 : 无.
* Note(s)    : 中断服务函数关系着数据流的异步唤醒控制以及扩展的状态
*              机应用.
*********************************************************************
*/
static void audio_iis_tx_irq_handler(void *priv, void *buf, u32 len)
{
    struct sound_iis_context *iis = (struct sound_iis_context *)priv;
    struct sound_pcm_substream *substream;
    list_for_each_entry(substream, &iis->stream_list, entry) {
        sound_pcm_substream_irq_handler(substream);
    }

    /* ALINK_CHx_IE(iis->hw, iis->alink_ch, 0); */
    /* ALINK_CLR_CHx_PND(iis->hw, iis->alink_ch); */
    alink_set_ch_ie(iis->hw_ch, 0);
    alink_clr_ch_pnd(iis->hw_ch);
}


/*
*********************************************************************
*       sound iis new context
* Description: 创建iis的驱动目录结构
* Arguments  : device   - 接口的私有数据二级指针
*			   data     - pcm硬件平台数据的数据
* Return	 : 0 - 成功，非0 - 失败.
* Note(s)    : 完成iis结构的创建，完成与alink驱动的关联.
*********************************************************************
*/
static int sound_iis_new_context(void **device, struct sound_pcm_platform_data *data)
{
    struct sound_iis_context *ctx = NULL;
    ctx = (struct sound_iis_context *)zalloc(sizeof(struct sound_iis_context));

    if (!ctx) {
        return -ENOMEM;
    }

    *device = ctx;

    ctx->pd = (ALINK_PARM *)data->private_data;

    ctx->hw_params.channels = 2;
    ctx->hw_params.rates = SOUND_PCM_RATE_8000_48000;
    ctx->hw_params.sample_rate = ctx->pd->sample_rate;
    ctx->hw_params.sample_bits = 16;

    INIT_LIST_HEAD(&ctx->stream_list);

    ctx->alink_ch = 0;
    ctx->hw = alink_init(ctx->pd);
    ctx->hw_ch = alink_channel_init(ctx->hw, ctx->alink_ch, ALINK_DIR_TX, ctx, audio_iis_tx_irq_handler);

    /*DMA fifo init*/
    ctx->fifo.addr = ctx->pd->ch_cfg[ctx->alink_ch].buf;
    ctx->fifo.bytes = ctx->pd->dma_len;
    ctx->fifo.frame_len = ctx->fifo.bytes / 2 / ctx->hw_params.channels;
    ctx->fifo.sync = sound_iis_dma_fifo_sync;

    ALINK_DA2BTSRC_SEL(ctx->hw, ctx->alink_ch);
    printf("[FIFO] : 0x%x, %d, %d\n", ctx->fifo.addr, ctx->fifo.bytes, ctx->fifo.frame_len);
    os_mutex_create(&ctx->mutex);

    return 0;
}

/*
*********************************************************************
*       sound iis free context
* Description: 释放iis的驱动目录结构
* Arguments  : device   - 设备私有数据
* Return	 : 无.
* Note(s)    : None.
*********************************************************************
*/
static void sound_iis_free_context(void *device)
{
    struct sound_iis_context *ctx = (struct sound_iis_context *)device;

    free(ctx);
}

static void sound_iis_set_runtime_params(struct sound_pcm_runtime *runtime, struct sound_iis_context *iis)
{
    runtime->fifo = &iis->fifo;
    runtime->status = &iis->status;
    runtime->channels = iis->hw_params.channels;
    runtime->sample_bits = iis->hw_params.sample_bits;
    runtime->hw_ptr = 0;
    runtime->sw_ptr = 0;
    runtime->hw_ptr_jiffies = 0;
    runtime->mutex = &iis->mutex;
}

/*
*********************************************************************
*       sound iis dma open
* Description: 打开一路IIS的DMA数据流
* Arguments  : substream  -     iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : 设置数据流的iis DMA信息，关联iis驱动.
*********************************************************************
*/
static int sound_iis_dma_open(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    if (!substream->runtime) {
        return -EINVAL;
    }

    sound_iis_set_runtime_params(substream->runtime, iis);

    list_add(&substream->entry, &iis->stream_list);

    return 0;
}


/*
*********************************************************************
*       sound iis dma prepare
* Description: DMA的数据流准备
* Arguments  : substream  -     iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : 多数据流复用iis的情况下仅设置一次硬件即可.
*********************************************************************
*/
static int sound_iis_dma_prepare(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    if (iis->status.state != SOUND_PCM_STATE_PREPARED && iis->status.state != SOUND_PCM_STATE_RUNNING) {
        alink_set_sr(iis->hw, audio_iis_hw_rates_match(iis->hw_params.sample_rate));
        alink_set_hwptr(iis->hw_ch, 0);
        alink_set_swptr(iis->hw_ch, 0);
        alink_set_tx_pns(iis->hw, 1);
        iis->hw_ptr = 0;
        iis->sw_ptr = 0;
        iis->status.state = SOUND_PCM_STATE_PREPARED;

        /*printf("[%s - %d] >>> debug iis, %d.\n", __FUNCTION__, __LINE__, alink_get_shn(iis->hw_ch));*/
    }

    iis->status.ref_count++;

    return 0;
}

static int sound_iis_dma_pointer(struct sound_pcm_substream *substream)
{
    return 0;
}

/*
*********************************************************************
*       sound iis dma stop
* Description: DMA的数据流停止
* Arguments  : substream  -     iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : None.
*********************************************************************
*/
static int sound_iis_dma_stop(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    if (!SOUND_IIS_STREAM_RUNNING(iis)) {
        return 0;
    }

    iis->status.ref_count--;

    if (!SOUND_IIS_STREAM_RUNNING(iis)) {
        iis->status.state = SOUND_PCM_STATE_SUSPENDED;
    }

    return 0;
}

/*
*********************************************************************
*       sound iis dma stop
* Description: DMA的数据流停止
* Arguments  : substream  -     iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : None.
*********************************************************************
*/
static void sound_iis_dma_start(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);
    struct sound_pcm_runtime *runtime = substream->runtime;

    if (iis->status.state == SOUND_PCM_STATE_PREPARED) {
        iis->status.state = SOUND_PCM_STATE_RUNNING;
    }

}

/*
*********************************************************************
*       sound iis dma irq request
* Description: DMA的数据流的中断请求
* Arguments  : substream  -     iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : sound pcm发生中断请求，由这里设置驱动的中断请求.
*********************************************************************
*/
static int sound_iis_dma_irq_request(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);
    struct sound_pcm_runtime *runtime = substream->runtime;

    local_irq_disable();
    u16 pns = runtime->sw_ptr - runtime->dma_irq_ptr;
    if (pns < 1) {
        pns = 1;
    }

    alink_set_tx_pns(iis->hw, pns);
    if (iis->status.state == SOUND_PCM_STATE_RUNNING) {
        alink_set_ch_ie(iis->hw_ch, 1);
    }
    local_irq_enable();

    return 0;
}


/*
*********************************************************************
*       sound iis dma trigger
* Description: DMA的数据流的一些触发动作
* Arguments  : substream    - iis的PCM数据子流
*              cmd          - 触发命令
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : 通常为即时性的操作，可扩展，仅执行命令.
*********************************************************************
*/
static int sound_iis_dma_trigger(struct sound_pcm_substream *substream, int cmd)
{
    int err = 0;

    switch (cmd) {
    case SOUND_PCM_TRIGGER_START:
        sound_iis_dma_start(substream);
        break;
    case SOUND_PCM_TRIGGER_STOP:
        err = sound_iis_dma_stop(substream);
        break;
    case SOUND_PCM_TRIGGER_IRQ:
        err = sound_iis_dma_irq_request(substream);
        break;
    default:
        break;
    }

    return 0;
}

static int sound_iis_get_pcm_hw_params(struct sound_pcm_substream *substream, struct sound_pcm_hw_params *params)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    memcpy(params, &iis->hw_params, sizeof(iis->hw_params));
    if (iis->status.state != SOUND_PCM_STATE_PREPARED && iis->status.state != SOUND_PCM_STATE_RUNNING) {
        params->sample_rate = 0;
    }

    return 0;
}

static int sound_iis_set_pcm_hw_params(struct sound_pcm_substream *substream, struct sound_pcm_hw_params *params)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    if (iis->hw_params.sample_rate != params->sample_rate) {
        log_error("IIS need setup right sample rate %d\n", iis->hw_params.sample_rate);
        iis->hw_params.sample_rate = params->sample_rate;
    }
    return 0;
}

/*
*********************************************************************
*       sound iis pcm ioctl
* Description: PCM设备相关的IO操作接口
* Arguments  : substream    - iis的PCM数据子流
*              cmd          - 触发命令
*              arg          - 参数
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : 通常用来设置/访问一些PCM或硬件特性的参数.
*********************************************************************
*/
static int sound_iis_pcm_ioctl(struct sound_pcm_substream *substream, u32 cmd, void *args)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);
    int err = 0;

    switch (cmd) {
    case SOUND_PCM_GET_HW_PARAMS:
        sound_iis_get_pcm_hw_params(substream, (struct sound_pcm_hw_params *)args);
        break;
    case SOUND_PCM_SET_HW_PARAMS:
        sound_iis_set_pcm_hw_params(substream, (struct sound_pcm_hw_params *)args);
        break;
    case SOUND_PCM_GET_HW_BUFFERED_LEN:
        return ALINK_FIFO_BUFFERED_FRAMES(iis->hw, iis->hw_ch);
    case SOUND_PCM_WAIT_SWN_MOVE:

        break;
    }

    return 0;
}

/*
*********************************************************************
*       sound iis dma close
* Description: 关闭一路IIS的DMA数据流
* Arguments  : substream    - iis的PCM数据子流
* Return	 : 0 - 成功，非0 - 出错.
* Note(s)    : None.
*********************************************************************
*/
static int sound_iis_dma_close(struct sound_pcm_substream *substream)
{
    struct sound_iis_context *iis = substream_to_iis_context(substream);

    list_del(&substream->entry);

    return 0;
}

const struct sound_pcm_ops iis_dma_ops = {
    .open           = sound_iis_dma_open,
    .close          = sound_iis_dma_close,
    .ioctl          = sound_iis_pcm_ioctl,
    .prepare        = sound_iis_dma_prepare,
    .trigger        = sound_iis_dma_trigger,
    .pointer        = sound_iis_dma_pointer,
};

/*
 * sound pcm接口结构注册
 * 对于IIS来说，仅存在DMA，控制器为ALINK驱动
 */
SOUND_PLATFORM_DRIVER(sound_iis_driver) = {
    .name = "iis",
    .ops = &iis_dma_ops,
    /*.controller = &sound_iis_controller,*/
    .create = sound_iis_new_context,
    .free = sound_iis_free_context,
};
