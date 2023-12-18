#include "audio_splicing.h"
#include "application/audio_bfilt.h"
#include "media/audio_eq_drc_apply.h"
#include "application/audio_echo_src.h"
#include "application/audio_energy_detect.h"
#include "clock_cfg.h"
#include "media/audio_stream.h"
#include "media/includes.h"
#include "mic_effect.h"
#include "asm/dac.h"
#include "audio_enc/audio_enc.h"
#include "audio_dec.h"
#include "stream_entry.h"
#include "effect_linein.h"
#include "audio_recorder_mix.h"
#include "app_task.h"
#include "amplitude_statistic.h"
#include "media/effects_adj.h"
#include "audio_effect/audio_eff_default_parm.h"
#include "audio_effect/audio_autotune_demo.h"
#include "media/convert_data.h"
#include "simpleAGC.h"

#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

#ifdef SUPPORT_MS_EXTENSIONS
//#pragma bss_seg(".audio_mic_stream_bss")
//#pragma data_seg(".audio_mic_stream_data")
#pragma const_seg(".audio_mic_effect_const")
#pragma code_seg(".audio_mic_effect_code")
#endif

#if defined(TCFG_MIC_EFFECT_ENABLE) && TCFG_MIC_EFFECT_ENABLE

#define  LOUDNESS_DEBUG_ENABLE 1//分贝指示器



struct aud_reverb_process {
    struct audio_stream_entry entry;	// 音频流入口
    s16 *tmpbuf[3];
    void *eff;//struct __mic_effect
    s16 *out_buf;
    u8 bit_wide;//eq是否输出32bit位宽
    u8 in_ch;
};

struct __mic_effect {
    OS_MUTEX				 		mutex;
    struct __mic_effect_parm     	parm;
    mic_stream 						*mic;
    struct audio_eq             *mic_eq0;    //eq 句柄
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
    struct audio_eq             *mic_tunning_eq;    //eq 句柄
#endif
    struct audio_drc             *mic_drc0;    //eq 句柄
    struct convert_data             *convert0;//32-》16

    struct audio_eq             *mic_eq4;    //eq drc句柄
    struct audio_drc             *mic_drc4;    //eq drc句柄
    struct convert_data             *convert4;//32-》16
    struct aud_gain_process     *gain;//最后一级 的增益调节

    struct audio_stream *stream;		// 音频流
    struct audio_stream_entry entry;	// effect 音频入口
    int out_len;
    int process_len;
    u8 input_ch_num;                      //mic输入给混响数据流的源声道数

    NOISEGATE_API_STRUCT	*noisegate;
    void            		*d_vol;
    HOWLING_API_STRUCT 		*howling_ps;
    HOWLING_API_STRUCT 		*notch_howling;
    voice_changer_hdl       *voice_changer;

#if defined(TCFG_MIC_AUTOTUNE_ENABLE) && TCFG_MIC_AUTOTUNE_ENABLE
    autotune_hdl       *autotune;
#endif

    struct channel_switch   *channel_zoom;
#if (RECORDER_MIX_EN)
    struct __stream_entry 	*rec_hdl;
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    struct __stream_entry 	*usbmic_hdl;
    u8    usbmic_start;
#endif
    void *energy_hdl;     //能量检测的hdl
    u8 dodge_en;          //能量检测运行过程闪避是否使能

    struct __effect_linein *linein;
    struct audio_drc       *drc;


    u8  pause_mark;
    LOUDNESS_M_STRUCT loudness_adc;
    LOUDNESS_M_STRUCT loudness_drc_out;
    struct __stream_entry 	*loudness_debug_hdl;

    /* struct audio_src_handle *src_hdl; */

    struct audio_eq     *mic_eq3;    //eq drc句柄
    struct audio_drc     *mic_drc3;    //eq drc句柄

    u8                      main_pause;
    //分支0节点
    PLATE_REVERB_API_STRUCT		*sub_0_plate_reverb_hdl;
    struct audio_eq     *mic_eq1;    //eq drc句柄
    struct audio_drc     *mic_drc1;    //eq drc句柄

    //分支1节点
    ECHO_API_STRUCT 		*sub_1_echo_hdl;
    struct audio_eq     *mic_eq2;    //eq drc句柄
    struct audio_drc     *mic_drc2;    //eq drc句柄


    //分支1 2 3的总处理
    struct aud_reverb_process *aud_reverb;
};

struct __mic_stream_parm *g_mic_parm = NULL;
static struct __mic_effect *p_effect = NULL;
#define __this  p_effect
#define R_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

void *mic_eq_open(u32 sample_rate, u8 ch_num, u8 eq_name);
void *mic_drc_open(u32 sample_rate, u8 ch_num, u8 eq_name);
void mic_eq_close(void *eq);
void mic_drc_close(void  *drc);
void mic_effect_echo_parm_parintf(ECHO_PARM_SET *parm);
void *mic_energy_detect_open(u32 sr, u8 ch_num);
void mic_energy_detect_close(void *hdl);
void *mic_tunning_eq_open(u32 sample_rate, u8 ch_num);
void mic_tunning_eq_close(void *eq);



s16 *aud_reverb_process_run(struct aud_reverb_process *hdl, s16 *data, int len);
static void audio_reverb_process_output_data_process_len(struct audio_stream_entry *entry,  int len)
{
    struct aud_reverb_process *hdl = container_of(entry, struct aud_reverb_process, entry);
}
static int audio_reverb_process_data_handler(struct audio_stream_entry *entry,
        struct audio_data_frame *in,
        struct audio_data_frame *out)
{
    struct aud_reverb_process *hdl = container_of(entry, struct aud_reverb_process, entry);
    /* if (in->data_len != 304) { */
    /* printf("in->data_len %d\n", in->data_len); */
    /* } */

    out->data_sync = in->data_sync;

    struct __mic_effect  *eff = hdl->eff;
    if (eff->sub_0_plate_reverb_hdl) {
        out->channel = 2;//in->channel;
    } else {
        out->channel = in->channel;
    }
    out->data_len = out->channel * in->data_len;
    hdl->in_ch = in->channel;
    out->data = aud_reverb_process_run(hdl, (short *)((int)in->data + in->offset), (in->data_len - in->offset)); //默认输出2ch
    if (out->channel == 1) {
        pcm_dual_to_single((void *)out->data, (void *)out->data, in->data_len * 2);
    }
    return in->data_len;
}


struct aud_reverb_process *aud_reverb_open(struct __mic_effect *eff, u8 bit_wide)
{
    struct aud_reverb_process *hdl = zalloc(sizeof(struct aud_reverb_process));
    hdl->eff = eff;
    hdl->bit_wide = bit_wide;
    hdl->entry.data_process_len = audio_reverb_process_output_data_process_len;
    hdl->entry.data_handler = audio_reverb_process_data_handler;

    return hdl;
}

void aud_reverb_close(struct aud_reverb_process *hdl)
{
    if (!hdl) {
        return;
    }
    local_irq_disable();
    audio_stream_del_entry(&hdl->entry);
    local_irq_enable();
    for (int i = 0; i < 3; i++) {
        if (hdl->tmpbuf[i]) {
            free(hdl->tmpbuf[i]);
            hdl->tmpbuf[i] = NULL;
        }
    }
    free(hdl);
    hdl = NULL;
}

void pcm_single_to_dual_32bit(void *out, void *in, u16 len)
{
    s32 *outbuf = out;
    s32 *inbuf = in;
    len >>= 2;
    while (len--) {
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        inbuf++;
    }
}

