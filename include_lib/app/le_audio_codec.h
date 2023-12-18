/*****************************************************************
>file name : le_audio_codec.h
>create time : Thu 26 May 2022 03:40:56 PM CST
*****************************************************************/
#ifndef _LE_AUDIO_CODEC_H_
#define _LE_AUDIO_CODEC_H_
#include "typedef.h"
#include "audio_base.h"

#define LE_AUDIO_ENCODING   0x01    /*编码*/
#define LE_AUDIO_DECODING   0x02    /*解码*/

enum {
    LE_SOUND_INPUT_USB = 0x0,
    LE_SOUND_INPUT_LINEIN,
    LE_SOUND_INPUT_MIC,
    LE_SOUND_INPUT_IIS,
    LE_SOUND_INPUT_FILE,
    LE_SOUND_INPUT_A2DP,
    LE_SOUND_INPUT_ESCO,
};

struct le_audio_input {
    u8 type;
    void *stream;
    union {
        struct {
            int (*fread)(void *stream, void *buf, int len);
            int (*fseek)(void *stream, u32 offset, int seek_mode);
            int (*flen)(void *stream);
        } file;

        struct {
            int (*get_frame)(void *stream, u8 **frame);
            int (*put_frame)(void *stream, u8 *frame);
        } frame;
    } ops;
};

struct le_audio_output {
    u8 type;
    void *stream;
    u32(*timestamp)(void *stream);
    int (*output)(void *stream, void *data, int len);
};

struct le_audio_codec_params {
    u8 device;          //音频设备选择
    u8 mode;            //0 - 编码，1 - 解码
    u8 channel;         //通道数
    u32 coding_type;     //格式
    int sample_rate;    //采样率
    int frame_size;     //帧长（单位ms）
    int bit_rate;       //码率
    int reserved;       //保留设计
    union {
        struct le_audio_input input;        //数据输入 - 对应解码模式
        struct le_audio_output output;      //数据输出 - 对应编码模式
    } data;             //数据接口
};

// void *le_audio_codec_open(struct le_audio_codec_params *params);
//
// int le_audio_codec_close(void *codec);

#endif
