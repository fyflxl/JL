#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_config.h"
#include "asm/audio_adc.h"
#include "sound_device_driver.h"
#include "audio_enc/audio_enc.h"
#include "audio_splicing.h"

#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
extern void audio_adc_set_irq_point_unit(u16 point_unit);
#define ADC_CAPTURE_LOW_LATENCY     1000 //us
#else
#define ADC_CAPTURE_LOW_LATENCY     0
#endif
//-----------------------------------------------------------------------------
// linein
//-----------------------------------------------------------------------------

struct broadcast_linein_hdl {
    struct audio_adc_output_hdl adc_output; //adc 输出
    struct adc_linein_ch linein_ch;      	//linein 通道
    u16 sample_rate;   					    //采样率
    u8 gain;								//增益
    u8 linein_ch_num;						//linein 打开的通道数
    u8 linein_output_ch_num;                //输出pcm数据声道数
    u8 start;								//linein 状态
    s16 temp_buf[256 * 3];                  //256 是中断点数
    void *output_priv;                      //数据接口私有参数
    int (*output_handler)(void *output_priv, s16 *data, int len);//输出接口

};

static struct broadcast_linein_hdl *linein_hdl = NULL;

static void linein_output_data(void *priv, s16 *data, int len)
{
    if (!linein_hdl || !linein_hdl->start) {
        return ;
    }
    int wlen = 0;
    if (linein_hdl->linein_ch_num >= 2) {
        /*输出两个linein的数据,默认输出第一个和第二个采样通道的数据*/
        if (linein_hdl->linein_output_ch_num >= 2) {
            if (linein_hdl->output_handler) {
                wlen = linein_hdl->output_handler(linein_hdl->output_priv, data, len * 2);
            }
            if (wlen < len * 2) {
                putchar('L');
            }
        } else { //硬件配置双声道,需要输出单声道,这种情况最好硬件也配成单声道避免资源浪费.
            pcm_dual_to_single(linein_hdl->temp_buf, data, len * 2);
            if (linein_hdl->output_handler) {
                wlen = linein_hdl->output_handler(linein_hdl->output_priv, linein_hdl->temp_buf, len);
            }
            if (wlen < len) {
                putchar('L');
            }
        }

    } else {
        if (linein_hdl->linein_output_ch_num >= 2) { //硬件配置单声道,需要输出双声道数据,
            pcm_single_to_dual(linein_hdl->temp_buf, data, len);
            if (linein_hdl->output_handler) {
                wlen = linein_hdl->output_handler(linein_hdl->output_priv, linein_hdl->temp_buf, len * 2);
            }
            if (wlen < len * 2) {
                putchar('L');
            }
        } else {
            if (linein_hdl->output_handler) {
                wlen = linein_hdl->output_handler(linein_hdl->output_priv, data, len);
            }
            if (wlen < len) {
                putchar('L');
            }

        }
    }
}

int broadcast_linein_open(struct broadcast_linein_params *params)
{
    struct broadcast_linein_params *broadcast_linein = (struct broadcast_linein_params *)params;
    if (linein_hdl) {
        printf("err !!!  linein has opened\n");
        return -EINVAL;
    }
    linein_hdl = zalloc(sizeof(struct broadcast_linein_hdl));
    ASSERT(linein_hdl);
    linein_hdl->sample_rate = broadcast_linein->sample_rate;
    linein_hdl->gain = broadcast_linein->gain;
    linein_hdl->linein_output_ch_num = broadcast_linein->channel_num;
    linein_hdl->output_priv = broadcast_linein->output_priv;
    linein_hdl->output_handler = broadcast_linein->output_handler;

#if ADC_CAPTURE_LOW_LATENCY
    u16 adc_latency = params->sample_rate * ADC_CAPTURE_LOW_LATENCY / 1000000;
    audio_adc_set_irq_point_unit(adc_latency);
#endif
    if (audio_linein_open(&linein_hdl->linein_ch, linein_hdl->sample_rate, linein_hdl->gain) == 0) {
        linein_hdl->adc_output.handler = linein_output_data;
        linein_hdl->adc_output.priv = linein_hdl;
        linein_hdl->linein_ch_num = get_audio_linein_ch_num();
        printf("-------------linein->channel_num:%d  output_num:%d----------\n", linein_hdl->linein_ch_num, linein_hdl->linein_output_ch_num);
        audio_linein_add_output(&linein_hdl->adc_output);
        audio_linein_start(&linein_hdl->linein_ch);
        linein_hdl->start = 1;
    }
    printf("audio_adc_linein_broadcast start succ\n");
    return 0;

}

