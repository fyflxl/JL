/*****************************************************************
>file name : le_audio_test.c
>create time : Fri 27 May 2022 02:21:46 PM CST
*****************************************************************/
#include "le_audio_codec.h"
#include "le_audio_stream.h"
#include "app_config.h"
#include "system/includes.h"
#include "le_common.h"
#include "wireless_dev_manager.h"
#include "big.h"

/* for debug */
/*#define BSB_SEND_DEBUG      1*/
#define LE_AUDIO_TX_TEST    0
#define BIG_PAIR_NAME       "br28_big_soundbox"
#define SET_SDU_PERIOD_MS   10
#define SET_SDU_SIZE        80

/* 内部函数声明 */
static void broadcast_tx_event_callback(const BIG_EVENT event, void *priv);
static void broadcast_rx_event_callback(const BIG_EVENT event, void *priv);
static void broadcast_rx_iso_callback(const void *const buf, size_t length, void *priv);
static int le_audio_bsb_tx_handler(void *priv, void *data, int len);

static const big_callback_t big_tx_cb = {
    .receive_packet_cb      = NULL,
    .event_cb               = broadcast_tx_event_callback,
};

static const big_callback_t big_rx_cb = {
    .receive_packet_cb      = broadcast_rx_iso_callback,
    .event_cb               = broadcast_rx_event_callback,
};

struct le_audio_bsb_context {
    void *audio;
#if LE_AUDIO_TX_TEST
    u16 timer;
#endif
};

static void broadcast_tx_event_callback(const BIG_EVENT event, void *priv)
{
    /*log_info("--func=%s, %d", __FUNCTION__, event);*/

    switch (event) {
    case BIG_EVENT_TRANSMITTER_CONNECT:
        break;
    case BIG_EVENT_TRANSMITTER_DISCONNECT:
        break;
    }
}

static void broadcast_rx_iso_callback(const void *const buf, size_t length, void *priv)
{
    big_stream_param_t *param;

    param = (big_stream_param_t *)priv;

    le_audio_rx_data_handler(NULL, buf, length, param->ts);
}

static void broadcast_rx_event_callback(const BIG_EVENT event, void *priv)
{
    /*log_info("--func=%s, %d", __FUNCTION__, event);*/
}

#if LE_AUDIO_TX_TEST
static void le_audio_bsb_tx_test_timer(void *arg)
{
    char data[32];
    static int test_count = 0;

    sprintf(data, "bsb tx %d", test_count++);
    le_audio_bsb_tx_handler(arg, data, sizeof(data));
}
#endif

void *le_audio_bsb_tx_create(void)
{
    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)zalloc(sizeof(struct le_audio_bsb_context));

    if (!ctx) {
        return NULL;
    }

#if LE_AUDIO_TX_TEST
    ctx->timer = sys_timer_add((void *)ctx, le_audio_bsb_tx_test_timer, 20);
#endif
    return ctx;
}

static void le_audio_bsb_tx_close(void *conn)
{
    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)conn;

    uint8_t big_hdl = 0;

    if (ctx) {
        wireless_dev_close("big_tx", &big_hdl);
#if LE_AUDIO_TX_TEST
        if (ctx->timer) {
            sys_timer_del(ctx->timer);
        }
#endif
        free(ctx);
    }
}

static int le_audio_bsb_tx_handler(void *priv, void *data, int len)
{
    big_stream_param_t param = {0};

    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)priv;

    param.bis_hdl = 0x70;

    if (ctx) {
        return wireless_dev_transmit("big_tx", data, len, &param);
    }

    return 0;
}

void *le_audio_bsb_rx_create(void)
{
    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)zalloc(sizeof(struct le_audio_bsb_context));
    void *audio_stream = le_audio_stream_open(NULL);

    ctx->audio = audio_stream;

    return ctx;
}

static void le_audio_bsb_rx_close(void *conn)
{
    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)conn;

    uint8_t big_hdl = 0;

    if (!ctx) {
        return;
    }

    wireless_dev_close("big_rx", &big_hdl);
    le_audio_stream_close(ctx->audio);
    free(ctx);
}

int le_audio_data_read(void *conn, void *buf, int len)
{
    struct le_audio_bsb_context *ctx = (struct le_audio_bsb_context *)conn;

    if (!ctx) {
        return 0;
    }

    return le_audio_stream_read(ctx->audio, buf, len);
}

int le_audio_data_seek(void *conn, u32 offset, int mode)
{
    return 0;
}


int le_audio_stream_test(void)
{
    void *conn = NULL;
    gpio_set_direction(IO_PORTC_04, 1);
    gpio_set_pull_down(IO_PORTC_04, 0);
    gpio_set_pull_up(IO_PORTC_04, 1);
    gpio_set_die(IO_PORTC_04, 1);
    os_time_dly(2);
    if (gpio_read(IO_PORTC_04) == 0) {
        conn = le_audio_bsb_tx_create();
        big_parameter_t tx_param = {
            .pair_name = BIG_PAIR_NAME,
            .cb        = &big_tx_cb,
            .big_hdl   = 0,
            .num_bis   = 1,
            .ext_phy   = 1,

            .tx = {
                .rtn            = 1,
                .phy            = BIT(0),
                .mtl            = 0,
                .max_sdu        = SET_SDU_SIZE,
                .sdu_int_us     = SET_SDU_PERIOD_MS * 1000L,
            },
        };
        wireless_dev_init("big_tx", NULL);
        wireless_dev_open("big_tx", &tx_param);
        struct le_audio_codec_params encoding_params = {
            .device = LE_SOUND_INPUT_LINEIN,
            .mode = LE_AUDIO_ENCODING,
            .coding_type = AUDIO_CODING_JLA,
            .channel = 2,
            .sample_rate = 44100,
            .frame_size = 100,
            .bit_rate = 64000,
            .data = {
                .output = {
                    .type = 0,
                    .stream = conn,
                    .output = le_audio_bsb_tx_handler,
                },
            },
        };
        le_audio_codec_open(&encoding_params);
    } else {
        conn = le_audio_bsb_rx_create();
        big_parameter_t rx_param = {
            .pair_name = BIG_PAIR_NAME,
            .num_bis   = 1,
            .cb        = &big_rx_cb,
            .ext_phy   = 1,

            .rx = {
                .bis            = {1},
                .bsync_to_ms    = 2000,
            },
        };
        wireless_dev_init("big_rx", NULL);
        wireless_dev_open("big_rx", &rx_param);
        struct le_audio_codec_params decoding_params = {
            .mode = LE_AUDIO_DECODING,
            .coding_type = AUDIO_CODING_JLA,
            .channel = 2,
            .sample_rate = 44100,
            .frame_size = 100,
            .bit_rate = 64000,
            .data = {
                .input = {
                    .type = 0,
                    .stream = conn,
                    .ops = {
                        .file = {
                            .fread = le_audio_data_read,
                            .fseek = le_audio_data_seek,
                        },
                    },
                },
            },
        };
        le_audio_codec_open(&decoding_params);
    }

    return 0;
}
