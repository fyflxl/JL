#include "system/includes.h"
#include "broadcast_api.h"
#include "app_broadcast.h"
#include "app_config.h"
#include "bt.h"
#include "app_task.h"
#include "btstack/avctp_user.h"
#include "tone_player.h"
#include "app_main.h"
#if TCFG_LINEIN_ENABLE
#include "linein/linein.h"
#endif
#include "music_player.h"
#if defined(RCSP_MODE) && RCSP_MODE
#include "ble_rcsp_module.h"
#endif

#if TCFG_BROADCAST_ENABLE

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

/**************************************************************************************************
  Data Types
**************************************************************************************************/

enum {
    APP_BROADCAST_STATUS_STOP,
    APP_BROADCAST_STATUS_START,
    APP_BROADCAST_STATUS_SUSPEND,
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
static u8 broadcast_last_role = 0; /*!< 挂起前广播角色 */
static u8 broadcast_status = 0; /*!< 当前广播状态 */
static int broadcast_hdl = 0;    /*!< 记录开启广播后返回的big值 */
static u8 broadcast_app_mode_exit = 0;  /*!< 音源模式退出标志 */
static struct broadcast_sync_info app_broadcast_data_sync;  /*!< 用于记录广播同步状态 */
static u8 config_broadcast_as_master = 0;   /*!< 配置广播强制做主机 */
static u8 enter_pair_flag = 0;

static bool is_broadcast_as_transmitter();

static void  broadcast_tone_play_end_callback(void *priv, int flag)
{
    u32 index = (u32)priv;
    int temp_broadcast_hdl;

    switch (index) {
    case IDEX_TONE_BROADCAST_OPEN:
#if 0
        if (is_broadcast_as_transmitter()) {
            temp_broadcast_hdl = broadcast_transmitter();
        } else {
            temp_broadcast_hdl = broadcast_receiver();
        }
        if (temp_broadcast_hdl < -1) {
            broadcast_hdl = 0;
        } else if (temp_broadcast_hdl >= 0) {
            broadcast_hdl = temp_broadcast_hdl;
            broadcast_status = APP_BROADCAST_STATUS_START;
        }
#endif
        break;
    case IDEX_TONE_BROADCAST_CLOSE:
#if 0
        broadcast_close(broadcast_hdl);
        broadcast_hdl = 0;
        broadcast_status = APP_BROADCAST_STATUS_STOP;
#endif
        break;
    default:
        break;
    }
}
/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前是否在退出模式的状态
 *
 * @return 1；是，0：否
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_app_mode_exit_flag(void)
{
    return broadcast_app_mode_exit;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 判断当前设备作为广播发送设备还是广播接收设备
 *
 * @return true:发送设备，false:接收设备
 */
/* ----------------------------------------------------------------------------*/
static bool is_broadcast_as_transmitter()
{
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
#if defined(WIRELESS_1tN_ROLE_SEL) && (WIRELESS_1tN_ROLE_SEL == WIRELESS_1tN_ROLE_TX)
    return true;
#else
    return false;
#endif
#endif

#if 1
    //当前处于蓝牙模式并且已连接手机设备时，
    //(1)播歌作为广播发送设备；
    //(2)暂停作为广播接收设备。
    if ((app_get_curr_task() == APP_BT_TASK) &&
        (get_bt_connect_status() != BT_STATUS_WAITINT_CONN)) {
        /* if (get_a2dp_decoder_status()) { */
        /* if (get_a2dp_start_flag()) { */
        if ((a2dp_get_status() == BT_MUSIC_STATUS_STARTING) ||
            get_a2dp_decoder_status() ||
            get_a2dp_start_flag()) {
            return true;
        } else {
            return false;
        }
    }

#if TCFG_LINEIN_ENABLE
    if (app_get_curr_task() == APP_LINEIN_TASK)  {
        if (linein_get_status() || config_broadcast_as_master) {
            return true;
        } else {
            return false;
        }
    }
#endif

    if (app_get_curr_task() == APP_MUSIC_TASK) {
        if ((music_player_get_play_status() == FILE_DEC_STATUS_PLAY) || config_broadcast_as_master) {
            return true;
        } else {
            return false;
        }
    }

    //当处于下面几种模式时，作为广播发送设备
    if ((app_get_curr_task() == APP_FM_TASK) ||
        (app_get_curr_task() == APP_PC_TASK) ||
        (app_get_curr_task() == APP_BC_MIC_TASK)) {
        return true;
    }

    return false;
#else
    gpio_set_direction(IO_PORTC_04, 1);
    gpio_set_pull_down(IO_PORTC_04, 0);
    gpio_set_pull_up(IO_PORTC_04, 1);
    gpio_set_die(IO_PORTC_04, 1);
    os_time_dly(2);
    if (gpio_read(IO_PORTC_04) == 0) {
        return true;
    } else {
        return false;
    }
#endif
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 检测广播当前是否处于挂起状态
 *
 * @return true:处于挂起状态，false:处于非挂起状态
 */
/* ----------------------------------------------------------------------------*/
static bool is_need_resume_broadcast()
{
    if (broadcast_status == APP_BROADCAST_STATUS_SUSPEND) {
        return true;
    } else {
        return false;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播从挂起状态恢复
 */
/* ----------------------------------------------------------------------------*/
static void app_broadcast_resume()
{
    int temp_broadcast_hdl;

    if (!app_bt_hdl.init_ok) {
        return;
    }

    if ((app_get_curr_task() == APP_BT_TASK) &&
        (get_call_status() != BT_CALL_HANGUP)) {
        return;
    }

    if ((app_get_curr_task() == APP_FM_TASK) ||
        (app_get_curr_task() == APP_PC_TASK) /*||
        (app_get_curr_task() == APP_MUSIC_TASK)*/) {
        return;
    }

    if (broadcast_status != APP_BROADCAST_STATUS_SUSPEND) {
        return;
    }

    if (is_broadcast_as_transmitter()) {
        temp_broadcast_hdl = broadcast_transmitter();
    } else {
        temp_broadcast_hdl = broadcast_receiver();
    }

    if (temp_broadcast_hdl < -1) {
        broadcast_hdl = 0;
    } else if (temp_broadcast_hdl >= 0) {
        broadcast_hdl = temp_broadcast_hdl;
        broadcast_status = APP_BROADCAST_STATUS_START;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播进入挂起状态
 */
/* ----------------------------------------------------------------------------*/
static void app_broadcast_suspend()
{
    if (broadcast_hdl) {
        broadcast_last_role = get_broadcast_role();
        broadcast_close(broadcast_hdl);
        broadcast_hdl = 0;
        broadcast_status = APP_BROADCAST_STATUS_SUSPEND;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 开启广播
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_open()
{
    int temp_broadcast_hdl;

    if (!app_bt_hdl.init_ok) {
        return;
    }
    if (broadcast_hdl) {
        return;
    }
    if ((app_get_curr_task() == APP_BT_TASK) &&
        (get_call_status() != BT_CALL_HANGUP)) {
        return;
    }
    if ((app_get_curr_task() == APP_FM_TASK) ||
        (app_get_curr_task() == APP_PC_TASK) /*||
        (app_get_curr_task() == APP_MUSIC_TASK)*/) {
        return;
    }
    log_info("broadcast_open");
#if defined(RCSP_MODE) && RCSP_MODE
    ble_module_enable(0);
#endif
    if (is_broadcast_as_transmitter()) {
        temp_broadcast_hdl = broadcast_transmitter();
    } else {
        temp_broadcast_hdl = broadcast_receiver();
    }
    if (temp_broadcast_hdl < -1) {
        broadcast_hdl = 0;
    } else if (temp_broadcast_hdl >= 0) {
        broadcast_hdl = temp_broadcast_hdl;
        broadcast_status = APP_BROADCAST_STATUS_START;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 关闭广播
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_close()
{
    if (broadcast_hdl) {
        log_info("broadcast_close");
#if defined(RCSP_MODE) && RCSP_MODE
        ble_module_enable(1);
#endif
        broadcast_close(broadcast_hdl);
        broadcast_hdl = 0;
        broadcast_status = APP_BROADCAST_STATUS_STOP;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播开关切换
 *
 * @return -1:切换失败，0:切换成功
 */
/* ----------------------------------------------------------------------------*/
int app_broadcast_switch(void)
{
    int temp_broadcast_hdl;
    if (!app_bt_hdl.init_ok) {
        return -1;
    }
    if ((app_get_curr_task() == APP_BT_TASK) &&
        (get_call_status() != BT_CALL_HANGUP)) {
        return -1;
    }
    if ((app_get_curr_task() == APP_FM_TASK) ||
        (app_get_curr_task() == APP_PC_TASK) /*||
        (app_get_curr_task() == APP_MUSIC_TASK)*/) {
        return -1;
    }
    if (broadcast_hdl) {
        log_info("broadcast_close");
#if defined(RCSP_MODE) && RCSP_MODE
        ble_module_enable(1);
#endif
        tone_play_with_callback_by_name(tone_table[IDEX_TONE_BROADCAST_CLOSE], 1, broadcast_tone_play_end_callback, (void *)IDEX_TONE_BROADCAST_CLOSE);
        broadcast_close(broadcast_hdl);
        broadcast_hdl = 0;
        broadcast_status = APP_BROADCAST_STATUS_STOP;
    } else {
        log_info("broadcast_open");
#if defined(RCSP_MODE) && RCSP_MODE
        ble_module_enable(0);
#endif
        tone_play_with_callback_by_name(tone_table[IDEX_TONE_BROADCAST_OPEN], 1, broadcast_tone_play_end_callback, (void *)IDEX_TONE_BROADCAST_OPEN);
        if (is_broadcast_as_transmitter()) {
            temp_broadcast_hdl = broadcast_transmitter();
        } else {
            temp_broadcast_hdl = broadcast_receiver();
        }
        if (temp_broadcast_hdl < -1) {
            broadcast_hdl = 0;
        } else if (temp_broadcast_hdl >= 0) {
            broadcast_hdl = temp_broadcast_hdl;
            broadcast_status = APP_BROADCAST_STATUS_START;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播开启情况下，不同场景的处理流程
 *
 * @return -1:无需处理，0:处理事件但不拦截后续流程，1:处理事件并拦截后续流程
 */
/* ----------------------------------------------------------------------------*/
int app_broadcast_deal(int switch_mode)
{
    int ret = -1;
    static int cur_mode = -1;
    static u8 phone_start_cnt = 0;

    if (!app_bt_hdl.init_ok) {
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        if (switch_mode != BROADCAST_APP_MODE_ENTER) {
            return ret;
        }
#else
        return ret;
#endif
    }

    if ((cur_mode == switch_mode) &&
        (switch_mode != BROADCAST_PHONE_START) &&
        (switch_mode != BROADCAST_PHONE_STOP)) {
        log_error("app_broadcast_deal,cur_mode not be modified:%d", switch_mode);
        return ret;
    }

    cur_mode = switch_mode;

    switch (switch_mode) {
    case BROADCAST_APP_MODE_ENTER:
        log_info("BROADCAST_APP_MODE_ENTER");
        //进入当前模式
        broadcast_app_mode_exit = 0;
        config_broadcast_as_master = 1;
        ret = 0;
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
#if 0
        if (app_get_curr_task() == APP_BT_TASK) {
            g_printf("%s APP_BT_TASK", __FUNCTION__);
            user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
            user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        }
        if (app_get_curr_task() == APP_LINEIN_TASK) {
            g_printf("%s APP_LINEIN_TASK", __FUNCTION__);
            app_broadcast_open();
            ret = 1;
        }
        break;
#else
        if (app_get_curr_task() == APP_LINEIN_TASK) {
            btstack_init_in_other_mode();
            ret = 1;
        }
        break;
#endif
#endif
        if (app_get_curr_task() == APP_BT_TASK) {
            if (broadcast_last_role == BROADCAST_ROLE_TRANSMITTER) {
                /* break; */
            }
        }
        if (is_need_resume_broadcast()) {
            app_broadcast_resume();
            ret = 1;
        }
        break;
    case BROADCAST_APP_MODE_EXIT:
        log_info("BROADCAST_APP_MODE_EXIT");
        //退出当前模式
        broadcast_app_mode_exit = 1;
        config_broadcast_as_master = 0;
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        app_broadcast_close();
#else
        app_broadcast_suspend();
#endif
        ret = 0;
        break;
    case BROADCAST_MUSIC_START:
    case BROADCAST_A2DP_START:
        log_info("BROADCAST_MUSIC_START");
        //启动a2dp播放
        ret = 0;
        broadcast_app_mode_exit = 0;
        config_broadcast_as_master = 1;
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        break;
#endif
        //(1)当处于广播开启并且作为接收设备时，挂起广播，播放当前手机音乐；
        //(2)当前广播处于挂起状态时，恢复广播并作为发送设备。
        if (broadcast_status == APP_BROADCAST_STATUS_START) {
            if (get_broadcast_role() == BROADCAST_ROLE_RECEIVER) {
                app_broadcast_suspend();
            } else if (get_broadcast_role() == BROADCAST_ROLE_TRANSMITTER) {
                ret = 1;
            }
        }
#if BT_SUPPORT_MUSIC_VOL_SYNC
        bt_set_music_device_volume(get_music_sync_volume());
#endif
        if (is_need_resume_broadcast()) {
            app_broadcast_resume();
            ret = 1;
        }
        break;
    case BROADCAST_MUSIC_STOP:
    case BROADCAST_A2DP_STOP:
        log_info("BROADCAST_MUSIC_STOP");
        //停止a2dp播放
        ret = 0;
        config_broadcast_as_master = 0;
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
        break;
#endif
        //当前处于广播挂起状态时，停止手机播放，恢复广播并接收其他设备的音频数据
        app_broadcast_suspend();
        if (is_need_resume_broadcast()) {
            app_broadcast_resume();
        }
        break;
    case BROADCAST_PHONE_START:
        log_info("BROADCAST_PHONE_START");
        //通话时，挂起广播
        phone_start_cnt++;
        app_broadcast_suspend();
        ret = 0;
        break;
    case BROADCAST_PHONE_STOP:
        log_info("BROADCAST_PHONE_STOP");
        //通话结束恢复广播
        ret = 0;
        phone_start_cnt--;
        printf("===phone_start_cnt:%d===\n", phone_start_cnt);
        if (phone_start_cnt) {
            log_info("phone_start_cnt:%d", phone_start_cnt);
            break;
        }
        //当前处于蓝牙模式并且挂起前广播作为发送设备，恢复广播的操作在播放a2dp处执行
        if (app_get_curr_task() == APP_BT_TASK) {
            if (broadcast_last_role == BROADCAST_ROLE_TRANSMITTER) {
            }
        }
        //当前处于蓝牙模式并且挂起前广播，恢复广播并作为接收设备
        if (is_need_resume_broadcast()) {
            app_broadcast_resume();
        }
        break;
    case BROADCAST_EDR_DISCONN:
        log_info("BROADCAST_EDR_DISCONN");
        ret = 0;
        //当经典蓝牙断开后，作为发送端的广播设备挂起广播
        if (get_broadcast_role() == BROADCAST_ROLE_TRANSMITTER) {
            app_broadcast_suspend();
        }
        if (is_need_resume_broadcast()) {
            app_broadcast_resume();
        }
        break;
    default:
        log_error("%s invalid operation\n", __FUNCTION__);
        break;
    }

    return ret;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化同步的状态数据的内容
 */
/* ----------------------------------------------------------------------------*/
static void app_broadcast_sync_data_init(void)
{
    app_broadcast_data_sync.volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    broadcast_sync_data_init(&app_broadcast_data_sync);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 更新广播同步状态的数据
 *
 * @param type:更新项
 * @param value:更新值
 *
 * @return -1:fail，0:success
 */
/* ----------------------------------------------------------------------------*/
int update_broadcast_sync_data(u8 type, int value)
{
    switch (type) {
    case BROADCAST_SYNC_VOL:
        if (app_broadcast_data_sync.volume == value) {
            return 0;
        }
        app_broadcast_data_sync.volume = value;
        break;
    case BROADCAST_SYNC_SOFT_OFF:
        if (app_broadcast_data_sync.softoff == value) {
            return 0;
        }
        app_broadcast_data_sync.softoff = value;
        break;
    default:
        return -1;
    }
    return broadcast_set_sync_data(broadcast_hdl, &app_broadcast_data_sync, sizeof(struct broadcast_sync_info));
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收到广播发送端的同步数据，并更新本地配置
 *
 * @param priv:接收到的同步数据
 *
 * @return
 */
/* ----------------------------------------------------------------------------*/
int broadcast_padv_data_deal(void *priv)
{
    struct broadcast_sync_info *sync_data = (struct broadcast_sync_info *)priv;
    if (app_broadcast_data_sync.volume != sync_data->volume) {
        app_broadcast_data_sync.volume = sync_data->volume;
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_broadcast_data_sync.volume, 1);
    }
    if (app_broadcast_data_sync.softoff != sync_data->softoff) {
        app_broadcast_data_sync.softoff = sync_data->softoff;
        if (!app_var.goto_poweroff_flag) {
            sys_timeout_add(NULL, sys_enter_soft_poweroff, app_broadcast_data_sync.softoff);
        }
    }
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播资源初始化
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_init(void)
{
    if (!app_bt_hdl.init_ok) {
        return;
    }

    broadcast_init();
    app_broadcast_sync_data_init();
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播资源卸载
 */
/* ----------------------------------------------------------------------------*/
void app_broadcast_uninit(void)
{
    if (!app_bt_hdl.init_ok) {
        return;
    }

    broadcast_uninit();
}

void app_broadcast_enter_pair(u8 role)
{
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
    app_broadcast_close();
    enter_pair_flag = 1;
#endif
    if (role == BROADCAST_ROLE_UNKNOW) {
        if (is_broadcast_as_transmitter()) {
            broadcast_enter_pair(BROADCAST_ROLE_TRANSMITTER);
        } else {
            broadcast_enter_pair(BROADCAST_ROLE_RECEIVER);
        }
    } else {
        broadcast_enter_pair(role);
    }
}

void app_broadcast_exit_pair(u8 role)
{
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
#if defined(WIRELESS_1tN_ROLE_SEL) && (WIRELESS_1tN_ROLE_SEL == WIRELESS_1tN_ROLE_RX)
    return;
#endif
    if(enter_pair_flag){
        enter_pair_flag = 0;
    }else{
        return;
    }
#endif
    if (role == BROADCAST_ROLE_UNKNOW) {
        if (is_broadcast_as_transmitter()) {
            broadcast_exit_pair(BROADCAST_ROLE_TRANSMITTER);
        } else {
            broadcast_exit_pair(BROADCAST_ROLE_RECEIVER);
        }
    } else {
        broadcast_exit_pair(role);
    }
}

void app_broadcast_config()
{
    app_broadcast_init();
#if defined(WIRELESS_1tN_EN) && (WIRELESS_1tN_EN)
    int pair_flag = 0;
    int ret = syscfg_read(VM_WIRELESS_PAIR_FLAG, &pair_flag, sizeof(int));
    if (ret > 0) {
        if (pair_flag == 1) {
            app_broadcast_open();
        }
    }
#else
    app_broadcast_open();
#endif
}

#endif

