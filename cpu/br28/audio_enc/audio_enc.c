#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "asm/audio_adc.h"
#include "audio_enc.h"
#include "app_main.h"
#include "clock_cfg.h"
#include "classic/hci_lmp.h"
#include "app_config.h"
#include "Resample_api.h"
#include "audio_link.h"
#include "audio_effect/audio_eff_default_parm.h"
#ifndef CONFIG_LITE_AUDIO
#include "app_task.h"
#include "aec_user.h"
#include "loud_speaker.h"
#include "mic_effect.h"
#include "audio_config.h"
#else
__attribute__((weak))void audio_aec_inbuf(s16 *buf, u16 len)
{
}
#endif/*CONFIG_LITE_AUDIO*/

/* #include "encode/encode_write.h" */
extern struct adc_platform_data adc_data;

#if defined(AUDIO_PGA_CONFIG)&&AUDIO_PGA_CONFIG
ADC_PGA_TOOL_SET  adc_pga;
#endif

struct audio_adc_hdl adc_hdl;
struct esco_enc_hdl *esco_enc = NULL;
struct audio_encoder_task *encode_task = NULL;

static u16 ladc_irq_point_unit = 0;

#define ESCO_ADC_BUF_NUM        3
#define ESCO_ADC_IRQ_POINTS     256
#define ESCO_ADC_BUFS_SIZE      (ESCO_ADC_BUF_NUM * ESCO_ADC_IRQ_POINTS)

struct esco_enc_hdl {
    struct audio_encoder encoder;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    //OS_SEM pcm_frame_sem;
    s16 output_frame[30];               //align 4Bytes
    int pcm_frame[60];                 //align 4Bytes
    /* s16 adc_buf[ESCO_ADC_BUFS_SIZE];    //align 4Bytes */
    u8 state;
    RS_STUCT_API *mic_sw_src_api ;
    u8 *mic_sw_src_buf;
};

static void adc_mic_output_handler(void *priv, s16 *data, int len)
{
    /* printf("buf:%x,data:%x,len:%d",esco_enc->adc_buf,data,len); */
    audio_aec_inbuf(data, len);
}


#if TCFG_AUDIO_INPUT_IIS
#define IIS_INBUF_SIZE (2 * 1024)
extern ALINK_PARM alink0_rx_platform_data;
struct esco_iis_mic_hdl {
    cbuffer_t *iis_mic_cbuf;
    s16 iis_rbuf[3][256];
    u8 iis_inbuf_idx;
    u32 in_sample_rate;
    u32 out_sample_rate;
    u32 sr_cal_timer;
    u32 points_total;
    RS_STUCT_API *iis_sw_src_api;
    u8 *iis_sw_src_buf;
};
struct esco_iis_mic_hdl *esco_iis_mic = NULL;
void *hw_alink = NULL;

s16 temp_iis_input_data[ALNK_BUF_POINTS_NUM * 3];
static void audio_iis_mic_output_handler(void *priv, s16 *data, u16 len)
{
    u16 temp_len = len;

    esco_iis_mic->points_total += len >> 1;

    //双声道转单声道
    for (int i = 0; i < len / 2 / 2; i++) {
        data[i] = data[2 * i];
    }
    len >>= 1;

    if (esco_iis_mic->iis_mic_cbuf) {
        if (esco_iis_mic->iis_sw_src_api && esco_iis_mic->iis_sw_src_buf) {
            len = esco_iis_mic->iis_sw_src_api->run(esco_iis_mic->iis_sw_src_buf, data, len >> 1,  temp_iis_input_data);
            len = len << 1;
        }
        u16 wlen = cbuf_write(esco_iis_mic->iis_mic_cbuf, temp_iis_input_data, len);
        if (wlen != len) {
            printf("wlen err : %d\n", wlen);
        }
        if (esco_iis_mic->iis_mic_cbuf->data_len >= 512) {
            u16 rlen = cbuf_read(esco_iis_mic->iis_mic_cbuf, &esco_iis_mic->iis_rbuf[esco_iis_mic->iis_inbuf_idx][0], 512);
            if (rlen != 512) {
                printf("rlen err : %d\n", rlen);
            }
            audio_aec_inbuf(&esco_iis_mic->iis_rbuf[esco_iis_mic->iis_inbuf_idx][0], 512);
            if (++esco_iis_mic->iis_inbuf_idx > 2) {
                esco_iis_mic->iis_inbuf_idx = 0;
            }
        }
    }

    alink_set_shn(&alink0_rx_platform_data.ch_cfg[1], temp_len / 4);
}

void audio_iis_enc_sr_cal_timer(void *param)
{
    if (esco_iis_mic) {
        esco_iis_mic->in_sample_rate = esco_iis_mic->points_total >> 1;
        esco_iis_mic->points_total = 0;
        printf("in_sr : %d\n", esco_iis_mic->in_sample_rate);
        if (esco_iis_mic->iis_sw_src_api && esco_iis_mic->iis_sw_src_buf) {
            esco_iis_mic->iis_sw_src_api->set_sr(esco_iis_mic->iis_sw_src_buf, esco_iis_mic->in_sample_rate);
        }
    }
}
#endif/*TCFG_AUDIO_INPUT_IIS*/

#if	TCFG_MIC_EFFECT_ENABLE
unsigned int jl_sr_table[] = {
    7616,
    10500,
    11424,
    15232,
    21000,
    22848,
    30464,
    42000,
    45696,
};

unsigned int normal_sr_table[] = {
    8000,
    11025,
    12000,
    16000,
    22050,
    24000,
    32000,
    44100,
    48000,
};
static u8 get_sample_rate_index(u32 sr)
{
    u8 i;
    for (i = 0; i < ARRAY_SIZE(normal_sr_table); i++) {
        if (normal_sr_table[i] == sr) {
            return i;
        }
    }
    return i - 1;
}
int mic_sw_src_init(u16 out_sr)
{
    if (!esco_enc) {
        printf(" mic  is not open !\n");
        return -1;
    }
    esco_enc->mic_sw_src_api = get_rsfast_context();
    esco_enc->mic_sw_src_buf = malloc(esco_enc->mic_sw_src_api->need_buf());

    ASSERT(esco_enc->mic_sw_src_buf);
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;
    /* rs_para_obj.new_insample = MIC_EFFECT_SAMPLERATE; */
    /* rs_para_obj.new_outsample = out_sr; */
    rs_para_obj.new_insample = jl_sr_table[get_sample_rate_index(MIC_EFFECT_SAMPLERATE)];
    rs_para_obj.new_outsample = jl_sr_table[get_sample_rate_index(out_sr)];
    esco_enc->mic_sw_src_api->open(esco_enc->mic_sw_src_buf, &rs_para_obj);
    return 0;
}