s16 *aud_reverb_process_run(struct aud_reverb_process *hdl, s16 *data, int len)
{
    struct __mic_effect  *eff = hdl->eff;
    u8 ch_num = eff->input_ch_num;
    if (eff->sub_0_plate_reverb_hdl) {
        //ch0
        if (hdl->in_ch == 2) {
            if (!hdl->tmpbuf[0]) {
                hdl->tmpbuf[0] = malloc(len);//2ch 16 bit buf
            }
            u8 *tmp = (u8 *)hdl->tmpbuf[0];
            s16 *tar = (s16 *)&tmp[len / 2];
            pcm_dual_to_single((void *)tar, (void *)data, len);
            run_plate_reverb(eff->sub_0_plate_reverb_hdl, tar, hdl->tmpbuf[0], len / 2); //内部单变双
        } else {
            if (!hdl->bit_wide) { //16 bit
                if (!hdl->tmpbuf[0]) {
                    hdl->tmpbuf[0] = malloc(len * 2);//2ch 16 bit buf
                }
                run_plate_reverb(eff->sub_0_plate_reverb_hdl, data, hdl->tmpbuf[0], len); //16bit 内部单变双
                audio_dec_eq_run(eff->mic_eq1, hdl->tmpbuf[0], hdl->tmpbuf[0], len * 2);// 16bit 2ch
                audio_dec_drc_run(eff->mic_drc1, hdl->tmpbuf[0], len * 2); //16bit 2ch
            } else {
                if (!hdl->tmpbuf[0]) {
                    hdl->tmpbuf[0] = malloc(len * 2 * 2);//2ch 32 bit buf
                }
                u8 *tmp = (u8 *)hdl->tmpbuf[0];
                s16 *tar = (s16 *)&tmp[len * 2 * 2 - 2 * len]; //放在最后 16bit 2ch
                run_plate_reverb(eff->sub_0_plate_reverb_hdl, data, tar, len); //16 bit 内部单变双
                audio_dec_eq_run(eff->mic_eq1, tar, hdl->tmpbuf[0], len * 2);// 32bit 2ch out
                audio_dec_drc_run(eff->mic_drc1, hdl->tmpbuf[0], len * 2 * 2); //32bit 2ch
            }
        }
    }

    //ch1
    u32 points = 0;
    if (hdl->in_ch == 1) {
        if (!hdl->bit_wide) { //16 bit

            u8 *tmp = NULL;
            s16 *tar = NULL;
            if (eff->sub_1_echo_hdl) {
                if (!hdl->tmpbuf[1]) {
                    hdl->tmpbuf[1] = malloc(len * 2);//2ch 16 bit buf
                }
                //ch1
                tmp = (u8 *)hdl->tmpbuf[1];
                tar = (s16 *)&tmp[len];
                run_echo(eff->sub_1_echo_hdl, data, tar, len);
                audio_dec_eq_run(eff->mic_eq2, tar, NULL, len); //1ch
                audio_dec_drc_run(eff->mic_drc2, tar, len); //16bit 1ch
                pcm_single_to_dual((void *)hdl->tmpbuf[1], (void *)tar,  len);//16bit 2ch
            }

            if (!hdl->tmpbuf[2]) {
                hdl->tmpbuf[2] = malloc(len * 2);//2ch 16 bit buf
            }
            //ch2
            audio_dec_eq_run(eff->mic_eq3, data, NULL, len);//1ch
            audio_dec_drc_run(eff->mic_drc3, data, len); //16bit 1ch
            pcm_single_to_dual((void *)hdl->tmpbuf[2], (void *)data,  len);//16bit 2ch
            points = (len * 2) / 2;
        } else {
            u8 *tmp = NULL;
            s16 *tar = NULL;
            if (eff->sub_1_echo_hdl) {
                if (!hdl->tmpbuf[1]) {
                    hdl->tmpbuf[1] = malloc(len * 2 * 2); //2ch 32 bit buf
                }
                //ch1
                tmp = (u8 *)hdl->tmpbuf[1];
                tar = (s16 *)&tmp[len * 2 * 2 - len]; //放在最后 16bit 1ch
                s16 *tar2 = (s16 *)&tmp[len * 2]; //buf中间位置
                run_echo(eff->sub_1_echo_hdl, data, tar, len);
                audio_dec_eq_run(eff->mic_eq2, tar, tar2, len); //16 ->32 1ch
                audio_dec_drc_run(eff->mic_drc2, tar2, len * 2); //32bit 1ch
                pcm_single_to_dual_32bit(hdl->tmpbuf[1], tar2, len * 2); //32bit 2ch out
            }

            //ch2
            if (!hdl->tmpbuf[2]) {
                hdl->tmpbuf[2] = malloc(len * 2 * 2); //2ch 32 bit buf
            }
            tmp = (u8 *)hdl->tmpbuf[2];
            tar = (s16 *)&tmp[len * 2]; //buf中间位置
            audio_dec_eq_run(eff->mic_eq3, data, tar, len);//16->32bit 1ch
            audio_dec_drc_run(eff->mic_drc3, tar, len * 2); //32bit 1ch
            pcm_single_to_dual_32bit(hdl->tmpbuf[2], tar,  len * 2); //32bit 2ch

            points = (len * 2 * 2) / 4;
        }
    } else {
        if (!hdl->bit_wide) { //16 bit

            u8 *tmp = NULL;
            s16 *tar = NULL;
            if (eff->sub_1_echo_hdl) {
                if (!hdl->tmpbuf[1]) {
                    hdl->tmpbuf[1] = malloc(len);//2ch 16 bit buf
                }
                tmp = (u8 *)hdl->tmpbuf[1];
                tar = (s16 *)&tmp[len / 2];
                pcm_dual_to_single((void *)tar, (void *)data, len);
                run_echo(eff->sub_1_echo_hdl, tar, tar, len / 2);
                pcm_single_to_dual((void *)tmp, (void *)tar,  len / 2);
                audio_dec_eq_run(eff->mic_eq2, (s16 *)tmp, hdl->tmpbuf[1], len); //立体声处理2ch
                audio_dec_drc_run(eff->mic_drc2, hdl->tmpbuf[1], len); //16bit 2ch
            }

            //ch2
            if (!hdl->tmpbuf[2]) {
                hdl->tmpbuf[2] = malloc(len);//2ch 16bit buf
            }
            audio_dec_eq_run(eff->mic_eq3, data, hdl->tmpbuf[2], len);
            audio_dec_drc_run(eff->mic_drc3, hdl->tmpbuf[2], len); //16bit 2ch
            points = len / 2;
        } else {

            u8 *tmp = NULL;
            s16 *tar = NULL;
            if (eff->sub_1_echo_hdl) {
                if (!hdl->tmpbuf[1]) {
                    hdl->tmpbuf[1] = malloc(len * 2); //2ch 32 bit buf
                }
                //ch1
                tmp = (u8 *)hdl->tmpbuf[1];
                tar = (s16 *)&tmp[len * 2 - len / 2];
                pcm_dual_to_single((void *)tar, (void *)data, len);
                run_echo(eff->sub_1_echo_hdl, tar, tar, len / 2);
                pcm_single_to_dual((void *)tmp, (void *)tar,  len / 2);
                audio_dec_eq_run(eff->mic_eq2, (s16 *)tmp, hdl->tmpbuf[1], len); //立体声处理2ch
                audio_dec_drc_run(eff->mic_drc2, hdl->tmpbuf[1], len * 2); //32bit 2ch
            }

            //ch2
            if (!hdl->tmpbuf[2]) {
                hdl->tmpbuf[2] = malloc(len * 2 * 2); //2ch 32 bit buf
            }
            audio_dec_eq_run(eff->mic_eq3, data, hdl->tmpbuf[2], len);
            audio_dec_drc_run(eff->mic_drc3, hdl->tmpbuf[2], len * 2); //32bit 2ch
            points = (len * 2) / 4;
        }
    }
    MixParam mix0  = {0};
    MixParam mix1  = {0};
    MixParam mix2  = {0};

    u8 mode = get_mic_eff_mode();
    Mix_TOOL_SET *mix_gain = &eff_mode[mode].mix_gain;
    mix0.data = hdl->tmpbuf[0];
    mix0.gain = mix_gain->gain1;
    mix1.data = hdl->tmpbuf[1];
    mix1.gain = mix_gain->gain2;
    mix2.data = hdl->tmpbuf[2];
    mix2.gain = mix_gain->gain3;
    u8 out_ch_num = 2;
    if (!hdl->bit_wide) { //16 bit
        if (eff->sub_1_echo_hdl && eff->sub_0_plate_reverb_hdl) {
            Mix16to16(&mix0, &mix1, &mix2, hdl->tmpbuf[0], out_ch_num, 3, points / out_ch_num);
        } else if (eff->sub_0_plate_reverb_hdl) {
            Mix16to16(&mix0, &mix2, NULL, hdl->tmpbuf[0], out_ch_num, 2, points / out_ch_num);
        } else {
            Mix16to16(&mix1, &mix2, NULL, hdl->tmpbuf[1], out_ch_num, 2, points / out_ch_num);
            return hdl->tmpbuf[1];
        }
        return hdl->tmpbuf[0];
    } else {
        if (eff->sub_1_echo_hdl && eff->sub_0_plate_reverb_hdl) {
            Mix32to16(&mix0, &mix1, &mix2, hdl->tmpbuf[0], out_ch_num, 3, points / out_ch_num);
        } else if (eff->sub_0_plate_reverb_hdl) {
            Mix32to16(&mix0, &mix2, NULL, hdl->tmpbuf[0], out_ch_num, 2, points / out_ch_num);
        } else {
            Mix32to16(&mix1, &mix2, NULL, hdl->tmpbuf[1], out_ch_num, 2, points / out_ch_num);
            return hdl->tmpbuf[1];
        }
    }
    return hdl->tmpbuf[0];
}


