#include "encode/encode_write.h"
#include "file_operate/file_bs_deal.h"
#include "audio_enc.h"
#include "media/audio_base.h"
#include "dev_manager.h"
#include "app_config.h"
#include "spi/nor_fs.h"
#include "rec_nor/nor_interface.h"
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_config.h"
#include "app_main.h"

#if TCFG_UNISOUND_ENABLE

#define LOG_TAG_CONST       UNISOUND
#define LOG_TAG             "[UNISOUND]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define THIS_TASK_NAME  	"unisound"
#define MULTIPLE_MIC_NUM    3//固定
#define LADC_BUF_NUM        2
#define LADC_IRQ_POINTS     256//512//256	/*中断点数*/ //固定
#define LADC_BUFS_SIZE      (MULTIPLE_MIC_NUM * LADC_BUF_NUM * LADC_IRQ_POINTS)
#define MIC_DATA_TOTAL      5//收到连续mic的n包数据再后激活线程进行处理，防止频繁进行系统调度
#define CBUF_SIZE 			(512 * MULTIPLE_MIC_NUM * MIC_DATA_TOTAL * 2)//存储mic采集到的数据流
#define TEMP_BUF_SIZE 		(512 * MULTIPLE_MIC_NUM)//存放读取cbuf的数据

#define ASR_VERIFY
#ifdef ASR_VERIFY
void *_calloc(size_t nmemb, size_t size)
{
    void *p = malloc(nmemb * size);
    memset(p, 0, nmemb * size);
    return p;
}
void *_calloc_r(void *r, size_t a, size_t b)
{
    return _calloc(a, b);
}
#endif

extern struct audio_adc_hdl adc_hdl;
typedef struct {
    struct audio_adc_output_hdl output;
    struct adc_linein_ch linein_ch;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[LADC_BUFS_SIZE];    //align 4Bytes
    s16 temp_buf[LADC_IRQ_POINTS * MULTIPLE_MIC_NUM];
} audio_adc_t;
static audio_adc_t *ladc_var = NULL;

typedef struct {
    u8 *task_name;			 //线程名称
    OS_SEM sem_unisound_run; //激活线程的信号量
    cbuffer_t *data_w_cbuf;  //cbuf指针
    u8 *data_buf;			 //存放mic数据流
    u8 *mic_rbuf;			 //存放从cbuf读取出的数据
    u8 init_ok;
} unisound_t;

static unisound_t info = {
    .task_name = THIS_TASK_NAME,
    .data_w_cbuf = NULL,
    .data_buf = NULL,
    .mic_rbuf = NULL,
};
#define __this  (&info)