int mic_sw_src_uninit(void)
{
    if (!esco_enc) {
        return 0;
    }
    if (esco_enc->mic_sw_src_buf) {
        free(esco_enc->mic_sw_src_buf);
        esco_enc->mic_sw_src_buf = NULL;
    }
    return 0;
}

#endif //TCFG_MIC_EFFECT_ENABLE

static void adc_mic_output_handler_downsr(void *priv, s16 *data, int len)
{
    //printf("buf:%x,data:%x,len:%d",esco_enc->adc_buf,data,len);
    u16 i;
    s16 temp_buf[160];
    if (esco_enc && esco_enc->mic_sw_src_buf) {
        int wlen = esco_enc->mic_sw_src_api->run(esco_enc->mic_sw_src_buf, data, len / 2, temp_buf);
        audio_aec_inbuf(temp_buf, wlen << 1);
    }
    /* audio_aec_inbuf(temp_buf, len / 2); */
}

__attribute__((weak)) int audio_aec_output_read(s16 *buf, u16 len)
{
    return 0;
}

void esco_enc_resume(void)
{
    if (esco_enc) {
        //os_sem_post(&esco_enc->pcm_frame_sem);
        audio_encoder_resume(&esco_enc->encoder);
    }
}

static int esco_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct esco_enc_hdl *enc = container_of(encoder, struct esco_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }

    while (1) {

        rlen = audio_aec_output_read(enc->pcm_frame, frame_len);

        if (rlen == frame_len) {
            /*esco编码读取数据正常*/
#if (RECORDER_MIX_EN)
            recorder_mix_sco_data_write(enc->pcm_frame, frame_len);
#endif/*RECORDER_MIX_EN*/
            break;
        } else if (rlen == 0) {
            /*esco编码读不到数,返回0*/
            return 0;
            /*esco编码读不到数，pend住*/
            /* int ret = os_sem_pend(&enc->pcm_frame_sem, 100);
            if (ret == OS_TIMEOUT) {
                r_printf("esco_enc pend timeout\n");
                break;
            } */
        } else {
            /*通话结束，aec已经释放*/
            printf("audio_enc end:%d\n", rlen);
            rlen = 0;
            break;
        }
    }

    *frame = enc->pcm_frame;
    return rlen;
}
static void esco_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input esco_enc_input = {
    .fget = esco_enc_pcm_get,
    .fput = esco_enc_pcm_put,
};

static int esco_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}

static int esco_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    lmp_private_send_esco_packet(NULL, frame, len);
    //printf("frame:%x,out:%d\n",frame, len);

    return len;
}

const static struct audio_enc_handler esco_enc_handler = {
    .enc_probe = esco_enc_probe_handler,
    .enc_output = esco_enc_output_handler,
};