/*----------------------------------------------------------------------------*/
/**@brief    mic数据流串接入口
  @param
  @return
  @note
*/
/*----------------------------------------------------------------------------*/
static u32 mic_effect_effect_run(void *priv, void *in, void *out, u32 inlen, u32 outlen)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;
    if (effect == NULL) {
        return 0;
    }
    struct audio_data_frame frame = {0};
    frame.channel = effect->input_ch_num;
    frame.sample_rate = effect->parm.sample_rate;
    frame.data_len = inlen;
    frame.data = in;
    effect->out_len = 0;
    effect->process_len = inlen;

    if (effect->pause_mark) {
        memset(in, 0, inlen);
        return inlen;
    } else {

    }
#if LOUDNESS_DEBUG_ENABLE
    loudness_meter_short(&effect->loudness_adc, in, inlen / 2);
#endif
    while (1) {
        audio_stream_run(&effect->entry, &frame);
        if (effect->out_len >= effect->process_len) {
            break;
        }
        frame.data = (s16 *)((u8 *)in + effect->out_len);
        frame.data_len = inlen - effect->out_len;
    }
    return outlen;
}

/*----------------------------------------------------------------------------*/
/**@brief   释放mic数据流资源
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_destroy(struct __mic_effect **hdl)
{
    if (hdl == NULL || *hdl == NULL) {
        return ;
    }
    struct __mic_effect *effect = *hdl;
    if (effect->mic) {
        log_i("mic_stream_destroy\n\n\n");
        mic_stream_destroy(&effect->mic);
    }

#if TCFG_MIC_DODGE_EN
    if (effect->energy_hdl) {
        mic_energy_detect_close(effect->energy_hdl);
    }
#endif

    if (effect->noisegate) {
        log_i("close_noisegate\n\n\n");
        audio_noisegate_close(effect->noisegate);
    }


    if (effect->howling_ps) {
        log_i("close_howling\n\n\n");
        close_howling(effect->howling_ps);
    }
    if (effect->notch_howling) {
        log_i("close_howling\n\n\n");
        close_howling(effect->notch_howling);
    }
#if defined(TCFG_MIC_VOICE_CHANGER_ENABLE) && TCFG_MIC_VOICE_CHANGER_ENABLE
    if (effect->voice_changer) {
        audio_voice_changer_close_demo(effect->voice_changer);
    }
#endif
#if defined(TCFG_MIC_AUTOTUNE_ENABLE) && TCFG_MIC_AUTOTUNE_ENABLE
    if (effect->autotune) {
        audio_autotune_close_demo(effect->autotune);
    }
#endif
    if (effect->mic_eq0) {
        log_i("mic_eq0_close\n\n\n");
        mic_eq_close(effect->mic_eq0);
    }
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
    if (effect->mic_tunning_eq) {
        mic_tunning_eq_close(effect->mic_tunning_eq);
    }
#endif
    if (effect->mic_drc0) {
        log_i("mic_drc0_close\n\n\n");
        mic_drc_close(effect->mic_drc0);
    }
    if (effect->convert0) {
        log_i("convet0_data_close\n\n\n");
        convet_data_close(effect->convert0);
    }

    if (effect->mic_eq1) {
        log_i("mic_eq1_close\n\n\n");
        mic_eq_close(effect->mic_eq1);
    }
    if (effect->mic_drc1) {
        log_i("mic_drc1_close\n\n\n");
        mic_drc_close(effect->mic_drc1);
    }

    if (effect->mic_eq2) {
        log_i("mic_eq2_close\n\n\n");
        mic_eq_close(effect->mic_eq2);
    }
    if (effect->mic_drc2) {
        log_i("mic_drc2_close\n\n\n");
        mic_drc_close(effect->mic_drc2);
    }

    if (effect->mic_eq3) {
        log_i("mic_eq3_close\n\n\n");
        mic_eq_close(effect->mic_eq3);
    }
    if (effect->mic_drc3) {
        log_i("mic_drc3_close\n\n\n");
        mic_drc_close(effect->mic_drc3);
    }

    if (effect->mic_eq4) {
        log_i("mic_eq4_close\n\n\n");
        mic_eq_close(effect->mic_eq4);
    }
    if (effect->mic_drc4) {
        log_i("mic_drc4_close\n\n\n");
        mic_drc_close(effect->mic_drc4);
    }

    if (effect->convert4) {
        log_i("convet4_data_close\n\n\n");
        convet_data_close(effect->convert4);
    }
    /* if (effect->gain) { */
    /* log_i("audio_gain_close_demo\n\n\n"); */
    /* audio_gain_close_demo(effect->gain); */
    /* effect->gain = NULL; */
    /* } */
    if (effect->sub_1_echo_hdl) {
        close_echo(effect->sub_1_echo_hdl);
    }
    if (effect->sub_0_plate_reverb_hdl) {
        close_plate_reverb(effect->sub_0_plate_reverb_hdl);
    }
    if (effect->d_vol) {
        audio_stream_del_entry(audio_dig_vol_entry_get(effect->d_vol));
#if defined(SYS_DIGVOL_GROUP_EN) && SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("mic_mic");
#else
        audio_dig_vol_close(effect->d_vol);
#endif/*SYS_DIGVOL_GROUP_EN*/
    }
    if (effect->linein) {
        effect_linein_close(&effect->linein);
    }

    /* if (effect->src_hdl) { */
    /* audio_stream_del_entry(&effect->src_hdl->entry); */
    /* audio_hw_src_stop(effect->src_hdl); */
    /* audio_hw_src_close(effect->src_hdl); */
    /* free(effect->src_hdl); */
    /* effect->src_hdl = NULL; */
    /* } */
    if (effect->channel_zoom) {
        channel_switch_close(&effect->channel_zoom);
        /*effect->channel_zoom = NULL;*/
    }
    if (effect->loudness_debug_hdl) {
        stream_entry_close(&effect->loudness_debug_hdl);
    }

#if (RECORDER_MIX_EN)
    if (effect->rec_hdl) {
        stream_entry_close(&effect->rec_hdl);
    }
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    if (effect->usbmic_hdl) {
        stream_entry_close(&effect->usbmic_hdl);
    }
