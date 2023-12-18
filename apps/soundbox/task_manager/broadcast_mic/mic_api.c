#include "system/includes.h"
#include "app_config.h"
#include "app_task.h"
#include "media/includes.h"
#include "asm/audio_adc.h"
#include "key_event_deal.h"
#include "user_cfg.h"
#include "clock_cfg.h"
#include "app_main.h"
#include "audio_config.h"
#ifndef CONFIG_MEDIA_NEW_ENABLE
#include "application/audio_output_dac.h"
#endif


#if TCFG_BROADCAST_ENABLE
#include "broadcast_api.h"
#include "broadcast_audio_sync.h"
#include "sound_device_driver.h"
#endif




/*************************************************************
   此文件函数主要是broadcast mic实现api
**************************************************************/

#if TCFG_APP_BC_MIC_EN

#define _debug  printf
#define log_info r_printf

//mic 使用到的结构体
struct mic_adc_hdl {
    struct audio_adc_output_hdl output;
    struct adc_mic_ch mic_ch;
    u16 sample_rate;			//mic 采样率
    u8 gain;
    u8 start;					//mic 状态
    s16 temp_buf[256 * 3];
};
static struct mic_adc_hdl *mic_hdl = NULL;

struct broadcast_mic_opr {
    void *rec_dev;
    u8	volume;
    u8 onoff;
    u8 audio_state;
};
static struct broadcast_mic_opr broadcast_mic_hdl = {0};
#define __this 	(&broadcast_mic_hdl)


#if TCFG_BROADCAST_ENABLE
u8 get_mic_onoff(void);
static int broadcast_mic_start(void);
static void broadcast_mic_stop(void);

//下面这函数暂时放这里，暂不放头文件里，使用时 extern 一下
int mic_start(void);
void mic_stop(void);
void broadcast_mic_hdl_register(void);


static struct broadcast_params_interface broadcast_ops_cb = {
    .start_original = mic_start,
    .stop_original = mic_stop,
    .start = broadcast_mic_start,
    .stop = broadcast_mic_stop,
    .get_status = get_mic_onoff,

    //下面的与 linein_api 的保持一致
    .sync_params = {
        .source = BROADCAST_SOURCE_DMA,
        .input_sample_rate = JLA_CODING_SAMPLERATE,
        .output_sample_rate = JLA_CODING_SAMPLERATE,
        .nch = JLA_CODING_CHANNEL,
        .delay_time = SET_SDU_PERIOD_MS + 5,//45,
    },
    .dec_params = {
        .nch = JLA_CODING_CHANNEL,
        .coding_type = AUDIO_CODING_JLA,
        .sample_rate = JLA_CODING_SAMPLERATE,
        .frame_size = JLA_CODING_FRAME_LEN,
        .bit_rate = JLA_CODING_BIT_RATE,
    },
    .enc_params = {
        .nch = JLA_CODING_CHANNEL,
        .coding_type = AUDIO_CODING_JLA,
        .sample_rate = JLA_CODING_SAMPLERATE,
        .frame_size = JLA_CODING_FRAME_LEN,
        .bit_rate = JLA_CODING_BIT_RATE,
    }
};
#endif  /* TCFG_BROADCAST_ENABLE */



#if TCFG_APP_BC_MIC_EN
static int broadcast_mic_start(void)
{
    if (__this->onoff == 1) {
        log_info("mic is already start\n");
        return true;
    }

    ASSERT(broadcast_ops_cb.sync_hdl->bcsync);

    struct broadcast_linein_params mic = {
        .sample_rate = JLA_CODING_SAMPLERATE,		//这里是48000
        .gain = 15,//10,
        .channel_num = JLA_CODING_CHANNEL,
        .output_priv = broadcast_ops_cb.sync_hdl->bcsync,
        .output_handler = (int (*)(void *, s16 *, int))broadcast_audio_sync_dma_input,
    };


    //开广播mic
    broadcast_mic_open(&mic);
    __this->volume = app_audio_get_volume(__this->audio_state);
    __this->onoff = 1;

    return true;
}

static void broadcast_mic_stop(void)
{
    _debug("\n -- FUNC: broadcast_mic_stop --\n\n");
    broadcast_mic_close();
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
    __this->onoff = 0;
}

u8 get_mic_onoff(void)
{
    return __this->onoff;
}

void broadcast_mic_hdl_register(void)
{
    broadcast_params_interface_register(&broadcast_ops_cb);
}



static void mic_output_demo(void *priv, s16 *data, int len)
{
    int wlen = 0;
#if 1	//推DAC进行播放，单声道变双声道
    for (int i = 0; i < len / 2; i++) {
        mic_hdl->temp_buf[i * 2] = data[i];
        mic_hdl->temp_buf[i * 2 + 1] = data[i];
    }
    wlen = app_audio_output_write(mic_hdl->temp_buf, len * 2);
#endif
    if (wlen != len * 2) {
        putchar('L');
    }
}

int mic_start(void)
{
    _debug("\n-- IN FUNC: mic_start --\n");

    if (__this->onoff == 1) {
        log_info("linein is aleady start\n");
        return true;
    }

    if (mic_hdl == NULL) {
        mic_hdl = zalloc(sizeof(struct mic_adc_hdl));
        if (!mic_hdl) {
            _debug("\n-- IN FUNC: mic_start, mic_hdl malloc failed!\n");
            return -ENOMEM;
        }

        /* 开麦 */
        mic_hdl->sample_rate =  JLA_CODING_SAMPLERATE;
        mic_hdl->gain = 10;
        if (audio_mic_open(&mic_hdl->mic_ch, mic_hdl->sample_rate, mic_hdl->gain) == 0) {
            mic_hdl->output.handler = mic_output_demo;
            mic_hdl->output.priv = &mic_hdl;
            audio_mic_add_output(&mic_hdl->output);
            audio_mic_start(&mic_hdl->mic_ch);
        }
    }
#if 1		//推DAC播放
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_audio_get_volume(APP_AUDIO_STATE_MUSIC), 1);
    app_audio_output_samplerate_set(mic_hdl->sample_rate);
    app_audio_output_start();
#endif


    __this->audio_state = APP_AUDIO_STATE_MUSIC;
    __this->volume = app_audio_get_volume(__this->audio_state);
    __this->onoff = 1;
    return true;
}

void mic_stop(void)
{
    if (__this->onoff == 0) {
        log_info("mic is aleady stop\n");
        return;
    }

    if (mic_hdl == NULL) {
        _debug("\n-- IN FUNC: mic_stop, error: mic_hdl is aleady NULL!\n\n");
        return;
    }
    audio_mic_close(&mic_hdl->mic_ch, &mic_hdl->output);
    local_irq_disable();
    free(mic_hdl);
    mic_hdl = NULL;
    local_irq_enable();
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
    __this->onoff = 0;
    _debug("\n\n------------------------ mic close succ ---------------------\n\n");
}



#endif /* TCFG_BROADCAST_ENABLE */




#endif /* TCFG_BC_MIC_EN */