void broadcast_linein_close(void)
{
    printf("audio_adc_linein_broadcast_close\n");
    if (!linein_hdl) {
        return ;
    }
    linein_hdl->start = 0;
    audio_linein_close(&linein_hdl->linein_ch, &linein_hdl->adc_output);
#if ADC_CAPTURE_LOW_LATENCY
    audio_adc_set_irq_point_unit(0);
#endif
    local_irq_disable();
    free(linein_hdl);
    linein_hdl = NULL;
    local_irq_enable();
}

#if TCFG_APP_BC_MIC_EN
//-----------------------------------------------------------------------------
// mic
//-----------------------------------------------------------------------------
struct boardcast_mic_hdl {
    struct audio_adc_output_hdl output;
    struct adc_mic_ch mic_ch;
    u16 sample_rate;			//mic 采样率
    u8 gain;					//mic 增益
    u8 mic_output_ch_num;       //输出pcm数据声道数
    u8 start;					//mic 状态
    s16 temp_buf[256 * 3];
    void *output_priv;		//数据接口私有参数
    int (*output_handler)(void *output_priv, s16 *data, int len);	//输出接口
};

static struct boardcast_mic_hdl *mic_hdl = NULL;

/* mic adc 回调 */
static void mic_output_demo(void *priv, s16 *data, int len)
{
    int wlen = 0;
    if (mic_hdl->mic_output_ch_num == 2) {		//广播要双声道数据
        for (int i = 0; i < len / 2; i++) {
            mic_hdl->temp_buf[i * 2] = data[i];
            mic_hdl->temp_buf[i * 2 + 1] = data[i];
        }
        /* wlen = app_audio_output_write(mic_hdl->temp_buf, len*2); */
        if (mic_hdl->output_handler) {
            wlen = mic_hdl->output_handler(mic_hdl->output_priv, mic_hdl->temp_buf, len * 2);
        }
    } else {
        /* wlen = app_audio_output_write(mic_hdl->temp_buf, len*2); */
        if (mic_hdl->output_handler) {
            wlen = mic_hdl->output_handler(mic_hdl->output_priv, data, len);
        }
    }
}

int broadcast_mic_open(struct broadcast_linein_params *params)
{
    struct broadcast_linein_params *broadcast_mic = params;
    if (mic_hdl) {
        printf("\n\n----------Error: mic has opened\n");
        return -EINVAL;
    }
    mic_hdl = zalloc(sizeof(struct boardcast_mic_hdl));
    if (!mic_hdl) {
        printf("\nFailed: mic_hdl malloc failed!\n");
        return -EINVAL;
    }

    mic_hdl->sample_rate = broadcast_mic->sample_rate;
    mic_hdl->gain = broadcast_mic->gain;
    mic_hdl->mic_output_ch_num = broadcast_mic->channel_num;
    mic_hdl->output_priv = broadcast_mic->output_priv;
    mic_hdl->output_handler = broadcast_mic->output_handler;
    r_printf("\n-------------------- mic sample rate: %d, mic gain: %d --------------------\n\n", mic_hdl->sample_rate, mic_hdl->gain);
    r_printf("\n-------------------- mic_output_ch_num: %d\n\n         --------------------\n\n", mic_hdl->mic_output_ch_num);

#if ADC_CAPTURE_LOW_LATENCY
    u16 adc_latency = params->sample_rate * ADC_CAPTURE_LOW_LATENCY / 1000000;
    audio_adc_set_irq_point_unit(adc_latency);
#endif
    if (audio_mic_open(&mic_hdl->mic_ch, mic_hdl->sample_rate, mic_hdl->gain) == 0) {
        mic_hdl->output.handler = mic_output_demo;
        mic_hdl->output.priv = &mic_hdl;
        audio_mic_add_output(&mic_hdl->output);
        audio_mic_start(&mic_hdl->mic_ch);
    }
#if 1
    // 打开dac 播放
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_audio_get_volume(APP_AUDIO_STATE_MUSIC), 1);
    app_audio_output_samplerate_set(mic_hdl->sample_rate);
    app_audio_output_start();
#endif
    printf("audio_adc_mic_broadcast start succ\n");
    return 0;
}


void broadcast_mic_close(void)
{
    if (mic_hdl) {
        audio_mic_close(&mic_hdl->mic_ch, &mic_hdl->output);
#if ADC_CAPTURE_LOW_LATENCY
        audio_adc_set_irq_point_unit(0);
#endif
        local_irq_disable();
        free(mic_hdl);
        mic_hdl = NULL;
        local_irq_enable();
        printf("\n\n----------- mic close succ ------------\n\n");
    }
}
#endif