static void esco_enc_event_handler(struct audio_encoder *encoder, int argc, int *argv)
{
    printf("esco_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}


int esco_enc_open(u32 coding_type, u8 frame_len)
{
    int err;
    struct audio_fmt fmt;

    printf("esco_enc_open: %d,frame_len:%d\n", coding_type, frame_len);

    fmt.channel = 1;
    fmt.frame_len = frame_len;
    if (coding_type == AUDIO_CODING_MSBC) {
        fmt.sample_rate = 16000;
        fmt.coding_type = AUDIO_CODING_MSBC;
        clock_add(ENC_MSBC_CLK);
    } else if (coding_type == AUDIO_CODING_CVSD) {
        fmt.sample_rate = 8000;
        fmt.coding_type = AUDIO_CODING_CVSD;
        clock_add(ENC_CVSD_CLK);
    } else {
        /*Unsupoport eSCO Air Mode*/
    }

#if (RECORDER_MIX_EN)
    recorder_mix_pcm_set_info(fmt.sample_rate, fmt.channel);
#endif/*RECORDER_MIX_EN*/

    audio_encoder_task_open();

    if (!esco_enc) {
        esco_enc = zalloc(sizeof(*esco_enc));
    }
    //os_sem_create(&esco_enc->pcm_frame_sem, 0);

    audio_encoder_open(&esco_enc->encoder, &esco_enc_input, encode_task);
    audio_encoder_set_handler(&esco_enc->encoder, &esco_enc_handler);
    audio_encoder_set_fmt(&esco_enc->encoder, &fmt);
    audio_encoder_set_event_handler(&esco_enc->encoder, esco_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&esco_enc->encoder, esco_enc->output_frame,
                                   sizeof(esco_enc->output_frame), 1);

    if (!esco_enc->encoder.enc_priv) {
        log_e("encoder err, maybe coding(0x%x) disable \n", fmt.coding_type);
        err = -EINVAL;
        goto __err;
    }

    audio_encoder_start(&esco_enc->encoder);

    printf("esco sample_rate: %d,mic_gain:%d\n", fmt.sample_rate, app_var.aec_mic_gain);

#if TCFG_AUDIO_INPUT_IIS
    if (!esco_iis_mic) {
        esco_iis_mic = zalloc(sizeof(struct esco_iis_mic_hdl));
    }
    if (!esco_iis_mic->iis_mic_cbuf) {
        esco_iis_mic->iis_mic_cbuf = zalloc(sizeof(cbuffer_t) + IIS_INBUF_SIZE);
        if (esco_iis_mic->iis_mic_cbuf) {
            cbuf_init(esco_iis_mic->iis_mic_cbuf, esco_iis_mic->iis_mic_cbuf + 1, IIS_INBUF_SIZE);
        }
    }
    esco_iis_mic->iis_inbuf_idx = 0;
    esco_iis_mic->points_total = 0;

#if TCFG_IIS_MODE/*slave mode*/
    esco_iis_mic->in_sample_rate = TCFG_IIS_INPUT_SR;
    esco_iis_mic->out_sample_rate = fmt.sample_rate;
    esco_iis_mic->sr_cal_timer = sys_hi_timer_add(NULL, audio_iis_enc_sr_cal_timer, 1000);
#else            /*master mode*/
    esco_iis_mic->in_sample_rate = 625 * 20;
    esco_iis_mic->out_sample_rate = 624 * 20;
#endif/*TCFG_IIS_MODE*/
    extern RS_STUCT_API *get_rs16_context();
    esco_iis_mic->iis_sw_src_api = get_rs16_context();
    printf("iis_sw_src_api:0x%x\n", esco_iis_mic->iis_sw_src_api);
    ASSERT(esco_iis_mic->iis_sw_src_api);
    int iis_sw_src_need_buf = esco_iis_mic->iis_sw_src_api->need_buf();
    printf("iis_sw_src_buf:%d\n", iis_sw_src_need_buf);
    esco_iis_mic->iis_sw_src_buf = zalloc(iis_sw_src_need_buf);
    ASSERT(esco_iis_mic->iis_sw_src_buf);
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;
    rs_para_obj.new_insample = esco_iis_mic->in_sample_rate;
    rs_para_obj.new_outsample = esco_iis_mic->out_sample_rate;

    printf("sw src,in = %d,out = %d\n", rs_para_obj.new_insample, rs_para_obj.new_outsample);
    esco_iis_mic->iis_sw_src_api->open(esco_iis_mic->iis_sw_src_buf, &rs_para_obj);

#if 0 //使用wm8978模块作为输入
    extern u8 WM8978_Init(u8 dacen, u8 adcen);
    WM8978_Init(0, 1);
#endif

    hw_alink = alink_init(&alink0_rx_platform_data);
    alink_channel_init(hw_alink, 1, ALINK_DIR_RX, NULL, audio_iis_mic_output_handler); //IIS输入使用通道1
    alink_start(hw_alink);
#else // TCFG_AUDIO_INPUT_IIS

#if TCFG_CVP_REF_IN_ADC_ENABLE
    int audio_adc_cvp_ref_open(u8 ch, u16 sr, u8 gain);
    audio_adc_cvp_ref_open(TCFG_CVP_REF_IN_ADC_CH, fmt.sample_rate, app_var.aec_mic_gain);
#else /*TCFG_CVP_REF_IN_ADC_ENABLE == 0*/

#if	TCFG_MIC_EFFECT_ENABLE
    if (fmt.sample_rate != MIC_EFFECT_SAMPLERATE) {//8K时需把mic数据采样率降低
        mic_sw_src_init(fmt.sample_rate);
        esco_enc->adc_output.handler = adc_mic_output_handler_downsr;
    } else {
        esco_enc->adc_output.handler = adc_mic_output_handler;
    }
    audio_mic_open(&esco_enc->mic_ch, MIC_EFFECT_SAMPLERATE, app_var.aec_mic_gain);
#else /*TCFG_MIC_EFFECT_ENABLE == 0*/
    esco_enc->adc_output.handler = adc_mic_output_handler;
    audio_mic_open(&esco_enc->mic_ch, fmt.sample_rate, app_var.aec_mic_gain);
#endif /*TCFG_MIC_EFFECT_ENABLE*/
    audio_mic_add_output(&esco_enc->adc_output);
#if (TCFG_AUDIO_OUTPUT_IIS == 0)
    audio_mic_start(&esco_enc->mic_ch);
#endif /*TCFG_AUDIO_OUTPUT_IIS*/

#endif /*TCFG_CVP_REF_IN_ADC_ENABLE*/

#endif//TCFG_AUDIO_INPUT_IIS

    clock_set_cur();
    esco_enc->state = 1;

    /* #if TCFG_AUDIO_OUTPUT_IIS */
    /*     extern void audio_aec_ref_start(u8 en); */
    /*     audio_aec_ref_start(1); */
    /* #endif #<{(|TCFG_AUDIO_OUTPUT_IIS|)}># */

    return 0;
__err:
    audio_encoder_close(&esco_enc->encoder);

    local_irq_disable();
    free(esco_enc);
    esco_enc = NULL;
    local_irq_enable();

    audio_encoder_task_close();

    return err;
}

int esco_adc_mic_en()
{
    if (esco_enc && esco_enc->state) {
        /* audio_adc_mic_start(&esco_enc->mic_ch); */
        audio_mic_start(&esco_enc->mic_ch);
        return 0;
    }
    return -1;
}

void esco_enc_close()
{
    printf("esco_enc_close\n");
    if (!esco_enc) {
        printf("esco_enc NULL\n");
        return;
    }

    if (esco_enc->encoder.fmt.coding_type == AUDIO_CODING_MSBC) {
        clock_remove(ENC_MSBC_CLK);
    } else if (esco_enc->encoder.fmt.coding_type == AUDIO_CODING_CVSD) {
        clock_remove(ENC_CVSD_CLK);
    }
#if TCFG_AUDIO_INPUT_IIS
    if (hw_alink) {
        alink_uninit(hw_alink);
        hw_alink = NULL;
    }
    if (esco_iis_mic->sr_cal_timer) {
        sys_hi_timer_del(esco_iis_mic->sr_cal_timer);
    }
    if (esco_iis_mic->iis_sw_src_api) {
        esco_iis_mic->iis_sw_src_api = NULL;
    }
    if (esco_iis_mic->iis_sw_src_buf) {
        free(esco_iis_mic->iis_sw_src_buf);
        esco_iis_mic->iis_sw_src_buf = NULL;
    }
    if (esco_iis_mic->iis_mic_cbuf) {
        free(esco_iis_mic->iis_mic_cbuf);
        esco_iis_mic->iis_mic_cbuf = NULL;
    }
    if (esco_iis_mic) {
        free(esco_iis_mic);
        esco_iis_mic = NULL;
    }
#else //TCFG_AUDIO_INPUT_IIS
#if TCFG_CVP_REF_IN_ADC_ENABLE
    void audio_adc_cvp_ref_close(void);
    audio_adc_cvp_ref_close();
#else /*TCFG_CVP_REF_IN_ADC_ENABLE ==0*/
    audio_mic_close(&esco_enc->mic_ch, &esco_enc->adc_output);
#if	TCFG_MIC_EFFECT_ENABLE
    mic_sw_src_uninit();
#endif //TCFG_MIC_EFFECT_ENABLE
#endif /*TCFG_CVP_REF_IN_ADC_ENABLE*/
#endif //TCFG_AUDIO_INPUT_IIS

    audio_encoder_close(&esco_enc->encoder);

    local_irq_disable();
    free(esco_enc);
    esco_enc = NULL;
    local_irq_enable();

    audio_encoder_task_close();
    clock_set_cur();
}

int esco_enc_mic_gain_set(u8 gain)
{
    app_var.aec_mic_gain = gain;
    if (esco_enc) {
        printf("esco mic 0 set gain %d\n", app_var.aec_mic_gain);
        audio_adc_mic_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
    }
    return 0;
}
int esco_enc_mic1_gain_set(u8 gain)
{
    app_var.aec_mic_gain = gain;
    if (esco_enc) {
        printf("esco mic 1 set gain %d\n", app_var.aec_mic_gain);
        audio_adc_mic1_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
    }
    return 0;
}
int esco_enc_mic2_gain_set(u8 gain)
{
    app_var.aec_mic_gain = gain;
    if (esco_enc) {
        printf("esco mic 2 set gain %d\n", app_var.aec_mic_gain);
        audio_adc_mic2_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
    }
    return 0;
}
int esco_enc_mic3_gain_set(u8 gain)
{
    app_var.aec_mic_gain = gain;
    if (esco_enc) {
        printf("esco mic 3 set gain %d\n", app_var.aec_mic_gain);
        audio_adc_mic3_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
    }
    return 0;
}

struct __encoder_task {
    u8 init_ok;
    atomic_t used;
    OS_MUTEX mutex;
};

static struct __encoder_task enc_task = {0};

int audio_encoder_task_open(void)
{
    local_irq_disable();
    if (enc_task.init_ok == 0) {
        atomic_set(&enc_task.used, 0);
        os_mutex_create(&enc_task.mutex);
        enc_task.init_ok = 1;
    }
    local_irq_enable();

    os_mutex_pend(&enc_task.mutex, 0);
    if (!encode_task) {
        encode_task = zalloc(sizeof(*encode_task));
        audio_encoder_task_create(encode_task, "audio_enc");
    }
    atomic_inc_return(&enc_task.used);
    os_mutex_post(&enc_task.mutex);
    return 0;
}

void audio_encoder_task_close(void)
{
    os_mutex_pend(&enc_task.mutex, 0);
    if (encode_task) {
        if (atomic_dec_and_test(&enc_task.used)) {
            audio_encoder_task_del(encode_task);
            //local_irq_disable();
            free(encode_task);
            encode_task = NULL;
            //local_irq_enable();
        }
    }
    os_mutex_post(&enc_task.mutex);
}



//////////////////////////////////////////////////////////////////////////////
int audio_enc_init()
{
    printf("audio_enc_init\n");

    audio_adc_init(&adc_hdl, &adc_data);

    audio_adc_mode_switch(ADC_MIC_MODE);
    return 0;
}


/**************************mic linein 接口***************************************************/
#define AUDIO_ADC_BUF_NUM        2	                /*采样buf数*/

#if TCFG_MIC_EFFECT_ENABLE
#if (RECORDER_MIX_EN)
#define LADC_IRQ_POINTS     160
#else
#if (TCFG_MIC_EFFECT_SEL == MIC_EFFECT_REVERB)
#define LADC_IRQ_POINTS    REVERB_LADC_IRQ_POINTS
#else
#define LADC_IRQ_POINTS     ((MIC_EFFECT_SAMPLERATE/1000)*4)
#endif
#endif/*RECORDER_MIX_EN*/
#else
#define LADC_IRQ_POINTS     256
#endif

/*******************************
 *28有4个LADC通道,都可用于mic输入(低压)和linenin输入(高压),
 *封装的mic接口通过宏TCFG_AUDIO_ADC_MIC_CHA来选择ladc通道，linein接口通过宏TCFG_LINEIN_LR_CH来选择ladc通道
 *要保证mic和linein选用的CH不冲突
 * PA1    PA3    PG6    PG8
 * BIT(0) BIT(1) BIT(2) BIT(3)
 * mic0   mic1   mic2   mic3
 * L0     L1     L2     L3
 ***********************/
//默认开启混响宏TCFG_MIC_EFFECT_ENABLE即使能多路LADC同开，

void audio_adc_set_irq_point_unit(u16 point_unit)
{
    ladc_irq_point_unit = point_unit;
}

void audio_linein_set_irq_point(u16 point_unit)
{
#if !TCFG_MIC_EFFECT_ENABLE
    ladc_irq_point_unit = point_unit;
#endif
}

#define AUDIO_ADC_IRQ_POINTS     LADC_IRQ_POINTS    /*采样中断点数*/

// mic/linein/fm 通道的状态记录结构体
struct _adc_ch_state {
    struct list_head head;  // 中断函数链表
    s16  *temp_buf;         // 通道中断用到的临时缓存
    int open_cnt;           // 通道打开的次数
    u8 enable;              // 通道是否使能 0:disable 1:enable (使能是指这个通道会不会使用，而不代表打开状态)
    u8 ch_start;            // ADC中断数据中，通道数据的起始位置
    u8 ch_num;              // ADC中断数据中，通道数据的通道数
    u8 gain;                // 模拟增益
};

// ADC 模式管理句柄
struct _adc_mode_hdl {
    struct adc_mic_ch adc_ch;               // ADC 的句柄
    struct _adc_ch_state ch[3];             // 通道状态 0:mic 1:linein 2:fm
    struct audio_adc_output_hdl output;     // ADC模块的中断输出汇总函数，在这个函数中将每个通道数据分开
    s16 *adc_buf;                           // ADC DMA BUF
    u32 adc_buf_len;                        // ADC DMA BUF LEN
    u32 sample_rate;                        // 采样率
    u8 state;                               // ADC 模块是否已经打开 0:off 1:on
    u8 dma_state;                           // ADC 模块 DMA 是否已经打开 0:off 1:on
    u8 mode;                                // ADC 当前模式
    u8 total_ch_num;                        // ADC 打开的总通道数
} *adc_mode_hdl = NULL;

#if TCFG_CVP_REF_IN_ADC_ENABLE
void audio_adc_cvp_ref_output_handler(void *priv, s16 *data, int len)
{
    if (adc_mode_hdl == NULL) {
        return;
    }

    s16 *mic0_data = data;
    s16 *ref0_data = adc_mode_hdl->ch[0].temp_buf;
    s16 *ref0_data_pos = data + (len >> 1);

    if (adc_mode_hdl->ch[0].ch_num == 2) {
        for (u16 i = 0; i < (len >> 1); i++) {
            mic0_data[i] = data[i * 2 + 0];
            ref0_data[i] = data[i * 2 + 1];
        }
    } else if (adc_mode_hdl->ch[0].ch_num == 3) {
        for (u16 i = 0; i < (len >> 1); i++) {
            mic0_data[i] = data[i * 3 + 0];
            ref0_data[i] = (short)(((int)data[i * 3 + 1] + (int)data[i * 3 + 2]) >> 1);
        }
    }

    memcpy(ref0_data_pos, ref0_data, len);
    audio_aec_in_refbuf(ref0_data_pos, len);
    audio_aec_inbuf(mic0_data, len);
}

int audio_adc_cvp_ref_open(u8 ch, u16 sr, u8 gain)
{
    printf("audio_adc_cvp_ref_open ch: %d, sr %d, gain %d\n", ch, sr, gain);
    if (adc_mode_hdl == NULL) {
        printf("[err] adc_mode_hdl NULL !\n");
        return -1;
    }
    adc_mode_hdl->sample_rate = sr;
    adc_mode_hdl->ch[0].gain = gain;

    adc_mode_hdl->ch[0].ch_num = 0;
    if (ch & AUDIO_ADC_MIC_0) {
        audio_adc_mic_open(&adc_mode_hdl->adc_ch, ch, &adc_hdl);
        audio_adc_mic_set_gain(&adc_mode_hdl->adc_ch, adc_mode_hdl->ch[0].gain);
        adc_mode_hdl->ch[0].ch_num++;
    }
    if (ch & AUDIO_ADC_MIC_1) {
        audio_adc_mic1_open(&adc_mode_hdl->adc_ch, ch, &adc_hdl);
        audio_adc_mic1_set_gain(&adc_mode_hdl->adc_ch, adc_mode_hdl->ch[0].gain);
        adc_mode_hdl->ch[0].ch_num++;
    }
    if (ch & AUDIO_ADC_MIC_2) {
        audio_adc_mic2_open(&adc_mode_hdl->adc_ch, ch, &adc_hdl);
        audio_adc_mic2_set_gain(&adc_mode_hdl->adc_ch, adc_mode_hdl->ch[0].gain);
        adc_mode_hdl->ch[0].ch_num++;
    }
    if (ch & AUDIO_ADC_MIC_3) {
        audio_adc_mic3_open(&adc_mode_hdl->adc_ch, ch, &adc_hdl);
        audio_adc_mic3_set_gain(&adc_mode_hdl->adc_ch, adc_mode_hdl->ch[0].gain);
        adc_mode_hdl->ch[0].ch_num++;
    }

    if ((adc_mode_hdl->ch[0].ch_num != 2) && (adc_mode_hdl->ch[0].ch_num != 3)) {
        printf("mic ch_num = %d err !!!", adc_mode_hdl->ch[0].ch_num);
        return -1;
    }
    printf("mic ch_num = %d !!!", adc_mode_hdl->ch[0].ch_num);

    u16 irq_points_unit = AUDIO_ADC_IRQ_POINTS;
    adc_mode_hdl->adc_buf_len = adc_mode_hdl->ch[0].ch_num \
                                * AUDIO_ADC_BUF_NUM \
                                * irq_points_unit;
    adc_mode_hdl->adc_buf = zalloc(adc_mode_hdl->adc_buf_len * 2);
    if (adc_mode_hdl->adc_buf == NULL) {
        printf(" audio_adc_cvp_ref_open adc_buf malloc err ! size:%d\n", adc_mode_hdl->adc_buf_len * 2);
        return -1;
    }
    printf("audio_adc_cvp_ref_open dma adr:%x len:%d\n", adc_mode_hdl->adc_buf, adc_mode_hdl->adc_buf_len * 2);

    adc_mode_hdl->ch[0].temp_buf = zalloc(irq_points_unit * 2);
    if (adc_mode_hdl->ch[0].temp_buf == NULL) {
        printf("temp_buf malloc err !");
        return -1;
    }
    printf("temp_buf size %d !", irq_points_unit * 2);

    audio_adc_mic_set_sample_rate(&adc_mode_hdl->adc_ch, adc_mode_hdl->sample_rate);
    audio_adc_mic_set_buffs(&adc_mode_hdl->adc_ch, \
                            adc_mode_hdl->adc_buf, \
                            irq_points_unit * 2, \
                            AUDIO_ADC_BUF_NUM);

    adc_mode_hdl->output.handler = audio_adc_cvp_ref_output_handler;
    adc_mode_hdl->output.priv = adc_mode_hdl;
    audio_adc_add_output_handler(&adc_hdl, &adc_mode_hdl->output);
    audio_adc_mic_start(&adc_mode_hdl->adc_ch);
    return 0;
}

void audio_adc_cvp_ref_close(void)
{
    if (adc_mode_hdl) {
        audio_adc_mic_close(&adc_mode_hdl->adc_ch);
        audio_adc_del_output_handler(&adc_hdl, &adc_mode_hdl->output);
        if (adc_mode_hdl->adc_buf) {
            free(adc_mode_hdl->adc_buf);
            adc_mode_hdl->adc_buf = NULL;
        }
        if (adc_mode_hdl->ch[0].temp_buf) {
            free(adc_mode_hdl->ch[0].temp_buf);
            adc_mode_hdl->ch[0].temp_buf = NULL;
        }
        /* free(adc_mode_hdl); */
        /* adc_mode_hdl = NULL; */
    }
}
#endif /*TCFG_CVP_REF_IN_ADC_ENABLE*/

static void audio_adc_mode_output_handler(void *priv, s16 *data, int len)
{
    if (adc_mode_hdl != NULL) {
        u16 done_points = len / 2;
        s16 *ptr = data;
        u16 remain_points = 0;
        s16 *wptr = NULL;
        s16 *rptr = NULL;
        u8 i = 0;
        struct audio_adc_output_hdl *p;

        for (i = 0; i < 3; i++) {
            if (adc_mode_hdl->ch[i].enable) {
                list_for_each_entry(p, &(adc_mode_hdl->ch[i].head), entry) {
                    remain_points = done_points;
                    if (adc_mode_hdl->ch[i].temp_buf != NULL) {
                        wptr = adc_mode_hdl->ch[i].temp_buf;
                        rptr = ptr;
                        rptr += adc_mode_hdl->ch[i].ch_start;
                        while (remain_points--) {
                            memcpy(wptr, rptr, adc_mode_hdl->ch[i].ch_num * 2);
                            rptr += adc_mode_hdl->total_ch_num;
                            wptr += adc_mode_hdl->ch[i].ch_num;
                        }
                        p->handler(p->priv, \
                                   adc_mode_hdl->ch[i].temp_buf, \
                                   done_points * 2);
                    } else {
                        p->handler(p->priv, \
                                   data, \
                                   len);
                    }
                }
            }
        }
    }
}

static int audio_adc_mode_open(void)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    if (adc_mode_hdl->state == 1) {
        return -1;
    }
    if (adc_mode_hdl->ch[0].enable) {
        adc_mode_hdl->ch[0].ch_start = 0;
        adc_mode_hdl->ch[0].ch_num = 0;
        if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_0) {
            audio_adc_mic_open(&adc_mode_hdl->adc_ch, \
                               TCFG_AUDIO_ADC_MIC_CHA, \
                               &adc_hdl);
            audio_adc_mic_set_gain(&adc_mode_hdl->adc_ch, \
                                   adc_mode_hdl->ch[0].gain);
            adc_mode_hdl->ch[0].ch_num++;
        }
        if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_1) {
            audio_adc_mic1_open(&adc_mode_hdl->adc_ch, \
                                TCFG_AUDIO_ADC_MIC_CHA, \
                                &adc_hdl);
            audio_adc_mic1_set_gain(&adc_mode_hdl->adc_ch, \
                                    adc_mode_hdl->ch[0].gain);
            adc_mode_hdl->ch[0].ch_num++;
        }
        if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_2) {
            audio_adc_mic2_open(&adc_mode_hdl->adc_ch, \
                                TCFG_AUDIO_ADC_MIC_CHA, \
                                &adc_hdl);
            audio_adc_mic2_set_gain(&adc_mode_hdl->adc_ch, \
                                    adc_mode_hdl->ch[0].gain);
            adc_mode_hdl->ch[0].ch_num++;
        }
        if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_3) {
            audio_adc_mic3_open(&adc_mode_hdl->adc_ch, \
                                TCFG_AUDIO_ADC_MIC_CHA, \
                                &adc_hdl);
            audio_adc_mic3_set_gain(&adc_mode_hdl->adc_ch, \
                                    adc_mode_hdl->ch[0].gain);
            adc_mode_hdl->ch[0].ch_num++;
        }
    } else {
        adc_mode_hdl->ch[0].ch_start = 0;
        adc_mode_hdl->ch[0].ch_num = 0;
    }

    if (adc_mode_hdl->ch[1].enable) {
        adc_mode_hdl->ch[1].ch_start = adc_mode_hdl->ch[0].ch_num;
        adc_mode_hdl->ch[1].ch_num = 0;
        if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE0) {
            audio_adc_linein_open(&adc_mode_hdl->adc_ch, \
                                  TCFG_LINEIN_LR_CH, \
                                  &adc_hdl);
            audio_adc_linein_set_gain(&adc_mode_hdl->adc_ch,
                                      adc_mode_hdl->ch[1].gain);
            adc_mode_hdl->ch[1].ch_num++;
        }
        if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE1) {
            audio_adc_linein1_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_LINEIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein1_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[1].gain);
            adc_mode_hdl->ch[1].ch_num++;
        }
        if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE2) {
            audio_adc_linein2_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_LINEIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein2_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[1].gain);
            adc_mode_hdl->ch[1].ch_num++;
        }
        if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE3) {
            audio_adc_linein3_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_LINEIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein3_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[1].gain);
            adc_mode_hdl->ch[1].ch_num++;
        }
    } else {
        adc_mode_hdl->ch[1].ch_start = 0;
        adc_mode_hdl->ch[1].ch_num = 0;
    }

    if (adc_mode_hdl->ch[2].enable) {
        adc_mode_hdl->ch[2].ch_start = adc_mode_hdl->ch[0].ch_num \
                                       + adc_mode_hdl->ch[1].ch_num;
        adc_mode_hdl->ch[2].ch_num = 0;
        if (TCFG_FMIN_LR_CH & AUDIO_ADC_LINE0) {
            audio_adc_linein_open(&adc_mode_hdl->adc_ch, \
                                  TCFG_FMIN_LR_CH, \
                                  &adc_hdl);
            audio_adc_linein_set_gain(&adc_mode_hdl->adc_ch,
                                      adc_mode_hdl->ch[2].gain);
            adc_mode_hdl->ch[2].ch_num++;
        }
        if (TCFG_FMIN_LR_CH & AUDIO_ADC_LINE1) {
            audio_adc_linein1_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_FMIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein1_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[2].gain);
            adc_mode_hdl->ch[2].ch_num++;
        }
        if (TCFG_FMIN_LR_CH & AUDIO_ADC_LINE2) {
            audio_adc_linein2_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_FMIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein2_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[2].gain);
            adc_mode_hdl->ch[2].ch_num++;
        }
        if (TCFG_FMIN_LR_CH & AUDIO_ADC_LINE3) {
            audio_adc_linein3_open(&adc_mode_hdl->adc_ch, \
                                   TCFG_FMIN_LR_CH, \
                                   &adc_hdl);
            audio_adc_linein3_set_gain(&adc_mode_hdl->adc_ch,
                                       adc_mode_hdl->ch[2].gain);
            adc_mode_hdl->ch[2].ch_num++;
        }
    } else {
        adc_mode_hdl->ch[2].ch_start = 0;
        adc_mode_hdl->ch[2].ch_num = 0;
    }

    adc_mode_hdl->total_ch_num = adc_mode_hdl->ch[0].ch_num \
                                 + adc_mode_hdl->ch[1].ch_num \
                                 + adc_mode_hdl->ch[2].ch_num;

    printf("ch[0].enable:%d", adc_mode_hdl->ch[0].enable);
    printf("ch[1].enable:%d", adc_mode_hdl->ch[1].enable);
    printf("ch[2].enable:%d", adc_mode_hdl->ch[2].enable);
    printf("ch[0] ch_start:%d ch_num:%d\n", adc_mode_hdl->ch[0].ch_start, adc_mode_hdl->ch[0].ch_num);
    printf("ch[1] ch_start:%d ch_num:%d\n", adc_mode_hdl->ch[1].ch_start, adc_mode_hdl->ch[1].ch_num);
    printf("ch[2] ch_start:%d ch_num:%d\n", adc_mode_hdl->ch[2].ch_start, adc_mode_hdl->ch[2].ch_num);
    printf("sr:%d total_ch_num:%d\n", adc_mode_hdl->sample_rate, adc_mode_hdl->total_ch_num);

    u16 irq_points_unit = AUDIO_ADC_IRQ_POINTS;
    if (ladc_irq_point_unit != 0) {
        irq_points_unit = ladc_irq_point_unit;
    }

    adc_mode_hdl->adc_buf_len = adc_mode_hdl->total_ch_num \
                                * AUDIO_ADC_BUF_NUM \
                                * irq_points_unit;
    adc_mode_hdl->adc_buf = zalloc(adc_mode_hdl->adc_buf_len * 2);
    if (adc_mode_hdl->adc_buf == NULL) {
        printf(" audio_adc_mode_open adc_buf malloc err ! size:%d\n", adc_mode_hdl->adc_buf_len * 2);
        return -1;
    }
    printf("audio_adc_mode_open dma adr:%x len:%d\n", adc_mode_hdl->adc_buf, adc_mode_hdl->adc_buf_len);

    audio_adc_mic_set_sample_rate(&adc_mode_hdl->adc_ch, adc_mode_hdl->sample_rate);
    audio_adc_mic_set_buffs(&adc_mode_hdl->adc_ch, \
                            adc_mode_hdl->adc_buf, \
                            irq_points_unit * 2, \
                            AUDIO_ADC_BUF_NUM);

    adc_mode_hdl->output.handler = audio_adc_mode_output_handler;
    adc_mode_hdl->output.priv = adc_mode_hdl;
    audio_adc_add_output_handler(&adc_hdl, &adc_mode_hdl->output);
    adc_mode_hdl->state = 1;
}

