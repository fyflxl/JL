/*
 * 模块：
 * 1、MIC
 *		demo_mic_open();
 *		demo_mic_close();
 * 2、ENC
 *		demo_enc_open();
 *		demo_enc_close();
 * 3、DEC
 *		demo_dec_open();
 *		demo_dec_close();
 *
 * 数据交互：
 * 1、mic采集数据写入cbuf中；然后调用demo_mic_out_set_resume_cb()注册的回调
 * 2、enc通过demo_read_mic_data()获取mic数据；
 * 3、enc编码后数据写入cbuf中；然后调用demo_enc_out_set_resume_cb()注册的回调
 * 4、dec通过demo_read_enc_data()获取编码后的数据；
 * 5、dec解码后数据流传给mixer节点，mixer再传给dac等
 *
 */

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_enc.h"
#include "audio_dec.h"
#include "app_main.h"
#include "audio_config.h"
#include "asm/audio_adc.h"


/*
 *  选择编解码类型
 *	注意：jla编解码时，要保证jla的参数与源数据类型保持一致,在板级进行修改
 */

#define  TEST_CODING_TYPE  AUDIO_CODING_JLA
/* #define  TEST_CODING_TYPE  AUDIO_CODING_SPEEX */
/* #define  TEST_CODING_TYPE  AUDIO_CODING_OPUS */

//-----------------------------------------------------------------------------
// mic
//-----------------------------------------------------------------------------

#define DEMO_MIC_PCM_BUF_LEN		(2*1024)

struct demo_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch    mic_ch;
    int pcm_buf[DEMO_MIC_PCM_BUF_LEN / 4];
    cbuffer_t pcm_cbuf;
    volatile u8 start;
    u32 lost;
    void (*out_resume)(void *);
    void *out_resume_priv;
};

static struct demo_mic_hdl *mic_hdl = NULL;

static void mic_output_data(void *priv, s16 *data, int len)
{
    if (!mic_hdl || !mic_hdl->start) {
        return ;
    }
    int wlen = cbuf_write(&mic_hdl->pcm_cbuf, data, len);
    if (!wlen) {
        mic_hdl->lost++;
        putchar('~');
    }
    if (mic_hdl->out_resume) {
        mic_hdl->out_resume(mic_hdl->out_resume_priv);
    }
}


void demo_mic_close(void)
{
    if (!mic_hdl) {
        return ;
    }
    mic_hdl->start = 0;
    audio_mic_close(&mic_hdl->mic_ch, &mic_hdl->adc_output);
    local_irq_disable();
    free(mic_hdl);
    mic_hdl = NULL;
    local_irq_enable();
}

int demo_mic_open(int sample_rate, int gain)
{
    demo_mic_close();

    struct demo_mic_hdl *mic = zalloc(sizeof(struct demo_mic_hdl));
    ASSERT(mic);

    cbuf_init(&mic->pcm_cbuf, mic->pcm_buf, DEMO_MIC_PCM_BUF_LEN);
    mic->adc_output.handler = mic_output_data;
    mic->adc_output.priv    = mic;
    if (audio_mic_open(&mic->mic_ch, sample_rate, gain) == 0) {
        audio_mic_add_output(&mic->adc_output);
        mic_hdl = mic;
        audio_mic_start(&mic->mic_ch);
        mic->start = 1;
    } else {
        free(mic);
        return -EINVAL;
    }
    return 0;
}

int demo_read_mic_data(void *buf, int len)
{
    if (!mic_hdl || !mic_hdl->start) {
        return 0;
    }
    int rlen = cbuf_read(&mic_hdl->pcm_cbuf, buf, len);
    return rlen;
}

int demo_mic_out_set_resume_cb(void (*resume)(void *), void *resume_priv)
{
    local_irq_disable();
    if (mic_hdl) {
        mic_hdl->out_resume_priv = resume_priv;
        mic_hdl->out_resume = resume;
    }
    local_irq_enable();
    return 0;
}



//-----------------------------------------------------------------------------
// enc
//-----------------------------------------------------------------------------

#define DEMO_ENC_BUF_LEN		(4*1024)

//jla编码读数据时要根据参数改大buf(sampe_rate *  一帧编码的时间*2*声道数)
//(JLA_CODING_SAMPLERATE  * JLA_CODING_FRAME_LEN/10  * 2字节/点   *  JLA_CODING_CHANNEL)
#define DEMO_ENC_FRAME_LEN		(640) //根据不同的编码每次读取的长度不同进行调整

