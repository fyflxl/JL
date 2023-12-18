#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
/* #include "audio_drc.h" */
/* #include "audio_reverb.h" */
#include "clock_cfg.h"
#include "audio_config.h"
#include "storage_dev/storage_dev.h"
#include "loud_speaker.h"
#include "audio_dec.h"
#include "audio_enc.h"
#include "media/pcm_decoder.h"
#include "media/audio_howling.h"
#include "media/audio_voice_changer.h"
#include "media/audio_echo_reverb.h"
#define LOG_TAG     "[APP-SPEAKER]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define ECHO_ENABLE    			    0 //K歌混响
#define HOWLING_ENABLE    			0 //啸叫抑制
#define PITCH_ENABLE    			0 //变声

// mic 采样率 若存在与mic录音/usb mic 等同开情况 需保证采样率一致
#define LOUND_SPEAKER_MIC_SAMPLE_RATE  44100


//简易版扩音器功能 使用解码数据流程把mic数据接入mix，能把数据输出到DAC/FM/IIS等 (mic_effect.c里面的混响只能输出到DAC)
// 可用于mic to dac的测试 /mic混响数据到fm/iis输出功能
//
#if 0

extern struct audio_adc_hdl adc_hdl;
extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;

#define PCM_BUF_LEN  		(2*1024)
enum {
    REVERB_STATUS_STOP = 0,
    REVERB_STATUS_START,
    REVERB_STATUS_PAUSE,
};

struct s_speaker_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;

    u32 process_len;
    struct audio_stream *stream;		// 音频流
    int mic_gain;
    u16 mic_sr;
    u16 src_out_sr;
    u16 src_out_sr_n;
    int begin_size;
    int top_size;
    int bottom_size;
    u16 audio_new_rate;
    u16 audio_max_speed;
    u16 audio_min_speed;
    u8 sync_start;
    u8 pcm_buf[PCM_BUF_LEN ];
    cbuffer_t pcm_cbuf;
    u32 status : 2;
    u32 source_ch_num : 2;
    u32 reverb_en : 2;
    u32 howling_en : 2;
    u8 first_start;
    u8 speaker_pause;
    /* struct audio_decoder decoder; */
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    struct pcm_decoder pcm_dec;		// pcm解码句柄

#if ECHO_ENABLE
    ECHO_API_STRUCT *p_echo_hdl;
#endif
#if HOWLING_ENABLE
    HOWLING_API_STRUCT *p_howling_hdl;
#endif
#if PITCH_ENABLE
    s_pitch_hdl *p_pitch_hdl;
#endif
    struct channel_switch   *channel_zoom;
};
static struct s_speaker_hdl *speaker_hdl = NULL;
static u8 pcm_dec_maigc = 0;

//echo 参数
EF_REVERB_FIX_PARM effect_echo_fix_parm = {
    .wetgain = 2048,			////湿声增益：[0:4096]
    .drygain = 4096,				////干声增益: [0:4096]
    .sr = LOUND_SPEAKER_MIC_SAMPLE_RATE,		////采样率
    .max_ms = 200,				////所需要的最大延时，影响 need_buf 大小

};

ECHO_PARM_SET effect_echo_parm = {
    .delay = 200,				//回声的延时时间 0-300ms
    .decayval = 50,				// 0-70%
    .direct_sound_enable = 1,	//直达声使能  0/1
    .filt_enable = 0,			//发散滤波器使能
};

