#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_enc.h"
#include "audio_dec.h"
#include "app_main.h"
#include "audio_config.h"
#include "asm/audio_adc.h"
#include "broadcast_enc.h"


#if TCFG_BROADCAST_ENABLE

/* 资源互斥方式配置 */
#if 0
#define BROADCAST_CRITICAL_INIT()
#define BROADCAST_ENTER_CRITICAL()  local_irq_disable()
#define BROADCAST_EXIT_CRITICAL()   local_irq_enable()
#else
#define BROADCAST_CRITICAL_INIT()   spin_lock_init(&ctx->lock)
#define BROADCAST_ENTER_CRITICAL()  spin_lock(&ctx->lock)
#define BROADCAST_EXIT_CRITICAL()   spin_unlock(&ctx->lock)
#endif

#define BROADCAST_ENC_FRAME_LEN		(2048)  //一定要大于编码一帧的数据长度

struct broadcast_enc_context {
    struct audio_encoder encoder;   				//编码句柄
    volatile u8 start;                              //编码运行状态
    s16 output_frame[BROADCAST_ENC_FRAME_LEN / 2];	//align 4Bytes
    int pcm_frame[BROADCAST_ENC_FRAME_LEN / 4];   	//align 4Bytes
    struct broadcast_input input;    			  	//编码数据输入
    struct broadcast_output output;    			  	//编码数据输出
    spinlock_t lock;
};

extern struct audio_encoder_task *encode_task;

/*----------------------------------------------------------------------------*/
/**@brief   编码更新码率接口
   @param   *priv:私有句柄
   @note
*/
/*----------------------------------------------------------------------------*/
void broadcast_enc_bitrate_updata(void *priv, u32 bit_rate)
{
    struct broadcast_enc_context *ctx = (struct broadcast_enc_context *)priv;
    if (bit_rate < 64000) {
        bit_rate = 64000;
    } else if (bit_rate > 320000) {
        bit_rate = 320000;
    }
    audio_encoder_ioctrl(&ctx->encoder, 2, AUDIO_ENCODER_IOCTRL_CMD_UPDATE_BITRATE, bit_rate);
}
/*----------------------------------------------------------------------------*/
/**@brief   外部激活编码接口
   @param   *priv:私有句柄
   @note
*/
/*----------------------------------------------------------------------------*/
void broadcast_enc_kick_start(void *priv)
{
    struct broadcast_enc_context *ctx = (struct broadcast_enc_context *)priv;

    if (!ctx) {
        return;
    }
    BROADCAST_ENTER_CRITICAL();
    if (ctx->start) {
        audio_encoder_resume(&ctx->encoder);
    }
    BROADCAST_EXIT_CRITICAL();
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast编码数据获取接口
   @param    *encoder: 编码器句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    struct broadcast_enc_context *ctx = container_of(encoder, struct broadcast_enc_context, encoder);
    int rlen = 0;
    if (ctx == NULL) {
        return 0;
    }
    rlen = ctx->input.file.fread(ctx->input.stream, ctx->pcm_frame, frame_len);

    if (rlen == frame_len) {
        /*编码读取数据正常*/
    } else if (rlen == 0) {
        /*编码读不到数据会挂起解码，由前级输出数据后激活解码*/
        return 0;
    } else {
        printf("audio_enc end:%d\n", rlen);
        rlen = 0;
    }

    *frame = (s16 *)ctx->pcm_frame;
    return rlen;
}

static void broadcast_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input broadcast_enc_input = {
    .fget = broadcast_enc_pcm_get,
    .fput = broadcast_enc_pcm_put,
};

/*----------------------------------------------------------------------------*/
/**@brief    broadcast编码预处理
   @param    *encoder: 编码器句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_enc_probe_handler(struct audio_encoder *encoder)
{
    struct broadcast_enc_context *ctx = container_of(encoder, struct broadcast_enc_context, encoder);
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast编码数据输出接口
   @param    *encoder: 编码器句柄
   @param    frame:    编码数据
   @param    len: 	   数据长度
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    struct broadcast_enc_context *ctx = container_of(encoder, struct broadcast_enc_context, encoder);
    int wlen = 0;
    if (!ctx || !ctx->start) {
        return 0;
    }
    /* putchar('+'); */
    wlen = ctx->output.output(ctx->output.stream, frame, len);

    return wlen;
}