// force 1:强制关闭  0:判断其他通道都关闭才关闭
static int audio_adc_mode_close(u8 force)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    if (adc_mode_hdl->state == 0) {
        return -1;
    }

    if (force == 0) {
        if (adc_mode_hdl->ch[0].open_cnt \
            || adc_mode_hdl->ch[1].open_cnt \
            || adc_mode_hdl->ch[2].open_cnt) {
            return -1;
        }
    }

    audio_adc_mic_close(&adc_mode_hdl->adc_ch);
    audio_adc_del_output_handler(&adc_hdl, &adc_mode_hdl->output);
    adc_mode_hdl->state = 0;
    adc_mode_hdl->dma_state = 0;
    if (adc_mode_hdl->adc_buf != NULL) {
        free(adc_mode_hdl->adc_buf);
        adc_mode_hdl->adc_buf = NULL;
    }
    return 0;
}

static int audio_adc_mode_start(void)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    if (adc_mode_hdl->state == 0) {
        return -1;
    }

    if (adc_mode_hdl->dma_state == 1) {
        return -1;
    }

    audio_adc_mic_start(&adc_mode_hdl->adc_ch);
    adc_mode_hdl->dma_state = 1;
}

static int audio_adc_mode_channel_open(u8 type, struct adc_mic_ch *adc, u16 sample_rate, u8 gain)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    if (adc_mode_hdl->ch[type].enable == 0) {
        printf("audio_adc_mode_channel is not enable err!\n");
        return -1;
    }
    printf("audio_adc_mode_channel_open sr :%d gain:%d\n", sample_rate, gain);
    if (adc_mode_hdl->ch[type].open_cnt == 0) {
        adc_mode_hdl->sample_rate = sample_rate;
        adc_mode_hdl->ch[type].gain = gain;
        audio_adc_mode_open();
        adc->adc = &adc_hdl;
    }
    adc_mode_hdl->ch[type].open_cnt++;
    return 0;
}

