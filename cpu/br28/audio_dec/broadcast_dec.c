#include "asm/includes.h"
#include "system/includes.h"
#include "app_main.h"
#include "audio_dec.h"
#include "audio_decoder.h"
#include "audio_config.h"
#include "asm/audio_adc.h"
#include "broadcast_dec.h"
#include "mixer.h"
#include "debug.h"
#include "audio_syncts.h"
#include "asm/dac.h"
#include "broadcast_api.h"

#include "media/effects_adj.h"
#include "audio_effect/audio_eq_drc_demo.h"


#if TCFG_BROADCAST_ENABLE

#define LOG_TAG         "[BROADCAST_DEC]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_WARN_ENABLE

#if TCFG_APP_BC_MIC_EN
#define ENABLE_HOWLING           1      //该宏是使能啸叫抑制算法的宏，相当于广播mic啸叫抑制的总开关
#define ENABLE_HOWLING_PS     	 1		//移频
#define ENABLE_HOWLING_TRAP		 1	    //馅波
#endif


#define BROADCAST_DEC_STATE_OPEN             1
#define BROADCAST_DEC_STATE_START            2
#define BROADCAST_DEC_STATE_STOP             3
#define BROADCAST_DEC_STATE_CLOSING          4
#define BROADCAST_DEC_STATE_CLOSE            5

/*下行播放延时设置*/
#define LIVE_STREAM_RX_LATENCY                  (500L)

#if ((TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE) || (TCFG_DRC_ENABLE && TCFG_BROADCAST_MODE_DRC_ENABLE))
#define LIVE_STREAM_EFFECT_LATENCY              (3 * 1000L)
#else
#define LIVE_STREAM_EFFECT_LATENCY              (500L)
#endif /*(TCFG_BROADCAST_MODE_EQ_ENABLE || TCFG_BROADCAST_MODE_DRC_ENABLE)*/

#if (JLA_CODING_FRAME_LEN > 50)
#define LIVE_STREAM_DECODING_LATENCY            (2 * 1000L + (JLA_CODING_CHANNEL * 1000L / 2))
#else
#define LIVE_STREAM_DECODING_LATENCY            (500L + (JLA_CODING_CHANNEL * 1000L / 2))
#endif /*JLA_CODING_FRAME_LEN > 50*/

#if (defined(WIRELESS_SOUND_TRACK_2_P_X_ENABLE)&&WIRELESS_SOUND_TRACK_2_P_X_ENABLE) || (defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE)

#define LIVE_STREAM_PLAY_LATENCY             (15 * 1000L)//无线2.x声道时，增加延时

#else

#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
#define LIVE_STREAM_PLAY_LATENCY            (LIVE_STREAM_RX_LATENCY + LIVE_STREAM_EFFECT_LATENCY + LIVE_STREAM_DECODING_LATENCY)
#define LIVE_STREAM_LOW_LATENCY             1
#else
#define LIVE_STREAM_PLAY_LATENCY            (LIVE_STREAM_RX_LATENCY + LIVE_STREAM_EFFECT_LATENCY + LIVE_STREAM_DECODING_LATENCY + ((JLA_CODING_FRAME_LEN / 10) * 1000L))
#define LIVE_STREAM_LOW_LATENCY             0
#endif /*((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)*/

#endif

#define JLA_FRAME_SIZE                       (JLA_CODING_FRAME_LEN * JLA_CODING_BIT_RATE / 1000 / 8 / 10 + 2)

extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;
extern struct audio_dac_hdl dac_hdl;

#define LEAUD_DECODE_TASK       &decode_task
#define LEAUD_MIXER             &mixer
#define LEAUD_DAC_HDL           &dac_hdl

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

struct broadcast_audio_effect {
    struct audio_stream_entry entry;
#if TCFG_APP_BC_MIC_EN
    HOWLING_API_STRUCT  *howling_ps;		//添加啸叫抑制 - 移频
    HOWLING_API_STRUCT  *notch_howling;        //添加啸叫抑制 - 馅波
#endif
    u8 nch;
    u8 first_ts;
    s16 used_len;
    int sample_rate;
    void *syncts;
    void *eq;
    void *drc;
    u32 base_time;
};

struct broadcast_dec_context {
    struct audio_decoder decoder;       //解码句柄
    struct audio_res_wait res_wait;		//解码资源等待
    struct audio_mixer_ch mix_ch;   	//叠加句柄
    struct audio_stream *stream;		// 音频流
    struct audio_fmt in_fmt;        	// 输入数据参数
    struct audio_dec_input dec_input;
    struct broadcast_input broadcast_input;    //数据输入
    struct broadcast_clock clock;
    struct broadcast_audio_effect effect; //音频音效处理

#if AUDIO_SURROUND_CONFIG
    surround_hdl *surround;         //环绕音效句柄
#endif
#if AUDIO_VBASS_CONFIG
    struct aud_gain_process *vbass_prev_gain;
    NOISEGATE_API_STRUCT *ns_gate;
    vbass_hdl *vbass;               //虚拟低音句柄
#endif


#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    struct audio_eq  *high_bass;
    struct audio_drc *hb_drc;//高低音后的drc
    struct convert_data *hb_convert;
#endif

