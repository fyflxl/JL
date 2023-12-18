#include "effect_parm.h"
#include "media/effects_adj.h"
#include "audio_effect/audio_eff_default_parm.h"
#if defined(TCFG_MIC_EFFECT_ENABLE) && TCFG_MIC_EFFECT_ENABLE
static u8 mic_eff_mode = 0;//EFFECT_REVERB_PARM_OST;
const u16 mic_eq_name[] = {AEID_MIC_EQ0, AEID_MIC_EQ1, AEID_MIC_EQ2, AEID_MIC_EQ3, AEID_MIC_EQ4};
const u16 mic_drc_name[] = {AEID_MIC_DRC0, AEID_MIC_DRC1, AEID_MIC_DRC2, AEID_MIC_DRC3, AEID_MIC_DRC4};

struct eff_parm  eff_mode[EFFECT_REVERB_PARM_MAX];//混响的8个模式

u8 get_mic_eff_mode()
{
    return mic_eff_mode;
}
int get_eq_module_index(u16 module_name)
{
    for (int i = 0; i < ARRAY_SIZE(mic_eq_name); i++) { //5个类型的eq
        if (module_name == mic_eq_name[i]) {
            return i;
        }
    }
    log_e("get eq index err\n");
    return 0;
}
int get_drc_module_index(u16 module_name)
{
    for (int i = 0; i < ARRAY_SIZE(mic_drc_name); i++) { //5个类型的eq
        if (module_name == mic_drc_name[i]) {
            return i;
        }
    }
    log_e("get drc index err\n");
    return 0;
}

void mic_eq_update_parm(u8 mode)
{
    struct eff_parm *mic_eff = &eff_mode[mode];
    for (int i = 0; i < ARRAY_SIZE(mic_eq_name); i++) { //5个类型的eq
        u8 max_nsection = get_eq_nsection(mic_eq_name[i]);
        u8 seg_num = mic_eff->eq_parm[i].seg_num;
        if (seg_num > max_nsection) {
            log_e("eff mode %d, eq%d, seg num %d\n", mode, i, seg_num);
            return;
        }
        float global_gain = mic_eff->eq_parm[i].global_gain;
        cur_eq_set_global_gain(mic_eq_name[i], global_gain);
        printf("seg num %d\n", seg_num);
        for (int j = 0; j < seg_num; j++) { //每个类型的eq有5段系数
            void *tar_seg = &mic_eff->eq_parm[i].seg[j];
            cur_eq_set_update(mic_eq_name[i], tar_seg, seg_num, 1);
        }
    }
}

void mic_drc_update_parm(u8 mode)
{
    u8 type = 0; //全带
    u8 bypass = 0;
    void *tar_wdrc = NULL;
    struct eff_parm *mic_eff = &eff_mode[mode];
    for (int i = 0; i < ARRAY_SIZE(mic_drc_name); i++) { //有5个wdrc
        tar_wdrc = &mic_eff->drc_parm[i];
        cur_drc_set_update(mic_drc_name[i], type, tar_wdrc);
        bypass = mic_eff->drc_parm[i].is_bypass;
        cur_drc_set_bypass(mic_drc_name[i], type, bypass);
    }
}


//模式切换时 参数更新接口
void set_mic_reverb_mode_by_id(u8 mode)
{
    REVERBN_PARM_SET *reverb_parm_obj;
    mic_eff_mode = mode;
    if (mic_eff_mode >= EFFECT_REVERB_PARM_MAX) {
        mic_eff_mode = 0;
    }
    mode = mic_eff_mode;
    if (!mic_effect_get_status()) {
        return ;
    }
    printf("tar mode %d\n", mic_eff_mode);
    switch (mic_eff_mode) {

    case EFFECT_REVERB_PARM_KTV:
        // ktv
        printf(" ktv ........................ \n");
        /* tone_play_by_path(tone_table[IDEX_TONE_NUM_1], 1); */
        break;
    case EFFECT_REVERB_PARM_POP:
        // 流行音效
        /* tone_play_by_path(tone_table[IDEX_TONE_NUM_2], 1); */
        printf(" liu xing.. \n");
        break;
    case EFFECT_REVERB_PARM_qingrou:
        // 柔
        printf(" qing rou.. \n");
        /* tone_play_by_path(tone_table[IDEX_TONE_NUM_3], 1); */
        break;
    case EFFECT_REVERB_PARM_SUPER_REVERB:
        // 超级混响
        printf(" chaoji.. \n");
        /* tone_play_by_path(tone_table[IDEX_TONE_NUM_4], 1); */
        break;
    case EFFECT_REVERB_PARM_SONG:
        // 民歌模式
        printf(" min ge........................ \n");
        /* tone_play_by_path(tone_table[IDEX_TONE_NUM_5], 1); */
        break;

    }
    struct eff_parm *mic_eff = &eff_mode[mode];
    noisegate_update_parm(&mic_eff->noise_gate_parm.parm, mic_eff->noise_gate_parm.is_bypass);
    howling_pitch_shift_update_parm(&mic_eff->howlingps_parm.parm, mic_eff->howlingps_parm.is_bypass);//内部未实现
    notchhowline_update_parm(&mic_eff->notchhowling_parm.parm, mic_eff->notchhowling_parm.is_bypass);//内部未实现
#if defined(TCFG_MIC_VOICE_CHANGER_ENABLE) && TCFG_MIC_VOICE_CHANGER_ENABLE
    audio_voice_changer_update_demo(AEID_MIC_VOICE_CHANGER, &mic_eff->voicechanger_parm.parm, mic_eff->voicechanger_parm.is_bypass);
#endif
    echo_updata_parm(&mic_eff->echo_parm.parm, mic_eff->echo_parm.is_bypass);
    plate_reverb_update_parm(&mic_eff->plate_reverb_parm.parm, mic_eff->plate_reverb_parm.is_bypass);
#if 0//GAIN_PROCESS_EN
    audio_gain_update_parm(AEID_MIC_GAIN, &mic_eff->gain_parm.parm, mic_eff->gain_parm.is_bypass);
#endif
    mic_eq_update_parm(mode);
    mic_tunning_eq_update_parm(mode);
    mic_drc_update_parm(mode);
    printf("mdoe[%d]\n\n", mic_eff_mode);
}


#endif


