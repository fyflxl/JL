/*********************************************************************************************
    *   Filename        : broadcast.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-07-07 14:37

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "app_task.h"
#include "btstack/avctp_user.h"
#include "big.h"
#include "broadcast_api.h"
#include "wireless_dev_manager.h"
#include "clock_cfg.h"
#include "bt.h"

#if TCFG_BROADCAST_ENABLE

#include "broadcast_codec.h"
#include "broadcast_audio_sync.h"
#include "sound_device_driver.h"
#include "broadcast_enc.h"

/**********************
 *      MACROS
 **********************/
#define LOG_TAG_CONST       APP
#define LOG_TAG             "[BC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
/*-----------------------------------------------------------*/

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! \brief 开关总中断 */
#define BROADCAST_ENTER_CRITICAL()  local_irq_disable()
#define BROADCAST_EXIT_CRITICAL()   local_irq_enable()

/*! \brief 正弦波测试使能 */
#define SINE_DATA_DEBUG_EN  0

/*! \brief 广播音频同步使能 */
#define BROADCAST_AUDIO_TIMESTAMP_ENABLE        1

/*! \brief 每包编码数据长度 */
/* (int)((JLA_CODING_FRAME_LEN / 10) * (JLA_CODING_BIT_RATE / 1000 / 8) + 2) */
/* 如果码率超过96K,即帧长超过112,就需要将每次传输数据大小 修改为一帧编码长度 */
#define ENCODE_OUTPUT_FRAME_LEN   (JLA_CODING_FRAME_LEN * JLA_CODING_BIT_RATE / 1000 / 8 / 10 + 2)

/*! \brief 每次传输数据大小 */
#define BROADCAST_TRANSMIT_DATA_LEN (ENCODE_OUTPUT_FRAME_LEN * (SET_SDU_PERIOD_MS * 10 / JLA_CODING_FRAME_LEN))

/*! \brief 编码输出buf大小 */
#define ENC_OUTPUT_BUF_LEN   (BROADCAST_TRANSMIT_DATA_LEN * 2)

/*! \brief:PCM数据buf大小 */
#define PCM_BUF_LEN          (10 * 1024)

/*! \brief 解码器数据获取buf大小 */
#define DEC_INPUT_BUF_LEN    (10 * BROADCAST_TRANSMIT_DATA_LEN)

/*! \brief 同步处理缓存buf大小 */
#define TS_PACK_BUF_LEN      (10 * 4)

/*! \brief 广播解码声道配置
 	JLA_CODING_CHANNEL == 2 即广播数据是双声道时配置有效
    AUDIO_CH_LR = 0,       	//立体声
    AUDIO_CH_L,           	//左声道（单声道）
    AUDIO_CH_R,           	//右声道（单声道）
    AUDIO_CH_DIFF,        	//差分（单声道） 单/双声道输出左右混合时配置
 */
#define BROADCAST_DEC_OUTPUT_CHANNEL  AUDIO_CH_LR

/*! \brief 配对名 */
#define BIG_PAIR_NAME       "br28_big_soundbox"

/*! \brief Broadcast Code */
#define BIG_BROADCAST_CODE  "JL_BROADCAST"

/*! \SDU配置 */
#define SET_SDU_SIZE        BROADCAST_TRANSMIT_DATA_LEN


/*! \配置广播通道数，不同通道可发送不同数据，例如多声道音频  */
#define TX_USED_BIS_NUM     1
#define RX_USED_BIS_NUM     1

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief 广播状态枚举 */
enum {
    BROADCAST_STATUS_STOP,      /*!< 广播停止 */
    BROADCAST_STATUS_STOPPING,  /*!< 广播正在停止 */
    BROADCAST_STATUS_OPEN,      /*!< 广播开启 */
    BROADCAST_STATUS_START,     /*!< 广播启动 */
};

/*! \brief 广播解码数据结构 */
struct broadcast_dec_hdl {
    struct list_head entry; /*!< 解码器链表项，用于多解码器管理 */
    u8 big_hdl;         /*!< big句柄，用于big控制接口 */
    u8 start;           /*!< 解码同步标志 */
    u8 channel_num;
#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    u8 timestamp_enable;
    u8 next_packet;
    u16 packet_size;
    u16 packet_remain;
    u32 timestamp;
    u8 ts_buf[TS_PACK_BUF_LEN];
    cbuffer_t ts_cbuf;
#endif
    int sample_rate;
    int coding_type;
    int frame_size;
    int bit_rate;
    u8 dec_input_buf[DEC_INPUT_BUF_LEN];
    cbuffer_t dec_input_cbuf;
    void *dec_hdl;
    u8 data_empty_flag;    //解码数据欠载标志
};

/*! \brief 广播编码数据结构 */
struct broadcast_enc_hdl {
    struct list_head entry; /*!< 编码器链表项，用于多编码器管理 */
    u8 big_hdl;         /*!< big句柄，用于big控制接口 */
    u8 channel_num;
    int sample_rate;
    int coding_type;
    int frame_size;
    int bit_rate;
    u8 pcm_buf[PCM_BUF_LEN];
    cbuffer_t pcm_cbuf;
    void *enc_hdl;
    void *sync_hdl;
    u8 pcm_block_flag;    //pcm buf阻塞标志
    u8 pcm_empty_flag;    //pcm buf欠载标志
    u16 expect_read_size;
};

/*! \brief 广播结构体 */
struct broadcast_hdl {
    struct list_head entry; /*!< big链表项，用于多big管理 */
    u8 del;
    u8 big_hdl;
    u16 bis_hdl;
    u8 channel_num;
    int sample_rate;
    u8 enc_block_flag;
    union {
        struct {
            u32 big_sync_delay;
            u32 rx_timestamp;
            u8 first_tx_sync;
            u8 enc_output_buf[ENC_OUTPUT_BUF_LEN];
            cbuffer_t enc_output_cbuf;
            struct broadcast_sync_hdl *sync_hdl;
            struct broadcast_enc_hdl *enc_hdl;
            struct broadcast_dec_hdl *local_dec_hdl;
        } tx;

        struct {
            struct broadcast_dec_hdl *dec_hdl;
        } rx;
    };
};

struct big_hdl_info {
    big_hdl_t hdl;  /*!< 记录蓝牙返回的广播参数 */
    u8 used;
    u16 volatile broadcast_status; /*< 记录当前广播状态 */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
static int broadcast_dec_data_receive_handler(void *_hdl, void *data, int len);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
static OS_MUTEX mutex;
static DEFINE_SPINLOCK(sync_lock);
static DEFINE_SPINLOCK(dec_lock);
static DEFINE_SPINLOCK(enc_lock);
static DEFINE_SPINLOCK(broadcast_lock);
static u8 broadcast_role;   /*!< 记录当前广播为接收端还是发送端 */
static u8 broadcast_init_flag;  /*!< 广播初始化标志 */
static u8 g_big_hdl;        /*!< 用于big_hdl获取 */
static u8 broadcast_num;    /*!< 记录当前开启了多少个big广播 */
static u8 transmit_buf[BROADCAST_TRANSMIT_DATA_LEN];    /*!< 用于发送端发数 */
static struct big_hdl_info big_hdl_info[BIG_MAX_NUMS];
static struct list_head sync_list_head = LIST_HEAD_INIT(sync_list_head);
static struct list_head dec_list_head = LIST_HEAD_INIT(dec_list_head);
static struct list_head enc_list_head = LIST_HEAD_INIT(enc_list_head);
static struct list_head broadcast_list_head = LIST_HEAD_INIT(broadcast_list_head);
static struct broadcast_params_interface *broadcast_params_interface;   /*!< 外部注册进来的参数和接口 */
static struct broadcast_sync_info broadcast_data_sync;  /*!< 用于接收同步状态的数据 */

#if SINE_DATA_DEBUG_EN
static u16 s_ptr;
static s16 sine_buffer[1024];
const s16 sin_48k[48] = {
    0, 2139, 4240, 6270, 8192, 9974, 11585, 12998,
    14189, 15137, 15826, 16244, 16384, 16244, 15826, 15137,
    14189, 12998, 11585, 9974, 8192, 6270, 4240, 2139,
    0, -2139, -4240, -6270, -8192, -9974, -11585, -12998,
    -14189, -15137, -15826, -16244, -16384, -16244, -15826, -15137,
    -14189, -12998, -11585, -9974, -8192, -6270, -4240, -2139
};
#endif


/* ***************************************************************************/
/**
 * @brief:获取正弦波数据
 *
 * @param s_cnt:记录上次获取的位置
 * @param data:获取数据的buf
 * @param points:获取点数
 * @param ch:数据通道数
 *
 * @return
 */
/* *****************************************************************************/
#if SINE_DATA_DEBUG_EN
static int get_sine_data(u16 *s_cnt, s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (*s_cnt >= ARRAY_SIZE(sin_48k)) {
            *s_cnt = 0;
        }
        *data++ = sin_48k[*s_cnt];
        if (ch == 2) {
            *data++ = sin_48k[*s_cnt];
        }
        (*s_cnt)++;
    }
    return 0;
}
#endif