static void audio_adc_mode_channel_add_output(u8 type, struct audio_adc_output_hdl *output)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return;
    }

    if (adc_mode_hdl->ch[type].enable == 0) {
        printf("audio_adc_mode_channel_add_output adc ch[%d] is not enable err!\n", type);
        return;
    }

    u16 irq_points_unit = AUDIO_ADC_IRQ_POINTS;
    if (ladc_irq_point_unit != 0) {
        irq_points_unit = ladc_irq_point_unit;
    }
    if (adc_mode_hdl->ch[type].temp_buf == NULL) {
        adc_mode_hdl->ch[type].temp_buf = zalloc(adc_mode_hdl->ch[type].ch_num * irq_points_unit * 2);
        if (adc_mode_hdl->ch[type].temp_buf == NULL) {
            printf("audio_adc_mode_channel_add_output adc ch[%d] temp buf malloc err size:%d!\n", type, adc_mode_hdl->ch[type].ch_num * irq_points_unit * 2);
            return;
        }
    }

    struct audio_adc_output_hdl *p;
    local_irq_disable();

    list_for_each_entry(p, &(adc_mode_hdl->ch[type].head), entry) {
        if (p == &output->entry) {
            goto __exit;
        }
    }

    list_add_tail(&output->entry, &(adc_mode_hdl->ch[type].head));