    struct audio_eq *eq;    //eq drc句柄
    struct audio_drc *drc;    // drc句柄
#if (defined(TCFG_DRC_SPILT_ENABLE) && TCFG_DRC_SPILT_ENABLE)
    struct audio_drc *drc_fr;    // drc fr句柄
#endif
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    struct convert_data *convert;
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    struct audio_eq *ext_eq;    //eq drc句柄 扩展eq
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    struct audio_eq *eq2;    //eq drc句柄
    struct dynamic_eq_hdl *dy_eq;
#if defined(TCFG_BROADCAST_MODE_LAST_DRC_ENABLE)&&TCFG_BROADCAST_MODE_LAST_DRC_ENABLE
    struct audio_drc *last_drc;
#endif
    struct convert_data *convert2;
#endif
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    struct aud_gain_process *gain;
#endif

#if TCFG_EQ_DIVIDE_ENABLE&& WIRELESS_SOUND_TRACK_2_P_X_ENABLE
    struct audio_eq *eq_rl_rr;    //eq rl_rr句柄
    struct audio_drc *drc_rl_rr;  // drc rl句柄
#if (defined(TCFG_DRC_SPILT_ENABLE) && TCFG_DRC_SPILT_ENABLE)
    struct audio_drc *drc_rr;     // drc rr句柄
#endif
    struct convert_data *convert_rl_rr;//位宽转换(32->16)

#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    struct aud_gain_process *gain_rl_rr;//rl rr左右声道合并、相位控制
#endif

#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    struct audio_eq *ext_eq2;    //eq drc句柄 扩展eq
#endif

#endif
    u8 bass_sel;                        //低音通路音效控制

    volatile u8 state;                  //解码状态
    u32 timer;							//数据释放timer
    u8 dec_out_ch_mode; 				//解码输出声道模式
    u8 dec_out_ch_num; 				    //解码输出声道数  解码输出给后级的声道数
    spinlock_t lock;

};

void broadcast_dec_close(void *priv);
static void broadcast_stream_stop_drain(struct broadcast_dec_context *ctx);
/*----------------------------------------------------------------------------*/
/**@brief   外部激活解码接口
   @param   *priv:私有句柄
   @note
*/
/*----------------------------------------------------------------------------*/
void  broadcast_dec_kick_start(void *priv)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    BROADCAST_ENTER_CRITICAL();
    if (ctx->state == BROADCAST_DEC_STATE_START) {
        audio_decoder_resume(&ctx->decoder);
    }
    BROADCAST_EXIT_CRITICAL();
}
/*----------------------------------------------------------------------------*/
/**@brief    读取解码数据
   @param    *decoder: 解码器句柄
   @param    *buf: 数据
   @param    len: 数据长度
   @return   >=0：读到的数据长度
   @return   <0：错误
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_stream_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    struct broadcast_dec_context *ctx = container_of(decoder, struct broadcast_dec_context, decoder);
    int rlen = 0;

    if (ctx->broadcast_input.file.get_timestamp) {
        u32 timestamp = 0;
        int err = ctx->broadcast_input.file.get_timestamp(ctx->broadcast_input.stream, &timestamp);
        if (!err && ctx->effect.syncts) {
            timestamp = (((timestamp & 0xfffffff) + LIVE_STREAM_PLAY_LATENCY) & 0xfffffff) | (timestamp & 0xf0000000);
            if (ctx->effect.first_ts) { //获取到的第一个时间戳，和当前时间相差较小时，说明数据同步时间不够，将数据丢掉.
                int time_diff = timestamp - ctx->clock.get_clock_time(ctx->clock.priv, CURRENT_TIME);
                if (time_diff < LIVE_STREAM_PLAY_LATENCY / 2) {
                    u8 temp_buf[128];
                    do {
                        rlen = ctx->broadcast_input.file.fread(ctx->broadcast_input.stream, temp_buf, sizeof(temp_buf));
                    } while (rlen != 0);
                    audio_decoder_resume(&ctx->decoder); //丢掉数据，返回挂起，要自己激活解码.
                    return -1;
                }
            }

            audio_syncts_next_pts(ctx->effect.syncts, timestamp);
            if (ctx->effect.first_ts) {
                ctx->effect.first_ts = 0;
                ctx->effect.base_time = timestamp;
            }
        }
    }

    rlen = ctx->broadcast_input.file.fread(ctx->broadcast_input.stream, buf, len);
    if (rlen == 0) {
        //实时文件流读不到数据返回 -1,挂起解码,由上层激活
        return -1;
    }
    return rlen;

}

/*----------------------------------------------------------------------------*/
/**@brief    文件指针定位
   @param    *decoder: 解码器句柄
   @param    offset: 定位偏移
   @param    seek_mode: 定位类型
   @return   0：成功
   @return   非0：错误
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_stream_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    struct broadcast_dec_context *ctx = container_of(decoder, struct broadcast_dec_context, decoder);


    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    读取文件长度
   @param    *decoder: 解码器句柄
   @return   文件长度
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_stream_flen(struct audio_decoder *decoder)
{
    struct broadcast_dec_context *ctx = container_of(decoder, struct broadcast_dec_context, decoder);
    /*数据流返回最大长度*/
    return 0xffffffff;
}

static const struct audio_dec_input broadcast_dec_input = {
    .coding_type = AUDIO_CODING_JLA,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = broadcast_dec_stream_fread,
            .fseek = broadcast_dec_stream_fseek,
            .flen  = broadcast_dec_stream_flen,
        }
    }
};