void broadcast_params_interface_register(struct broadcast_params_interface *ops)
{
    broadcast_params_interface = ops;
}

static int broadcast_pcm_stream_read(void *_hdl, void *buf, int len)
{
    struct broadcast_enc_hdl *hdl = (struct broadcast_enc_hdl *)_hdl;
    struct broadcast_hdl *broadcast_hdl = 0;

    if (!hdl) {
        return 0;
    }

    spin_lock(&broadcast_lock);
    list_for_each_entry(broadcast_hdl, &broadcast_list_head, entry) {
        if (broadcast_hdl->big_hdl == hdl->big_hdl && broadcast_hdl->del) {
            spin_unlock(&broadcast_lock);
            return 0;
        }
    }
    spin_unlock(&broadcast_lock);
    int rlen = cbuf_read(&hdl->pcm_cbuf, buf, len);
    spin_lock(&enc_lock);
    if (rlen == 0) {   //编码读不到数据,数据欠载。
        hdl->pcm_empty_flag = 1;
        hdl->expect_read_size = len;
    }

    if (hdl->pcm_block_flag) { //数据阻塞时，读取数据后，激活前级;
        hdl->pcm_block_flag = 0;
        broadcast_audio_sync_Kick_start(hdl->sync_hdl);
    }
    spin_unlock(&enc_lock);


    return rlen;
}

static int broadcast_pcm_stream_input(void *_hdl, void *buf, int len)
{
    int wlen = 0;
    struct broadcast_enc_hdl *p;
    spin_lock(&enc_lock);
    list_for_each_entry(p, &enc_list_head, entry) {
#if SINE_DATA_DEBUG_EN
        if (len <= sizeof(sine_buffer)) {
            get_sine_data(&s_ptr, sine_buffer, len / 2, 2);
            wlen = cbuf_write(&p->pcm_cbuf, sine_buffer, len);
        }
#else
        if (!broadcast_params_interface->get_status()) {
            memset(buf, 0, len);
        }
        wlen = cbuf_write(&p->pcm_cbuf, buf, len);
#endif
        if (p->pcm_empty_flag && (cbuf_get_data_len(&p->pcm_cbuf) >= p->expect_read_size)) { //数据欠载时，写入数据，激活后级;
            p->pcm_empty_flag = 0;
            broadcast_enc_kick_start(p->enc_hdl);
        }
        if (wlen < len) {
            p->pcm_empty_flag = 0;
            p->pcm_block_flag = 1;
            broadcast_enc_kick_start(p->enc_hdl);//解码任务与编码任务在双核环境下，的互斥问题，导致编码已经停的情况下，此处设置需要编码激活解码的操作无效问题，引起的JLA解码无法激活的问题
            /*putchar('K');*/
        }
    }
    spin_unlock(&enc_lock);

    return wlen;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 编码器编码数据输出回调
 *
 * @param _list_head:编码器返回的参数
 * @param data:编码器输出的数据buffer
 * @param len:输出数据长度
 *
 * @return 写进cbuf的实际长度
 */
/* ----------------------------------------------------------------------------*/
static int broadcast_enc_output_handler(void *_list_head, void *data, int len)
{
    struct list_head *list_head = (struct list_head *)_list_head;

    if (!list_head) {
        return 0;
    }

    struct broadcast_hdl *p;
    int wlen = 0;
    spin_lock(&broadcast_lock);
    list_for_each_entry(p, list_head, entry) {
        if (p->del) {
            continue;
        }
        wlen = cbuf_write(&p->tx.enc_output_cbuf, data, len);
        if (wlen < len) {
            p->enc_block_flag = 1;
            /*putchar('~');*/
        }
    }
    spin_unlock(&broadcast_lock);

    return wlen;
}

static u32 broadcast_clock_time(void *priv, u32 type)
{
    u32 clock_time;
    switch (type) {
    case CURRENT_TIME:
        int err = wireless_dev_get_cur_clk("big_tx", &clock_time);
        break;
    case PACKET_RX_TIME:
        struct broadcast_sync_hdl  *sync_hdl = (struct broadcast_sync_hdl *)priv;
        struct broadcast_hdl  *broadcast_hdl =  sync_hdl->broadcast_hdl;
        clock_time = broadcast_hdl->tx.rx_timestamp    ;
        break;
    }

    return clock_time;
}

static u32 broadcast_near_trans_time(void *priv)
{
    struct broadcast_sync_hdl *hdl = (struct broadcast_sync_hdl *)priv;
    u16 bis_hdl = 0;

    if (cpu_in_irq()) {
        printf("not support irq handle.\n");
        return 0;
    }

    if (!hdl) {
        return 0;
    }

    struct broadcast_hdl *p;
    spin_lock(&broadcast_lock);
    list_for_each_entry(p, &broadcast_list_head, entry) {
        if (p->big_hdl == hdl->big_hdl) {
            bis_hdl = p->bis_hdl;
        }
    }
    spin_unlock(&broadcast_lock);

    os_sem_set(&hdl->sem, 0);
    wireless_dev_get_last_tx_clk("big_tx", (void *)bis_hdl);
    os_sem_pend(&hdl->sem, 3);

    return hdl->tx_sync_time;
}

extern int a2dp_media_get_remain_play_time(u8 include_tws);
#define RB16(b)    (u16)(((u8 *)b)[0] << 8 | (((u8 *)b))[1])
static int a2dp_rx_buffered_time(void *priv)
{
    struct broadcast_sync_hdl *hdl = (struct broadcast_sync_hdl *)priv;
    int len = 0;
    u8 *packet = a2dp_media_fetch_packet(&len, NULL);
    if (!packet) {
        return -EINVAL;
    }

    u16 seqn = RB16(packet + 2);
    if (seqn == hdl->seqn) {
        return -EINVAL;
    }
    hdl->seqn = seqn;
    return a2dp_media_get_remain_play_time(0);
}
static void *broadcast_audio_sync_init(struct broadcast_sync_params params, u8 big_hdl)
{
    struct broadcast_sync_hdl *hdl = (struct broadcast_sync_hdl *)zalloc(sizeof(struct broadcast_sync_hdl));

    log_info("%s %d", __FUNCTION__, __LINE__);
    os_sem_create(&hdl->sem, 0);

    hdl->channel_num = params.nch;
    hdl->sample_rate = params.input_sample_rate;
    hdl->broadcast_sample_rate = params.output_sample_rate;
    hdl->big_hdl = big_hdl;

    struct broadcast_audio_stream bcsync_params = {
        .source = params.source,
        .nch = params.nch,
        .delay_time = params.delay_time,
        .sample_rate = params.input_sample_rate,
        .broadcast_sample_rate = params.output_sample_rate,
        .priv = hdl,
        .output = broadcast_pcm_stream_input,
        .clock_data = hdl,
        .clock_time = broadcast_clock_time,
        .tick_clock = broadcast_near_trans_time,
        .private_input = hdl,
        .buffered_time = (params.source == BROADCAST_SOURCE_BT_BLOCK ? a2dp_rx_buffered_time : NULL),
        .task = "bcsync",
    };
    hdl->bcsync = broadcast_audio_sync_open(&bcsync_params);
    list_add_tail(&hdl->entry, &sync_list_head);
    return hdl;
}

static void broadcast_audio_sync_uninit(struct broadcast_sync_hdl *hdl)
{
    log_info("broadcast_audio_sync_uninit");
    if (hdl->bcsync) {
        broadcast_audio_sync_close(hdl->bcsync);
    }

    spin_lock(&sync_lock);
    if (hdl) {
        free(hdl);
    }
    spin_unlock(&sync_lock);
}

static void *broadcast_encoder_open(struct broadcast_enc_params params, u8 big_hdl)
{
    log_info("broadcast_encoder_open");
    struct broadcast_enc_hdl *hdl = (struct broadcast_enc_hdl *)zalloc(sizeof(struct broadcast_enc_hdl));

    cbuf_init(&hdl->pcm_cbuf, hdl->pcm_buf, PCM_BUF_LEN);

    hdl->sample_rate = params.sample_rate;
    hdl->coding_type = params.coding_type;
    hdl->channel_num = params.nch;
    hdl->frame_size = params.frame_size;
    hdl->bit_rate = params.bit_rate;
    hdl->big_hdl = big_hdl;

    struct broadcast_codec_params encoder_param = {
        .channel = params.nch,
        .coding_type = params.coding_type,
        .sample_rate = params.sample_rate,
        .frame_size = params.frame_size,
        .bit_rate = params.bit_rate,
        .input = {
            .stream = hdl,
            .file = {
                .fread = broadcast_pcm_stream_read,
            },
        },
        .output = {
            .stream = &broadcast_list_head,
            .output = broadcast_enc_output_handler,
        },
    };
    hdl->enc_hdl = broadcast_enc_open(&encoder_param);
    list_add_tail(&hdl->entry, &enc_list_head);
    return hdl;
}

static void broadcast_transmitter_trigger_audio(void *_hdl)
{
    struct broadcast_sync_hdl *hdl = (struct broadcast_sync_hdl *)_hdl;

    if (hdl->bcsync) {
        broadcast_audio_sync_trigger_start(hdl->bcsync);
    }
}

static void broadcast_encoder_close(struct broadcast_enc_hdl *hdl)
{
    log_info("broadcast_encoder_close");
    if (hdl->enc_hdl) {
        broadcast_enc_close(hdl->enc_hdl);
    }

    spin_lock(&enc_lock);
    if (hdl) {
        free(hdl);
    }
    spin_unlock(&enc_lock);
}

static int broadcast_transmitter_update_smaples(void *_hdl, int frames)
{
    struct broadcast_sync_hdl *hdl = (struct broadcast_sync_hdl *)_hdl;

    if (hdl->bcsync) {
        broadcast_audio_update_tx_num(hdl->bcsync, frames);
    }

    return 0;
}

static int broadcast_rx_data_drain(struct broadcast_dec_hdl *hdl, int len)
{
    void *data;
    int data_len = 0;
    cbuf_read_alloc(&hdl->dec_input_cbuf, &data_len);
    if (data_len >= len) {
        cbuf_read_updata(&hdl->dec_input_cbuf, len);
    }
    return len;
}

static int broadcast_dec_stream_pack_timestamp(void *_hdl, u32 timestamp, int missed)
{
    struct broadcast_dec_hdl *hdl = (struct broadcast_dec_hdl *)_hdl;

    if (!hdl) {
        return 0;
    }

#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    if (hdl->coding_type == AUDIO_CODING_PCM && !hdl->start) {
        hdl->start = 1;
    }

    /*printf("--ts : %d, %d--\n", timestamp, broadcast_clock_time(NULL));*/
    if (hdl->start == 1) {
        hdl->packet_remain = hdl->packet_size;
        hdl->start = 2;
    }

    int wlen = cbuf_write(&hdl->ts_cbuf, &timestamp, sizeof(timestamp));
    if (wlen < sizeof(timestamp)) {
        putchar('E');
        return -ENOMEM;
    }
    if (hdl->data_empty_flag) {
        hdl->data_empty_flag = 0;
        broadcast_dec_kick_start(hdl->dec_hdl);
    }
#endif

    return 0;
}