#endif
    if (effect->aud_reverb) {
        aud_reverb_close(effect->aud_reverb);
    }

    audio_reverb_stream_dac_uninit();
    if (effect->stream) {
        audio_stream_close(effect->stream);
    }
    local_irq_disable();
    free(effect);
    *hdl = NULL;
    local_irq_enable();

    mem_stats();
    clock_remove_set(REVERB_CLK);
}
/*----------------------------------------------------------------------------*/
/**@brief    串流唤醒
   @param
   @return
   @note 暂未使用
*/
/*----------------------------------------------------------------------------*/
static void mic_stream_resume(void *p)
{
    struct __mic_effect *effect = (struct __mic_effect *)p;
    /* audio_decoder_resume_all(&decode_task); */
}

/*----------------------------------------------------------------------------*/
/**@brief    串流数据处理长度回调
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void mic_effect_data_process_len(struct audio_stream_entry *entry, int len)
{

    struct __mic_effect *effect = container_of(entry, struct __mic_effect, entry);
    effect->out_len += len;
    /* printf("out len[%d]",effect->out_len); */
}

static int mic_effect_record_stream_callback(void *priv, struct audio_data_frame *in)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;

    s16 *data = in->data;
    u32 len = in->data_len;

    return recorder_mix_pcm_write((u8 *)data, len);
}
extern int usb_audio_mic_write_do(void *data, u16 len);
static int mic_effect_otherout_stream_callback(void *priv, struct audio_data_frame *in)
{
    struct __mic_effect *effect = (struct __mic_effect *)priv;
    s16 *data = in->data;
    u32 len = in->data_len;

#if ((TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC))
    if (effect->usbmic_start) {
        if (len) {
            usb_audio_mic_write_do(data, len);
        }
    } else {
    }
#endif
    return len;
}

void mic_effect_to_usbmic_onoff(u8 mark)
{
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    if (__this) {
        __this->usbmic_start = mark ? 1 : 0;
    }
#endif
}

static void  inline rl_rr_mix_to_rl_rr(short *data, int len)
{
    s32 tmp32_1;
    s32 tmp32_2;
    s16 *inbuf = data;
    inbuf = inbuf + 2;  //定位到第三通道
    len >>= 3;
    __asm__ volatile(
        "1:                      \n\t"
        "rep %0 {                \n\t"
        "  %2 = h[%1 ++= 2](s)     \n\t"  //取第三通道值，并地址偏移两个字节指向第四通道数据
        " %3 = h[%1 ++= -2](s)   \n\t"   //取第四通道值，并地址偏移两个字节指向第三通道数据
        " %2 = %2 + %3           \n\t"
        " %2 = sat16(%2)(s)      \n\t"  //饱和处理
        " h[%1 ++= 2] = %2      \n\t"  //存取第三通道数据，并地址偏移两个字节指向第四通道数据
        " h[%1 ++= 6] = %2      \n\t"  //存取第四通道数据，并地址偏移六个字节指向第三通道相邻的数据
        "}                      \n\t"
        "if(%0 != 0) goto 1b    \n\t"
        :
        "=&r"(len),
        "=&r"(inbuf),
        "=&r"(tmp32_1),
        "=&r"(tmp32_2)
        :
        "0"(len),
        "1"(inbuf),
        "2"(tmp32_1),
        "3"(tmp32_2)
        :
    );
}
static int effect_to_dac_data_pro_handle(struct audio_stream_entry *entry,  struct audio_data_frame *in)
{
#if (SOUNDCARD_ENABLE)
    if (in->data_len == 0) {
        return 0;
    }
#if 1
    rl_rr_mix_to_rl_rr(in->data, in->data_len);//汇编加速
#else
    s16 *outbuf = in->data;
    s16 *inbuf = in->data;
    s32 tmp32;
    u16 len = in->data_len;
    len >>= 3;
    while (len--) {
        *outbuf++ = inbuf[0];
        *outbuf++ = inbuf[1];
        tmp32 = (inbuf[2] + inbuf[3]);
        if (tmp32 < -32768) {
            tmp32 = -32768;
        } else if (tmp32 > 32767) {
            tmp32 = 32767;
        }
        *outbuf++ = tmp32;
        *outbuf++ = tmp32;
        inbuf += 4;
    }
#endif
#endif
    return 0;
}

#if TCFG_APP_RECORD_EN
static int prob_handler_to_record(struct audio_stream_entry *entry,  struct audio_data_frame *in)
{
    if (app_get_curr_task() != APP_RECORD_TASK) {
        return 0;
    }
    if (in->data_len == 0) {
        return 0;
    }
    if (recorder_is_encoding() == 0) {
        return 0;
    }
    int  wlen = recorder_userdata_to_enc(in->data, in->data_len);
    if (wlen != in->data_len) {
        putchar('N');
    }
    return 0;
}
#endif

int audio_data_check_cb(void *priv,  struct audio_data_frame *data_buf)
{
    if (__this) {
        loudness_meter_short(&__this->loudness_drc_out, data_buf->data, data_buf->data_len / 2);
    }
    return data_buf->data_len;
}

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)
#define DAC_OUTPUT_CHANNELS     2
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_L || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_R || TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR_DIFF)
#define DAC_OUTPUT_CHANNELS     1
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
#define DAC_OUTPUT_CHANNELS     4
#else
#define DAC_OUTPUT_CHANNELS     3
#endif

static u8 reverb_mode = 0;
static u8 voicechange_mode = 0;
void switch_mic_effect_mode(void)
{
    reverb_mode++;
    if (reverb_mode > 2) {
        reverb_mode = 0;
    }
    printf("\n--func=%s [%d] \n", __FUNCTION__, reverb_mode);

    set_mic_reverb_mode_by_id(reverb_mode);
}
/*----------------------------------------------------------------------------*/
/**@brief    (mic数据流)混响打开接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
bool mic_effect_start(void)
{
    bool ret = false;
    mem_stats();
    printf("\n--func=%s\n", __FUNCTION__);
    if (__this) {
        log_e("reverb is already start \n");
        return ret;
    }
    struct __mic_effect *effect = (struct __mic_effect *)zalloc(sizeof(struct __mic_effect));
    if (effect == NULL) {
        return false;
    }
    u8 mode = get_mic_eff_mode();
    struct eff_parm *mic_eff = &eff_mode[mode];

    clock_add_set(REVERB_CLK);
    os_mutex_create(&effect->mutex);
    memcpy(&effect->parm, &effect_parm_default, sizeof(struct __mic_effect_parm));
    struct __mic_stream_parm *mic_parm = (struct __mic_stream_parm *)&effect_mic_stream_parm_default;
    if (g_mic_parm) {
        mic_parm = g_mic_parm;
    }

#if LOUDNESS_DEBUG_ENABLE
    loudness_meter_init(&effect->loudness_adc, effect_mic_stream_parm_default.sample_rate, 50, 1);
    loudness_meter_init(&effect->loudness_drc_out, effect_mic_stream_parm_default.sample_rate, 50, 2);
    effect->loudness_debug_hdl = stream_entry_open(effect, audio_data_check_cb, 0);// 能量计算及打印节点
#endif

    /* if ((TCFG_MIC_EFFECT_SEL & MIC_EFFECT_ECHO)
        && (TCFG_MIC_EFFECT_SEL & MIC_EFFECT_REVERB)) {
        log_e("effect config err ?? !!!, cann't support echo && reverb at the same time\n");
        mic_effect_destroy(&effect);
        return false;
    } */

    u16 irq_points = mic_parm->point_unit;
    u8 ch_num = 1; //??
    effect->input_ch_num = ch_num;
#if TCFG_MIC_DODGE_EN
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_ENERGY_DETECT)) {
        effect->energy_hdl = mic_energy_detect_open(effect->parm.sample_rate, ch_num);
        effect->dodge_en = 0;//默认关闭， 需要通过按键触发打开
    }
#endif
    ///声音门限初始化
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_NOISEGATE)) {
        effect->noisegate = audio_noisegate_open_demo(AEID_MIC_NS_GATE, MIC_EFFECT_SAMPLERATE, ch_num);
    }