__exit:
    local_irq_enable();
}

static void audio_adc_mode_channle_close(u8 type, struct adc_mic_ch *adc, struct audio_adc_output_hdl *output)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return;
    }

    adc_mode_hdl->ch[type].open_cnt--;
    if (adc_mode_hdl->ch[type].open_cnt < 0) {
        printf("audio_adc_mode_channle_close[%d] cnt=%d err !\n", type, adc_mode_hdl->ch[type].open_cnt);
        return;
    }

    struct audio_adc_output_hdl *p;
    local_irq_disable();
    list_for_each_entry(p, &(adc_mode_hdl->ch[type].head), entry) {
        if (p == &output->entry) {
            list_del(&output->entry);
            break;
        }
    }
    local_irq_enable();

    if (adc_mode_hdl->ch[type].open_cnt == 0) {
        if (adc_mode_hdl->ch[type].temp_buf) {
            free(adc_mode_hdl->ch[type].temp_buf);
            adc_mode_hdl->ch[type].temp_buf = NULL;
        }
        audio_adc_mode_close(0);
    }
}

int audio_adc_mode_switch(u8 mode)
{
    printf("audio_adc_mode_switch %d\n", mode);
    if (adc_mode_hdl == NULL) {
        adc_mode_hdl = zalloc(sizeof(struct _adc_mode_hdl));
        if (adc_mode_hdl == NULL) {
            printf("adc_mode_hdl malloc err !\n");
            return -1;
        }
        INIT_LIST_HEAD(&(adc_mode_hdl->ch[0].head));
        INIT_LIST_HEAD(&(adc_mode_hdl->ch[1].head));
        INIT_LIST_HEAD(&(adc_mode_hdl->ch[2].head));
    }

#if TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD

    if (adc_mode_hdl->mode != mode) {
        local_irq_disable();
        if (mode == ADC_MIC_MODE) {
            adc_mode_hdl->ch[0].enable = 1;
            adc_mode_hdl->ch[1].enable = 0;
            adc_mode_hdl->ch[2].enable = 0;

        } else if (mode == ADC_LINEIN_MODE) {
            adc_mode_hdl->ch[0].enable = 1;
            adc_mode_hdl->ch[1].enable = 1;
            adc_mode_hdl->ch[2].enable = 0;

        } else if (mode == ADC_FM_MODE) {
            adc_mode_hdl->ch[0].enable = 1;
            adc_mode_hdl->ch[1].enable = 0;
            adc_mode_hdl->ch[2].enable = 1;
        }
        local_irq_enable();

        if (adc_mode_hdl->state) {
            audio_adc_mode_close(1);
            adc_mode_hdl->mode = mode;
            audio_adc_mode_open();
            audio_adc_mode_start();
        } else {
            adc_mode_hdl->mode = mode;
        }
    }
    return 0;
#else
    return 0;
#endif
}