/*----------------------------------------------------------------------------*/
/**@brief    解码预处理
   @param    *decoder: 解码器句柄
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_probe_handler(struct audio_decoder *decoder)
{
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    文件解码后处理
   @param    *decoder: 解码器句柄
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

static const struct audio_dec_handler broadcast_dec_handler = {
    .dec_probe  = broadcast_dec_probe_handler,
    .dec_post   = broadcast_dec_post_handler,

};

/*----------------------------------------------------------------------------*/
/**@brief    broadcast解码流激活
   @param    *priv: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_dec_resume(void *priv)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    audio_decoder_resume(&ctx->decoder);
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast解码事件处理
   @param    *encoder: 解码器句柄
   @param    argc: 参数个数
   @param    *argv: 参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_dec_event_handler(struct audio_decoder *decoder, int arg, int *argv)
{
    struct broadcast_dec_context *ctx = container_of(decoder, struct broadcast_dec_context, decoder);

    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
    case AUDIO_DEC_EVENT_ERR:
        broadcast_dec_close(ctx);
        break;
    default:
        break;
    }
}


static void broadcast_syncts_mix_ch_event_handler(void *priv, int event, int param)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    int time_diff = 0;
    int slience_frames = 0;
    switch (event) {
    case MIXER_EVENT_CH_OPEN:
        sound_pcm_device_mount_syncts(AUDIO_OUT_WAY_TYPE, ctx->effect.syncts);
        if (ctx->clock.get_clock_time) {
            time_diff = ctx->effect.base_time - ctx->clock.get_clock_time(ctx->clock.priv, CURRENT_TIME);
        }
        if (time_diff > 0 && time_diff < 500000) {
            slience_frames = (u64)time_diff * ctx->effect.sample_rate / 1000000 - sound_pcm_device_buffered_len(AUDIO_OUT_WAY_TYPE);
            if (slience_frames <= 0) {
                break;
            }
            log_info("=====slience_frames %d=====\n", slience_frames);
            audio_mixer_ch_add_slience_samples(&ctx->mix_ch, slience_frames * audio_output_channel_num());//因使用mix做声道变换，此处计算使用mix的实际输出声道数
            sound_pcm_update_frame_num(ctx->effect.syncts, -slience_frames);

        }
        break;
    case MIXER_EVENT_CH_CLOSE:
        sound_pcm_device_unmount_syncts(AUDIO_OUT_WAY_TYPE, ctx->effect.syncts);
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast解码数据流激活
   @param    *priv: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_play_resume_from_stream(void *priv)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;

    audio_decoder_resume(&ctx->decoder);
}


static int audio_syncts_output_handler(void *priv, void *data, int len)
{
    struct broadcast_audio_effect *effect = (struct broadcast_audio_effect *)priv;

    struct audio_data_frame frame = {
        .data = data,
        .data_len = len,
        .channel = effect->nch,
        .sample_rate = effect->sample_rate,
    };

    audio_stream_run(&effect->entry, &frame);

    return effect->used_len;
}

static int broadcast_effect_data_handler(struct audio_stream_entry *entry,  struct audio_data_frame *in, struct audio_data_frame *out)
{
    struct broadcast_audio_effect *effect = container_of(entry, struct broadcast_audio_effect, entry);


    if (in->data_len) {
        out->no_subsequent = 1;  //数据流节点自己调用audio_stream_run,需要置1，不跑数据流节点的递归调用;
    }

    if (effect->syncts) {
        int wlen = audio_syncts_frame_filter(effect->syncts, in->data, in->data_len);
        if (wlen < in->data_len) {
            audio_syncts_trigger_resume(effect->syncts, (void *)entry, (void (*)(void *))audio_stream_resume);
        } else {
#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
            audio_syncts_push_data_out(effect->syncts);
#endif
        }
        return wlen;
    }

    return in->data_len;
}

static void broadcast_effect_data_process_len(struct audio_stream_entry *entry, int len)
{
    struct broadcast_audio_effect *effect = container_of(entry, struct broadcast_audio_effect, entry);

    effect->used_len = len;
}

static int audio_syncts_latch_reference_time(void *priv, int cmd, void *param)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    int err = 0;

    switch (cmd) {
    case BLE_LATCH_REFERENCE_TIME:
        if (ctx->clock.latch_reference_time) {
            err = ctx->clock.latch_reference_time(ctx->clock.priv);
        }
        break;
    case BLE_GET_REFERENCE_TIME_US:
        if (ctx->clock.get_reference_time) {
            struct reference_time *time = (struct reference_time *)param;
            if (!time) {
                break;
            }
            err = ctx->clock.get_reference_time(ctx->clock.priv, &time->micro_parts, &time->clock_us, &time->event);
        }
        break;
    default:
        break;
    }

    return err;
}

static void broadcast_mic_howling_open(struct broadcast_dec_context *ctx)
{
    printf("\n----------------- open howling!! -----------------------n\n");
#if (ENABLE_HOWLING_PS) 		//移频
    ctx->effect.howling_ps = open_howling(NULL, 48000, 0, 1);  //48000为mic的采样率
#endif
#if (ENABLE_HOWLING_TRAP)		//馅波
    //这些参数可以通过写立即数的方法来调整参数，下面这些参数是自己调试后觉得相对不错的，但不一定是最好的
    HOWLING_PARM_SET  howling_param = {
        .threshold = -20.0f,
        .fade_time = 5,
        .notch_Q = 1.1f,
        .notch_gain = 12.0f,
        .sample_rate = 48000,			//改成自己的48000
        .channel = 1,
    };
    ctx->effect.notch_howling = open_howling(&howling_param, 48000, 0, 0);	//馅波
#endif
}

static int broadcast_audio_effect_open(struct broadcast_dec_context *ctx)
{
    ctx->effect.nch = ctx->dec_out_ch_num;
    ctx->effect.sample_rate = audio_mixer_get_sample_rate(LEAUD_MIXER);

    /*音效处理之同步模块初始化*/
    struct audio_syncts_params params;
    params.nch = ctx->effect.nch;
#if TCFG_AUDIO_OUTPUT_IIS
    params.pcm_device = PCM_OUTSIDE_DAC;