static int broadcast_dec_data_receive_handler(void *_hdl, void *data, int len)
{
    struct broadcast_dec_hdl *hdl = (struct broadcast_dec_hdl *)_hdl;

    if (!hdl) {
        return  0;
    }

    int wlen = cbuf_write(&hdl->dec_input_cbuf, data, len);
    if (hdl->data_empty_flag) {
        hdl->data_empty_flag = 0;
        broadcast_dec_kick_start(hdl->dec_hdl);
    }
    if (wlen < len) {
        putchar('H');
        return wlen;
    }

    return wlen;
}

static int broadcast_dec_get_timestamp(void *_hdl, u32 *ts)
{
    struct broadcast_dec_hdl *hdl = (struct broadcast_dec_hdl *)_hdl;

#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    if (!ts) {
        return -EINVAL;
    }

    if (hdl->start != 2 || hdl->next_packet) {
        *ts = 0;
        return -EINVAL;
    }

    int rlen = cbuf_read(&hdl->ts_cbuf, ts, sizeof(u32));
    if (rlen < sizeof(u32)) {
        return -EINVAL;
    }
    hdl->next_packet = 1;
    return 0;
#else
    *ts = 0;
    return -EINVAL;
#endif
}

static int broadcast_dec_input_handler(void *_hdl, void *data, int len)
{
    struct broadcast_dec_hdl *hdl = (struct broadcast_dec_hdl *)_hdl;

    if (!hdl) {
        return 0;
    }
#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    if (hdl->timestamp_enable) {
        if (hdl->start != 2 || !hdl->next_packet) {
            hdl->data_empty_flag  = 1;
            return 0;
        }

        if (len >= hdl->packet_remain) {
            len = hdl->packet_remain;
        }
    }
#endif
    int rlen = cbuf_read(&hdl->dec_input_cbuf, data, len);
    if (rlen == 0) {
        hdl->data_empty_flag  = 1;
    }

#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    hdl->packet_remain -= rlen;
    if (hdl->packet_remain == 0) {
        hdl->packet_remain = hdl->packet_size;
        hdl->next_packet = 0;
    }
#endif

    return rlen;
}

static int broadcast_latch_reference_time(void *priv)
{
    return wireless_dev_trigger_latch_time(broadcast_role == BROADCAST_ROLE_RECEIVER ? "big_rx" : "big_tx", priv);
}

static int broadcast_get_reference_time(void *priv, u16 *us_1_12th, u32 *us, u32 *event)
{
    int err = wireless_dev_get_latch_time_us(broadcast_role == BROADCAST_ROLE_RECEIVER ? "big_rx" : "big_tx", us_1_12th, us, event, priv);

    return err;
}

static void *broadcast_decoder_open(struct broadcast_dec_params params, u8 big_hdl, u16 bis_hdl)
{
    log_info("broadcast_decoder_open");
    struct broadcast_dec_hdl *hdl = (struct broadcast_dec_hdl *)zalloc(sizeof(struct broadcast_dec_hdl));

#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    switch (params.coding_type) {
    case AUDIO_CODING_JLA:
        hdl->start = 1;
        hdl->packet_size = BROADCAST_TRANSMIT_DATA_LEN;
        break;
    case AUDIO_CODING_PCM:
        /*PCM 对应TX的一包PCM封装长度，对上下行播放很重要*/
        hdl->packet_size = (params.sample_rate * SET_SDU_PERIOD_MS) / 1000 * params.nch * 2;
        break;
    default:
        break;
    }

    cbuf_init(&hdl->ts_cbuf, hdl->ts_buf, sizeof(hdl->ts_buf));