struct demo_enc_hdl {
    struct audio_encoder encoder;
    OS_SEM pcm_frame_sem;
    int enc_buf[DEMO_ENC_BUF_LEN / 4];
    cbuffer_t enc_cbuf;
    volatile u8 start;
    s16 output_frame[DEMO_ENC_FRAME_LEN / 2];	//align 4Bytes
    int pcm_frame[DEMO_ENC_FRAME_LEN / 4];   	//align 4Bytes
    void (*out_resume)(void *);
    void *out_resume_priv;
};

static struct demo_enc_hdl *enc_hdl = NULL;

extern struct audio_encoder_task *encode_task;

static int demo_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    if (enc_hdl == NULL) {
        return 0;
    }
    /* printf("frame_len:%d \n", frame_len); */

    while (enc_hdl->start) {
        rlen = demo_read_mic_data(enc_hdl->pcm_frame, frame_len);

        if (rlen == frame_len) {
            /*编码读取数据正常*/
            break;
        } else if (rlen == 0) {
            /*编码读不到数，pend住*/
            os_sem_set(&enc_hdl->pcm_frame_sem, 0);
            int ret = os_sem_pend(&enc_hdl->pcm_frame_sem, 5);
            if (ret == OS_TIMEOUT) {
                r_printf("demo_enc pend timeout\n");
                break;
            }
        } else {
            printf("audio_enc end:%d\n", rlen);
            rlen = 0;
            break;
        }
    }

    *frame = enc_hdl->pcm_frame;
    return rlen;
}
static void demo_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input demo_enc_input = {
    .fget = demo_enc_pcm_get,
    .fput = demo_enc_pcm_put,
};

static int demo_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}
static int demo_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    if (!enc_hdl || !enc_hdl->start) {
        return 0;
    }
    /* printf("enc olen:%d \n", len); */
    /* put_buf(frame, len); */


    int wlen = cbuf_write(&enc_hdl->enc_cbuf, frame, len);
    if (enc_hdl->out_resume) {
        enc_hdl->out_resume(enc_hdl->out_resume_priv);
    }
    return wlen;

}

const static struct audio_enc_handler demo_enc_handler = {
    .enc_probe = demo_enc_probe_handler,
    .enc_output = demo_enc_output_handler,
};