#else
    params.pcm_device = PCM_INSIDE_DAC;
#endif /*TCFG_AUDIO_OUTPUT_IIS*/
    params.network = AUDIO_NETWORK_BLE;
    params.rin_sample_rate = ctx->in_fmt.sample_rate;
    params.rout_sample_rate = ctx->effect.sample_rate;
    params.priv = &ctx->effect;
    params.output = audio_syncts_output_handler;
    params.reference_clock = ctx;
    params.reference_time_handler = audio_syncts_latch_reference_time;
    bt_audio_sync_nettime_select(2);//0 - a2dp主机，1 - tws, 2 - BLE
    int err = audio_syncts_open(&ctx->effect.syncts, &params);
    if (ctx->effect.syncts) {
        audio_mixer_ch_set_event_handler(&ctx->mix_ch, (void *)ctx, broadcast_syncts_mix_ch_event_handler);
    }
    /*TODO : 后续的音效处理可以在这里接入*/
    ctx->effect.first_ts = 1;

    ctx->effect.entry.data_handler = broadcast_effect_data_handler;
    ctx->effect.entry.data_process_len = broadcast_effect_data_process_len;
    return err;
}

static void broadcast_audio_effect_close(struct broadcast_dec_context *ctx)
{
    audio_stream_del_entry(&ctx->effect.entry);

    if (ctx->effect.syncts) {
        audio_syncts_close(ctx->effect.syncts);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast 解码开始
   @param    解码私有参数
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
static int broadcast_dec_start(struct broadcast_dec_context *ctx)
{
    int err = 0;
    struct audio_fmt f = {0};

    if (!ctx) {
        return -EINVAL;
    }
    printf("broadcast_dec_start: in\n");

    memcpy(&ctx->dec_input, &broadcast_dec_input, sizeof(struct audio_dec_input));
    ctx->dec_input.coding_type = ctx->in_fmt.coding_type;

    // 打开broadcast_dec解码
    err = audio_decoder_open(&ctx->decoder, &ctx->dec_input, LEAUD_DECODE_TASK);
    if (err) {
        goto __err1;
    }
    // 设置运行句柄
    audio_decoder_set_handler(&ctx->decoder, &broadcast_dec_handler);
    f.coding_type = ctx->in_fmt.coding_type;
    f.sample_rate = ctx->in_fmt.sample_rate;
    f.bit_rate = ctx->in_fmt.bit_rate;
    f.frame_len = ctx->in_fmt.frame_len;
    /* memcpy(&ctx->decoder.fmt,&f,sizeof(struct audio_fmt)); */
    ctx->decoder.fmt.sample_rate = ctx->in_fmt.sample_rate;
    ctx->decoder.fmt.channel = ctx->in_fmt.channel;
    ctx->decoder.fmt.bit_rate = ctx->in_fmt.bit_rate;
    ctx->decoder.fmt.frame_len = ctx->in_fmt.frame_len;
    if (ctx->in_fmt.channel == 2) {
        if (ctx->dec_out_ch_mode == AUDIO_CH_L || ctx->dec_out_ch_mode == AUDIO_CH_R) {
            ctx->decoder.fmt.channel = JLA_2CH_L_OR_R; //jla 解码只解一个声道;
        }
    }
    err = audio_decoder_set_fmt(&ctx->decoder, &f);
    if (err) {
        goto __err2;
    }

    // 使能事件回调
    audio_decoder_set_event_handler(&ctx->decoder, broadcast_dec_event_handler, 0);
    // 设置解码输出声道类型  会根据配置更新实际输出的声道数到格式里;
    audio_decoder_set_output_channel(&ctx->decoder, ctx->dec_out_ch_mode);
    //解码的源数据是单声道数据时，解码声道类型配置无效，输出单声道数据，
    ctx->dec_out_ch_num = ctx->in_fmt.channel == 1 ? 1 : ctx->decoder.fmt.channel;
    // 配置mixer通道参数
    audio_mixer_ch_open_head(&ctx->mix_ch, LEAUD_MIXER); // 挂载到mixer最前面
    audio_mixer_ch_set_sample_rate(&ctx->mix_ch, f.sample_rate);
    broadcast_audio_effect_open(ctx);

    //从机在进入mic接收的情况下需要打开 TCFG_APP_BC_MIC_EN 宏才能开啸叫
#if (ENABLE_HOWLING && TCFG_APP_BC_MIC_EN)
    if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {	//当前状态是接收端
        broadcast_mic_howling_open(ctx);		//打开啸叫抑制
    }
#endif	/* ENABLE_HOWLING && TCFG_APP_BC_MIC_EN */



    ctx->bass_sel = 0;
#if defined(WIRELESS_SOUND_TRACK_2_P_X_ENABLE)&&WIRELESS_SOUND_TRACK_2_P_X_ENABLE
#if (TWO_POINT_X_CH == WIRELESS_SCENE_ONE)
    if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {	//接收端,做低音
        ctx->bass_sel = 1;
    }
#elif (TWO_POINT_X_CH == WIRELESS_SCENE_TWO)
    if (get_broadcast_role() == BROADCAST_ROLE_TRANSMITTER) {	//发射端,做低音
        ctx->bass_sel = 1;
    }
#endif
#endif/*WIRELESS_SOUND_TRACK_2_P_X_ENABLE*/

    log_info("---------bass_sel %d\n", ctx->bass_sel);
    if (ctx->bass_sel) {	//低音通路
#if TCFG_EQ_DIVIDE_ENABLE&& WIRELESS_SOUND_TRACK_2_P_X_ENABLE
#if TCFG_EQ_ENABLE &&TCFG_BT_MUSIC_EQ_ENABLE
        ctx->eq_rl_rr = music_eq_rl_rr_open(ctx->effect.sample_rate, ctx->effect.nch);// eq
#if TCFG_DRC_ENABLE && TCFG_BT_MUSIC_DRC_ENABLE
        ctx->drc_rl_rr = music_drc_rl_rr_open(ctx->effect.sample_rate, ctx->effect.nch);//drc
#endif
        if (ctx->eq_rl_rr && ctx->eq_rl_rr->out_32bit) {
            ctx->convert_rl_rr = convet_data_open(0, 512);
        }
#endif
        ctx->gain_rl_rr = audio_gain_open_demo(AEID_MUSIC_RL_GAIN, ctx->effect.nch);
#if defined(MUSIC_EXT_EQ2_AFTER_DRC) && MUSIC_EXT_EQ2_AFTER_DRC
        ctx->ext_eq2 = music_ext_eq2_open(ctx->effect.sample_rate, ctx->effect.nch);
#endif
#endif
    } else {

#if AUDIO_SURROUND_CONFIG
        //环绕音效
        u8 ch_type = 0;
        ch_type = ctx->dec_out_ch_mode;
        if ((ctx->effect.nch == 1) && ctx->dec_out_ch_mode == AUDIO_CH_LR) {//此情况时左箱体与右箱体无法确认
            ch_type = 0xff;//not support surround effect
        }
        ctx->surround = surround_open_demo(AEID_MUSIC_SURROUND, ch_type);
#endif

#if AUDIO_VBASS_CONFIG
        ctx->vbass_prev_gain = audio_gain_open_demo(AEID_MUSIC_VBASS_PREV_GAIN, ctx->effect.nch);
        ctx->ns_gate = audio_noisegate_open_demo(AEID_MUSIC_NS_GATE, ctx->effect.sample_rate, ctx->effect.nch);
        //虚拟低音
        ctx->vbass = audio_vbass_open_demo(AEID_MUSIC_VBASS, ctx->effect.sample_rate, ctx->effect.nch);
#endif


#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
        ctx->high_bass = high_bass_eq_open(ctx->effect.sample_rate, ctx->effect.nch);
        ctx->hb_drc = high_bass_drc_open(ctx->effect.sample_rate, ctx->effect.nch);
        if (ctx->hb_drc && ctx->hb_drc->run32bit) {
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
            ctx->hb_convert = convet_data_open(0, 512);
#endif
        }
#endif/*TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE */

#if TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE
        ctx->eq = live_music_eq_open(ctx->effect.sample_rate, ctx->effect.nch, LIVE_STREAM_LOW_LATENCY);// eq
#if TCFG_DRC_ENABLE && TCFG_BROADCAST_MODE_DRC_ENABLE
        ctx->drc = music_drc_open(ctx->effect.sample_rate, ctx->effect.nch);//drc
#endif/*TCFG_BROADCAST_MODE_DRC_ENABLE*/

#if !TCFG_DYNAMIC_EQ_ENABLE
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
        if (ctx->eq && ctx->eq->out_32bit) {
            ctx->convert = convet_data_open(0, 512);
        }
#endif
#endif/* TCFG_DYNAMIC_EQ_ENABLE */

#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
        ctx->ext_eq = music_ext_eq_open(ctx->effect.sample_rate, ctx->effect.nch);
#endif/*MUSIC_EXT_EQ_AFTER_DRC*/

#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
        ctx->eq2 = music_eq2_open(ctx->effect.sample_rate, ctx->effect.nch);// eq
        ctx->dy_eq = audio_dynamic_eq_ctrl_open(AEID_MUSIC_DYNAMIC_EQ, ctx->effect.sample_rate, ctx->effect.nch);//动态eq
#if defined(TCFG_BROADCAST_MODE_LAST_DRC_ENABLE)&&TCFG_BROADCAST_MODE_LAST_DRC_ENABLE
        ctx->last_drc = music_last_drc_open(ctx->effect.sample_rate, ctx->effect.nch);//广播音箱默认关闭dynamic_eq后的drc处理
#endif
        ctx->convert2 = convet_data_open(0, 512);
#endif/*TCFG_DYNAMIC_EQ_ENABLE*/

#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
        ctx->gain = audio_gain_open_demo(AEID_MUSIC_GAIN, ctx->effect.nch);
#endif

#endif/*TCFG_BROADCAST_MODE_EQ_ENABLE*/
    }


#ifdef CONFIG_MIXER_CYCLIC
    //解码按配置输出数据， 通过mixer做声道变换
    u8 ch_num = audio_output_channel_num(); //获取输出设备的声道数
    if (ctx->dec_out_ch_num != ch_num) {
        if (ctx->dec_out_ch_num == 1) {
            audio_mixer_ch_set_aud_ch_out(&ctx->mix_ch, 0, BIT(0) | BIT(1)); //单变双
        } else {
            audio_mixer_ch_set_aud_ch_out(&ctx->mix_ch, 0, BIT(0));       //双变单
            audio_mixer_ch_set_aud_ch_out(&ctx->mix_ch, 1, BIT(0));
        }
    }
#endif
    // 数据流串联
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    u8 rl_rr_entry_start = 0;
    entries[entry_cnt++] = &ctx->decoder.entry;
    // 添加自定义数据流节点等
    entries[entry_cnt++] = &ctx->effect.entry;

    //只有接收端在打开广播mic情况下才能有啸叫抑制
#if (ENABLE_HOWLING && TCFG_APP_BC_MIC_EN)
    if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {	//接收端
        //添加抑制啸叫数据流节点
#if (ENABLE_HOWLING_PS)		//移频
        if (ctx->effect.howling_ps) {
            entries[entry_cnt++] = &ctx->effect.howling_ps->entry;
        }
#endif
#if(ENABLE_HOWLING_TRAP)	//馅波
        if (ctx->effect.notch_howling) {
            entries[entry_cnt++] = &ctx->effect.notch_howling->entry;
        }
#endif
    }
#endif /* ENABLE_HOWLING && TCFG_APP_BC_MIC_EN */

    if (ctx->bass_sel) {	//接收端
#if TCFG_EQ_DIVIDE_ENABLE&& WIRELESS_SOUND_TRACK_2_P_X_ENABLE
        if (ctx->eq_rl_rr) {
            if (ctx->gain_rl_rr) {
                entries[entry_cnt++] = &ctx->gain_rl_rr->entry;
            }
            entries[entry_cnt++] = &ctx->eq_rl_rr->entry;
            if (ctx->drc_rl_rr) {
                entries[entry_cnt++] = &ctx->drc_rl_rr->entry;
            }
            if (ctx->convert_rl_rr) {
                entries[entry_cnt++] = &ctx->convert_rl_rr->entry;
            }

#if defined(MUSIC_EXT_EQ2_AFTER_DRC) && MUSIC_EXT_EQ2_AFTER_DRC
            if (ctx->ext_eq2) {
                entries[entry_cnt++] = &ctx->ext_eq2->entry;
            }
#endif
        }
#endif
    } else {

#if AUDIO_VBASS_CONFIG
        if (ctx->vbass_prev_gain) {
            entries[entry_cnt++] = &ctx->vbass_prev_gain->entry;
        }
        if (ctx->ns_gate) {
            entries[entry_cnt++] = &ctx->ns_gate->entry;
        }
        if (ctx->vbass) {
            entries[entry_cnt++] = &ctx->vbass->entry;
        }
#endif


#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
        if (ctx->high_bass) { //高低音
            entries[entry_cnt++] = &ctx->high_bass->entry;
        }
        if (ctx->hb_drc) { //高低音后drc
            entries[entry_cnt++] = &ctx->hb_drc->entry;
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
            if (ctx->hb_convert) {
                entries[entry_cnt++] = &ctx->hb_convert->entry;
            }
#endif
        }
#endif/* TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE */

        rl_rr_entry_start = entry_cnt - 1;//记录eq的上一个节点
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
        if (ctx->gain) {
            entries[entry_cnt++] = &ctx->gain->entry;
        }
#endif

#if AUDIO_SURROUND_CONFIG
        if (ctx->surround) {
            entries[entry_cnt++] = &ctx->surround->entry;
        }
#endif

#if TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE
        if (ctx->eq) {
            entries[entry_cnt++] = &ctx->eq->entry;
#if TCFG_DRC_ENABLE && TCFG_BT_MUSIC_DRC_ENABLE
            if (ctx->drc) {
                entries[entry_cnt++] = &ctx->drc->entry;
            }
#endif/*TCFG_BT_MUSIC_DRC_ENABLE*/
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
            if (ctx->convert) {
                entries[entry_cnt++] = &ctx->convert->entry;
            }
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
            if (ctx->ext_eq) {
                entries[entry_cnt++] = &ctx->ext_eq->entry;
            }
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
            if (ctx->eq2) {
                entries[entry_cnt++] = &ctx->eq2->entry;
            }
            if (ctx->dy_eq && ctx->dy_eq->dy_eq) {
                entries[entry_cnt++] = &ctx->dy_eq->dy_eq->entry;
            }
#if defined(TCFG_BROADCAST_MODE_LAST_DRC_ENABLE)&&TCFG_BROADCAST_MODE_LAST_DRC_ENABLE
            if (ctx->last_drc) {
                entries[entry_cnt++] = &ctx->last_drc->entry;
            }
#endif
            if (ctx->convert2) {
                entries[entry_cnt++] = &ctx->convert2->entry;
            }
#endif
        }
#endif/* TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE */
    }


    // 最后输出到mix数据流节点
    entries[entry_cnt++] = &ctx->mix_ch.entry;
    // 创建数据流，把所有节点连接起来
    ctx->stream = audio_stream_open(ctx, broadcast_play_resume_from_stream);
    audio_stream_add_list(ctx->stream, entries, entry_cnt);
    // 设置音频输出类型
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    clock_add_set(BROADCAST_DEC_CLK);

    broadcast_stream_stop_drain(ctx);
    ctx->state = BROADCAST_DEC_STATE_START;
    err = audio_decoder_start(&ctx->decoder);
    printf("broadcast_dec_start succ\n");
    if (err) {
        goto __err3;
    }

    return 0;
__err3:
    ctx->state = BROADCAST_DEC_STATE_CLOSE;

#if AUDIO_VBASS_CONFIG
    audio_gain_close_demo(ctx->vbass_prev_gain);
    audio_noisegate_close_demo(ctx->ns_gate);
    audio_vbass_close_demo(ctx->vbass);
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    high_bass_eq_close(ctx->high_bass);
    high_bass_drc_close(ctx->hb_drc);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(ctx->hb_convert);
#endif
#endif/*TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE*/

#if AUDIO_SURROUND_CONFIG
    surround_close_demo(ctx->surround);
#endif


#if TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE
    music_eq_close(ctx->eq);
#if TCFG_DRC_ENABLE && TCFG_BROADCAST_MODE_DRC_ENABLE
    music_drc_close(ctx->drc);
#endif/*TCFG_BROADCAST_MODE_DRC_ENABLE*/
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(ctx->convert);
#endif

#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    music_ext_eq_close(ctx->ext_eq);
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    music_eq2_close(ctx->eq2);
    audio_dynamic_eq_ctrl_close(ctx->dy_eq);
#if defined(TCFG_BROADCAST_MODE_LAST_DRC_ENABLE)&&TCFG_BROADCAST_MODE_LAST_DRC_ENABLE
    music_last_drc_close(ctx->last_drc);
#endif
    convet_data_close(ctx->convert2);
#endif/*TCFG_DYNAMIC_EQ_ENABLE*/
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    audio_gain_close_demo(ctx->gain);
#endif
#endif /*TCFG_BROADCAST_MODE_EQ_ENABLE*/
#if TCFG_EQ_DIVIDE_ENABLE&& WIRELESS_SOUND_TRACK_2_P_X_ENABLE
    music_eq_rl_rr_close(ctx->eq_rl_rr);
    music_drc_rl_rr_close(ctx->drc_rl_rr);
    convet_data_close(ctx->convert_rl_rr);
    audio_gain_close_demo(ctx->gain_rl_rr);

#if defined(MUSIC_EXT_EQ2_AFTER_DRC) && MUSIC_EXT_EQ2_AFTER_DRC
    music_ext_eq2_close(ctx->ext_eq2);
#endif
#endif



#if (ENABLE_HOWLING && TCFG_APP_BC_MIC_EN)
    //关闭啸叫抑制
    if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {		//目前只有是在接收端的情况下加的抑制
#if ENABLE_HOWLING_PS  //移频
        if (ctx->effect.howling_ps) {
            close_howling(ctx->effect.howling_ps);
        }
#endif
#if ENABLE_HOWLING_TRAP //馅波
        if (ctx->effect.notch_howling) {
            close_howling(ctx->effect.notch_howling);
        }
#endif
    }
#endif /* ENABLE_HOWLING  && TCFG_APP_BC_MIC_EN*/

    broadcast_audio_effect_close(ctx);
    audio_mixer_ch_close(&ctx->mix_ch);
    // 先关闭各个节点，最后才close数据流
    if (ctx->stream) {
        audio_stream_close(ctx->stream);
        ctx->stream = NULL;
    }
__err2:
    audio_decoder_close(&ctx->decoder);
__err1:
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast解码停止
   @param    私有参数
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/

static int broadcast_dec_stop(struct broadcast_dec_context *ctx)
{
    BROADCAST_ENTER_CRITICAL();
    ctx->state = BROADCAST_DEC_STATE_STOP;
    BROADCAST_EXIT_CRITICAL();

    audio_decoder_close(&ctx->decoder);

#if AUDIO_VBASS_CONFIG
    audio_gain_close_demo(ctx->vbass_prev_gain);
    audio_noisegate_close_demo(ctx->ns_gate);
    audio_vbass_close_demo(ctx->vbass);
#endif


#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    high_bass_eq_close(ctx->high_bass);
    high_bass_drc_close(ctx->hb_drc);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(ctx->hb_convert);
#endif
#endif/*TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE*/

#if AUDIO_SURROUND_CONFIG
    surround_close_demo(ctx->surround);
#endif

#if TCFG_EQ_ENABLE && TCFG_BROADCAST_MODE_EQ_ENABLE
    music_eq_close(ctx->eq);
#if TCFG_DRC_ENABLE && TCFG_BROADCAST_MODE_DRC_ENABLE
    music_drc_close(ctx->drc);
#endif/*TCFG_MUSIC_MODE_DRC_ENABLE*/
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(ctx->convert);
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    music_ext_eq_close(ctx->ext_eq);
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    music_eq2_close(ctx->eq2);
    audio_dynamic_eq_ctrl_close(ctx->dy_eq);
#if defined(TCFG_BROADCAST_MODE_LAST_DRC_ENABLE)&&TCFG_BROADCAST_MODE_LAST_DRC_ENABLE
    music_last_drc_close(ctx->last_drc);
#endif
    convet_data_close(ctx->convert2);
#endif/*TCFG_DYNAMIC_EQ_ENABLE*/
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    audio_gain_close_demo(ctx->gain);
#endif
#endif /*TCFG_MUSIC_MODE_EQ_ENABLE*/

#if TCFG_EQ_DIVIDE_ENABLE&& WIRELESS_SOUND_TRACK_2_P_X_ENABLE
    music_eq_rl_rr_close(ctx->eq_rl_rr);
    music_drc_rl_rr_close(ctx->drc_rl_rr);
    convet_data_close(ctx->convert_rl_rr);
    audio_gain_close_demo(ctx->gain_rl_rr);

#if defined(MUSIC_EXT_EQ2_AFTER_DRC) && MUSIC_EXT_EQ2_AFTER_DRC
    music_ext_eq2_close(ctx->ext_eq2);
#endif
#endif


#if (ENABLE_HOWLING && TCFG_APP_BC_MIC_EN)
    //关闭啸叫抑制
    if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {		//目前只有是在接收端的情况下加的抑制
#if ENABLE_HOWLING_PS  //移频
        if (ctx->effect.howling_ps) {
            close_howling(ctx->effect.howling_ps);
        }
#endif
#if ENABLE_HOWLING_TRAP //馅波
        if (ctx->effect.notch_howling) {
            close_howling(ctx->effect.notch_howling);
        }
#endif
    }
#endif /* ENABLE_HOWLING  && TCFG_APP_BC_MIC_EN*/

    broadcast_audio_effect_close(ctx);
    audio_mixer_ch_close(&ctx->mix_ch);
    // 先关闭各个节点，最后才close数据流
    if (ctx->stream) {
        audio_stream_close(ctx->stream);
        ctx->stream = NULL;
    }

    clock_remove_set(BROADCAST_DEC_CLK);

    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);

    return 0;
}


/*----------------------------------------------------------------------------*/
/**@brief    解码释放
   @param    解码私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_dec_free(struct broadcast_dec_context *ctx)
{
    // 解码释放
    audio_decoder_task_del_wait(LEAUD_DECODE_TASK, &ctx->res_wait);

    local_irq_disable();
    ctx->state = BROADCAST_DEC_STATE_CLOSE;
    free(ctx);
    ctx = NULL;
    local_irq_enable();
}

/*----------------------------------------------------------------------------*/
/**@brief    解码打断的数据处理
   @param    解码私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void __broadcast_stream_drain(void *priv)
{
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    u8 buf[128];
    int rlen;
    if (ctx->state == BROADCAST_DEC_STATE_START) {
        return;
    }

    while (1) {
        if (ctx->broadcast_input.file.get_timestamp) {
            u32 timestamp = 0;
            int err = ctx->broadcast_input.file.get_timestamp(ctx->broadcast_input.stream, &timestamp);
            if (err) {
                break;
            }
        }

        do {
            rlen = ctx->broadcast_input.file.fread(ctx->broadcast_input.stream, buf, sizeof(buf));
        } while (rlen != 0);
    }
}

static void broadcast_stream_auto_drain(struct broadcast_dec_context *ctx)
{
    BROADCAST_ENTER_CRITICAL();
    if (!ctx->timer) {
        ctx->timer = sys_timer_add(ctx, __broadcast_stream_drain, 30);
    }
    BROADCAST_EXIT_CRITICAL();
}
static void broadcast_stream_stop_drain(struct broadcast_dec_context *ctx)
{
    BROADCAST_ENTER_CRITICAL();
    if (ctx->timer) {
        sys_timer_del(ctx->timer);
        ctx->timer = 0;
    }
    BROADCAST_EXIT_CRITICAL();
}

/*----------------------------------------------------------------------------*/
/**@brief    broadcast 解码资源等待
   @param    *wait: 句柄
   @param    event: 事件
   @return   0：成功
   @note     用于多解码打断处理。被打断时关闭解码
*/
/*----------------------------------------------------------------------------*/
static int broadcast_wait_res_handler(struct audio_res_wait *wait, int event)
{
    struct broadcast_dec_context *ctx = container_of(wait, struct broadcast_dec_context, res_wait);
    int err = 0;

    log_debug("broadcast res handler : %d", event);

    if (event == AUDIO_RES_GET) {
        err = broadcast_dec_start(ctx);
        if (err) {
            broadcast_dec_free(ctx);
        }
    } else if (event == AUDIO_RES_PUT) {
        if (ctx->state == BROADCAST_DEC_STATE_START) {
            broadcast_stream_auto_drain(ctx);
        }
        broadcast_dec_stop(ctx);
    }

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    打开 broadcast 解码
   @param    解码参数
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
void *broadcast_dec_open(struct broadcast_codec_params *params)
{
    struct broadcast_codec_params *broadcast = params;
    struct broadcast_dec_context *ctx;

    if (strcmp(os_current_task(), "app_core") != 0) {
        log_error("broadcast open in task : %s\n", os_current_task());
    }

    printf("broadcast dec open !!\n");
    ctx = zalloc(sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    BROADCAST_CRITICAL_INIT();

    ctx->state = BROADCAST_DEC_STATE_OPEN;
    ctx->in_fmt.coding_type = broadcast->coding_type;
    ctx->in_fmt.sample_rate = broadcast->sample_rate;
    ctx->in_fmt.channel = broadcast->channel;
    ctx->dec_out_ch_mode = broadcast->dec_out_ch_mode;
    memcpy(&ctx->broadcast_input, &params->input, sizeof(ctx->broadcast_input));
    memcpy(&ctx->clock, &params->clock, sizeof(ctx->clock));

    switch (broadcast->coding_type) {
#if TCFG_DEC_JLA_ENABLE
    case AUDIO_CODING_JLA:

        ctx->in_fmt.bit_rate = broadcast->bit_rate;
        ctx->in_fmt.frame_len = broadcast->frame_size;

        break;
#endif

#if TCFG_DEC_PCM_ENABLE
    case AUDIO_CODING_PCM:

        break;
#endif

    default:
        printf("do not support this type !!!\n");
        return NULL;
        break;
    }

    ctx->res_wait.priority = 1;
    ctx->res_wait.protect = 0;
    ctx->res_wait.preemption = 0;
    ctx->res_wait.handler = broadcast_wait_res_handler;
    audio_decoder_task_add_wait(LEAUD_DECODE_TASK, &ctx->res_wait);
    if (ctx && (ctx->state != BROADCAST_DEC_STATE_START)) {
        broadcast_stream_auto_drain(ctx);
    }

    return ctx;
}

/*----------------------------------------------------------------------------*/
/**@brief    关闭broadcast解码
   @param    *priv: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void broadcast_dec_close(void *priv)
{
    u32 rets_addr;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    g_printf("%s 0x%x", __FUNCTION__, rets_addr);
    struct broadcast_dec_context *ctx = (struct broadcast_dec_context *)priv;
    if (!ctx) {
        return;
    }
    broadcast_stream_stop_drain(ctx);
    if (ctx->state == BROADCAST_DEC_STATE_START) {
        broadcast_dec_stop(ctx);
    }

    broadcast_dec_free(ctx);
}


#endif/*TCFG_BROADCAST_ENABLE*/