#endif
    cbuf_init(&hdl->dec_input_cbuf, hdl->dec_input_buf, sizeof(hdl->dec_input_buf));

#if BROADCAST_AUDIO_TIMESTAMP_ENABLE
    if (broadcast_role) {
        hdl->timestamp_enable = 1;
    }
#endif
    hdl->channel_num = params.nch;
    hdl->sample_rate = params.sample_rate;
    hdl->coding_type = params.coding_type;
    hdl->frame_size = params.frame_size;
    hdl->bit_rate = params.bit_rate;
    hdl->big_hdl = big_hdl;

    struct broadcast_codec_params decoding_params = {
        .coding_type = params.coding_type,
        .channel = params.nch,
        .sample_rate = params.sample_rate,
        .frame_size = params.frame_size,
        .bit_rate = params.bit_rate,
        .dec_out_ch_mode = BROADCAST_DEC_OUTPUT_CHANNEL,
        .input = {
            .stream = hdl,
            .file = {
                .fread = broadcast_dec_input_handler,
                .get_timestamp = broadcast_dec_get_timestamp,
            },
        },
        .clock = {
            .priv = (void *)bis_hdl,
            .latch_reference_time = broadcast_latch_reference_time,
            .get_reference_time = broadcast_get_reference_time,
            .get_clock_time = broadcast_clock_time,
        },
    };
    hdl->dec_hdl = broadcast_dec_open(&decoding_params);
    list_add_tail(&hdl->entry, &dec_list_head);
    return hdl;
}