static void adc_output_to_buf(void *priv, s16 *data, int len)
{
    if ((!speaker_hdl) || (speaker_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (speaker_hdl->speaker_pause) {
        return;
    }
    int wlen = cbuf_write(&speaker_hdl->pcm_cbuf, data, len);
    if (!wlen) {
        putchar('W');
    }
    audio_decoder_resume(&speaker_hdl->pcm_dec.decoder);
}


static void pcm_dec_relaese()
{
    audio_decoder_task_del_wait(&decode_task, &speaker_hdl->wait);
}

static void pcm_dec_close(void)
{
    audio_decoder_close(&speaker_hdl->pcm_dec.decoder);
    audio_mixer_ch_close(&speaker_hdl->mix_ch);
    if (speaker_hdl->stream) {
        audio_stream_close(speaker_hdl->stream);
        speaker_hdl->stream = NULL;
    }
}

static int pcm_fread(void *hdl, void *buf, int len)
{
    struct s_speaker_hdl *dec = (struct s_speaker_hdl *)hdl;
    int rlen = cbuf_read(&dec->pcm_cbuf, buf, len);
    /* printf("rlen %d",rlen); */
    return rlen;
}

static void pcm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if ((u8)argv[1] != (u8)(pcm_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        /* pcm_dec_close(); */
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    pcm解码数据输出
   @param    *entry: 音频流句柄
   @param    *in: 输入信息
   @param    *out: 输出信息
   @return   输出长度
   @note     *out未使用
*/
/*----------------------------------------------------------------------------*/
static int pcm_dec_data_handler(struct audio_stream_entry *entry,
                                struct audio_data_frame *in,
                                struct audio_data_frame *out)
{
    struct audio_decoder *decoder = container_of(entry, struct audio_decoder, entry);
    struct pcm_decoder *pcm_dec = container_of(decoder, struct pcm_decoder, decoder);
    struct s_speaker_hdl *dec = container_of(pcm_dec, struct s_speaker_hdl, pcm_dec);
    audio_stream_run(&decoder->entry, in);
    return decoder->process_len;
}
/*----------------------------------------------------------------------------*/
/**@brief    pcm解码数据流激活
   @param    *p: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void dec_out_stream_resume(void *p)
{
    struct s_speaker_hdl *dec = (struct s_speaker_hdl *)p;

    audio_decoder_resume(&dec->pcm_dec.decoder);
}

static int pcm_dec_start(void)
{
    int err;
    struct audio_fmt f;
    struct s_speaker_hdl *dec = speaker_hdl;

    log_i("\n--func=%s\n", __FUNCTION__);
    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    pcm_decoder_set_event_handler(&dec->pcm_dec, pcm_dec_event_handler, 0);
    pcm_decoder_set_read_data(&dec->pcm_dec, pcm_fread, dec);
    pcm_decoder_set_data_handler(&dec->pcm_dec, pcm_dec_data_handler);

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 1);
    /* audio_mixer_ch_set_no_wait(&dec->mix_ch, 1, 10); // 超时自动丢数 */


// 数据流串联
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->pcm_dec.decoder.entry;
#if HOWLING_ENABLE
    entries[entry_cnt++] = &dec->p_howling_hdl->entry;
#endif

#if ECHO_ENABLE
    entries[entry_cnt++] = &dec->p_echo_hdl->entry;
#endif
#if PITCH_ENABLE
    entries[entry_cnt++] = &dec->p_pitch_hdl->entry;
    //for test pitch
    loundspeaker_set_pitch_para(66, LOUND_SPEAKER_MIC_SAMPLE_RATE, EFFECT_VOICECHANGE_KIN0, 150);
#endif
    if (dec->channel_zoom) {
        entries[entry_cnt++] = &dec->channel_zoom->entry;
    }

    entries[entry_cnt++] = &dec->mix_ch.entry;
    dec->stream = audio_stream_open(dec, dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);


    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    /* audio_adc_mic_start(&dec->mic_ch); */
    log_i("\n\n audio decoder start \n");
    dec->status = REVERB_STATUS_START;
    err = audio_decoder_start(&dec->pcm_dec.decoder);
    if (err) {
        goto __err3;
    }
    log_i("\n\n audio mic start  1 \n");
    return 0;
__err3:

    dec->status = 0;
    audio_mic_close(&dec->mic_ch, &dec->adc_output);
__err2:
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }

    pcm_decoder_close(&dec->pcm_dec);
__err1:
    audio_decoder_task_del_wait(&decode_task, &dec->wait);
    return err;
}



static int pcmdec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    if (!speaker_hdl) {
        log_e("speaker_hdl err \n");
        return -1;
    }
    log_i("pcmdec_wait_res_handler, event:%d;status:%d; \n", event, speaker_hdl->status);
    if (event == AUDIO_RES_GET) {
        if (speaker_hdl->status == REVERB_STATUS_STOP) {
            err = pcm_dec_start();
        } else if (speaker_hdl->status == REVERB_STATUS_PAUSE) {
            speaker_hdl->status = REVERB_STATUS_START;
        }
    } else if (event == AUDIO_RES_PUT) {
        if (speaker_hdl->status == REVERB_STATUS_START) {
            /* reverb_hdl->status = REVERB_STATUS_PAUSE; */
        }
    }
    return err;
}
//************************* MIC to DAC API *****************************//
void stop_loud_speaker(void)
{
    if (!speaker_hdl) {
        return;
    }
    log_i("\n--func=%s\n", __FUNCTION__);

    speaker_hdl->status = REVERB_STATUS_STOP;

    audio_mic_close(&speaker_hdl->mic_ch, &speaker_hdl->adc_output);
    pcm_dec_close();

#if ECHO_ENABLE
    close_echo(speaker_hdl->p_echo_hdl);
#endif
#if HOWLING_ENABLE
    close_howling(speaker_hdl->p_howling_hdl);
#endif
#if PITCH_ENABLE
    close_pitch(speaker_hdl->p_pitch_hdl);
#endif
    if (speaker_hdl->channel_zoom) {
        channel_switch_close(&speaker_hdl->channel_zoom);
    }
    pcm_dec_relaese();

    free(speaker_hdl);
    speaker_hdl = NULL;
    clock_remove_set(REVERB_CLK);
}
void start_loud_speaker(struct audio_fmt *fmt)
{
    struct s_speaker_hdl *reverb = NULL;
    int err;
    if (speaker_hdl) {
        stop_loud_speaker();
    }
    clock_add_set(REVERB_CLK);
    reverb = zalloc(sizeof(struct s_speaker_hdl));
    log_i("speaker hdl:%lu", sizeof(struct s_speaker_hdl));
    ASSERT(reverb);

    struct audio_fmt f = {0};
    if (fmt) {
        f.sample_rate = fmt->sample_rate;
    }
    if (f.sample_rate == 0) {
        f.sample_rate = LOUND_SPEAKER_MIC_SAMPLE_RATE;
    }
    f.channel = 1;

    reverb->source_ch_num = f.channel;
    reverb->mic_sr = f.sample_rate;
    reverb->mic_gain = 2;


#if ECHO_ENABLE
    reverb->p_echo_hdl = open_echo(&effect_echo_parm, &effect_echo_fix_parm);
    ASSERT(reverb->p_echo_hdl);
#endif
#if HOWLING_ENABLE
    reverb->p_howling_hdl = open_howling(NULL, f.sample_rate, audio_output_channel_num(), 1);
#endif


#if PITCH_ENABLE
    reverb->p_pitch_hdl = open_pitch(NULL);
    /* pause_pitch(reverb->p_pitch_hdl, 1); */
#endif

    if (audio_output_channel_num() != 1) {
        reverb->channel_zoom = channel_switch_open(audio_output_channel_type(), 1024);
    }
#if 1
    cbuf_init(&reverb->pcm_cbuf, reverb->pcm_buf, sizeof(reverb->pcm_buf));
    if (audio_mic_open(&reverb->mic_ch, f.sample_rate, reverb->mic_gain) == 0) {
        reverb->adc_output.handler = adc_output_to_buf;
        audio_mic_add_output(&reverb->adc_output);
        audio_mic_start(&reverb->mic_ch);
    }
#endif
    reverb->pcm_dec.ch_num = 1;
    reverb->pcm_dec.output_ch_num = 1;//audio_output_channel_num(); echo/pitch 只能处理单声道数据
    reverb->pcm_dec.output_ch_type = audio_output_channel_type();
    reverb->pcm_dec.sample_rate = reverb->mic_sr;

    reverb->wait.priority = 0;
    reverb->wait.preemption = 0;
    reverb->wait.protect = 1;
    reverb->wait.handler = pcmdec_wait_res_handler;
    speaker_hdl = reverb;
    err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);
    if (err == 0) {
        return ;
    }

    log_e("audio decoder task add wait err \n");
    audio_mic_close(&reverb->mic_ch, &reverb->adc_output);

#if ECHO_ENABLE
    close_echo(reverb->p_echo_hdl);
#endif
#if HOWLING_ENABLE
    close_howling(reverb->p_howling_hdl);
#endif
#if PITCH_ENABLE
    close_pitch(reverb->p_pitch_hdl);
#endif
    if (speaker_hdl->channel_zoom) {
        channel_switch_close(&speaker_hdl->channel_zoom);
    }
    clock_remove_set(REVERB_CLK);
}
/***************************************************************/
int speaker_if_working(void)
{
    if (speaker_hdl && (speaker_hdl->status == REVERB_STATUS_START)) {
        return 1;
    }
    return 0;
}