static void unisound_multiple_mic_deal_output_demo(void *priv, s16 *data, int len);
/*----------------------------------------------------------------------------*/
/**@brief    mic初始化
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static int unisound_open_multiple_mic_init_demo(void)
{
    u16 ladc_sr = 16000;
    u8 mic_gain = 10;
    u8 linein_gain = 3;
    log_info("audio_adc_open_demo,sr:%d,mic_gain:%d,linein_gain:%d", ladc_sr, mic_gain, linein_gain);
    if (ladc_var) {
        log_error("ladc already open");
        return -1;
    }
    ladc_var = zalloc(sizeof(audio_adc_t));
    if (ladc_var) {
        audio_adc_mic_open(&ladc_var->mic_ch, TCFG_UNISOUND_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic_set_gain(&ladc_var->mic_ch, mic_gain);
        audio_adc_mic1_open(&ladc_var->mic_ch, TCFG_UNISOUND_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic1_set_gain(&ladc_var->mic_ch, mic_gain);
        audio_adc_mic2_open(&ladc_var->mic_ch, TCFG_UNISOUND_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic2_set_gain(&ladc_var->mic_ch, mic_gain);//不能注释，否则影响识别，因为需要这个通道的数据做aec消除扬声器声音
        audio_adc_mic_set_sample_rate(&ladc_var->mic_ch, ladc_sr);//不能注释，否则影响识别，因为需要这个通道的数据做aec消除扬声器声音
        audio_adc_mic_set_buffs(&ladc_var->mic_ch, ladc_var->adc_buf,  LADC_IRQ_POINTS * 2, LADC_BUF_NUM);
        ladc_var->output.handler = unisound_multiple_mic_deal_output_demo;
        ladc_var->output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &ladc_var->output);
        audio_adc_mic_start(&ladc_var->mic_ch);

        SFR(JL_ADDA->DAA_CON1, 0, 3, 7);
        SFR(JL_ADDA->DAA_CON1, 4, 3, 7);
        SFR(JL_AUDIO->DAC_VL0, 0, 16, 16384);
        //app_audio_output_samplerate_set(ladc_sr);
        //app_audio_output_start();
        return 0;
    } else {
        return -1;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief
   @param
   @return
   @note	 将mic采集到的数据写入cbuf，并激活unisound对数据进行处理
*/
/*----------------------------------------------------------------------------*/
static int unisound_mic_deal_run(void *dat, u16 len)
{
    int wlen = 0;
    static u8 write_cnt = 0;
    if (cbuf_is_write_able(__this->data_w_cbuf, len)) {
        wlen = cbuf_write(__this->data_w_cbuf, dat, len);
        write_cnt ++;
    }
    if (wlen == 0) {
        log_error("data_w_cbuf_full");
    }
    if (write_cnt == MIC_DATA_TOTAL) {
        write_cnt = 0;
        os_sem_post(&(__this->sem_unisound_run));
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    mic中断回调处理函数
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void unisound_multiple_mic_deal_output_demo(void *priv, s16 *data, int len)
{
    if (ladc_var == NULL) {
        return;
    }

    unisound_mic_deal_run(data, len * MULTIPLE_MIC_NUM);//线程处理数据

    //log_info("len=%d\n",len);
    putchar('*');
}

static void unisound_multiple_mic_close_demo()
{
    if (ladc_var) {
        audio_adc_del_output_handler(&adc_hdl, &ladc_var->output);
        extern int audio_adc_mic_close(struct adc_mic_ch * mic);
        audio_adc_mic_close(&ladc_var->mic_ch);
        free(ladc_var);
        ladc_var = NULL;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    关闭unisound模块
   @param
   @return
   @note	 关闭mic，关闭语音识别，释放相关资源
*/
/*----------------------------------------------------------------------------*/
void unisound_uninit(void)
{
    log_info(">>>>>>>>>>>>>>>unisound_uninit<<<<<<<<<<<<<<<<<");

    if (!__this->init_ok) {
        return;
    }

    //关mic
    unisound_multiple_mic_close_demo();

    //关闭语音识别
    uniAsrUninit();

    //kill unisound
    task_kill(__this->task_name);

    if (__this->data_w_cbuf) {
        free(__this->data_w_cbuf);
        __this->data_w_cbuf = NULL;
    }
    if (__this->data_buf) {
        free(__this->data_buf);
        __this->data_buf = NULL;
    }
    if (__this->mic_rbuf) {
        free(__this->mic_rbuf);
        __this->mic_rbuf = NULL;
    }

    __this->init_ok = 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    unisound线程入口函数
   @param
   @return
   @note	 对mic采集到的数据进行语音算法处理
*/
/*----------------------------------------------------------------------------*/
static void unisound_mic_data_deal_task(void *p)
{
    u8 pend;
    int ret;

    while (1) {
        pend = 1;
        for (u8 i = 0; i < MIC_DATA_TOTAL; i++) {
            memset(__this->mic_rbuf, 0x0, TEMP_BUF_SIZE);
            ret = cbuf_get_data_size(__this->data_w_cbuf);
            if (ret >= TEMP_BUF_SIZE) {
                ret = cbuf_read(__this->data_w_cbuf, __this->mic_rbuf, TEMP_BUF_SIZE);
            }
            uniAsrProcess(__this->mic_rbuf, TEMP_BUF_SIZE);//语音数据处理
        }

        if (pend) {
            os_sem_pend(&(__this->sem_unisound_run), 0);
        }
    }
    while (1) {
        os_time_dly(100);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    unisound资源申请函数
   @param
   @return   0:成功， 1:失败
   @note
*/
/*----------------------------------------------------------------------------*/
static int unisound_buf_init()
{
    __this->data_buf = (u8 *)zalloc(CBUF_SIZE);
    if (__this->data_buf == NULL) {
        log_error("data_buf malloc error");
        return 1;
    }

    __this->data_w_cbuf = (cbuffer_t *)zalloc(sizeof(cbuffer_t));
    if (__this->data_w_cbuf == NULL) {
        free(__this->data_buf);
        __this->data_buf == NULL;
        log_error("data_w_cbuf malloc err!");
        return 1;
    }

    __this->mic_rbuf = (u8 *)zalloc(TEMP_BUF_SIZE);
    if (__this->mic_rbuf == NULL) {
        free(__this->data_buf);
        __this->data_buf == NULL;
        free(__this->data_w_cbuf);
        __this->data_w_cbuf == NULL;
        log_error("mic_rbuf malloc err!");
        return 1;
    }

    cbuf_init(__this->data_w_cbuf, __this->data_buf, CBUF_SIZE);
    log_info("unisound buf init success");
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    unisound模块初始化
   @param
   @return   0:成功， 1:失败
   @note
*/
/*----------------------------------------------------------------------------*/
int unisound_init(void)
{
    if (__this->init_ok) {
        return 0;
    }

    log_info(">>>>>>>>>>>>>>>unisound_init<<<<<<<<<<<<<<<<<");

    //初始化cbuf
    if (unisound_buf_init()) {
        log_info("unisound buf init error");
        return 1;
    }

    //创建信号量
    os_sem_create(&(__this->sem_unisound_run), 0);

    //创建unisound算法线程
    int err = task_create(unisound_mic_data_deal_task, NULL, __this->task_name);
    if (err != OS_NO_ERR) {
        log_error("unisound task create error");
        goto _exit;
    }

    //语音识别初始化
    if (uniAsrInit()) {
        goto _exit;
    }

    //mic初始化
    int ret = unisound_open_multiple_mic_init_demo();
    if (ret < 0) {
        log_error("unisound mic open error");
        goto _exit;
    }

    __this->init_ok = 1;

    return 0;

_exit:
    unisound_uninit();
    log_error("unidound init error");
    return 1;
}

u8 get_unisound_init_ok(void)
{
    return (__this->init_ok);
}

#endif // TCFG_UNISOUND_ENABLE