///啸叫抑制初始化
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_HOWLING)) {
        log_i("open_howling\n\n\n");
        effect->howling_ps = open_howling(NULL, effect->parm.sample_rate, 0, 1);//以频
    }

    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_HOWLING_TRAP)) {
        log_i("open_howling\n\n\n");

        HOWLING_PARM_SET howling_param = {
            .threshold = mic_eff->notchhowling_parm.parm.threshold,
            .fade_time = mic_eff->notchhowling_parm.parm.fade_n,
            .notch_Q = mic_eff->notchhowling_parm.parm.Q,
            .notch_gain = mic_eff->notchhowling_parm.parm.gain,
            .sample_rate = MIC_EFFECT_SAMPLERATE,
            .channel = 1,

        };
        effect->notch_howling = open_howling(&howling_param, effect->parm.sample_rate, 0, 0);//陷波
    }

#if defined(TCFG_MIC_VOICE_CHANGER_ENABLE) && TCFG_MIC_VOICE_CHANGER_ENABLE
    effect->voice_changer = audio_voice_changer_open_demo(AEID_MIC_VOICE_CHANGER, effect->parm.sample_rate);
#endif
#if defined(TCFG_MIC_AUTOTUNE_ENABLE) && TCFG_MIC_AUTOTUNE_ENABLE
    effect->autotune = audio_autotune_open_demo(AEID_MIC_AUTOTUNE, effect->parm.sample_rate);
#endif
    log_i("effect->parm.sample_rate %d\n", effect->parm.sample_rate);
    effect->mic_eq0 = mic_eq_open(effect->parm.sample_rate, ch_num, AEID_MIC_EQ0);
    if (effect->mic_eq0 && effect->mic_eq0->out_32bit) {
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
        effect->mic_tunning_eq = mic_tunning_eq_open(effect->parm.sample_rate, ch_num);
#endif
        effect->mic_drc0 = mic_drc_open(effect->parm.sample_rate, ch_num, AEID_MIC_DRC0);
        effect->convert0 = convet_data_open(0, (irq_points << 1) * ch_num);
    }

    effect->mic_eq3 = mic_eq_open(effect->parm.sample_rate, ch_num, AEID_MIC_EQ3);
    if (effect->mic_eq3 && effect->mic_eq3->out_32bit) {
        effect->mic_drc3 = mic_drc_open(effect->parm.sample_rate, ch_num, AEID_MIC_DRC3);
    }

// sub 0 分流节点
    if (TCFG_MIC_EFFECT_SEL & MIC_EFFECT_ECHO) {
        effect->sub_1_echo_hdl = open_echo((ECHO_PARM_SET *)&mic_eff->echo_parm.parm, (EF_REVERB_FIX_PARM *)&effect_echo_fix_parm_default);

        if (effect->sub_1_echo_hdl) {
            effect->mic_eq2 = mic_eq_open(effect->parm.sample_rate, ch_num, AEID_MIC_EQ2);
            if (effect->mic_eq2 && effect->mic_eq2->out_32bit) {
                effect->mic_drc2 = mic_drc_open(effect->parm.sample_rate, ch_num, AEID_MIC_DRC2);
            }
        }
    }

    if (TCFG_MIC_EFFECT_SEL & MIC_EFFECT_REVERB) {
        effect->sub_0_plate_reverb_hdl = open_plate_reverb((Plate_reverb_parm *)&mic_eff->plate_reverb_parm.parm, effect->parm.sample_rate);
        if (effect->sub_0_plate_reverb_hdl) {
            ch_num = 2;
        }

        effect->mic_eq1 = mic_eq_open(effect->parm.sample_rate, ch_num, AEID_MIC_EQ1);
        if (effect->mic_eq1 && effect->mic_eq1->out_32bit) {
            effect->mic_drc1 = mic_drc_open(effect->parm.sample_rate, ch_num, AEID_MIC_DRC1);
        }
    }

    //多路混合后 EQ drc
    effect->mic_eq4 = mic_eq_open(effect->parm.sample_rate, ch_num, AEID_MIC_EQ4);
    if (effect->mic_eq4 && effect->mic_eq4->out_32bit) {
        effect->mic_drc4 = mic_drc_open(effect->parm.sample_rate, ch_num, AEID_MIC_DRC4);
        effect->convert4 = convet_data_open(0, (irq_points << 1) * ch_num);
    }
    /* effect->gain = audio_gain_open_demo(AEID_MIC_GAIN, ch_num); */

    ///初始化数字音量
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_DVOL)) {
        effect_dvol_default_parm.ch_total = ch_num;
        struct audio_stream_entry *dvol_entry;
#if defined(SYS_DIGVOL_GROUP_EN)&&SYS_DIGVOL_GROUP_EN
        dvol_entry = sys_digvol_group_ch_open("mic_mic", -1, &effect_dvol_default_parm);
        effect->d_vol = audio_dig_vol_group_hdl_get(sys_digvol_group, "mic_mic");
#else
        effect->d_vol = audio_dig_vol_open((audio_dig_vol_param *)&effect_dvol_default_parm);
        dvol_entry = audio_dig_vol_entry_get(effect->d_vol);
#endif /*SYS_DIGVOL_GROUP_EN*/
    }

    //打开混响变采样
    /* if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_SOFT_SRC)) { */
    /* u32 out_sr = audio_output_nor_rate(); */

    /* effect->src_hdl = zalloc(sizeof(struct audio_src_handle)); */
    /* audio_hw_src_open(effect->src_hdl, ch_num, SRC_TYPE_RESAMPLE); */
    /* audio_hw_src_set_rate(effect->src_hdl, effect->parm.sample_rate, out_sr); */
    /* } */

    //混响通路混合linein
    if (effect->parm.effect_config & BIT(MIC_EFFECT_CONFIG_LINEIN)) {
        effect->linein = effect_linein_open();
    }

    u8 output_channels = DAC_OUTPUT_CHANNELS;
    if (output_channels != ch_num) {
        u32 points_num = REVERB_LADC_IRQ_POINTS * 4;
        effect->channel_zoom = channel_switch_open(output_channels == 2 ? AUDIO_CH_LR : (output_channels == 4 ? AUDIO_CH_QUAD : AUDIO_CH_DIFF), output_channels == 4 ? (points_num * 2 + 128) : 1024);
        //effect->channel_zoom = channel_switch_open(AUDIO_CH_LR, 1024);
    }



    effect->entry.data_process_len = mic_effect_data_process_len;

#if (RECORDER_MIX_EN)
    ///送录音数据流
    effect->rec_hdl = stream_entry_open(effect, mic_effect_record_stream_callback, 1);
#endif
#if (TCFG_USB_MIC_DATA_FROM_MICEFFECT||TCFG_USB_MIC_DATA_FROM_DAC)
    effect->usbmic_hdl = stream_entry_open(effect, mic_effect_otherout_stream_callback, 1);
#endif

    u8 bit_wide = 0;
    if (effect->mic_eq1 && effect->mic_eq1->out_32bit) {
        bit_wide = 1;
    }
#if(TCFG_MIC_EFFECT_SEL != MIC_EFFECT_MEGAPHONE)
    effect->aud_reverb = aud_reverb_open(effect, bit_wide);
#endif

// 数据流串联
    struct audio_stream_entry *entries[25] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &effect->entry;

#if TCFG_MIC_DODGE_EN
    if (effect->energy_hdl) {
        entries[entry_cnt++] = audio_energy_detect_entry_get(effect->energy_hdl);
    }
#endif
    if (effect->noisegate) {
        entries[entry_cnt++] = &effect->noisegate->entry;
    }

    if (effect->mic_eq0) {
        entries[entry_cnt++] = &effect->mic_eq0->entry;
        if (effect->mic_eq0->out_32bit) {
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
            if (effect->mic_tunning_eq) {
                entries[entry_cnt++] = &effect->mic_tunning_eq->entry;
            }
#endif
            if (effect->mic_drc0) {
                entries[entry_cnt++] = &effect->mic_drc0->entry;
            }
            if (effect->convert0) {
                entries[entry_cnt++] = &effect->convert0->entry;
            }
        }
    }

    if (effect->howling_ps) {
        entries[entry_cnt++] = &effect->howling_ps->entry;
    }
    if (effect->notch_howling) {
        entries[entry_cnt++] = &effect->notch_howling->entry;
    }
    if (effect->voice_changer) {
        entries[entry_cnt++] = &effect->voice_changer->entry;
    }