void loud_speaker_pause(void)
{
    if (speaker_hdl) {
        speaker_hdl->speaker_pause = 1;
        audio_mixer_ch_pause(&speaker_hdl->mix_ch, 1);
        printf("speaker_pause [%d]", speaker_hdl->speaker_pause);
    }
}
void loud_speaker_resume(void)
{
    if (speaker_hdl) {
        audio_mixer_ch_pause(&speaker_hdl->mix_ch, 0);
        speaker_hdl->speaker_pause = 0;
        printf("speaker_pause [%d]", speaker_hdl->speaker_pause);
    }
}

/****************************效果设置接口***********************************/

void switch_holwing_en(void)
{
    if (speaker_hdl) {
        speaker_hdl->howling_en ^= 1;
#if HOWLING_ENABLE
        if (speaker_hdl->p_howling_hdl) {
            pause_howling(speaker_hdl->p_howling_hdl, speaker_hdl->howling_en);
            printf("howling_en [%d]", speaker_hdl->howling_en);
        }
#endif
    }
}

void switch_echo_en(void)
{
    if (speaker_hdl) {
        speaker_hdl->reverb_en ^= 1;
        printf("reverb_en [%d]", speaker_hdl->reverb_en);
#if ECHO_ENABLE
        if (speaker_hdl->reverb_en) {
            pause_echo(speaker_hdl->p_echo_hdl, speaker_hdl->reverb_en);
        }
#endif
    }

}