int audio_mic_open(struct adc_mic_ch *mic, u16 sample_rate, u8 gain)
{
#if	TCFG_AUDIO_ADC_ENABLE

    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

#if !TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD

    adc_mode_hdl->ch[0].enable = 1;
    adc_mode_hdl->ch[1].enable = 0;
    adc_mode_hdl->ch[2].enable = 0;
#endif
    audio_adc_mode_channel_open(0, mic, sample_rate, gain);
    return 0;

#endif // #if TCFG_AUDIO_ADC_ENABLE

    return -1;
}

void audio_mic_add_output(struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channel_add_output(0, output);
}

void audio_mic_start(struct adc_mic_ch *mic)
{
    audio_adc_mode_start();
}

void audio_mic_close(struct adc_mic_ch *mic, struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channle_close(0, mic, output);
}

void audio_mic_set_gain(u8 gain)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return;
    }

    printf("audio_mic_set_gain:%d\n", gain);
    adc_mode_hdl->ch[0].gain = gain;
    if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_0) {
        audio_adc_mic_set_gain(&adc_mode_hdl->adc_ch, \
                               adc_mode_hdl->ch[0].gain);
    }
    if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_1) {
        audio_adc_mic1_set_gain(&adc_mode_hdl->adc_ch, \
                                adc_mode_hdl->ch[0].gain);
    }
    if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_2) {
        audio_adc_mic2_set_gain(&adc_mode_hdl->adc_ch, \
                                adc_mode_hdl->ch[0].gain);
    }
    if (TCFG_AUDIO_ADC_MIC_CHA & AUDIO_ADC_MIC_3) {
        audio_adc_mic3_set_gain(&adc_mode_hdl->adc_ch, \
                                adc_mode_hdl->ch[0].gain);
    }
}