#if defined(TCFG_MIC_AUTOTUNE_ENABLE) && TCFG_MIC_AUTOTUNE_ENABLE
    if (effect->autotune) {
        entries[entry_cnt++] = &effect->autotune->entry;
    }
#endif
    if (effect->aud_reverb) {
        entries[entry_cnt++] = &effect->aud_reverb->entry;
    }


    if (effect->mic_eq4) {
        entries[entry_cnt++] = &effect->mic_eq4->entry;
        if (effect->mic_eq4->out_32bit) {
            if (effect->mic_drc4) {
                entries[entry_cnt++] = &effect->mic_drc4->entry;
            }
            if (effect->convert4) {
                entries[entry_cnt++] = &effect->convert4->entry;
            }
        }
    }
#if (RECORDER_MIX_EN) //增加数据流节点
    //数据流分流给录音用
    u8 record_entry_cnt = entry_cnt - 1;
    if (effect->rec_hdl) {
        audio_stream_add_entry(entries[record_entry_cnt], &effect->rec_hdl->entry);		//`把这个数据节点添加给录音的使用
    }
#endif
    /* if (effect->gain) { */
    /* entries[entry_cnt++] = &effect->gain->entry; */
    /* } */
    if (effect->loudness_debug_hdl) {
        entries[entry_cnt++] = &effect->loudness_debug_hdl->entry;
    }

    if (effect->channel_zoom) {
        entries[entry_cnt++] = &effect->channel_zoom->entry;
    }
    audio_reverb_stream_dac_init();
    entries[entry_cnt++] = audio_reverb_stream_dac_get_entry();

    effect->stream = audio_stream_open(effect, mic_stream_resume);
    audio_stream_add_list(effect->stream, entries, entry_cnt);

    /* effect->main_pause = 2; */
    ///mic 数据流初始化
    effect->mic = mic_stream_creat(mic_parm);
    if (effect->mic == NULL) {
        mic_effect_destroy(&effect);
        return false;
    }
    mic_stream_set_output(effect->mic, (void *)effect, mic_effect_effect_run);
    mic_stream_start(effect->mic);
#if (RECORDER_MIX_EN)
    recorder_mix_pcm_stream_open(effect->parm.sample_rate, ch_num);
#endif

    clock_set_cur();
    __this = effect;


    log_info("--------------------------effect start ok\n");
    mem_stats();

    mic_effect_change_mode(mode);

    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief    mic增益调节
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
/* void mic_effect_mic_gain_parm_fill(EFFECTS_MIC_GAIN_PARM *parm, u8 fade, u8 online) */
/* { */
/* if (__this == NULL || parm == NULL) { */
/* return ; */
/* } */
/* audio_mic_set_gain(parm->gain); */
/* } */
/*----------------------------------------------------------------------------*/
/**@brief    mic效果模式切换（数据流音效组合切换）
   @param
   @return
   @note 使用效果配置文件时生效
*/
/*----------------------------------------------------------------------------*/
void mic_effect_change_mode(u16 mode)
{
    set_mic_reverb_mode_by_id(mode);
}
/*----------------------------------------------------------------------------*/
/**@brief    获取mic效果模式（数据流音效组合）
   @param
   @return
   @note 使用效果配置文件时生效
*/
/*----------------------------------------------------------------------------*/
u16 mic_effect_get_cur_mode(void)
{
    return get_mic_eff_mode();
}

#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
/*
 *混响mic tunning_eq 打开
 * */
struct eq_tool mic_tunning_eq_parm = {0};
void *mic_tunning_eq_open(u32 sample_rate, u8 ch_num)
{
    u8 mode = get_mic_eff_mode();
    struct eff_parm *mic_eff = &eff_mode[mode];
    u8 seg_num = 3;
    mic_tunning_eq_parm.seg_num = seg_num;
    mic_tunning_eq_parm.global_gain = 0;
    for (int i = 0; i < seg_num; i++) {
        mic_tunning_eq_parm.seg[i].index    = i;
        mic_tunning_eq_parm.seg[i].iir_type = mic_eff->tunning_eq_parm.seg[i].filt_type;
        mic_tunning_eq_parm.seg[i].freq     = mic_eff->tunning_eq_parm.seg[i].center_freq;
        mic_tunning_eq_parm.seg[i].q        = mic_eff->tunning_eq_parm.seg[i].q;
    }
    struct eq_parm eparm = {0};
    eparm.in_mode = DATI_INT;
    eparm.out_mode = DATO_INT;
    eparm.run_mode = NORMAL;
    eparm.data_in_mode = SEQUENCE_DAT_IN;
    eparm.data_out_mode = SEQUENCE_DAT_OUT;
    struct audio_eq_param parm = {0};
    parm.parm = &eparm;
    parm.channels = ch_num;
    parm.cb = eq_get_filter_info;
    parm.sr = sample_rate;
    parm.eq_name = AEID_MIC_TUNNING_EQ;
    parm.max_nsection = mic_tunning_eq_parm.seg_num;
    parm.nsection = mic_tunning_eq_parm.seg_num;
    parm.seg = mic_tunning_eq_parm.seg;
    parm.global_gain = mic_tunning_eq_parm.global_gain;

    parm.fade = 1;//高低音增益更新差异大，会引入哒哒音，此处使能系数淡入
    parm.fade_step = 0.4f;//淡入步进（0.1f~1.0f）

    log_d("=====mic tunning eq_name %d\n", parm.eq_name);
    struct audio_eq *eq = audio_dec_eq_open(&parm);
    return eq;
}
/*
 *混响mic tunning_eq 关闭
 * */
void mic_tunning_eq_close(void *eq)
{
    if (eq) {
        audio_dec_eq_close(eq);
    }
}
#endif

/*
 *index:0 low, 1 mid, 2 high
 *gain:增益（-12~12）
 * */
void mic_tunning_eq_update(u8 index, float gain)
{
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
    u8 mode = get_mic_eff_mode();
    struct eff_parm *mic_eff = &eff_mode[mode];
    //printf("max %d\n", (int)mic_eff->tunning_eq_parm.seg[index].max_gain);
    //printf("min %d\n", (int)mic_eff->tunning_eq_parm.seg[index].min_gain);
    if (gain > mic_eff->tunning_eq_parm.seg[index].max_gain) {
        gain = 	mic_eff->tunning_eq_parm.seg[index].max_gain;
    } else if (gain < mic_eff->tunning_eq_parm.seg[index].min_gain) {
        gain = 	mic_eff->tunning_eq_parm.seg[index].min_gain;
    }
    mic_tunning_eq_parm.seg[index].gain = gain;
    /* log_d("mic tunning eq index %d, %d\n", index, (int)gain); */
    struct audio_eq *hdl = get_cur_eq_hdl_by_name(AEID_MIC_TUNNING_EQ);
    if (hdl) {
        cur_eq_set_update(AEID_MIC_TUNNING_EQ, &mic_tunning_eq_parm.seg[index], mic_tunning_eq_parm.seg_num, 0);
    }
#endif
}
/*
*混响切换模式时更新高低音参数
*/
void mic_tunning_eq_update_parm(u8 mode)
{
#if TCFG_EQ_ENABLE && TCFG_MIC_BASS_AND_TREBLE_ENABLE
    u8 seg_num = 3;
    struct eff_parm *mic_eff = &eff_mode[mode];
    struct audio_eq *hdl = get_cur_eq_hdl_by_name(AEID_MIC_TUNNING_EQ);
    for (int i = 0; i < seg_num; i++) {
        mic_tunning_eq_parm.seg[i].index    = i;
        mic_tunning_eq_parm.seg[i].iir_type = mic_eff->tunning_eq_parm.seg[i].filt_type;
        mic_tunning_eq_parm.seg[i].freq     = mic_eff->tunning_eq_parm.seg[i].center_freq;
        mic_tunning_eq_parm.seg[i].q        = mic_eff->tunning_eq_parm.seg[i].q;
        if (hdl) {
            cur_eq_set_update(AEID_MIC_TUNNING_EQ, &mic_tunning_eq_parm.seg[i], mic_tunning_eq_parm.seg_num, 0);
        }
    }
#endif
}