/*----------------------------------------------------------------------------*/
/**@brief    loundspeaker echo 回声延时调节接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void loundspeaker_set_echo_delay(u32 delay)
{
#if ECHO_ENABLE
    if (speaker_hdl == NULL || speaker_hdl->p_echo_hdl == NULL) {
        return ;
    }
    effect_echo_parm.delay = delay;
    update_echo_parm(speaker_hdl->p_echo_hdl, &effect_echo_parm);
#endif
}
u32 loundspeaker_get_echo_delay(void)
{
#if ECHO_ENABLE
    if (speaker_hdl && speaker_hdl->p_echo_hdl) {
        return effect_echo_parm.delay;
    }
#endif
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief   loundspeaker echo 回声衰减系数调节接口
  @param
  @return
  @note
  */
/*----------------------------------------------------------------------------*/
void loundspeaker_set_echo_decay(u32 decay)
{
#if ECHO_ENABLE
    if (speaker_hdl == NULL || speaker_hdl->p_echo_hdl == NULL) {
        return ;
    }
    effect_echo_parm.decayval = decay;
    update_echo_parm(speaker_hdl->p_echo_hdl, &effect_echo_parm);
#endif
}

u32 loundspeaker_get_echo_decay(void)
{
#if ECHO_ENABLE
    if (speaker_hdl && speaker_hdl->p_echo_hdl) {
        return effect_echo_parm.decayval;
    }
#endif
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    loundspeaker 变声参数设置
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void loundspeaker_set_pitch_para(u32 shiftv, u32 sr, u8 effect, u32 formant_shift)
{
#if PITCH_ENABLE
    if (speaker_hdl == NULL || speaker_hdl->p_pitch_hdl == NULL) {
        return ;
    }
    PITCH_SHIFT_PARM p_pitch_parm = {0};//get_pitch_parm();

    p_pitch_parm.sr = sr;
    p_pitch_parm.shiftv = shiftv;
    p_pitch_parm.effect_v = effect;
    p_pitch_parm.formant_shift = formant_shift;

    printf("\n\n\nshiftv[%d],sr[%d],effect[%d],formant_shift[%d] \n\n", p_pitch_parm.shiftv, p_pitch_parm.sr, p_pitch_parm.effect_v, p_pitch_parm.formant_shift);
    update_picth_parm(speaker_hdl->p_pitch_hdl, &p_pitch_parm);
#endif
}
#endif /*TCFG_LOUDSPEAKER_ENABLE*/