const static struct audio_enc_handler broadcast_enc_handler = {
    .enc_probe = broadcast_enc_probe_handler,
    .enc_output = broadcast_enc_output_handler,
};

/*----------------------------------------------------------------------------*/
/**@brief    broadcast编码事件处理
   @param    *decoder: 编码器句柄
   @param    argc: 参数个数
   @param    *argv: 参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_enc_event_handler(struct audio_encoder *encoder, int argc, int *argv)
{
    printf("broadcast_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast编码流激活
   @param    *priv: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_enc_resume(void *priv)
{
    struct broadcast_enc_context *ctx = (struct broadcast_enc_context *)priv;
    if (ctx) {
        audio_encoder_resume(&ctx->encoder);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    打开 broadcast 编码
   @param  	 编码参数
   @return   编码私有参数
   @note
*/
/*----------------------------------------------------------------------------*/
void *broadcast_enc_open(struct broadcast_codec_params *params)
{
    struct broadcast_codec_params *broadcast = params;
    int ret;
    struct audio_fmt fmt = {0};
    fmt.coding_type = broadcast->coding_type;
    fmt.sample_rate = broadcast->sample_rate;
    fmt.channel = broadcast->channel;

    switch (broadcast->coding_type) {
        /*jla编解码时，要保证jla的参数与原数据类型保持一致*/
#if TCFG_ENC_JLA_ENABLE
    case AUDIO_CODING_JLA:
        fmt.bit_rate = broadcast->bit_rate;
        fmt.frame_len = broadcast->frame_size;
        break;
#endif
    default:
        printf("do not support this type !!!\n");
        break;
    }

    struct broadcast_enc_context *ctx = zalloc(sizeof(struct broadcast_enc_context));
    ASSERT(ctx);
    BROADCAST_CRITICAL_INIT();

    audio_encoder_task_open();

    audio_encoder_open(&ctx->encoder, &broadcast_enc_input, encode_task);
    audio_encoder_set_handler(&ctx->encoder, &broadcast_enc_handler);
    audio_encoder_set_fmt(&ctx->encoder, &fmt);
    audio_encoder_set_event_handler(&ctx->encoder, broadcast_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&ctx->encoder, ctx->output_frame,
                                   sizeof(ctx->output_frame), 1);
    //配置数据输入输出接口
    memcpy(&ctx->input, &(broadcast->input), sizeof(struct broadcast_input));
    memcpy(&ctx->output, &(broadcast->output), sizeof(struct broadcast_output));

    if (!ctx->encoder.enc_priv) {
        log_e("encoder err, maybe coding(0x%x) disable \n", fmt.coding_type);
        audio_encoder_close(&ctx->encoder);
        free(ctx);
        return NULL;
    }
    clock_add_set(BROADCAST_ENC_CLK);

    ctx->start = 1;
    audio_encoder_start(&ctx->encoder);
    printf("----  audio_encoder_start succ \n");

    return ctx;
}

/*----------------------------------------------------------------------------*/
/**@brief   关闭 broadcast 编码
   @param   私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void broadcast_enc_close(void *priv)
{
    struct broadcast_enc_context *ctx = (struct broadcast_enc_context *)priv;
    if (!ctx) {
        return ;
    }
    BROADCAST_ENTER_CRITICAL();
    ctx->start = 0;
    BROADCAST_EXIT_CRITICAL();
    audio_encoder_close(&ctx->encoder);
    audio_encoder_task_close();
    local_irq_disable();
    free(ctx);
    ctx = NULL;
    local_irq_enable();
    clock_remove_set(BROADCAST_ENC_CLK);
}

#endif/*TCFG_BROADCAST_ENABLE*/