static void demo_enc_event_handler(struct audio_encoder *encoder, int argc, int *argv)
{
    printf("demo_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}

void demo_enc_close(void)
{
    if (!enc_hdl) {
        return ;
    }
    enc_hdl->start = 0;
    os_sem_post(&enc_hdl->pcm_frame_sem);
    audio_encoder_close(&enc_hdl->encoder);
    audio_encoder_task_close();
    local_irq_disable();
    free(enc_hdl);
    enc_hdl = NULL;
    local_irq_enable();
}

int demo_enc_open(int sample_rate, u8 ch_num, u32 code_type, u32 code_param)
{
    demo_enc_close();

    struct audio_fmt fmt = {0};
    fmt.sample_rate = sample_rate;
    fmt.channel = ch_num;
    fmt.coding_type = code_type;
    switch (code_type) {
#if TCFG_ENC_SPEEX_ENABLE
    case AUDIO_CODING_SPEEX:
        fmt.quality = 4; // 4-8kbps; 5-12kbps。需对应修改speex_max_framelen
        fmt.complexity = 2;
        break;
#endif

#if TCFG_ENC_OPUS_ENABLE
    case AUDIO_CODING_OPUS:
        fmt.quality = 0 | code_param;
        break;
#endif

        /*jla编解码时，要保证jla的参数与原数据类型保持一致*/
#if TCFG_ENC_JLA_ENABLE
    case AUDIO_CODING_JLA:
        fmt.bit_rate = 64000;
        fmt.sample_rate = JLA_CODING_SAMPLERATE;
        fmt.frame_len = JLA_CODING_FRAME_LEN;
        fmt.channel = JLA_CODING_CHANNEL;
        break;
#endif
    default:
        printf("do not support this type !!!\n");
        while (1);
        break;
    }

    struct demo_enc_hdl *enc = zalloc(sizeof(struct demo_enc_hdl));
    ASSERT(enc);

    cbuf_init(&enc->enc_cbuf, enc->enc_buf, DEMO_ENC_BUF_LEN);

    audio_encoder_task_open();

    os_sem_create(&enc->pcm_frame_sem, 0);
    audio_encoder_open(&enc->encoder, &demo_enc_input, encode_task);
    audio_encoder_set_handler(&enc->encoder, &demo_enc_handler);
    audio_encoder_set_fmt(&enc->encoder, &fmt);
    audio_encoder_set_event_handler(&enc->encoder, demo_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&enc->encoder, enc->output_frame,
                                   sizeof(enc->output_frame), 1);

    if (!enc->encoder.enc_priv) {
        log_e("encoder err, maybe coding(0x%x) disable \n", fmt.coding_type);
        audio_encoder_close(&enc->encoder);
        free(enc);
        return -EINVAL;
    }

    enc_hdl = enc;
    printf(">> audio_encoder_start\n");

    audio_encoder_start(&enc->encoder);
    enc->start = 1;
    printf(">> audio_encoder_start  succ\n");

    return 0;
}

int demo_read_enc_data(void *buf, int len)
{
    if (!enc_hdl || !enc_hdl->start) {
        return 0;
    }
    int rlen = cbuf_read(&enc_hdl->enc_cbuf, buf, len);
    /* printf("encr:%d, %d \n", len, rlen); */
    return rlen;
}

int demo_get_enc_data_len(void)
{
    if (!enc_hdl || !enc_hdl->start) {
        return 0;
    }
    int len = cbuf_get_data_len(&enc_hdl->enc_cbuf);
    return len;
}

int demo_enc_out_set_resume_cb(void (*resume)(void *), void *resume_priv)
{
    local_irq_disable();
    if (enc_hdl) {
        enc_hdl->out_resume_priv = resume_priv;
        enc_hdl->out_resume = resume;
    }
    local_irq_enable();
    return 0;
}

void demo_enc_resume(void *priv)
{
    if (enc_hdl) {
        os_sem_post(&enc_hdl->pcm_frame_sem);
        audio_encoder_resume(&enc_hdl->encoder);
    }
}


//-----------------------------------------------------------------------------
// dec
//-----------------------------------------------------------------------------

struct demo_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;		// 资源等待句柄
    struct audio_mixer_ch mix_ch;	// 叠加句柄
    struct audio_stream *stream;	// 音频流
    struct audio_fmt in_fmt;
    struct audio_dec_input dec_input;
    volatile u8 start;
};

static struct demo_dec_hdl *dec_hdl = NULL;


static int demo_dec_file_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    struct demo_dec_hdl *dec = container_of(decoder, struct demo_dec_hdl, decoder);
    u32 wait_timeout = jiffies + msecs_to_jiffies(200);
    int rlen;

__retry_read:
    rlen = demo_read_enc_data(buf, len);
    if (!rlen) {
        if (dec->start && time_before(jiffies, wait_timeout)) {
            os_time_dly(1);
            goto __retry_read;
        }
        printf("read data err \n");
    }
    return rlen;
}

static int demo_dec_file_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    return 0;
}

static int demo_dec_file_flen(struct audio_decoder *decoder)
{
    return 0x7fffffff;
}

static const struct audio_dec_input demo_dec_file_input = {
    .coding_type = AUDIO_CODING_SPEEX,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = demo_dec_file_fread,
            .fseek = demo_dec_file_fseek,
            .flen  = demo_dec_file_flen,
        }
    }
};


// 解码预处理
static int demo_dec_probe_handler(struct audio_decoder *decoder)
{
    return 0;
}

// 解码后处理
static int demo_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

//解码后输出
static int demo_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct demo_dec_hdl *dec = container_of(decoder, struct demo_dec_hdl, decoder);
    char err = 0;
    int rlen = len;
    int wlen = 0;

    do {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        if (!wlen) {
            err++;
            if (err < 2) {
                continue;
            }
            break;
        }
        err = 0;
        data += wlen / 2;
        rlen -= wlen;
    } while (rlen > 0);

    return len - rlen;
}

static const struct audio_dec_handler demo_dec_handler = {
    .dec_probe  = demo_dec_probe_handler,
    .dec_post   = demo_dec_post_handler,
#ifndef CONFIG_MEDIA_DEVELOP_ENABLE
    .dec_output = demo_dec_output_handler,
#endif

};


