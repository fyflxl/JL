/*****************************************************************
>file name : le_audio.h
>create time : Wed 30 Mar 2022 02:23:41 PM CST
*****************************************************************/
#ifndef _LE_AUDIO_APP_H_
#define _LE_AUDIO_APP_H_
#include "typedef.h"

struct le_audio_codec_format {
    int frame_len;
    int bit_rate;
    int sample_rate;
    int channel;
};


void *le_audio_stream_open(struct le_audio_codec_format *fmt);

int le_audio_rx_data_handler(void *stream, void *buf, int len, u32 timestamp);

u32 le_audio_stream_read_timestamp(void *stream, int *error);

int le_audio_stream_read(void *stream, void *buf, u32 len);

void le_audio_stream_close(void *stream);

#endif
