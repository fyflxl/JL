#include "encode/encode_write.h"
#include "audio_enc.h"
#include "media/audio_base.h"
#include "dev_manager.h"
#include "app_config.h"
#include "spi/nor_fs.h"
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_config.h"

/**********************
 *      MACROS
 **********************/
#define THIS_TASK_NAME                      "data_export"

#if TCFG_MULTIPLE_MIC_REC_ENABLE

#define MULTIPLE_MIC_REC_FOLDER_NAME		"MULTIPLE_MIC_REC"
#define MULTIPLE_MIC_NUM                    3

#define LADC_BUF_NUM        2
#define LADC_IRQ_POINTS     512//256	/*中断点数*/
#define LADC_BUFS_SIZE      (MULTIPLE_MIC_NUM * LADC_BUF_NUM * LADC_IRQ_POINTS)

#define SD_PCM_BUF_SIZE		4096//512	// 每次写的数据长度

#define MIC_SAMPLE_RATE     16000
#define MIC_GAIN            8

#define APP_IO_DEBUG_0(i,x)       //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT &= ~BIT(x);}
#define APP_IO_DEBUG_1(i,x)       //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT |= BIT(x);}

/**********************
 *      TYPEDEFS
 **********************/
struct multiple_mic_rec {
    volatile u32 init_ok : 1;
        volatile u32 wait_idle : 1;
        volatile u32 start : 1;
        volatile u32 wait_stop : 1;
        OS_SEM sem_task_run;
        void *set_head_hdl;
        int (*set_head)(void *, char **head);
    };

    typedef struct {
    struct audio_adc_output_hdl output;
    struct adc_linein_ch linein_ch;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[LADC_BUFS_SIZE];    //align 4Bytes
    s16 temp_buf[LADC_IRQ_POINTS * MULTIPLE_MIC_NUM];
} audio_adc_t;

typedef struct {
    struct device *sd_hdl;
    u32 sd_write_sector;
    OS_SEM sd_sem;
    u8 sd_wbuf[SD_PCM_BUF_SIZE];			// 数据导出buf
    s16 mic0[LADC_IRQ_POINTS];
    s16 mic1[LADC_IRQ_POINTS];
    s16 mic2[LADC_IRQ_POINTS];
    s16 ch_buf[MULTIPLE_MIC_NUM][LADC_IRQ_POINTS * 20];
    cbuffer_t ch_cb[MULTIPLE_MIC_NUM];
} multiple_mic_t;

/**********************
 *  STATIC VARIABLES
 **********************/
static multiple_mic_t dm;
static audio_adc_t *ladc_var = NULL;
static struct multiple_mic_rec *wfil_hdl = NULL;
static FILE *mic_file_hdl[MULTIPLE_MIC_NUM] = {0};

extern struct audio_adc_hdl adc_hdl;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void multiple_mic_output_demo(void *priv, s16 *data, int len);
void multiple_mic_write_file_stop(void *hdl, u32 delay_ms);

/**********************
 *  FUNCTIONS
 **********************/
static u32 get_creat_path_len(char *root_path, const char *folder, const char *filename)
{
    return (strlen(root_path) + strlen(folder) + strlen(filename) + strlen("/") + 1);
}

static char *create_path(char *path, char *root_path, const char *folder, const char *filename)
{
    strcat(path, root_path);
    /* strcat(path, folder); */
    /* strcat(path, "/"); */
    strcat(path, filename);
#if TCFG_NOR_REC
#elif FLASH_INSIDE_REC_ENABLE
    int index = 0;
    int last_num = -1;
    char *file_num_str;

    index = sdfile_rec_scan_ex();
    last_num = index + 1;
    file_num_str = strrchr(path, '.') - 4;

    if (last_num > 9999) {
        last_num = 0;
    }
    file_num_str[0] = last_num / 1000 + '0';
    file_num_str[1] = last_num % 1000 / 100 + '0';
    file_num_str[2] = last_num % 100 / 10 + '0';
    file_num_str[3] = last_num % 10 + '0';
#endif
    printf(">>>[test]:create path =%s\n", path);
    return path;
}