static void broadcast_decoder_close(struct broadcast_dec_hdl *hdl)
{
    log_info("broadcast_decoder_close");
    if (hdl->dec_hdl) {
        broadcast_dec_close(hdl->dec_hdl);
    }

    spin_lock(&dec_lock);
    if (hdl) {
        free(hdl);
    }
    spin_unlock(&dec_lock);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 设置需要同步的状态数据
 *
 * @param big_hdl:big句柄
 * @param data:数据buffer
 * @param length:数据长度
 *
 * @return -1:fail，0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_set_sync_data(u8 big_hdl, void *data, size_t length)
{
#if (!BROADCAST_DATA_SYNC_EN)
    return -1;
#endif

    memcpy(&broadcast_data_sync, data, sizeof(struct broadcast_sync_info));

    u8 find = 0;
    struct big_hdl_info *hdl_info;
    for (u8 i = 0; i < BIG_MAX_NUMS; i++) {
        if ((big_hdl_info[i].hdl.big_hdl == big_hdl) && big_hdl_info[i].used) {
            find = 1;
            hdl_info = &big_hdl_info[i];
            break;
        }
    }

    if ((!find) || (hdl_info->broadcast_status != BROADCAST_STATUS_START)) {
        return -1;
    }

    wireless_dev_status_sync("big_tx", data, length, NULL);

    return 0;
}

static int broadcast_transmitter_connect_callback(void *priv, int crc16)
{
    u8 find = 0;
    int tx_crc = 0;
    struct big_hdl_info *hdl_info;
    struct broadcast_hdl *broadcast_hdl = 0;
    big_hdl_t *hdl = (big_hdl_t *)priv;
    struct broadcast_enc_hdl *broadcast_enc_hdl = NULL;
    struct broadcast_dec_hdl *broadcast_dec_hdl = NULL;

    //互斥量保护临界区代码，避免broadcast_close的代码与其同时运行
    int os_ret = os_mutex_pend(&mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    tx_crc = CRC16(hdl, sizeof(big_hdl_t));

    for (u8 i = 0; i < BIG_MAX_NUMS; i++) {
        if ((big_hdl_info[i].hdl.big_hdl == hdl->big_hdl) &&
            big_hdl_info[i].used &&
            (tx_crc == crc16)) {
            find = 1;
            hdl_info = &big_hdl_info[i];
            break;
        }
    }

    if ((!find) || (hdl_info->broadcast_status < BROADCAST_STATUS_OPEN)) {
        log_error("broadcast not open");
        os_ret = os_mutex_post(&mutex);
        if (os_ret != OS_NO_ERR) {
            log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
            ASSERT(os_ret != OS_ERR_PEND_ISR);
        }
        return 0;
    }

    log_info("broadcast_transmitter_connect_callback");
    log_info("hdl->big_hdl:%d, hdl->bis_hdl:%d", hdl->big_hdl, hdl->bis_hdl[0]);

    if (broadcast_params_interface) {
        broadcast_params_interface->stop_original();
        broadcast_enc_hdl = broadcast_encoder_open(broadcast_params_interface->enc_params, hdl->big_hdl);
#if BROADCAST_LOCAL_DEC_EN
        broadcast_dec_hdl = broadcast_decoder_open(broadcast_params_interface->dec_params, hdl->big_hdl, hdl->bis_hdl[0]);
#endif
        broadcast_params_interface->sync_hdl = broadcast_audio_sync_init(broadcast_params_interface->sync_params, hdl->big_hdl);
        broadcast_enc_hdl->sync_hdl = broadcast_params_interface->sync_hdl->bcsync;
        broadcast_params_interface->start();
    } else {
        log_error("broadcast_params_interface is NULL");
        ASSERT(0);
    }

    for (u8 i = 0; i < TX_USED_BIS_NUM; i++) {
        broadcast_hdl = (struct broadcast_hdl *)zalloc(sizeof(struct broadcast_hdl));
        cbuf_init(&broadcast_hdl->tx.enc_output_cbuf, broadcast_hdl->tx.enc_output_buf, ENC_OUTPUT_BUF_LEN);
        broadcast_hdl->big_hdl = hdl->big_hdl;
        broadcast_hdl->bis_hdl = hdl->bis_hdl[i];
        broadcast_hdl->sample_rate = JLA_CODING_SAMPLERATE;
        broadcast_hdl->tx.big_sync_delay = hdl->big_sync_delay;
        broadcast_hdl->tx.sync_hdl = broadcast_params_interface->sync_hdl;
        broadcast_hdl->tx.enc_hdl = broadcast_enc_hdl;
#if BROADCAST_LOCAL_DEC_EN
        broadcast_hdl->tx.local_dec_hdl = broadcast_dec_hdl;
#endif
        broadcast_hdl->tx.first_tx_sync = 1;
        broadcast_hdl->tx.sync_hdl->broadcast_hdl = broadcast_hdl;
        list_add_tail(&broadcast_hdl->entry, &broadcast_list_head);
    }

    hdl_info->broadcast_status = BROADCAST_STATUS_START;

    broadcast_set_sync_data(hdl->big_hdl, &broadcast_data_sync, sizeof(struct broadcast_sync_info));

    //释放互斥量
    os_ret = os_mutex_post(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 分配big_hdl，并检查hdl是否已被使用
 *
 * @param id:希望分配的id
 * @param head:链表头
 *
 * @return hdl:实际分配的id
 */
/* ----------------------------------------------------------------------------*/
static u16 get_available_big_hdl(u8 id, struct list_head *head)
{
    struct broadcast_hdl *p;
    u8 hdl = id;
    if ((hdl == 0) || (hdl > 0xEF)) {
        hdl = 1;
        g_big_hdl = 1;
    }

    spin_lock(&broadcast_lock);
__again:
    list_for_each_entry(p, head, entry) {
        if (hdl == p->big_hdl) {
            hdl++;
            goto __again;
        }
    }

    if (hdl > 0xEF) {
        hdl = 0;
    }

    if (hdl == 0) {
        hdl++;
        goto __again;
    }

    g_big_hdl = hdl;
    spin_unlock(&broadcast_lock);
    return hdl;
}

void broadcast_init(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (broadcast_init_flag) {
        return;
    }

    broadcast_init_flag = 1;

    int os_ret = os_mutex_create(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(0);
    }

#if LEA_BIG_CTRLER_TX_EN
    //初始化bis发送参数及注册回调
    ret = wireless_dev_init("big_tx", NULL);
    if (ret != 0) {
        log_error("wireless_dev_init fail:0x%x\n", ret);
    }
#endif

#if LEA_BIG_CTRLER_RX_EN
    //初始化bis接收参数及注册回调
    ret = wireless_dev_init("big_rx", NULL);
    if (ret != 0) {
        log_error("wireless_dev_init fail:0x%x\n", ret);
    }
#endif
}

void broadcast_uninit(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (!broadcast_init_flag) {
        return;
    }

    broadcast_init_flag = 0;

    int os_ret = os_mutex_del(&mutex, OS_DEL_NO_PEND);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
    }

#if LEA_BIG_CTRLER_TX_EN
    //初始化bis发送参数及注册回调
    ret = wireless_dev_uninit("big_tx", NULL);
    if (ret != 0) {
        log_error("wireless_dev_uninit fail:0x%x\n", ret);
    }
#endif

#if LEA_BIG_CTRLER_RX_EN
    //初始化bis接收参数及注册回调
    ret = wireless_dev_uninit("big_rx", NULL);
    if (ret != 0) {
        log_error("wireless_dev_uninit fail:0x%x\n", ret);
    }
#endif
}

#define broadcast_get_bis_tick_time(bis)  wireless_dev_get_last_tx_clk("big_tx", (void *)((u32)bis))

static int broadcast_transmitter_audio_data_handler(struct broadcast_hdl *broadcast_hdl)
{
    int err = 0;
    if (broadcast_hdl->tx.first_tx_sync) {
        err = broadcast_get_bis_tick_time(broadcast_hdl->bis_hdl);
        if (err) {
            return -EINVAL;
        }
        broadcast_hdl->tx.first_tx_sync = 0;
        return 0;
    }

    broadcast_transmitter_trigger_audio(broadcast_hdl->tx.sync_hdl);
    int rlen = cbuf_read(&broadcast_hdl->tx.enc_output_cbuf, transmit_buf, BROADCAST_TRANSMIT_DATA_LEN);
    if (broadcast_hdl->enc_block_flag) {
        //取数后激活编码
        broadcast_hdl->enc_block_flag = 0;
        broadcast_enc_kick_start(broadcast_hdl->tx.enc_hdl->enc_hdl);
    }

    if (rlen == BROADCAST_TRANSMIT_DATA_LEN) {
        big_stream_param_t param = {0};
        param.bis_hdl = broadcast_hdl->bis_hdl;
        err = wireless_dev_transmit("big_tx", transmit_buf, BROADCAST_TRANSMIT_DATA_LEN, &param);
        if (err == -1) {
            log_error("wireless_dev_transmit fail\n");
        }
        broadcast_hdl->tx.rx_timestamp = (broadcast_hdl->tx.rx_timestamp + SET_SDU_PERIOD_MS * 1000L) & 0xfffffff;
        broadcast_transmitter_update_smaples(broadcast_hdl->tx.sync_hdl, SET_SDU_PERIOD_MS * broadcast_hdl->sample_rate / 1000);
#if BROADCAST_LOCAL_DEC_EN
        if (broadcast_hdl->tx.local_dec_hdl->coding_type == AUDIO_CODING_JLA) {
            broadcast_dec_data_receive_handler(broadcast_hdl->tx.local_dec_hdl, transmit_buf, BROADCAST_TRANSMIT_DATA_LEN);
        }
        broadcast_dec_stream_pack_timestamp(broadcast_hdl->tx.local_dec_hdl, broadcast_hdl->tx.rx_timestamp, 0);
#endif
        broadcast_get_bis_tick_time(broadcast_hdl->bis_hdl);
    } else {
        putchar('^');
    }

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播发送端上层事件处理回调
 *
 * @param event:具体事件
 * @param priv:事件处理用到的相关参数
 */
/* ----------------------------------------------------------------------------*/
static void broadcast_tx_event_callback(const BIG_EVENT event, void *priv)
{
    u8 i;
    u8 find = 0;
    int rlen, ret;
    big_stream_param_t param = {0};
    struct broadcast_hdl *broadcast_hdl = 0;

    /* log_info("--func=%s, %d", __FUNCTION__, event); */

    switch (event) {
    //bis发射端开启成功后回调事件
    case BIG_EVENT_TRANSMITTER_CONNECT:
        log_info("BIS_EVENT_TRANSMITTER_CONNECT\n");
        big_hdl_t *hdl = (big_hdl_t *)priv;
        for (i = 0; i < BIG_MAX_NUMS; i++) {
            if ((big_hdl_info[i].hdl.big_hdl == hdl->big_hdl) && big_hdl_info[i].used) {
                find = 1;
                memcpy(&big_hdl_info[i].hdl, priv, sizeof(big_hdl_t));
                break;
            }
        }
        if (!find) {
            break;
        }
        u16 crc16 = CRC16(&big_hdl_info[i].hdl, sizeof(big_hdl_t));
        int msg[4];
        msg[0] = (int)broadcast_transmitter_connect_callback;
        msg[1] = 2;
        msg[2] = (int)(&big_hdl_info[i].hdl);
        msg[3] = (int)crc16;
        int os_err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
        if (os_err != OS_ERR_NONE) {
            log_error("os_taskq_post_type ERROR, os_err:0x%x", os_err);
        }
        break;

    case BIG_EVENT_TRANSMITTER_DISCONNECT:
        log_info("BIG_EVENT_TRANSMITTER_DISCONNECT\n");
        break;

    //蓝牙取数发射回调事件
    case BIG_EVENT_TRANSMITTER_ALIGN:
        /* WARNING:该事件为中断函数回调, 不要添加过多打印 */
        u8 big_hdl = *((u8 *)priv);
        spin_lock(&broadcast_lock);
        list_for_each_entry(broadcast_hdl, &broadcast_list_head, entry) {
            if (broadcast_hdl->big_hdl != big_hdl) {
                continue;
            }

            if (broadcast_hdl->del) {
                continue;
            }

            spin_unlock(&broadcast_lock);
            broadcast_transmitter_audio_data_handler(broadcast_hdl);
            spin_lock(&broadcast_lock);
        }
        spin_unlock(&broadcast_lock);
        break;

    case BIG_EVENT_TRANSMITTER_READ_TX_SYNC:
        big_stream_param_t *param = (big_stream_param_t *)priv;
        spin_lock(&broadcast_lock);
        list_for_each_entry(broadcast_hdl, &broadcast_list_head, entry) {
            if (broadcast_hdl->del) {
                continue;
            }
            if (broadcast_hdl->bis_hdl == param->bis_hdl) {
                u32 send_timestamp = (param->ts + broadcast_hdl->tx.big_sync_delay + SET_MTL_TIME * 1000L + SET_SDU_PERIOD_MS * 1000L) & 0xfffffff;
                if (broadcast_hdl->tx.rx_timestamp) {
                    if ((int)(send_timestamp - broadcast_hdl->tx.rx_timestamp) < -3) {
                        log_error("send ts error : %d, %d", broadcast_hdl->tx.rx_timestamp, send_timestamp);
                        break;
                    }
                }
                broadcast_hdl->tx.rx_timestamp = send_timestamp;
                /*log_info("tx_ts=%d", param->ts);*/
                break;
            }
        }
        spin_unlock(&broadcast_lock);
        break;
    }
}

/* 全局常量 */
static const big_callback_t big_tx_cb = {
    .receive_packet_cb      = NULL,
    .receive_padv_data_cb   = NULL,
    .event_cb               = broadcast_tx_event_callback,
};

/* ***************************************************************************/
/**
 * @brief open broadcast as transmitter
 *
 * @return err:-1, success:available_big_hdl
 */
/* *****************************************************************************/
int broadcast_transmitter(void)
{
    log_info("--func=%s", __FUNCTION__);
    u8 i;
    int ret;

    if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
        log_error("broadcast_role err");
        return -1;
    }

    if (broadcast_num >= BIG_MAX_NUMS) {
        log_error("broadcast_num overflow");
        return -1;
    }

    if (!broadcast_init_flag) {
        return -2;
    }

    u8 available_big_hdl = get_available_big_hdl(++g_big_hdl, &broadcast_list_head);

    for (i = 0; i < BIG_MAX_NUMS; i++) {
        if (!big_hdl_info[i].used) {
            big_hdl_info[i].used = 1;
            big_hdl_info[i].hdl.big_hdl = available_big_hdl;
            break;
        }
    }

    u32 pair_code;
    ret = syscfg_read(VM_WIRELESS_PAIR_CODE, &pair_code, sizeof(u32));
    if (ret <= 0) {
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_code = 0xFFFFFFFE;
#else
        pair_code = 0;
#endif
    }
    g_printf("wireless_pair_code:0x%x", pair_code);

    //配置bis发送参数
    big_parameter_t tx_param = {0};

    memcpy(tx_param.pair_name, BIG_PAIR_NAME, sizeof(BIG_PAIR_NAME));
    tx_param.cb                 = &big_tx_cb;
    tx_param.big_hdl            = available_big_hdl;
    tx_param.num_bis            = TX_USED_BIS_NUM;
    tx_param.ext_phy            = 1;
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
    tx_param.form               = 1;
#else
    tx_param.form               = 0;
#endif
#if (JLA_CODING_BIT_RATE > 96000)
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
    tx_param.tx.rtn             = 3;
#else
    tx_param.tx.rtn             = 4;
#endif
#else
    tx_param.tx.rtn             = 4;
#endif
    tx_param.tx.phy             = BIT(1);
    tx_param.tx.aux_phy         = 2;
    tx_param.tx.mtl             = SET_MTL_TIME; /*这个信息很关键，TX端的时间戳需要根据这个相应做delay*/
    tx_param.tx.max_sdu         = SET_SDU_SIZE;
    tx_param.tx.sdu_int_us      = SET_SDU_PERIOD_MS * 1000L;
#if (BROADCAST_TRANSMIT_DATA_LEN > 251)
    tx_param.tx.vdr.max_pdu     = ENCODE_OUTPUT_FRAME_LEN;
#endif /* (BROADCAST_TRANSMIT_DATA_LEN > 251) */
    tx_param.pri_ch             = pair_code;

    //启动广播
    ret = wireless_dev_open("big_tx", &tx_param);
    if (ret != 0) {
        log_error("wireless_dev_open fail:0x%x\n", ret);
        if (broadcast_num == 0) {
            broadcast_role = BROADCAST_ROLE_UNKNOW;
        }
        big_hdl_info[i].used = 0;
        return -1;
    }

    big_hdl_info[i].broadcast_status = BROADCAST_STATUS_OPEN;
    broadcast_role = BROADCAST_ROLE_TRANSMITTER;	//发送模式

    /* bt_close_discoverable_and_connectable(); */

    broadcast_num++;

    clock_add_set(BROADCAST_CLK);

    return available_big_hdl;
}

static int broadcast_receiver_connect_callback(void *priv, int crc16)
{
    u8 find = 0;
    int rx_crc = 0;
    struct big_hdl_info *hdl_info;
    struct broadcast_hdl *broadcast_hdl = 0;
    big_hdl_t *hdl = (big_hdl_t *)priv;

    int os_ret = os_mutex_pend(&mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    rx_crc = CRC16(hdl, sizeof(big_hdl_t));

    for (u8 i = 0; i < BIG_MAX_NUMS; i++) {
        if ((big_hdl_info[i].hdl.big_hdl == hdl->big_hdl) &&
            big_hdl_info[i].used &&
            (rx_crc == crc16)) {
            find = 1;
            hdl_info = &big_hdl_info[i];
            break;
        }
    }

    if ((!find) || (hdl_info->broadcast_status < BROADCAST_STATUS_OPEN)) {
        log_error("broadcast not open");
        os_ret = os_mutex_post(&mutex);
        if (os_ret != OS_NO_ERR) {
            log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
            ASSERT(os_ret != OS_ERR_PEND_ISR);
        }
        return 0;
    }

    log_info("broadcast_receiver_connect_callback");
    log_info("hdl->big_hdl:%d, hdl->bis_hdl:%d", hdl->big_hdl, hdl->bis_hdl[0]);

    struct broadcast_dec_params params = {
        .nch = JLA_CODING_CHANNEL,
        .coding_type = AUDIO_CODING_JLA,
        .sample_rate = JLA_CODING_SAMPLERATE,
        .frame_size = JLA_CODING_FRAME_LEN,
        .bit_rate = JLA_CODING_BIT_RATE,
    };

    //打开解码器
    struct broadcast_dec_hdl *broadcast_dec_hdl = broadcast_decoder_open(params, hdl->big_hdl, hdl->bis_hdl[0]);

    for (u8 i = 0; i < RX_USED_BIS_NUM; i++) {
        broadcast_hdl = (struct broadcast_hdl *)zalloc(sizeof(struct broadcast_hdl));
        broadcast_hdl->big_hdl = hdl->big_hdl;
        broadcast_hdl->bis_hdl = hdl->bis_hdl[i];
        broadcast_hdl->sample_rate = JLA_CODING_SAMPLERATE;
        broadcast_hdl->rx.dec_hdl = broadcast_dec_hdl;
        list_add_tail(&broadcast_hdl->entry, &broadcast_list_head);
    }

    hdl_info->broadcast_status = BROADCAST_STATUS_START;

    os_ret = os_mutex_post(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    return 0;
}

static int broadcast_receiver_disconnect_callback(void *priv)
{
    u8 find = 0;
    struct big_hdl_info *hdl_info;
    struct broadcast_hdl *p, *n;
    struct broadcast_dec_hdl *dec_p, *dec_n;
    u8 big_hdl = *((u8 *)priv);

    //互斥量保护临界区代码，避免broadcast_close的代码与其同时运行
    int os_ret = os_mutex_pend(&mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    for (u8 i = 0; i < BIG_MAX_NUMS; i++) {
        if ((big_hdl_info[i].hdl.big_hdl == big_hdl) && big_hdl_info[i].used) {
            find = 1;
            hdl_info = &big_hdl_info[i];
            break;
        }
    }

    if ((!find) || (hdl_info->broadcast_status < BROADCAST_STATUS_OPEN)) {
        log_error("broadcast not open");
        os_ret = os_mutex_post(&mutex);
        if (os_ret != OS_NO_ERR) {
            log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
            ASSERT(os_ret != OS_ERR_PEND_ISR);
        }
        return 0;
    }

    log_info("%s, big_hdl:%d", __FUNCTION__, big_hdl);

    spin_lock(&broadcast_lock);
    list_for_each_entry_safe(p, n, &broadcast_list_head, entry) {
        if (p->big_hdl == big_hdl) {
            list_del(&p->entry);
            free(p);
        }
    }
    spin_unlock(&broadcast_lock);

    spin_lock(&dec_lock);
    list_for_each_entry_safe(dec_p, dec_n, &dec_list_head, entry) {
        if (dec_p->big_hdl == big_hdl) {
            //关闭解码器
            list_del(&dec_p->entry);
            spin_unlock(&dec_lock);
            broadcast_decoder_close(dec_p);
            spin_lock(&dec_lock);
        }
    }
    spin_unlock(&dec_lock);

    //释放互斥量
    os_ret = os_mutex_post(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播接收端上层事件处理回调
 *
 * @param event:具体事件
 * @param priv:事件处理用到的相关参数
 */
/* ----------------------------------------------------------------------------*/
static void broadcast_rx_event_callback(const BIG_EVENT event, void *priv)
{
    u8 i;
    u8 find = 0;
    int msg[4];
    int os_err;

    log_info("--func=%s, %d", __FUNCTION__, event);

    switch (event) {
    //bis接收端开启成功后回调事件
    case BIG_EVENT_RECEIVER_CONNECT:
        log_info("BIS_EVENT_RECEIVER_CONNECT\n");
        big_hdl_t *hdl = (big_hdl_t *)priv;
        for (i = 0; i < BIG_MAX_NUMS; i++) {
            if ((big_hdl_info[i].hdl.big_hdl == hdl->big_hdl) && big_hdl_info[i].used) {
                find = 1;
                memcpy(&big_hdl_info[i].hdl, priv, sizeof(big_hdl_t));
                break;
            }
        }
        if (!find) {
            break;
        }
        u16 crc16 = CRC16(&big_hdl_info[i].hdl, sizeof(big_hdl_t));
        msg[0] = (int)broadcast_receiver_connect_callback;
        msg[1] = 2;
        msg[2] = (int)(&big_hdl_info[i].hdl);
        msg[3] = (int)crc16;
        os_err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
        if (os_err != OS_ERR_NONE) {
            log_error("os_taskq_post_type ERROR, os_err:0x%x", os_err);
        }
        break;
    //bis接收端关闭成功后回调事件
    case BIG_EVENT_RECEIVER_DISCONNECT:
        log_info("BIG_EVENT_RECEIVER_DISCONNECT\n");
        u8 rx_big_hdl = *((u8 *)priv);
        for (i = 0; i < BIG_MAX_NUMS; i++) {
            if ((big_hdl_info[i].hdl.big_hdl == rx_big_hdl) && big_hdl_info[i].used) {
                find = 1;
                break;
            }
        }
        if (!find) {
            break;
        }
        msg[0] = (int)broadcast_receiver_disconnect_callback;
        msg[1] = 1;
        msg[2] = (int) & (big_hdl_info[i].hdl.big_hdl);
        os_err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        if (os_err != OS_ERR_NONE) {
            log_error("os_taskq_post_type ERROR, os_err:0x%x", os_err);
        }
        break;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端接收数据回调函数
 *
 * @param buf:数据缓存
 * @param length:数据长度
 * @param priv:时间戳等参数
 */
/* ----------------------------------------------------------------------------*/
static void broadcast_rx_iso_callback(const void *const buf, size_t length, void *priv)
{
    big_stream_param_t *param;
    struct broadcast_hdl *hdl;

    param = (big_stream_param_t *)priv;

    /* log_info("<<- BIS Data Out <<- TS:%d,%d", param->ts, length); */
    /* put_buf(buf, 2); */

    spin_lock(&broadcast_lock);
    list_for_each_entry(hdl, &broadcast_list_head, entry) {
        if (hdl->del) {
            continue;
        }
        if (hdl->bis_hdl == param->bis_hdl) {
            int catch = broadcast_dec_data_receive_handler(hdl->rx.dec_hdl, buf, length);
            if (catch) {
                broadcast_dec_stream_pack_timestamp(hdl->rx.dec_hdl, param->ts, 0);
            }
        }
    }
    spin_unlock(&broadcast_lock);
}

static void broadcast_rx_padv_data_callback(const void *const buf, size_t length, u8 big_hdl)
{
#if (!BROADCAST_DATA_SYNC_EN)
    return;
#endif
    u8 i;
    u8 *temp_buf = buf;
    u8 need_deal_flag = 0;
    int msg[4];
    int os_err;
    if (memcmp(&broadcast_data_sync, temp_buf, sizeof(struct broadcast_sync_info))) {
        memcpy(&broadcast_data_sync, temp_buf, sizeof(struct broadcast_sync_info));
        need_deal_flag = 1;
    }
    extern int broadcast_padv_data_deal(void *priv);
    if (need_deal_flag) {
        msg[0] = (int)broadcast_padv_data_deal;
        msg[1] = 1;
        msg[2] = (int)&broadcast_data_sync;
        os_err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        if (os_err != OS_ERR_NONE) {
            log_error("os_taskq_post_type ERROR, os_err:0x%x", os_err);
        }
    }
}

/* 全局常量 */
static const big_callback_t big_rx_cb = {
    .receive_packet_cb      = broadcast_rx_iso_callback,
    .receive_padv_data_cb   = broadcast_rx_padv_data_callback,
    .event_cb               = broadcast_rx_event_callback,
};

/* ***************************************************************************/
/**
 * @brief open broadcast as receiver
 *
 * @return err:-1, success:available_big_hdl
 */
/* *****************************************************************************/
int broadcast_receiver(void)
{
    u8 i;
    int ret;

    if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
        log_error("broadcast_role err");
        return -1;
    }

    if (broadcast_num >= BIG_MAX_NUMS) {
        log_error("broadcast_num overflow");
        return -1;
    }

    if (!broadcast_init_flag) {
        return -2;
    }

    u8 available_big_hdl = get_available_big_hdl(++g_big_hdl, &broadcast_list_head);

    for (i = 0; i < BIG_MAX_NUMS; i++) {
        if (!big_hdl_info[i].used) {
            big_hdl_info[i].used = 1;
            big_hdl_info[i].hdl.big_hdl = available_big_hdl;
            break;
        }
    }

    u32 pair_code;
    ret = syscfg_read(VM_WIRELESS_PAIR_CODE, &pair_code, sizeof(u32));
    if (ret <= 0) {
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_code = 0xFFFFFFFF;
#else
        pair_code = 0;
#endif
    }
    g_printf("wireless_pair_code:0x%x", pair_code);

    //配置bis接收参数
    big_parameter_t rx_param = {0};

    memcpy(rx_param.pair_name, BIG_PAIR_NAME, sizeof(BIG_PAIR_NAME));
    rx_param.big_hdl            = available_big_hdl;
    rx_param.num_bis            = RX_USED_BIS_NUM;
    rx_param.cb                 = &big_rx_cb;
    rx_param.ext_phy            = 1;
    rx_param.rx.bis[0]          = 1;
    rx_param.rx.bsync_to_ms     = 2000;
    rx_param.pri_ch             = pair_code;

    log_info("--func=%s", __FUNCTION__);

    //启动广播
    ret = wireless_dev_open("big_rx", &rx_param);
    if (ret != 0) {
        log_error("wireless_dev_open fail:0x%x\n", ret);
        if (broadcast_num == 0) {
            broadcast_role = BROADCAST_ROLE_UNKNOW;
        }
        big_hdl_info[i].used = 0;
        return -1;
    }

    //接收方开启广播后关闭经典蓝牙可发现可连接,并默认切回蓝牙模式运行
#if BROADCAST_RECEIVER_CLOSE_EDR_CONN
    bt_close_discoverable_and_connectable();
#endif
    if (app_get_curr_task() != APP_BT_TASK) {
        /* app_task_switch_to(APP_BT_TASK); */
    }

    big_hdl_info[i].broadcast_status = BROADCAST_STATUS_OPEN;
    broadcast_role = BROADCAST_ROLE_RECEIVER;

    broadcast_num++;

    clock_add_set(BROADCAST_CLK);

    return available_big_hdl;
}

/* ***************************************************************************/
/**
 * @brief close broadcast function
 *
 * @param big_hdl:need closed of big_hdl
 */
/* *****************************************************************************/
void broadcast_close(u8 big_hdl)
{
    struct big_hdl_info *hdl_info;
    u8 status = 0;
    u8 find = 0;
    int ret;

    if (!broadcast_init_flag) {
        return;
    }

    for (u8 i = 0; i < BIG_MAX_NUMS; i++) {
        if ((big_hdl_info[i].hdl.big_hdl == big_hdl) && big_hdl_info[i].used) {
            find = 1;
            hdl_info = &big_hdl_info[i];
            break;
        }
    }

    if ((!find) || (hdl_info->broadcast_status < BROADCAST_STATUS_OPEN)) {
        return;
    }

    log_info("--func=%s", __FUNCTION__);

    int os_ret = os_mutex_pend(&mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

    //互斥量保护临界区代码
    struct broadcast_hdl *hdl;
    spin_lock(&broadcast_lock);
    list_for_each_entry(hdl, &broadcast_list_head, entry) {
        if (hdl->big_hdl != big_hdl) {
            continue;
        }
        hdl->del = 1;
    }
    spin_unlock(&broadcast_lock);

    if ((broadcast_role == BROADCAST_ROLE_TRANSMITTER) &&
        (hdl_info->broadcast_status == BROADCAST_STATUS_START)) {
        status = broadcast_params_interface->get_status();
        broadcast_params_interface->stop();
    }

    hdl_info->broadcast_status = BROADCAST_STATUS_STOPPING;

    //关闭同步模块
    struct broadcast_sync_hdl *sync_p, *sync_n;
    spin_lock(&sync_lock);
    list_for_each_entry_safe(sync_p, sync_n, &sync_list_head, entry) {
        if (sync_p->big_hdl != big_hdl) {
            continue;
        }

        list_del(&sync_p->entry);
        spin_unlock(&sync_lock);
        broadcast_audio_sync_uninit(sync_p);
        spin_lock(&sync_lock);
    }
    broadcast_params_interface->sync_hdl = NULL;
    spin_unlock(&sync_lock);

    //关闭编码模块
    struct broadcast_enc_hdl *enc_p, *enc_n;
    spin_lock(&enc_lock);
    list_for_each_entry_safe(enc_p, enc_n, &enc_list_head, entry) {
        if (enc_p->big_hdl != big_hdl) {
            continue;
        }

        list_del(&enc_p->entry);
        spin_unlock(&enc_lock);
        broadcast_encoder_close(enc_p);
        spin_lock(&enc_lock);
    }
    spin_unlock(&enc_lock);

    //关闭解码模块
    struct broadcast_dec_hdl *dec_p, *dec_n;
    spin_lock(&dec_lock);
    list_for_each_entry_safe(dec_p, dec_n, &dec_list_head, entry) {
        if (dec_p->big_hdl != big_hdl) {
            continue;
        }

        list_del(&dec_p->entry);
        spin_unlock(&dec_lock);
        broadcast_decoder_close(dec_p);
        spin_lock(&dec_lock);
    }
    spin_unlock(&dec_lock);

    //关闭广播
    if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
        ret = wireless_dev_close("big_rx", &big_hdl);
        if (ret != 0) {
            log_error("wireless_dev_close fail:0x%x\n", ret);
        }
    } else if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
        ret = wireless_dev_close("big_tx", &big_hdl);
        if (ret != 0) {
            log_error("wireless_dev_close fail:0x%x\n", ret);
        }
    }

    //释放广播链表
    struct broadcast_hdl *p, *n;
    spin_lock(&broadcast_lock);
    list_for_each_entry_safe(p, n, &broadcast_list_head, entry) {
        if (p->big_hdl != big_hdl) {
            continue;
        }

        list_del(&p->entry);
        free(p);
    }
    spin_unlock(&broadcast_lock);

    hdl_info->broadcast_status = BROADCAST_STATUS_STOP;
    hdl_info->used = 0;

    //释放互斥量
    os_ret = os_mutex_post(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR);
    }

#if BROADCAST_RECEIVER_CLOSE_EDR_CONN
    if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
        //恢复经典蓝牙可发现可连接
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }
#endif

    broadcast_num--;
    if (broadcast_num == 0) {
        broadcast_role = BROADCAST_ROLE_UNKNOW;
        clock_remove_set(BROADCAST_CLK);
    }

    if (status) {
        broadcast_params_interface->start_original();
    }
}

static void broadcast_pair_tx_event_callback(const PAIR_EVENT event, void *priv)
{
    int pair_flag = 0;
    switch (event) {
    case PAIR_EVENT_TX_PRI_CHANNEL_CREATE_SUCCESS:
        u32 *private_connect_access_addr = (u32 *)priv;
        g_printf("PAIR_EVENT_TX_PRI_CHANNEL_CREATE_SUCCESS:0x%x", *private_connect_access_addr);
        int ret = syscfg_write(VM_WIRELESS_PAIR_CODE, private_connect_access_addr, sizeof(u32));
        if (ret <= 0) {
            r_printf(">>>>>>wireless pair code save err");
        }
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_flag = 0;
        syscfg_write(VM_WIRELESS_PAIR_FLAG, &pair_flag, sizeof(int));
#endif
        break;
    case PAIR_EVENT_TX_OPEN_PAIR_MODE_SUCCESS:
        g_printf("PAIR_EVENT_TX_OPEN_PAIR_MODE_SUCCESS");
        break;
    case PAIR_EVENT_TX_CLOSE_PAIR_MODE_SUCCESS:
        g_printf("PAIR_EVENT_TX_CLOSE_PAIR_MODE_SUCCESS");
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_flag = 1;
        syscfg_write(VM_WIRELESS_PAIR_FLAG, &pair_flag, sizeof(int));
        app_broadcast_open();
#endif
        break;
    }
}

static const pair_callback_t pair_tx_cb = {
    .pair_event_cb = broadcast_pair_tx_event_callback,
};

static void broadcast_pair_rx_event_callback(const PAIR_EVENT event, void *priv)
{
    int pair_flag = 0;
    switch (event) {
    case PAIR_EVENT_RX_PRI_CHANNEL_CREATE_SUCCESS:
        u32 *private_connect_access_addr = (u32 *)priv;
        g_printf("PAIR_EVENT_RX_PRI_CHANNEL_CREATE_SUCCESS:0x%x", *private_connect_access_addr);
        int ret = syscfg_write(VM_WIRELESS_PAIR_CODE, private_connect_access_addr, sizeof(u32));
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        app_broadcast_open();
#endif
        if (ret <= 0) {
            r_printf(">>>>>>wireless pair code save err");
        }
        break;
    case PAIR_EVENT_RX_OPEN_PAIR_MODE_SUCCESS:
        g_printf("PAIR_EVENT_RX_OPEN_PAIR_MODE_SUCCESS");
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_flag = 0;
        syscfg_write(VM_WIRELESS_PAIR_FLAG, &pair_flag, sizeof(int));
#endif
        break;
    case PAIR_EVENT_RX_CLOSE_PAIR_MODE_SUCCESS:
        g_printf("PAIR_EVENT_RX_CLOSE_PAIR_MODE_SUCCESS");
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        pair_flag = 1;
        syscfg_write(VM_WIRELESS_PAIR_FLAG, &pair_flag, sizeof(int));
#endif
        break;
    }
}

static const pair_callback_t pair_rx_cb = {
    .pair_event_cb = broadcast_pair_rx_event_callback,
};

int broadcast_enter_pair(u8 role)
{
    g_printf("%s", __FUNCTION__);
    int err = -1;
    if (broadcast_role != BROADCAST_ROLE_UNKNOW) {
        return -1;
    }
    if (!broadcast_init_flag) {
        return -2;
    }
    if (role == BROADCAST_ROLE_RECEIVER) {
        err = wireless_dev_enter_pair("big_rx", 0, (void *)&pair_rx_cb);
    } else if (role == BROADCAST_ROLE_TRANSMITTER) {
        err = wireless_dev_enter_pair("big_tx", 0, (void *)&pair_tx_cb);
    }

    return err;
}

int broadcast_exit_pair(u8 role)
{
    g_printf("%s", __FUNCTION__);
    int err = -1;
    if (broadcast_role != BROADCAST_ROLE_UNKNOW) {
        return -1;
    }
    if (!broadcast_init_flag) {
        return -2;
    }
    if (role == BROADCAST_ROLE_RECEIVER) {
        err = wireless_dev_exit_pair("big_rx", (void *)0);
    } else if (role == BROADCAST_ROLE_TRANSMITTER) {
        err = wireless_dev_exit_pair("big_tx", (void *)0);
    }

    return err;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief get current broadcast role
 *
 * @return broadcast role
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_role(void)
{
    return broadcast_role;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化同步的状态数据的内容
 *
 * @param data:用来同步的数据
 */
/* ----------------------------------------------------------------------------*/
void broadcast_sync_data_init(struct broadcast_sync_info *data)
{
    memcpy(&broadcast_data_sync, data, sizeof(struct broadcast_sync_info));
}

#endif