int audio_linein_open(struct adc_linein_ch *linein, u16 sample_rate, int gain)
{
#if	TCFG_AUDIO_ADC_ENABLE

    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

#if TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD
    audio_adc_mode_switch(ADC_LINEIN_MODE);
#else
    adc_mode_hdl->ch[0].enable = 0;
    adc_mode_hdl->ch[1].enable = 1;
    adc_mode_hdl->ch[2].enable = 0;
#endif

    audio_adc_mode_channel_open(1, linein, sample_rate, gain);
    return 0;

#endif // #if TCFG_AUDIO_ADC_ENABLE

    return -1;
}

void audio_linein_add_output(struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channel_add_output(1, output);
}

void audio_linein_start(struct adc_linein_ch *mic)
{
    audio_adc_mode_start();
}

void audio_linein_close(struct adc_linein_ch *linein, struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channle_close(1, linein, output);
#if TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD
    audio_adc_mode_switch(ADC_MIC_MODE);
#endif
}


void audio_linein_set_gain(int gain)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return;
    }

    printf("audio_linein_set_gain:%d\n", gain);
    adc_mode_hdl->ch[1].gain = gain;
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE0) {
        audio_adc_linein_set_gain(&adc_mode_hdl->adc_ch,
                                  adc_mode_hdl->ch[1].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE1) {
        audio_adc_linein1_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[1].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE2) {
        audio_adc_linein2_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[1].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE3) {
        audio_adc_linein3_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[1].gain);
    }
}

u8 get_audio_linein_ch_num(void)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    return adc_mode_hdl->ch[1].ch_num;
}


int audio_linein_fm_open(struct adc_linein_ch *linein, u16 sample_rate, int gain)
{
#if	TCFG_AUDIO_ADC_ENABLE

    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

#if TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD
    audio_adc_mode_switch(ADC_FM_MODE);
#else
    adc_mode_hdl->ch[0].enable = 0;
    adc_mode_hdl->ch[1].enable = 0;
    adc_mode_hdl->ch[2].enable = 1;
#endif

    audio_adc_mode_channel_open(2, linein, sample_rate, gain);
    return 0;

#endif // #if TCFG_AUDIO_ADC_ENABLE

    return -1;
}

void audio_linein_fm_add_output(struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channel_add_output(2, output);
}

void audio_linein_fm_start(struct adc_linein_ch *linein)
{
    audio_adc_mode_start();
}


void audio_linein_fm_close(struct adc_linein_ch *linein, struct audio_adc_output_hdl *output)
{
    audio_adc_mode_channle_close(2, linein, output);
#if TCFG_MIC_EFFECT_ENABLE //开混响时启用多路AD
    audio_adc_mode_switch(ADC_MIC_MODE);
#endif
}


void audio_linein_fm_set_gain(int gain)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return;
    }

    printf("audio_linein_fm_set_gain:%d\n", gain);
    adc_mode_hdl->ch[2].gain = gain;
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE0) {
        audio_adc_linein_set_gain(&adc_mode_hdl->adc_ch,
                                  adc_mode_hdl->ch[2].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE1) {
        audio_adc_linein1_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[2].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE2) {
        audio_adc_linein2_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[2].gain);
    }
    if (TCFG_LINEIN_LR_CH & AUDIO_ADC_LINE3) {
        audio_adc_linein3_set_gain(&adc_mode_hdl->adc_ch,
                                   adc_mode_hdl->ch[2].gain);
    }
}

u8 get_audio_linein_fm_ch_num(void)
{
    if (adc_mode_hdl == NULL) {
        printf("%s %d hdl is NULL err !\n", __func__, __LINE__);
        return -1;
    }

    return adc_mode_hdl->ch[2].ch_num;
}

/*****************************************************************************/
