//typedef  struct  TMPAGC_G_STRUCT_
//{
//	int  gainval;
//	int  energy;
//}TMPAGC_G_STRUCT;
//
//#define  TST_MAXVAL   10000
//#define  TST_MAXVALh   7000
#include "simpleAGC.h"

void audio_mic_set_gain(u8 gain);

void  adjust_gain_init(TMPAGC_G_STRUCT *agc_obj)
{
    agc_obj->gainval = 512;
    agc_obj->energy = TST_MAXVAL;
}

void  adjust_gain(TMPAGC_G_STRUCT *agc_obj, short *data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        int absv = (data[i] > 0) ? data[i] : -data[i];
        agc_obj->energy = ((agc_obj->energy * 4095) + absv) >> 12;
        if (agc_obj->energy < absv) {
            agc_obj->energy = absv;
        }

        if (agc_obj->energy >= TST_MAXVAL) {
            agc_obj->gainval = (agc_obj->gainval * 8170) >> 13;
            if (agc_obj->gainval < 512) {
                agc_obj->gainval = 512;
            }
        } else if (agc_obj->energy > 100) {
            int destgain = 512; // 512 * TST_MAXVAL / agc_obj->energy;
            if (destgain > 8192) {
                destgain = 8192;
            }
            if (agc_obj->gainval > destgain) {
                agc_obj->gainval = (agc_obj->gainval * 8180) >> 13;
                if (agc_obj->gainval < destgain) {
                    agc_obj->gainval = destgain;
                }

            } else {
                agc_obj->gainval = (agc_obj->gainval * 8210) >> 13;
                if (agc_obj->gainval > destgain) {
                    agc_obj->gainval = destgain;
                }
            }
        }

//		int tmp = (data[i] * agc_obj->gainval) >> 9;
        int tmp = (data[i] * agc_obj->gainval) >> 11;
        if (tmp > 32767) {
            tmp = 32767;
        } else if (tmp < -32768) {
            tmp = -32768;
        }
        data[i] = tmp;
    }
}

/* extern void eq_sqrt(int a, int aQ, int *r, int *rQ); */
#include "asm/math_fast_function.h"
int rms_calc(const short *mono_pcm, int npoint)
{
    long long acc = 0;
    for (int i = 0; i < npoint; i++) {
        acc += (long long)(*mono_pcm) * (long long)(*mono_pcm);
        mono_pcm++;
    }
    struct data_q_struct x;
    x.data = acc / npoint;
    x.q = 0;
    x = root_fix(x);
    x.data >>= x.q;
    return x.data;
}
int peak_calc(const short *mono_pcm, int npoint)
{
    int peak = mono_pcm[0];
    mono_pcm++;
    for (int i = 1; i < npoint; i++) {
        if (peak < (int)(*mono_pcm)) {
            peak = *mono_pcm;
        }
        mono_pcm++;
    }
    return peak;
}

//初始化的fs应该是level_follower_process调用的频率.如果是ADC中断里面调用,fs应该是 采样率/adc中断点数
/* void level_follower_init(level_follower *lvl_flw,float attack_time,float release_time,float fs) */
level_follower *level_follower_init(float attack_time, float release_time, float fs, s8 gain)
{
    level_follower *lvl_flw;
    lvl_flw = zalloc(sizeof(level_follower));
    lvl_flw->attack_factor = expf(-1.f / (attack_time * fs));
    lvl_flw->release_factor = expf(-1.f / (release_time * fs));
    lvl_flw->targe_level = round(pow(10.0, TARGE_VAL_DB / 20.0) * 32767);
    lvl_flw->targe_up_level = round(pow(10.0, (TARGE_VAL_DB + 2) / 20.0) * 32767);
    lvl_flw->targe_down_level = round(pow(10.0, (TARGE_VAL_DB - 2) / 20.0) * 32767);
    lvl_flw->slience_level = round(pow(10.0, SILENCE_THRESHOLD_DB / 20.0) * 32767);
    lvl_flw->gain = gain;
    printf("\n\n targe_up[%d]targe_down[%d],slience[%d]\n\n", lvl_flw->targe_up_level, lvl_flw->targe_down_level, lvl_flw->slience_level);
    return lvl_flw;
}
void level_follower_process(level_follower *lvl_flw, float cur_lvl)
{
    /* printf("energy[%d][%d]\n",lvl_flw->level,cur_lvl); */
    if (cur_lvl > lvl_flw->level) {
        lvl_flw->level = (lvl_flw->level - cur_lvl) * lvl_flw->attack_factor + cur_lvl;
    } else {
        lvl_flw->level = (lvl_flw->level - cur_lvl) * lvl_flw->release_factor + cur_lvl;
    }
}