// 解码释放
static void demo_dec_release(void)
{
    // 删除解码资源等待
    audio_decoder_task_del_wait(&decode_task, &dec_hdl->wait);
    /* clock_remove(DEC_SBC_CLK); */
    // 释放空间
    local_irq_disable();
    free(dec_hdl);
    dec_hdl = NULL;
    local_irq_enable();
}

// 解码关闭
static void demo_dec_audio_res_close(void)
{
    if (dec_hdl->start == 0) {
        printf("dec_hdl->start == 0");
        return ;
    }

    // 关闭数据流节点
    dec_hdl->start = 0;
    audio_decoder_close(&dec_hdl->decoder);
    audio_mixer_ch_close(&dec_hdl->mix_ch);

    // 先关闭各个节点，最后才close数据流
#if defined(CONFIG_MEDIA_DEVELOP_ENABLE)
    if (dec_hdl->stream) {
        audio_stream_close(dec_hdl->stream);
        dec_hdl->stream = NULL;
    }
#endif
    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);
}

// 解码事件处理
static void demo_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        printf("AUDIO_DEC_EVENT_END\n");
        void demo_dec_close(void);
        demo_dec_close();
        break;
    }
}

static void demo_dec_out_stream_resume(void *p)
{
    struct demo_dec_hdl *dec = (struct demo_dec_hdl *)p;

    audio_decoder_resume(&dec->decoder);
}


static int demo_dec_dec_start(void)
{
    int err;
    struct audio_fmt *fmt;
    struct demo_dec_hdl *dec = dec_hdl;

    if (!dec_hdl) {
        return -EINVAL;
    }

    printf("demo_dec_start: in\n");

    memcpy(&dec->dec_input, &demo_dec_file_input, sizeof(struct audio_dec_input));
    dec->dec_input.coding_type = dec->in_fmt.coding_type;
    // 打开demo_dec解码
    err = audio_decoder_open(&dec->decoder, &dec->dec_input, &decode_task);
    if (err) {
        goto __err1;
    }

    // 设置运行句柄
    audio_decoder_set_handler(&dec->decoder, &demo_dec_handler);
    struct audio_fmt f = {0};
    f.coding_type = dec->in_fmt.coding_type;
    f.sample_rate = dec->in_fmt.sample_rate;
    f.bit_rate = dec->in_fmt.bit_rate;
    f.frame_len = dec->in_fmt.frame_len;
    dec->decoder.fmt.sample_rate = dec->in_fmt.sample_rate;
    dec->decoder.fmt.channel = dec->in_fmt.channel;
    dec->decoder.fmt.bit_rate = dec->in_fmt.bit_rate;
    dec->decoder.fmt.frame_len = dec->in_fmt.frame_len;
    /* printf("sr:%d \n", dec->decoder.fmt.sample_rate); */
    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

    // 使能事件回调
    audio_decoder_set_event_handler(&dec->decoder, demo_dec_event_handler, 0);

#if defined(CONFIG_MEDIA_DEVELOP_ENABLE)
    // 设置输出声道类型
    audio_decoder_set_output_channel(&dec->decoder, audio_output_channel_type());
    // 配置mixer通道参数
    audio_mixer_ch_open_head(&dec->mix_ch, &mixer); // 挂载到mixer最前面
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 0);

    // 数据流串联
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->decoder.entry;
    // 添加自定义数据流节点等
    // 最后输出到mix数据流节点
    entries[entry_cnt++] = &dec->mix_ch.entry;
    // 创建数据流，把所有节点连接起来
    dec->stream = audio_stream_open(dec, demo_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    // 设置音频输出类型
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
#else
    // 设置输出声道类型
    enum audio_channel channel;
    int dac_output = audio_dac_get_channel(&dac_hdl);
    if (dac_output == DAC_OUTPUT_LR) {
        channel = AUDIO_CH_LR;
    } else if (dac_output == DAC_OUTPUT_MONO_L) {
        channel = AUDIO_CH_L;
    } else if (dac_output == DAC_OUTPUT_MONO_R) {
        channel = AUDIO_CH_R;
    } else {
        channel = AUDIO_CH_DIFF;
    }
    audio_decoder_set_output_channel(&dec->decoder, channel);
    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_resume_handler(&dec->mix_ch, (void *)&dec->decoder, (void (*)(void *))audio_decoder_resume);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, fmt->sample_rate);
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, 10, 1);
#endif

    // 开始解码
    dec->start = 1;
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    return 0;

__err3:
    dec->start = 0;

    audio_mixer_ch_close(&dec->mix_ch);

    // 先关闭各个节点，最后才close数据流