void *mic_eq_open(u32 sample_rate, u8 ch_num, u8 eq_name)
{
#if TCFG_EQ_ENABLE

#if(TCFG_MIC_EFFECT_SEL == MIC_EFFECT_MEGAPHONE)
    if ((eq_name == AEID_MIC_EQ1) || (eq_name == AEID_MIC_EQ2) || (eq_name == AEID_MIC_EQ3)) {
        return NULL;
    }
#endif


    u8 mode = get_mic_eff_mode();

    struct audio_eq_param parm = {0};
    parm.channels = ch_num;
    if ((eq_name == AEID_MIC_EQ0) || (eq_name == AEID_MIC_EQ4)) {
        parm.out_32bit = 1;//32bit位宽输出
    }
    parm.no_wait = 0;//同步方式
    parm.cb = eq_get_filter_info;
    parm.sr = sample_rate;
    parm.eq_name = eq_name;

    u8 index = get_eq_module_index(eq_name);
    log_d("index %d\n", index);
    parm.max_nsection = eff_mode[mode].eq_parm[index].seg_num;
    parm.nsection = eff_mode[mode].eq_parm[index].seg_num;
    parm.seg = eff_mode[mode].eq_parm[index].seg;
    parm.global_gain = eff_mode[mode].eq_parm[index].global_gain;
    log_d("=====mic eq_name %d\n", eq_name);
    struct audio_eq *eq = audio_dec_eq_open(&parm);
    return eq;

#else
    return NULL;
#endif//TCFG_EQ_ENABLE
}

void *mic_drc_open(u32 sample_rate, u8 ch_num, u8 drc_name)
{
#if TCFG_DRC_ENABLE
    u8 mode = get_mic_eff_mode();
    log_i("sample_rate %d %d\n", sample_rate, ch_num);
    struct audio_drc_param parm = {0};
    parm.channels = ch_num;
    parm.sr = sample_rate;
    parm.out_32bit = 1;
    parm.cb = drc_get_filter_info;
    parm.drc_name = drc_name;
    u8 index = get_drc_module_index(drc_name);
    parm.wdrc = &eff_mode[mode].drc_parm[index];
    log_d("=====drc_name %d\n", drc_name);
    struct audio_drc *drc = audio_dec_drc_open(&parm);
    clock_add(EQ_CLK);
    return drc;
#else
    return NULL;
#endif//TCFG_DRC_ENABLE

}

void mic_eq_close(void *eq)
{
#if TCFG_EQ_ENABLE
    if (eq) {
        audio_dec_eq_close(eq);
        clock_remove(EQ_CLK);
    }
#endif
    return;
}

void mic_drc_close(void *drc)
{
#if TCFG_DRC_ENABLE
    if (drc) {
        audio_dec_drc_close(drc);
        drc = NULL;
        clock_remove(EQ_CLK);
    }
#endif
    return;
}





#if TCFG_MIC_DODGE_EN
void mic_e_det_handler(u8 event, u8 ch)
{
    //printf(">>>> ch:%d %s\n", ch, event ? ("MUTE") : ("UNMUTE"));
    struct __mic_effect *effect = (struct __mic_effect *)__this;
#if SYS_DIGVOL_GROUP_EN
    //printf("effect_dvol_default_parm.ch_total %d effect->dodge_en %d\n", effect_dvol_default_parm.ch_total, effect->dodge_en);
    if (ch == effect_dvol_default_parm.ch_total) {
        if (effect->dodge_en) {
            if (effect && effect->d_vol) {
                if (event) { //推出闪避
                    audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 100);
                } else { //启动闪避
                    audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 0);
                }
            }
        }
    }
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief    打开mic 能量检测
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void *mic_energy_detect_open(u32 sr, u8 ch_num)
{
    audio_energy_detect_param e_det_param = {0};
    e_det_param.mute_energy = dodge_parm.dodge_out_thread;//人声能量小于mute_energy 退出闪避
    e_det_param.unmute_energy = dodge_parm.dodge_in_thread;//人声能量大于 100触发闪避
    e_det_param.mute_time_ms = dodge_parm.dodge_out_time_ms;
    e_det_param.unmute_time_ms = dodge_parm.dodge_in_time_ms;
    e_det_param.count_cycle_ms = 2;
    e_det_param.sample_rate = sr;
    e_det_param.event_handler = mic_e_det_handler;
    e_det_param.ch_total = ch_num;
    e_det_param.dcc = 1;
    void *audio_e_det_hdl = audio_energy_detect_open(&e_det_param);
    return audio_e_det_hdl;
}
/*----------------------------------------------------------------------------*/
/**@brief    关闭mic 能量检测
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_energy_detect_close(void *hdl)
{
    if (hdl) {
        audio_stream_del_entry(audio_energy_detect_entry_get(hdl));
#if SYS_DIGVOL_GROUP_EN
        struct __mic_effect *effect = (struct __mic_effect *)__this;
        if (effect->d_vol) {
            audio_dig_vol_group_dodge(sys_digvol_group, "mic_mic", 100, 100);         // undodge
        }
#endif

        audio_energy_detect_close(hdl);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    能量检测运行过程，是否触发闪避
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_dodge_ctr(void)
{
    struct __mic_effect *effect = (struct __mic_effect *)__this;
    if (effect) {
        effect->dodge_en = !effect->dodge_en;
    }
}
u8 mic_dodge_get_status(void)
{
    struct __mic_effect *effect = (struct __mic_effect *)__this;
    if (effect) {
        return effect->dodge_en;
    }
    return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief    (mic数据流)混响关闭接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_stop(void)
{
    mic_effect_destroy(&__this);
}
/*----------------------------------------------------------------------------*/
/**@brief    (mic数据流)混响暂停接口(整个数据流不运行)
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_pause(u8 mark)
{
    if (__this) {
        __this->pause_mark = mark ? 1 : 0;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief    (mic数据流)混响状态获取接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
u8 mic_effect_get_status(void)
{
    return ((__this) ? 1 : 0);
}
/*----------------------------------------------------------------------------*/
/**@brief    数字音量调节接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_dvol(u8 vol)
{
    if (__this && __this->d_vol) {
        audio_dig_vol_set(__this->d_vol, 3, vol);
    }
}
u8 mic_effect_get_dvol(void)
{
    if (__this && __this->d_vol) {
        return audio_dig_vol_get(__this->d_vol, 1);
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    获取mic增益接口
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
u8 mic_effect_get_micgain(void)
{
    //动态调的需实时记录
    return effect_mic_stream_parm_default.mic_gain;
}


/*----------------------------------------------------------------------------*/
/**@brief    reverb 效果声增益调节接口
  @param     wet增益系数%[0,300];dry增益系数%[0,200]
  @return
  @note
  */
