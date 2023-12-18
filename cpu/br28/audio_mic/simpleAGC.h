typedef  struct  TMPAGC_G_STRUCT_ {
    int  gainval;
    int  energy;
} TMPAGC_G_STRUCT;

// #define  TST_MAXVAL   1000
// #define  TST_MINVAL   500
#define  TST_MAXVAL   20000
#define  TST_MINVAL   10000

#define TARGE_VAL_DB   (-18)
#define SILENCE_THRESHOLD_DB   (-35)
#define MIC_MIN_GAIN     1
#define MIC_MAX_GAIN     (MIC_MIN_GAIN+3)
#define AGC_CAC_CNT      1
#include "system/includes.h"
void  adjust_gain_init(TMPAGC_G_STRUCT *agc_obj);
void  adjust_gain(TMPAGC_G_STRUCT *agc_obj, short *data, int len);

/*-----------------------------------------*/
//u8 gain_tab[] = {}
typedef  struct  AGC_G_STRUCT_ {
    int energy;
    u8  default_gain;
    u8  default_gain_index;//0λ
    u8  cur_gain_index;
    u8  new_gain_index;
    u8  gain_step;
    u8  adjust_rang;
} AGC_G_STRUCT;
//void  adjust_gain_init(AGC_G_STRUCT *agc_obj,u8 gain,u8 gain_step);
// void  adjust_mic_gain(AGC_G_STRUCT* agc_obj,short *data,int len);
/*-----------------------------------------*/
#include <math.h>
typedef struct {
    float attack_factor;
    float release_factor;
    float level;
    int targe_level;
    int targe_up_level;
    int targe_down_level;
    int slience_level;
    float energy;
    s8 gain;
    s16 pbuf[176 * AGC_CAC_CNT];
    u8 pause;
} level_follower;
// void level_follower_init(level_follower *lvl_flw,float attack_time,float release_time,float fs);
level_follower *level_follower_init(float attack_time, float release_time, float fs, s8 gain);
void  adjust_mic_gain(level_follower *agc_obj, short *data, int len);
void  dajust_mic_gain_pause(level_follower *agc_obj, u8 mark);
void  level_follower_close(level_follower *agc_obj);
