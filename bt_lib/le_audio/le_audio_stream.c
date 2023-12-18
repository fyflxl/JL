/*****************************************************************
>file name : le_audio_stream.c
>create time : Wed 30 Mar 2022 02:21:37 PM CST
*****************************************************************/
#include "le_audio_stream.h"
#include "system/includes.h"

struct le_audio_stream_context {
    u8 state;
    /*struct le_audio_codec_format fmt;*/
    cbuffer_t cbuf;
    u8 buf[0];
};

#define LE_AUDIO_TS_ENABLE          0
#define LE_AUDIO_BUFFER_SIZE        1024

static struct le_audio_stream_context *le_audio = NULL;

void *le_audio_stream_open(struct le_audio_codec_format *fmt)
{
    struct le_audio_stream_context *ctx = NULL;
    ctx = (struct le_audio_stream_context *)zalloc(sizeof(struct le_audio_stream_context) + LE_AUDIO_BUFFER_SIZE);
    if (!ctx) {
        return NULL;
    }

    cbuf_init(&ctx->cbuf, ctx->buf, LE_AUDIO_BUFFER_SIZE);
    le_audio = ctx;
#if 0
    memcpy(&ctx->fmt, fmt, sizeof(ctx->fmt));

    int argv[4];
    argv[0] = (int)le_audio_decoder_open;
    argv[1] = 1;
    argv[2] = (int)&ctx->fmt;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
#endif
    return ctx;
}

static void __le_audio_stream_close(struct le_audio_stream_context *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

void le_audio_stream_close(void *stream)
{
    if (!stream) {
        return __le_audio_stream_close(le_audio);
    }

    return __le_audio_stream_close((struct le_audio_stream_context *)stream);
}

int le_audio_rx_data_handler(void *stream, void *buf, int len, u32 timestamp)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)stream;
    if (!len) {
        return 0;
    }

    ctx = le_audio;
    /* if (!ctx) { */
    /*     if (!le_audio) { */
    /*         putchar('*'); */
    /*         return 0; */
    /*     } */
    /*     ctx = le_audio; */
    /* } */

    u32 space = 0;
#if LE_AUDIO_TS_ENABLE
    if (!cbuf_is_write_able(&ctx->cbuf, (len + sizeof(timestamp)))) {
        return 0;
    }
    /*printf("write : %u, %d\n", timestamp, len);*/
    cbuf_write(&ctx->cbuf, &timestamp, sizeof(timestamp));
#else
    if (!cbuf_is_write_able(&ctx->cbuf, len)) {
        putchar('&');
        return 0;
    }
#endif
    cbuf_write(&ctx->cbuf, buf, len);
    return len;
}

u32 le_audio_stream_read_timestamp(void *stream, int *error)
{
#if LE_AUDIO_TS_ENABLE
    u32 timestamp;
    rlen = cbuf_read(&le_audio->cbuf, &timestamp, sizeof(timestamp));
    if (rlen < sizeof(timestamp)) {
        *error = -EINVAL;
        return 0;
    }
    *error = 0;
    return timestamp;
#else
    return 0;
#endif
}

int le_audio_stream_read(void *stream, void *buf, u32 len)
{
    int rlen = 0;
    if (!le_audio) {
        return 0;
    }


    /*ASSERT(len >= le_audio->fmt.frame_len, " , jla decoder need right config.\n"); */

    len = cbuf_read(&le_audio->cbuf, buf, len);//le_audio->fmt.frame_len);

    return len;
}