#if defined(CONFIG_MEDIA_DEVELOP_ENABLE)
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }
#endif

__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    demo_dec_release();

    return err;
}

static int demo_dec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    y_printf("demo_dec_wait_res_handler: %d\n", event);

    if (event == AUDIO_RES_GET) {
        // 可以开始解码
        err = demo_dec_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        if (dec_hdl->start) {
            demo_dec_audio_res_close();
        }
    }
    return err;
}


void demo_dec_close(void)
{
    if (!dec_hdl) {
        return ;
    }
    if (dec_hdl->start) {
        demo_dec_audio_res_close();
    }
    demo_dec_release();
}

int demo_dec_open(int sample_rate, u8 ch_num, u32 code_type, u32 code_param)
{
    demo_dec_close();


    struct demo_dec_hdl *dec = zalloc(sizeof(struct demo_dec_hdl));
    ASSERT(dec);

    dec->in_fmt.sample_rate = sample_rate;
    dec->in_fmt.channel = ch_num;
    dec->in_fmt.coding_type = code_type;

    printf("demo_dec_open, sr:%d, ch:%d, code:0x%x \n", sample_rate, ch_num, code_type);

    switch (code_type) {
#if TCFG_DEC_SPEEX_ENABLE
    case AUDIO_CODING_SPEEX:
        break;
#endif
#if TCFG_DEC_OPUS_ENABLE
    case AUDIO_CODING_OPUS:
        break;
#endif
#if TCFG_DEC_JLA_ENABLE
    case AUDIO_CODING_JLA:

        dec->in_fmt.bit_rate = 64000;
        dec->in_fmt.frame_len = JLA_CODING_FRAME_LEN;
        dec->in_fmt.sample_rate = JLA_CODING_SAMPLERATE;
        dec->in_fmt.channel = JLA_CODING_CHANNEL;

        break;
#endif
    default:
        /* printf("do not support this type !!!\n"); */
        /* while(1); */
        break;

    }

    dec_hdl = dec;

    dec->wait.priority = 4;		// 解码优先级
    dec->wait.preemption = 0;	// 不使能直接抢断解码
    dec->wait.snatch_same_prio = 1;	// 可抢断同优先级解码
    dec->wait.handler = demo_dec_wait_res_handler;
    audio_decoder_task_add_wait(&decode_task, &dec->wait);

    return 0;
}

void demo_dec_resume(void *priv)
{
    if (dec_hdl) {
        audio_decoder_resume(&dec_hdl->decoder);
    }
}


//-----------------------------------------------------------------------------
// mic enc to dec
//-----------------------------------------------------------------------------

void demo_mic_enc_dec_open(void)
{
    int ret;
    int sample_rate = 8000;
    int ch_num = 1;

    u32 code_type = TEST_CODING_TYPE;

    u32 code_param = 0;

    ret = demo_mic_open(sample_rate, 8);
    if (ret) {
        printf("demo_mic_open error \n");
        while (1);
    }

    ret = demo_enc_open(sample_rate, ch_num, code_type, code_param);
    if (ret) {
        printf("demo_enc_open error \n");
        while (1);
    }

    demo_mic_out_set_resume_cb(demo_enc_resume, NULL);



    // 存点数据再启动解码
    u8 delay_cnt = 100 / 10 + 1;
    while (delay_cnt) {
        if (demo_get_enc_data_len() >= DEMO_ENC_BUF_LEN / 2) {
            break;
        }
        os_time_dly(1);
        if (demo_get_enc_data_len() < 512) {
            continue;
        }
        delay_cnt--;
    }

    ret = demo_dec_open(sample_rate, ch_num, code_type, code_param);
    if (ret) {
        printf("demo_dec_open error \n");
        while (1);
    }

    demo_enc_out_set_resume_cb(demo_dec_resume, NULL);

}

void demo_mic_enc_dec_close(void)
{
    demo_dec_close();
    demo_enc_close();
    demo_mic_close();
}


static void test_stop(void *priv)
{
    y_printf("test_stop \n");
    demo_mic_enc_dec_close();
}

static void test_start(void *priv)
{
    y_printf("test_start \n");
    demo_mic_enc_dec_open();
    sys_timeout_add(NULL, test_stop, 30000);
}

void demo_mic_enc_dec_test(void)
{
    sys_timeout_add(NULL, test_start, 8000);
}