/*----------------------------------------------------------------------------*/
void mic_effect_set_reverb_wet(int wet)
{
    if (__this == NULL || __this->sub_0_plate_reverb_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    Plate_reverb_parm parm;
    memcpy(&parm, & __this->sub_0_plate_reverb_hdl->reverb_parm_obj, sizeof(Plate_reverb_parm));
    parm.wet = wet;
    update_plate_reverb_parm(__this->sub_0_plate_reverb_hdl, &parm);
    os_mutex_post(&__this->mutex);
}

int mic_effect_get_reverb_wet(void)
{
    if (__this && __this->sub_0_plate_reverb_hdl) {
        return __this->sub_0_plate_reverb_hdl->reverb_parm_obj.wet;
    }
    return 0;
}
void mic_effect_set_reverb_dry(int dry)
{
    if (__this == NULL || __this->sub_0_plate_reverb_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    Plate_reverb_parm parm;
    memcpy(&parm, & __this->sub_0_plate_reverb_hdl->reverb_parm_obj, sizeof(Plate_reverb_parm));
    parm.dry = dry;
    update_plate_reverb_parm(__this->sub_0_plate_reverb_hdl, &parm);
    os_mutex_post(&__this->mutex);
}

int mic_effect_get_reverb_dry(void)
{
    if (__this && __this->sub_0_plate_reverb_hdl) {
        return __this->sub_0_plate_reverb_hdl->reverb_parm_obj.dry;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    echo 回声延时调节接口
   @param   delay 回声延时[0,max_ms]
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_delay(u32 delay)
{
    if (__this == NULL || __this->sub_1_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    ECHO_PARM_SET parm;
    memcpy(&parm, &__this->sub_1_echo_hdl->echo_parm_obj, sizeof(ECHO_PARM_SET));
    parm.delay = delay;
    update_echo_parm(__this->sub_1_echo_hdl, &parm);
    os_mutex_post(&__this->mutex);
}
u32 mic_effect_get_echo_delay(void)
{
    if (__this && __this->sub_1_echo_hdl) {
        return __this->sub_1_echo_hdl->echo_parm_obj.delay;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    echo 回声衰减系数调节接口
   @param    decay 衰减系数值%[0,90]
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_decay(u32 decay)
{
    if (__this == NULL || __this->sub_1_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    ECHO_PARM_SET parm;
    memcpy(&parm, &__this->sub_1_echo_hdl->echo_parm_obj, sizeof(ECHO_PARM_SET));
    parm.decayval = decay;
    update_echo_parm(__this->sub_1_echo_hdl, &parm);
    os_mutex_post(&__this->mutex);
}

u32 mic_effect_get_echo_decay(void)
{
    if (__this && __this->sub_1_echo_hdl) {
        return __this->sub_1_echo_hdl->echo_parm_obj.decayval;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    echo 回声wetgain调节接口
   @param    wetgain 湿声增益系数%[0,200]
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_wetgain(u32 wetgain)
{
    if (__this == NULL || __this->sub_1_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    ECHO_PARM_SET parm;
    memcpy(&parm, &__this->sub_1_echo_hdl->echo_parm_obj, sizeof(ECHO_PARM_SET));
    parm.wetgain = wetgain;
    update_echo_parm(__this->sub_1_echo_hdl, &parm);
    os_mutex_post(&__this->mutex);
}

u32 mic_effect_get_echo_wetgain(void)
{
    if (__this && __this->sub_1_echo_hdl) {
        return __this->sub_1_echo_hdl->echo_parm_obj.wetgain;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief    echo 回声drygain调节接口
  @param    干声增益系数%[0,100]
  @return
  @note
  */
/*----------------------------------------------------------------------------*/
void mic_effect_set_echo_drygain(u32 drygain)
{
    if (__this == NULL || __this->sub_1_echo_hdl == NULL) {
        return ;
    }
    os_mutex_pend(&__this->mutex, 0);
    ECHO_PARM_SET parm;
    memcpy(&parm, &__this->sub_1_echo_hdl->echo_parm_obj, sizeof(ECHO_PARM_SET));
    parm.drygain = drygain;
    update_echo_parm(__this->sub_1_echo_hdl, &parm);
    os_mutex_post(&__this->mutex);
}

u32 mic_effect_get_echo_drygain(void)
{
    if (__this && __this->sub_1_echo_hdl) {
        return __this->sub_1_echo_hdl->echo_parm_obj.drygain;
    }
    return 0;
}
//*********************变声音效切换例程**********************************//

/* VOICE_CHANGER_NONE,//原声 */
/* VOICE_CHANGER_UNCLE,//大叔 */
/* VOICE_CHANGER_GODDESS,//女神 */
/* VOICE_CHANGER_BABY,//娃娃音 */
/* VOICE_CHANGER_MAGIC,//魔音女声 */
/* VOICE_CHANGER_MONSTER,//怪兽音 */
/* VOICE_CHANGER_DONALD_DUCK,//唐老鸭 */
/* VOICE_CHANGER_MINIONS,//小黄人 */
/* VOICE_CHANGER_ROBOT,//机器音 */
/* VOICE_CHANGER_WHISPER,//气音 */
/* VOICE_CHANGER_MELODY,//固定旋律音 */
/* VOICE_CHANGER_FEEDBACK,//调制音 */

/*----------------------------------------------------------------------------*/
/**@brief    变声音效循环切换
   @param    NULL
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_voicechange_loop(void)
{
    voicechange_mode++;
    if (voicechange_mode >= VOICE_CHANGER_MAX) {
        voicechange_mode = VOICE_CHANGER_NONE;
    }
    mic_voicechange_switch(voicechange_mode);
}

/*----------------------------------------------------------------------------*/
/**@brief    变声音效指定模式切换
   @param    eff_mode 模式索引
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void mic_voicechange_switch(u8 eff_mode)
{
#if TCFG_MIC_VOICE_CHANGER_ENABLE
    if (!mic_effect_get_status()) {
        mic_effect_start();
        /* return; */
    }
    switch (eff_mode) {
    case VOICE_CHANGER_NONE://原声,录音棚
        /* tone_play_index(IDEX_TONE_MIC_OST, 1); */
        break;
    case VOICE_CHANGER_UNCLE://大叔
        /* tone_play_index(IDEX_TONE_UNCLE, 1); */
        break;
    case VOICE_CHANGER_GODDESS://女神
        /* tone_play_index(IDEX_TONE_GODNESS, 1); */
        break;
    case VOICE_CHANGER_BABY://娃娃音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_MAGIC://魔音女声
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_MONSTER://怪兽音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_DONALD_DUCK://唐老鸭
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_MINIONS://小黄人
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_ROBOT://机器音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_WHISPER://气音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_MELODY://固定旋律音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    case VOICE_CHANGER_FEEDBACK://调制音
        /* tone_play_index(IDEX_TONE_BABY, 1); */
        break;
    default:
        puts("mic_ERROR\n");
        /* mic_effect_stop(); */
        break;
    }
    audio_voice_changer_mode_switch(AEID_MIC_VOICE_CHANGER, eff_mode);
#endif//TCFG_MIC_VOICE_CHANGER_ENABLE
}

/**************************各个音效模块参数设置接口(调试工具/模式切换会使用勿修改)************************************/
void plate_reverb_update_parm(void *parm, int bypass)
{
    if (__this && __this->sub_0_plate_reverb_hdl) {
        update_plate_reverb_parm(__this->sub_0_plate_reverb_hdl, parm);
        plate_reverb_update_bypass(__this->sub_0_plate_reverb_hdl, bypass);
    }
}

void echo_updata_parm(void *parm, int bypass)
{
    if (__this && __this->sub_1_echo_hdl) {
        update_echo_parm(__this->sub_1_echo_hdl, parm);
        echo_update_bypass(__this->sub_1_echo_hdl, bypass);
    }
}

void noisegate_update_parm(void *parm, int bypass)
{
    audio_noisegate_update(AEID_MIC_NS_GATE, parm);
    audio_noisegate_bypass(AEID_MIC_NS_GATE, bypass);
}

void howling_pitch_shift_update_parm(void *parm, int bypass)
{
    if (__this && __this->howling_ps) {
        update_howling_parm(__this->howling_ps, parm);
        howling_update_bypass(__this->howling_ps, bypass);
    }
}

void notchhowline_update_parm(void *parm, int bypass)
{
    if (__this && __this->notch_howling) {
        update_howling_parm(__this->notch_howling, parm);
        howling_update_bypass(__this->notch_howling, bypass);
    }

}

/**************************各个音效模块参数设置接口 END************************************/
#endif