static int data_export_init()
{
    u8 i;
    for (i = 0; i < MULTIPLE_MIC_NUM; i++) {
        cbuf_init(&dm.ch_cb[i], dm.ch_buf[i], sizeof(dm.ch_buf[i]));
    }

    return 0;
}

static int data_export_run(void *dat, u16 len, u8 ch)
{
    int wlen = 0;
    //printf("export_run:%d",ch);
    wlen = cbuf_write(&dm.ch_cb[ch], dat, len);
    if (wlen != len) {
        printf("[dm]sd%d buf full\n", ch);
    }
    os_sem_set(&wfil_hdl->sem_task_run, 0);
    os_sem_post(&wfil_hdl->sem_task_run);
    return 0;
}

int multiple_mic_open_init_demo(void)
{
    u16 ladc_sr = MIC_SAMPLE_RATE;
    u8 mic_gain = MIC_GAIN;
    printf("audio_adc_open_demo,sr:%d,mic_gain:%d\n", ladc_sr, mic_gain);
    if (ladc_var) {
        printf("ladc already open \n");
        return -1;
    }
    ladc_var = zalloc(sizeof(audio_adc_t));
    if (ladc_var) {
#if (MULTIPLE_MIC_NUM >= 1)
        audio_adc_mic_open(&ladc_var->mic_ch, TCFG_AUDIO_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic_set_gain(&ladc_var->mic_ch, mic_gain);
#endif
#if (MULTIPLE_MIC_NUM >= 2)
        audio_adc_mic1_open(&ladc_var->mic_ch, TCFG_AUDIO_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic1_set_gain(&ladc_var->mic_ch, mic_gain);
#endif
#if (MULTIPLE_MIC_NUM >= 3)
        audio_adc_mic2_open(&ladc_var->mic_ch, TCFG_AUDIO_ADC_MIC_CHA, &adc_hdl);
        audio_adc_mic2_set_gain(&ladc_var->mic_ch, mic_gain);
#endif
        audio_adc_mic_set_sample_rate(&ladc_var->mic_ch, ladc_sr);
        audio_adc_mic_set_buffs(&ladc_var->mic_ch, ladc_var->adc_buf,  LADC_IRQ_POINTS * 2, LADC_BUF_NUM);
        ladc_var->output.handler = multiple_mic_output_demo;
        ladc_var->output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &ladc_var->output);
        audio_adc_mic_start(&ladc_var->mic_ch);

        SFR(JL_ADDA->DAA_CON1, 0, 3, 7);
        SFR(JL_ADDA->DAA_CON1, 4, 3, 7);
        SFR(JL_AUDIO->DAC_VL0, 0, 16, 16384);
        app_audio_output_samplerate_set(ladc_sr);
        app_audio_output_start();
        return 0;
    } else {
        return -1;
    }
}

static void multiple_mic_output_demo(void *priv, s16 *data, int len)
{
    struct audio_adc_hdl *hdl = priv;
    int wlen = 0;

    if (ladc_var == NULL) {
        return;
    }

    /* printf("linein:%x,len:%d,ch:%d,data:%d",data,len,hdl->channel,data[0]); */
    for (u16 i = 0; i < (len >> 1); i++) {
        dm.mic0[i] = data[i * 3];
        dm.mic1[i] = data[i * 3 + 1];
        dm.mic2[i] = data[i * 3 + 2];
    }

    /* memcpy(dm.mic0, data, len); */
#if (MULTIPLE_MIC_NUM >= 1)
    data_export_run(dm.mic0, len, 0);
#endif
#if (MULTIPLE_MIC_NUM >= 2)
    data_export_run(dm.mic1, len, 1);
#endif
#if (MULTIPLE_MIC_NUM >= 3)
    data_export_run(dm.mic2, len, 2);
#endif
    /* os_sem_set(&wfil_hdl->sem_task_run, 0); */
    /* os_sem_post(&wfil_hdl->sem_task_run); */
    putchar('*');

#if 0
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//双声道数据结构
    for (int i = 0; i < (len / 2); i++) {
        /* ladc_var->temp_buf[i * 2] = data[i]; */
        /* ladc_var->temp_buf[i * 2 + 1] = data[i]; */
        ladc_var->temp_buf[i * 2] = dm.mic0[i];
        ladc_var->temp_buf[i * 2 + 1] = dm.mic0[i];
        /* ladc_var->temp_buf[i * 2] = dm.mic1[i]; */
        /* ladc_var->temp_buf[i * 2 + 1] = dm.mic1[i]; */
    }
    wlen = app_audio_output_write(ladc_var->temp_buf, len * 2);
#else
    wlen = app_audio_output_write(data, len * hdl->channel);
#endif
#endif
}

void multiple_mic_close_demo()
{
    if (ladc_var) {
        audio_adc_del_output_handler(&adc_hdl, &ladc_var->output);
        extern int audio_adc_mic_close(struct adc_mic_ch * mic);
        audio_adc_mic_close(&ladc_var->mic_ch);
        free(ladc_var);
        ladc_var = NULL;
    }
}

static void multiple_mic_rec_task(void *p)
{
    u8 i;
    u8 pend;
    int ret, len;
    struct multiple_mic_rec *wfil = p;

    while (1) {
        pend = 1;
        if (!wfil->init_ok) {
            wfil->wait_idle = 0;
            wfil->start = 0;
            wfil->wait_stop = 0;
            break;
        }
        if (!wfil->start) {
            continue;
        }
        for (i = 0; i < MULTIPLE_MIC_NUM; i++) {
            len = cbuf_read(&dm.ch_cb[i], dm.sd_wbuf, sizeof(dm.sd_wbuf));
            if (len == sizeof(dm.sd_wbuf)) {
                pend = 0;
                putchar(',');
                APP_IO_DEBUG_1(A, 5);
                ret = fwrite(mic_file_hdl[i], dm.sd_wbuf, len);
                APP_IO_DEBUG_0(A, 5);
                putchar('.');
                if (ret != len) {
                    printf("mic_file_hdl[%d] write err\n", i);
                }
            }
        }
        if (pend) {
            os_sem_pend(&wfil->sem_task_run, 0);
        }
    }
    while (1) {
        os_time_dly(100);
    }
}

FILE *multiple_mic_rec_file_open(char *logo, const char *folder, const char *filename)
{
    if (!logo || !folder || !filename) {
        return NULL;
    }

    struct __dev *dev = dev_manager_find_spec(logo, 0);
    if (!dev) {
        return NULL;
    }

    char *root_path = dev_manager_get_root_path(dev);
    char *temp_filename = zalloc(strlen(filename) + 5);
    if (temp_filename == NULL) {
        return NULL;
    }
    strcat(temp_filename, filename);
    strcat(temp_filename, ".raw");
    printf("temp_filename:%s\n", temp_filename);

    u32 path_len = get_creat_path_len(root_path, folder, temp_filename);
    char *path = zalloc(path_len);
    if (path == NULL) {
        free(temp_filename);
        return NULL;
    }
    create_path(path, root_path, folder, temp_filename);
    printf("path:%s\n", path);
    FILE *wfile_hdl = fopen(path, "w+");
    free(temp_filename);
    free(path);
    if (!wfile_hdl) {
        return NULL;
    }

    return wfile_hdl;
}

int multiple_mic_rec_uninit()
{
    u8 i;
    int f_len;
    struct vfs_attr attr = {0};
    printf(">>>>>>>>>>>>>>>>>multiple_mic_rec_uninit\n");

    //关mic
    multiple_mic_close_demo();

    multiple_mic_write_file_stop(wfil_hdl, 1000);

    if (wfil_hdl->init_ok) {
        wfil_hdl->wait_idle = 1;
        wfil_hdl->init_ok = 0;
        os_sem_set(&wfil_hdl->sem_task_run, 0);
        os_sem_post(&wfil_hdl->sem_task_run);
        while (wfil_hdl->wait_idle) {
            os_time_dly(1);
        }
        task_kill(THIS_TASK_NAME);
    }

    for (i = 0; i < MULTIPLE_MIC_NUM; i++) {
        if (mic_file_hdl[i]) {
            f_len = fpos(mic_file_hdl[i]);
            if (wfil_hdl->set_head) {
                char *head;
                int len = wfil_hdl->set_head(wfil_hdl->set_head_hdl, &head);
                if (f_len <= len) {
                    fdelete(mic_file_hdl[i]);
                    mic_file_hdl[i] = NULL;
                    continue;
                }
                if (len) {
                    fseek(mic_file_hdl[i], 0, SEEK_SET);
                    fwrite(mic_file_hdl[i], head, len);
                }
            }
            fseek(mic_file_hdl[i], f_len, SEEK_SET);
            /* fget_attrs(mic_file_hdl[i], &attr); */
            fclose(mic_file_hdl[i]);
            mic_file_hdl[i] = NULL;
        }
    }

    if (wfil_hdl) {
        free(wfil_hdl);
        wfil_hdl = NULL;
    }

    for (i = 0; i < MULTIPLE_MIC_NUM; i++) {
        printf("mic%d_file_hdl:0x%x\n", i, mic_file_hdl[i]);
    }

    return 0;
}

int multiple_mic_rec_init()
{
    char folder[] = {MULTIPLE_MIC_REC_FOLDER_NAME};         //录音文件夹名称
    char mic_filename[] = {"mic****"};     //录音文件名，不需要加后缀，录音接口会根据编码格式添加后缀
    char *logo = dev_manager_get_phy_logo(dev_manager_find_active(0));//普通设备录音，获取最后活动设备
    if (wfil_hdl) {
        return -1;
    }

    wfil_hdl = zalloc(sizeof(struct multiple_mic_rec));
    if (!wfil_hdl) {
        return -1;
    }

    //打开mic*录音文件句柄
    for (u8 i = 0; i < MULTIPLE_MIC_NUM; i++) {
        mic_file_hdl[i] = multiple_mic_rec_file_open(logo, folder, mic_filename);
        if (!mic_file_hdl[i]) {
            printf("mic%d rec hdl open fail\n", i);
            goto _exit;
        }
    }

    //初始化cbuf
    data_export_init();

    //创建写卡线程
    os_sem_create(&wfil_hdl->sem_task_run, 0);
    int err;
    err = task_create(multiple_mic_rec_task, wfil_hdl, THIS_TASK_NAME);
    if (err != OS_NO_ERR) {
        log_e("task create err ");
        goto _exit;
    }
    wfil_hdl->init_ok = 1;

    //启动写卡
    multiple_mic_write_file_start(wfil_hdl);

    //开mic
    int ret = multiple_mic_open_init_demo();
    if (ret < 0) {
        printf("mic open fail\n");
        goto _exit;
    }

    return 0;

_exit:
    multiple_mic_rec_uninit();
    printf("multiple mic rec init fail\n");
    return -1;
}

int multiple_mic_write_file_resume(void *hdl)
{
    struct multiple_mic_rec *wfil = hdl;
    if (wfil->start) {
        os_sem_set(&wfil->sem_task_run, 0);
        os_sem_post(&wfil->sem_task_run);
        return 0;
    }
    return -1;
}

int multiple_mic_write_file_start(void *hdl)
{
    struct multiple_mic_rec *wfil = hdl;

    if (wfil->init_ok) {
        wfil->start = 1;
        return 0;
    }
    return -1;
}

void multiple_mic_write_file_stop(void *hdl, u32 delay_ms)
{
    struct multiple_mic_rec *wfil = hdl;

    if (wfil->start) {
        wfil->wait_stop = 1;
        u32 dly = 0;
        do {
            if (!wfil->wait_stop) {
                break;
            }
            os_time_dly(1);
            dly += 10;
        } while (dly < delay_ms);
        wfil->start = 0;
    }
}

#endif