void  dajust_mic_gain_pause(level_follower *agc_obj, u8 mark)
{
    if (!agc_obj) {
        return;
    }
    agc_obj->pause = mark ? 1 : 0;
    printf("pause:[%d]\n\n", agc_obj->pause);
}



void  level_follower_close(level_follower *agc_obj)
{
    if (!agc_obj) {
        return;
    }
    free(agc_obj);
    printf("pause:[%d]\n\n", agc_obj->pause);
}


void  adjust_mic_gain(level_follower *agc_obj, short *data, int len)
{
    if (!agc_obj) {
        return;
    }
    if (agc_obj->pause) {
        /* if(agc_obj->gain != MIC_MIN_GAIN){ */
        if (agc_obj->gain != 2) {
            /* agc_obj->gain = MIC_MIN_GAIN; */
            agc_obj->gain = 2;
            audio_mic_set_gain(agc_obj->gain);
        }
        return;
    }

    static u8 cnt = 0;
    /* printf("cnt[%d] len[%d]\n",cnt,len); */
    memcpy(&agc_obj->pbuf[cnt * len / 2], data, len);
    if (cnt != (AGC_CAC_CNT - 1)) {
        cnt++;
        return;
    } else {
        cnt = 0;
    }
    agc_obj->energy = rms_calc(agc_obj->pbuf, len * AGC_CAC_CNT / 2);

    /* agc_obj->energy = rms_calc(data,len/2); */
    /* printf("energy[%d]\n",agc_obj->energy); */
    level_follower_process(agc_obj, agc_obj->energy);
    if (agc_obj->level > agc_obj->slience_level) {
        /* putchar('A'); */
        if (agc_obj->level > agc_obj->targe_up_level) {
            /* putchar('-'); */
            if (agc_obj->gain > MIC_MIN_GAIN) {
                agc_obj->gain--;
                audio_mic_set_gain(agc_obj->gain);
            }
        } else if (agc_obj->level < agc_obj->targe_down_level) {

            /* putchar('+'); */
            if (agc_obj->gain < MIC_MAX_GAIN) {
                agc_obj->gain++;
                audio_mic_set_gain(agc_obj->gain);
            }
        }
    } else {
        /* if(agc_obj->gain > MIC_MIN_GAIN){	 */
        /* agc_obj->gain--; */
        /* audio_mic_set_gain(agc_obj->gain); */
        /* } */
    }
}

#if 0
void  adjust_mic_gain(AGC_G_STRUCT *agc_obj, short *data, int len)
{
    if (!agc_obj) {
        return;
    }
#if 10
    static s8 gain = 0;
    static u8 up_mark = 1;
    /* s8 gain = 0; */
    /* return; */
    agc_obj->energy = peak_calc(data, len / 2);
    /* agc_obj->energy = rms_calc(data,len/2); */
    static u16 mark = 0;
    mark++;
    if (mark == 125) {
        if (up_mark) {

            gain++;
            audio_mic_set_gain(gain);
            if (gain > 6) {
                up_mark = 0;
            }
        } else {
            gain--;
            audio_mic_set_gain(gain);
            if (gain == 0) {
                up_mark = 1;
            }
        }
        mark = 0;
    }

#endif

#if 0
    /* printf("energy[%d]\n",agc_obj->energy); */
    if (agc_obj->energy >= TST_MAXVAL) {
        if (agc_obj->new_gain_index) {
            agc_obj->new_gain_index--;
        }

        if (agc_obj->new_gain_index != agc_obj->cur_gain_index) {
            agc_obj->cur_gain_index =    agc_obj->new_gain_index;
            gain = agc_obj->new_gain_index - agc_obj->default_gain_index;
            gain += agc_obj->default_gain;
            if (gain < 0) {
                gain = 0;
            }
            audio_mic_set_gain(gain);
        }
    } else if (agc_obj->energy < TST_MINVAL) {
        agc_obj->new_gain_index++;
        if (agc_obj->new_gain_index > agc_obj->gain_step) {
            agc_obj->new_gain_index = agc_obj->gain_step;
        }
        if (agc_obj->new_gain_index != agc_obj->cur_gain_index) {
            agc_obj->cur_gain_index =    agc_obj->new_gain_index;
            gain = agc_obj->new_gain_index - agc_obj->default_gain_index;
            gain += agc_obj->default_gain;
            if (gain < 0) {
                gain = 0;
            }
            audio_mic_set_gain(gain);
        }
    }
#endif
}
#endif

