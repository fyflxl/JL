/*****************************************************************
>file name : broadcast_codec.h
>create time : Thu 26 May 2022 03:40:56 PM CST
*****************************************************************/
#ifndef BROADCAST_CODEC_H_
#define BROADCAST_CODEC_H_
#include "typedef.h"
#include "audio_base.h"

#define BROADCAST_ENCODING   0x01    /*编码*/
#define BROADCAST_DECODING   0x02    /*解码*/

enum {
    BROADCAST_SOUND_INPUT_USB = 0x0,
    BROADCAST_SOUND_INPUT_LINEIN,
    BROADCAST_SOUND_INPUT_MIC,
    BROADCAST_SOUND_INPUT_IIS,
    BROADCAST_SOUND_INPUT_FILE,
    BROADCAST_SOUND_INPUT_A2DP,
    BROADCAST_SOUND_INPUT_ESCO,
};

struct broadcast_input {
    u8 type;
    void *stream;
    union {
        struct {
            int (*fread)(void *stream, void *buf, int len);
            int (*fseek)(void *stream, u32 offset, int seek_mode);
            int (*get_timestamp)(void *stream, u32 *timestamp);
            int (*flen)(void *stream);
        } file;

        struct {
            int (*get_frame)(void *stream, u8 **frame);
            int (*put_frame)(void *stream, u8 *frame);
        } frame;
    };
};

struct broadcast_output {
    u8 type;
    void *stream;
    int (*output)(void *stream, void *data, int len);
};

struct broadcast_clock {
    void *priv;
    int (*latch_reference_time)(void *priv);
    int (*get_reference_time)(void *priv, u16 *us_1_12th, u32 *us, u32 *event);
    int (*get_clock_time)(void *priv, u32 type);

};

struct broadcast_codec_params {
    u8 device;          //音频设备选择
    u8 mode;            //0 - 编码，1 - 解码
    u8 channel;         //通道数
    u32 coding_type;     //格式
    int sample_rate;    //采样率
    int frame_size;     //帧长（单位ms）
    int bit_rate;       //码率
    int reserved;       //保留设计
    u8 dec_out_ch_mode;  	//解码输出声道模式
    struct broadcast_input input;        //数据输入 - 对应解码模式
    struct broadcast_output output;      //数据输出 - 对应编码模式
    struct broadcast_clock clock;
};


#endif

