#include "system/includes.h"
#include "media/includes.h"
#include "device/vm.h"
#include "tone_player.h"

#include "app_config.h"
#include "app_task.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "classic/hci_lmp.h"
#include "user_cfg.h"
#include "bt_tws.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "ui_manage.h"

#include "app_chargestore.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "user_cfg.h"

#include "btcontroller_config.h"
#include "bt_common.h"
#include "asm/pwm_led.h"
#include "bt_common.h"

#ifdef CONFIG_NEW_BREDR_ENABLE

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#include "include/bt_ble.h"
#endif

#if TCFG_USER_TWS_ENABLE

#define LOG_TAG_CONST       BT_TWS
#define LOG_TAG             "[BT-TWS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_DEC2TWS_ENABLE
#define    CONFIG_BT_TWS_SNIFF                  0       //[WIP]
#else
#define    CONFIG_BT_TWS_SNIFF                  1       //[WIP]
#endif

#define    BT_TWS_UNPAIRED                      0x0001
#define    BT_TWS_PAIRED                        0x0002
#define    BT_TWS_WAIT_SIBLING_SEARCH           0x0004
#define    BT_TWS_SEARCH_SIBLING                0x0008
#define    BT_TWS_CONNECT_SIBLING               0x0010
#define    BT_TWS_SIBLING_CONNECTED             0x0020
#define    BT_TWS_PHONE_CONNECTED               0x0040
#define    BT_TWS_POWER_ON                      0x0080
#define    BT_TWS_TIMEOUT                       0x0100
#define    BT_TWS_AUDIO_PLAYING                 0x0200
#define    BT_TWS_DISCON_DLY_TIMEOUT            0x0400
#define    BT_TWS_REMOVE_PAIRS                  0x0800

#define TWS_DLY_DISCONN_TIME   2000//超时断开，快速连接上不播提示音
struct tws_user_var {
    u8 addr[6];
    u16 state;
    s16 auto_pair_timeout;
    int pair_timer;
    u32 connect_time;
    u8  device_role;  //tws 记录那个是active device 活动设备，音源控制端
    u8 bt_task;   ///标志对箱在bt task情况，BIT(0):local  BIT(1):remote
    u8 tws_dly_discon_time;
    u16 sniff_timer;
};

struct tws_user_var  gtws;

///记录tws是否在蓝牙模式
#define    TWS_LOCAL_IN_BT()      (gtws.bt_task |=BIT(0))
#define    TWS_LOCAL_OUT_BT()   (gtws.bt_task &=~BIT(0))
#define    TWS_REMOTE_IN_BT()      (gtws.bt_task |=BIT(1))
#define    TWS_REMOTE_OUT_BT()    (gtws.bt_task &=~BIT(1))

extern void tone_play_deal(const char *name, u8 preemption, u8 add_en);

extern u8 check_tws_le_aa(void);
extern const char *bt_get_local_name();
extern u8 get_bredr_link_state();
extern u8 get_esco_coder_busy_flag();
void bt_init_ok_search_index(void);
extern void phone_num_play_timer(void *priv);
extern u8 phone_ring_play_start(void);
extern bool get_esco_busy_flag();
extern void tws_api_set_connect_aa(int);
extern void tws_api_clear_connect_aa();
extern u8 tws_remote_state_check(void);
extern void tws_remote_state_clear(void);
extern u16 bt_get_tws_device_indicate(u8 *tws_device_indicate);
void tws_le_acc_generation_init(void);
static void bt_tws_connect_and_connectable_switch();
extern int earphone_a2dp_codec_get_low_latency_mode();
extern int earphone_a2dp_codec_set_low_latency_mode(int enable, int);
extern u32 get_bt_slot_time(u8 type, u32 time, int *ret_time, int (*local_us_time)(void));
extern u32 get_sync_rec_instant_us_time();
extern int update_check_sniff_en(void);
void tws_sniff_controle_check_disable(void);

static u32 local_us_time = 0;
static u32 local_instant_us_time = 0;
static u8 tone_together_by_systime = 0;
static u32 tws_tone_together_time = 0;
static u16 poweroff_sametime_timer_id = 0;
static u8 poweroff_sametime_flag = 0;

void tws_sniff_controle_check_enable(void);

static void bt_tws_delete_pair_timer();
static void bt_tws_wait_pair_and_phone_connect();
static void bt_tws_wait_sibling_connect(int timeout);
static void bt_tws_connect_sibling(int timeout);
void bt_tws_remove_pairs();

#include "tws_box_api.c"

u8 tws_network_audio_was_started(void)
{
    if (gtws.state & BT_TWS_AUDIO_PLAYING) {
        return 1;
    }

    return 0;
}

void tws_network_local_audio_start(void)
{
    gtws.state &= ~BT_TWS_AUDIO_PLAYING;
}


static void tws_sync_call_fun(int cmd, int err)
{
    struct sys_event event;
    int diff_time_us = 0;

    event.type = SYS_BT_EVENT;
    event.arg = (void *)SYS_BT_EVENT_FROM_TWS;

    event.u.bt.event = TWS_EVENT_SYNC_FUN_CMD;
    event.u.bt.args[0] = 0;
    event.u.bt.args[1] = 0;
    event.u.bt.args[2] = cmd;

    sys_event_notify(&event);
}

TWS_SYNC_CALL_REGISTER(tws_tone_sync) = {
    .uuid = 'T',
    .func = tws_sync_call_fun,
};

u8 tws_tone_together_without_bt(void)
{
    return tone_together_by_systime;
}

void tws_tone_together_clean(void)
{
    tone_together_by_systime = 0;
}

u32 tws_tone_local_together_time(void)
{
    return tws_tone_together_time;
}

u32 get_local_us_time(u32 *instant_time)
{
    *instant_time = local_instant_us_time;
    return local_us_time;
}

#define msecs_to_bt_slot_clk(m)     (((m + 1)* 1000) / 625)
u32 bt_tws_master_slot_clk(void);
u32 bt_tws_future_slot_time(u32 msecs)
{
    return bt_tws_master_slot_clk() + msecs_to_bt_slot_clk(msecs);
}

u16 tws_host_get_battery_voltage()
{
    return get_vbat_level();
}

int tws_host_channel_match(char remote_channel)
{
    /*r_printf("tws_host_channel_match: %c, %c\n", remote_channel,
             bt_tws_get_local_channel());*/
#ifdef CONFIG_NEW_BREDR_ENABLE
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_SECECT_BY_CHARGESTORE
    return 1;
#else
    if (remote_channel != bt_tws_get_local_channel()) {
        return 1;
    }
#endif

#else

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT)
    return 1;
#endif
    if (remote_channel != bt_tws_get_local_channel() || remote_channel == 'U') {
        return 1;
    }
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 1;
    }
#endif
#endif

    return 0;
}

char tws_host_get_local_channel()
{
    char channel;

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'R';
    }
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'L';
    }
#endif
    channel = bt_tws_get_local_channel();
    if (channel != 'R') {
        channel = 'L';
    }
    /*y_printf("tws_host_get_local_channel: %c\n", channel);*/

    return channel;
}

#if CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_LEFT
#ifdef CONFIG_NEW_BREDR_ENABLE
void tws_host_get_common_addr(u8 *remote_mac_addr, u8 *common_addr, char channel)
{
    if (channel == 'L') {
        memcpy(common_addr, bt_get_mac_addr(), 6);
    } else {
        memcpy(common_addr, remote_mac_addr, 6);
    }
}
#endif
#endif

#if TCFG_DEC2TWS_ENABLE
int tws_host_get_local_role()
{
    if (app_var.have_mass_storage) {
        return TWS_ROLE_MASTER;
    }

    return TWS_ROLE_SLAVE;
}
#endif

/*
 * 设置自动回连的识别码 6个byte
 * */
u8 auto_pair_code[6] = {0x34, 0x66, 0x33, 0x87, 0x09, 0x42};

u8 *tws_set_auto_pair_code(void)
{
    u16 code = bt_get_tws_device_indicate(NULL);
    auto_pair_code[0] = code >> 8;
    auto_pair_code[1] = code & 0xff;
    return auto_pair_code;
}
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_FAST_CONN
u8 tws_auto_pair_enable = 1;
#else
u8 tws_auto_pair_enable = 0;
#endif


u8 tws_get_sibling_addr(u8 *addr, int *result)
{
    u8 all_ff[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    int len = syscfg_read(CFG_TWS_REMOTE_ADDR, addr, 6);
    if (len != 6 || !memcmp(addr, all_ff, 6)) {
        *result = len;
        return -ENOENT;
    }
    return 0;
}

void lmp_hci_write_local_address(const u8 *addr);

/*
 * 获取左右耳信息
 * 'L': 左耳
 * 'R': 右耳
 * 'U': 未知
 */
char bt_tws_get_local_channel()
{
    char channel = 'U';

    syscfg_read(CFG_TWS_CHANNEL, &channel, 1);

    return channel;
}

int get_bt_tws_connect_status()
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return 1;
    }

    return 0;
}
u8 get_bt_tws_discon_dly_state()
{
    if (gtws.tws_dly_discon_time) {
        return 1;
    }
    return 0;
}
void tws_disconn_dly_deal(void *priv)
{
    int role = (int)priv;
    r_printf("tws_disconn_dly_deal=0x%x\n", role);
    STATUS *p_tone = get_tone_config();
    gtws.tws_dly_discon_time = 0;
    if (gtws.state & BT_TWS_DISCON_DLY_TIMEOUT) {
        tone_play_index(p_tone->tws_disconnect, 1);
    }
    if ((role == TWS_ROLE_SLAVE) && (!app_var.goto_poweroff_flag)) { /*关机不播断开提示音*/

        tone_play_index(p_tone->bt_disconnect, 1);
    }
}


void tws_sync_phone_num_play_wait(void *priv)
{
    puts("tws_sync_phone_num_play_wait\n");
    if (bt_user_priv_var.phone_con_sync_num_ring) {
        puts("phone_con_sync_num_ring\n");
        return;
    }
    if (bt_user_priv_var.phone_num_flag) {
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            bt_tws_play_tone_at_same_time(SYNC_TONE_PHONE_NUM, 400);
        } else { //从机超时还没获取到
            phone_ring_play_start();
        }
    } else {
        /*电话号码还没有获取到，定时查询*/
        if (bt_user_priv_var.get_phone_num_timecnt > 60) {
            bt_user_priv_var.get_phone_num_timecnt = 0;
            bt_tws_play_tone_at_same_time(SYNC_TONE_PHONE_NUM_RING, 400);
        } else {
            bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, 10);
        }
    }
    bt_user_priv_var.get_phone_num_timecnt++;

}

int bt_tws_sync_phone_num(void *priv)
{
    int state = tws_api_get_tws_state();
    if ((state & TWS_STA_SIBLING_CONNECTED) && (state & TWS_STA_PHONE_CONNECTED)) {
        puts("bt_tws_sync_phone_num\n");
#if BT_PHONE_NUMBER
        bt_user_priv_var.get_phone_num_timecnt = 0;
        if (tws_api_get_role() == TWS_ROLE_SLAVE && bt_user_priv_var.phone_con_sync_num_ring) { //从机同步播来电ring,不在发起sync_play
            puts("phone_con_sync_num_ring_ing return\n");
            return 1;
        }
#endif
        if (tws_api_get_role() == TWS_ROLE_SLAVE || bt_user_priv_var.phone_con_sync_num_ring) {

            if (!(state & TWS_STA_MONITOR_START)) {
                puts(" not monitor ring_tone\n");
                /* tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 5); */
                bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, (void (*)(void *priv))bt_tws_sync_phone_num, 10);
                return 1;
            }
#if BT_PHONE_NUMBER
            if (bt_user_priv_var.phone_con_sync_num_ring) {
                puts("<<<<<<<<<<<send_SYNC_CMD_PHONE_SYNC_NUM_RING_TONE\n");
                bt_tws_play_tone_at_same_time(SYNC_TONE_PHONE_NUM_RING, 400);

            } else {
                bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, 10);
            }
#else
            bt_tws_play_tone_at_same_time(SYNC_TONE_PHONE_RING, 600);
#endif
        } else {
#if BT_PHONE_NUMBER
            if (!bt_user_priv_var.phone_timer_id) { //从机超时还没获取到
                /* bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, TWS_SYNC_TIME_DO * 2); */
            }
#endif
        }
        return 1;
    }
    return 0;
}


static void bt_tws_delete_pair_timer()
{
    if (gtws.pair_timer) {
        sys_timeout_del(gtws.pair_timer);
        gtws.pair_timer = 0;
    }
}

/*
 * 根据配对码搜索TWS设备
 * 搜索超时会收到事件: TWS_EVENT_SEARCH_TIMEOUT
 * 搜索到连接超时会收到事件: TWS_EVENT_CONNECTION_TIMEOUT
 * 搜索到并连接成功会收到事件: TWS_EVENT_CONNECTED
 */
static void bt_tws_search_sibling_and_pair()
{
    u8 mac_addr[6];

    if (gtws.state & BT_TWS_UNPAIRED) {
        bt_tws_delete_pair_timer();
        tws_api_get_local_addr(gtws.addr);
#if CONFIG_TWS_USE_COMMMON_ADDR
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
        get_random_number(mac_addr, 6);
        lmp_hci_write_local_address(mac_addr);
#endif
#endif
        tws_api_search_sibling_by_code(bt_get_tws_device_indicate(NULL), 15000);
    }
}

/*
 *打开可发现, 可连接，可被手机和tws搜索到
 */
static void bt_tws_wait_pair_and_phone_connect()
{
    bt_tws_delete_pair_timer();
    tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
}

static void bt_tws_wait_sibling_connect(int timeout)
{
    bt_tws_delete_pair_timer();
    tws_api_wait_connection(timeout);
}

static void bt_tws_connect_sibling(int timeout)
{
    int err;

    bt_tws_delete_pair_timer();
    err = tws_api_create_connection(timeout * 1000);
    if (err) {
        bt_tws_connect_and_connectable_switch();
    }
}


static int bt_tws_connect_phone()
{
    int timeout = 6500;

    bt_user_priv_var.auto_connection_counter -= timeout ;
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                          bt_user_priv_var.auto_connection_addr);
    return timeout;
}


/*
 * TWS 回连手机、开可发现可连接、回连已配对设备、搜索tws设备 4个状态间定时切换函数
 *
 */
#ifdef CONFIG_NEW_BREDR_ENABLE
static void connect_and_connectable_switch(void *_sw)
{
    int timeout = 0;
    int sw = (int)_sw;
    int end_sw = 1;

    gtws.pair_timer = 0;

    log_info("switch: %d, state = %x, %d\n", sw, gtws.state,
             bt_user_priv_var.auto_connection_counter);

    if (sw == 0) {
__again:
        if (bt_user_priv_var.auto_connection_counter > 0) {
            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                timeout = 8000;
            } else {
#ifdef CONFIG_TWS_BY_CLICK_PAIR_WITHOUT_PAIR
                timeout = 2000 + (rand32() % 4 + 1) * 500;
#else
                timeout = 4000 + (rand32() % 4 + 1) * 500;
#endif
            }
            bt_user_priv_var.auto_connection_counter -= timeout ;
            tws_api_cancle_wait_pair();
            tws_api_cancle_create_connection();
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                                  bt_user_priv_var.auto_connection_addr);
        } else {
            bt_user_priv_var.auto_connection_counter = 0;

            /* EARPHONE_STATE_CANCEL_PAGE_SCAN(); */

            if (gtws.state & BT_TWS_POWER_ON) {
                /*
                 * 开机回连,获取下一个设备地址
                 */
                if (get_current_poweron_memory_search_index(NULL)) {
                    bt_init_ok_search_index();
                    goto __again;
                }
                gtws.state &= ~BT_TWS_POWER_ON;
            }

            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);

                tws_sniff_controle_check_enable();
                return;
            }
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            if (gtws.state & BT_TWS_UNPAIRED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }
#endif
            sw = 1;
        }
    }

    if (sw == 1) {
        tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
            if (!(gtws.state & BT_TWS_UNPAIRED)) {
#ifdef CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR
                end_sw = 2;
#endif
            } else {
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
                goto __set_time;
#endif
            }
            tws_api_create_connection(0);
            //tws_api_power_saving_mode_enable();
        }
__set_time:
#ifdef CONFIG_TWS_BY_CLICK_PAIR_WITHOUT_PAIR
        timeout = 500 + (rand32() % 4 + 1) * 500;
#else
        timeout = 2000 + (rand32() % 4 + 1) * 500;
#endif

        /* EARPHONE_STATE_SET_PAGE_SCAN_ENABLE(); */

        if (bt_user_priv_var.auto_connection_counter <= 0 && end_sw == 1) {
            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                tws_sniff_controle_check_enable();
            }
            return;
        }

    } else if (sw == 2) {
        u8 addr[6];
#ifdef CONFIG_TWS_BY_CLICK_PAIR_WITHOUT_PAIR
        timeout = 500 + (rand32() % 4 + 1) * 500;
        tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);

#else
        timeout = 2000 + (rand32() % 4 + 1) * 500;
        memcpy(addr, tws_api_get_quick_connect_addr(), 6);
        tws_api_set_quick_connect_addr(tws_set_auto_pair_code());
        tws_api_create_connection(0);
        tws_api_set_quick_connect_addr(addr);
        /*tws_api_power_saving_mode_enable();*/
#endif
    }
    if (++sw > end_sw) {
        sw = 0;
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, connect_and_connectable_switch, timeout);
}

#else

static void connect_and_connectable_switch(void *_sw)
{
    int timeout = 0;
    int sw = (int)_sw;

    gtws.pair_timer = 0;

    log_info("switch: %d, state = %x, %d\n", sw, gtws.state,
             bt_user_priv_var.auto_connection_counter);

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
    if (tws_auto_pair_enable == 1) {
        tws_auto_pair_enable = 0;
        tws_le_acc_generation_init();
    }
#endif

    if (tws_remote_state_check()) {
        if (sw == 0) {
            sw = 1;
        } else if (sw == 3) {
            sw = 2;
        }
        puts("tws_exist\n");
    }

    if (sw == 0) {
__again:
        if (bt_user_priv_var.auto_connection_counter > 0) {
            timeout = 4000 + (rand32() % 4 + 1) * 500;
            bt_user_priv_var.auto_connection_counter -= timeout ;
            tws_api_cancle_wait_pair();
            tws_api_cancle_create_connection();
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                                  bt_user_priv_var.auto_connection_addr);
        } else {
            bt_user_priv_var.auto_connection_counter = 0;

            /* EARPHONE_STATE_CANCEL_PAGE_SCAN(); */

            if (gtws.state & BT_TWS_POWER_ON) {
                /*
                 * 开机回连,获取下一个设备地址
                 */
                if (get_current_poweron_memory_search_index(NULL)) {
                    bt_init_ok_search_index();
                    goto __again;
                }
                gtws.state &= ~BT_TWS_POWER_ON;
            }

            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            if (gtws.state & BT_TWS_UNPAIRED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }
#endif
            sw = 1;
        }
    }

    if (sw == 1) {
        if (tws_remote_state_check()) {
            timeout = 1000 + (rand32() % 4 + 1) * 500;
        } else {
            timeout = 2000 + (rand32() % 4 + 1) * 500;
        }
        tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);

        /* EARPHONE_STATE_SET_PAGE_SCAN_ENABLE(); */

    } else if (sw == 2) {
        if (tws_remote_state_check()) {
            timeout = 4000;
        } else {
            timeout = 1000 + (rand32() % 4 + 1) * 500;
        }
        tws_remote_state_clear();
        tws_api_create_connection(0);
    } else if (sw == 3) {
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        timeout = 2000 + (rand32() % 4 + 1) * 500;
        tws_auto_pair_enable = 1;
        tws_le_acc_generation_init();
        tws_api_create_connection(0);
#endif
    }

    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        if (++sw > 1) {
            sw = 0;
        }
    } else {
        int end_sw = 2;
#ifdef CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR
        end_sw = 3;
#elif CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        if (gtws.state & BT_TWS_UNPAIRED) {
            end_sw = 3;
        }
#endif
        if (++sw > end_sw) {
            sw = 0;
        }
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, connect_and_connectable_switch, timeout);

}
#endif

static void bt_tws_connect_and_connectable_switch()
{
    bt_tws_delete_pair_timer();

#if TCFG_AUDIO_ANC_ENABLE
#if TCFG_ANC_BOX_ENABLE
    if (ancbox_get_status()) {
        /* EARPHONE_STATE_SET_PAGE_SCAN_ENABLE(); */
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        return;
    }
#endif
#endif

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        connect_and_connectable_switch(0);
    }
}


/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 启动tws 主机搜索链接
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
int bt_tws_start_search_and_pair()
{

    /* int state = get_bt_connect_status(); */

    if ((get_call_status() == BT_CALL_ACTIVE) ||
        (get_call_status() == BT_CALL_OUTGOING) ||
        (get_call_status() == BT_CALL_ALERT) ||
        (get_call_status() == BT_CALL_INCOMING)) {
        return 0;//通话过程不允许
    }

    if (check_tws_le_aa()) {
        return 0;
    }

#if CONFIG_TWS_USE_COMMMON_ADDR
    if (tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED) {
        return 0;
    }
#endif

#if CONFIG_TWS_DISCONN_NO_RECONN
    if (tws_detach_by_remove_key) {
        bt_tws_connect_sibling(CONFIG_TWS_CONNECT_SIBLING_TIMEOUT);
        return 0;
    }
#endif

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
    log_i("bt_tws_start_search_sibling\n");
    gtws.state |= BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT
    tws_api_set_local_channel('U');
#endif
    bt_tws_search_sibling_and_pair();
    return 1;
#endif

    return 0;

}

u8  bt_tws_remove_tws_pair()
{
    /* int state = get_bt_connect_status(); */

    if ((get_call_status() == BT_CALL_ACTIVE) ||
        (get_call_status() == BT_CALL_OUTGOING) ||
        (get_call_status() == BT_CALL_ALERT) ||
        (get_call_status() == BT_CALL_INCOMING)) {
        return 0;//通话过程不允许
    }

#if CONFIG_TWS_USE_COMMMON_ADDR
    if (tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED) {
        return 0;
    }
#endif

    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
#ifdef CONFIG_TWS_REMOVE_PAIR_ENABLE
        puts("===========remove_pairs\n");
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_api_remove_pairs();
            gtws.state |= BT_TWS_REMOVE_PAIRS;
        }
#endif
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 取消tws配对发起tws 链接
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void  bt_tws_search_or_remove_pair()
{
    if (bt_tws_remove_tws_pair()) {
        return;
    }

    bt_tws_start_search_and_pair();
}

int bt_tws_start_search_sibling()
{
    int state = get_bt_connect_status();

    if (gtws.state & BT_TWS_PHONE_CONNECTED) {
        return 0;
    }

    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
#ifdef CONFIG_TWS_REMOVE_PAIR_ENABLE
        puts("===========remove_pairs\n");
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_api_remove_pairs();
            gtws.state |= BT_TWS_REMOVE_PAIRS;
        }
#endif
        return 0;
    }

    if (check_tws_le_aa()) {
        return 0;
    }

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
    log_i("bt_tws_start_search_sibling\n");
    gtws.state |= BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT
    tws_api_set_local_channel('U');
#endif
    bt_tws_search_sibling_and_pair();
    return 1;
#endif

    return 0;
}

void bt_disconnect_tws_conn()
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) { // tws 没有连接上
        tws_disconnect();
    }
}

int bt_open_tws_conn(u16 timeout)
{
    return tws_api_create_connection(timeout);
}

/*
 * 自动配对状态定时切换函数
 */
static void __bt_tws_auto_pair_switch(void *priv)
{
    u32 timeout;
    int sw = (int)priv;

    gtws.pair_timer = 0;

    printf("bt_tws_auto_pair: %d, %d\n", gtws.auto_pair_timeout, bt_user_priv_var.auto_connection_counter);

    if (gtws.auto_pair_timeout < 0 && bt_user_priv_var.auto_connection_counter > 0) {
        gtws.auto_pair_timeout = 4000;
        timeout = bt_tws_connect_phone();
    } else {

        timeout = 1000 + ((rand32() % 10) * 200);

        gtws.auto_pair_timeout -= timeout;

        if (sw == 0) {
            tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        } else if (sw == 1) {
            bt_tws_search_sibling_and_pair();
        }

        if (++sw > 1) {
            sw = 0;
        }
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, __bt_tws_auto_pair_switch, timeout);
}

static void bt_tws_auto_pair_switch(int timeout)
{
    bt_tws_delete_pair_timer();

    gtws.auto_pair_timeout = timeout;
    __bt_tws_auto_pair_switch(NULL);
}



static u8 set_channel_by_code_or_res(void)
{
    u8 count = 0;
    char channel = 0;
    char last_channel = 0;
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_LEFT)
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_pull_down(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_pull_down(CONFIG_TWS_CHANNEL_CHECK_IO, 0);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 0);
    channel = (count >= 3) ? 'L' : 'R';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_LEFT)
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_pull_up(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 0);
    gpio_set_pull_up(CONFIG_TWS_CHANNEL_CHECK_IO, 0);
    channel = (count >= 3) ? 'R' : 'L';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_LEFT_CHANNEL)
    channel = 'L';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_RIGHT_CHANNEL)
    channel = 'R';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_SECECT_BY_CHARGESTORE)
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

#if CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

    if (channel) {
        syscfg_read(CFG_TWS_CHANNEL, &last_channel, 1);
        if (channel != last_channel) {
            syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        }
        tws_api_set_local_channel(channel);
        return 1;
    }
    return 0;
}

/*
 * 开机tws初始化
 */
__BANK_INIT_ENTRY
int bt_tws_poweron()
{
    int err;
    u8 addr[6];
    char channel;

#if TCFG_TEST_BOX_ENABLE
    if (testbox_get_ex_enter_dut_flag()) {
        log_info("tws poweron enter dut case\n");
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        return 0;
    }
#endif

    gtws.tws_dly_discon_time = 0;
    gtws.state = BT_TWS_POWER_ON;
    gtws.connect_time = 0;
    gtws.device_role = TWS_ACTIVE_DEIVCE;
    TWS_LOCAL_IN_BT();

#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
    bt_set_pair_code_en(0);
#endif

#if CONFIG_TWS_USE_COMMMON_ADDR
    tws_api_common_addr_en(1);
#else
    tws_api_common_addr_en(0);
    tws_api_auto_role_switch_disable();
#endif

#if CONFIG_TWS_PAIR_ALL_WAY
    tws_api_pair_all_way(1);
#endif

    lmp_hci_write_page_timeout(12000);
#ifdef CONFIG_A2DP_GAME_MODE_ENABLE
    extern void bt_set_low_latency_mode(int enable);

    /* tws_api_auto_role_switch_disable(); */
    /* bt_set_low_latency_mode(1);//开机默认低延时模式设置 */
#endif

    int result = 0;
    err = tws_get_sibling_addr(addr, &result);
    if (err == 0) {
        /*
         * 获取到对方地址, 开始连接
         */
        printf("\n ---------have tws info----------\n");
        gtws.state |= BT_TWS_PAIRED;

        /* EARPHONE_STATE_TWS_INIT(1); */

        tws_api_set_sibling_addr(addr);
        if (set_channel_by_code_or_res() == 0) {
            channel = bt_tws_get_local_channel();
            tws_api_set_local_channel(channel);
        }
#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
        } else
#endif
        {
#ifdef CONFIG_NEW_BREDR_ENABLE
            if (bt_tws_get_local_channel() == 'L') {
                syscfg_read(CFG_TWS_LOCAL_ADDR, addr, 6);
            }
            tws_api_set_quick_connect_addr(addr);
#endif
            bt_tws_connect_sibling(CONFIG_TWS_CONNECT_SIBLING_TIMEOUT);
        }

    } else {
        printf("\n ---------no tws info----------\n");

        /* EARPHONE_STATE_TWS_INIT(0); */

        // no_tws for test
        //EARPHONE_STATE_TWS_INIT(1);
        //EARPHONE_STATE_SET_PAGE_SCAN_ENABLE();


        gtws.state |= BT_TWS_UNPAIRED;
        if (set_channel_by_code_or_res() == 0) {
            tws_api_set_local_channel('U');
        }
#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
        } else
#endif
        {

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
            /*
             * 未配对, 开始自动配对
             */
#ifdef CONFIG_NEW_BREDR_ENABLE
            tws_api_set_quick_connect_addr(tws_set_auto_pair_code());
#else
            tws_auto_pair_enable = 1;
            tws_le_acc_generation_init();
#endif
            bt_tws_connect_sibling(6);

#else
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            if (result == VM_INDEX_ERR) {
                printf("tws_get_sibling_addr index_err and bt_tws_start_search_sibling =%d\n", result);
                bt_tws_start_search_sibling();
            } else
#endif
            {
                /*
                 * 未配对, 等待发起配对
                 */
                bt_tws_connect_and_connectable_switch();
                /*tws_api_create_connection(0);*/
                /*bt_tws_search_sibling_and_pair();*/

            }
#endif
        }
    }

#ifndef CONFIG_NEW_BREDR_ENABLE
    tws_remote_state_clear();
#endif

    return 0;
}

/*
 * 手机开始连接
 */
void bt_tws_hci_event_connect()
{
    printf("bt_tws_hci_event_connect: %x\n", gtws.state);

    gtws.state &= ~BT_TWS_POWER_ON;

    bt_user_priv_var.auto_connection_counter = 0;

    bt_tws_delete_pair_timer();
    sys_auto_shut_down_disable();
    tws_api_power_saving_mode_disable();

    tws_sniff_controle_check_disable();

#ifndef CONFIG_NEW_BREDR_ENABLE
    tws_remote_state_clear();
#endif
}

int bt_tws_phone_connected()
{
    int state;

    printf("bt_tws_phone_connected: %x\n", gtws.state);

#if TCFG_TEST_BOX_ENABLE
    if (testbox_get_status()) {
        return 1;
    }
#endif

    gtws.state |= BT_TWS_PHONE_CONNECTED;

    if (gtws.state & BT_TWS_UNPAIRED) {
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)

#if CONFIG_TWS_USE_COMMMON_ADDR

#else
        if (get_call_status() == BT_CALL_HANGUP) {  ///不在通话状态，开启tws 被搜索链接状态
            /* bt_open_tws_wait_pair(); */
            tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        }
#endif
#endif
        return 0;
    }

    if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_wait_sibling_connect(0);
        return 0;
    }

    /* if (app_var.phone_dly_discon_time) { */
    /*     sys_timeout_del(app_var.phone_dly_discon_time); */
    /*     app_var.phone_dly_discon_time = 0; */
    /*     return 1; */
    /* } */

    /*
     * 获取tws状态，如果正在播歌或打电话则返回1,不播连接成功提示音
     */

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        //此时手机已经连接上，同步PHONE_CONN状态
        bt_tws_led_state_sync(SYNC_LED_STA_PHONE_CONN, 300);
        state = tws_api_get_tws_state();
        if (state & (TWS_STA_SBC_OPEN | TWS_STA_ESCO_OPEN) ||
            (get_call_status() != BT_CALL_HANGUP)) {
            return 1;
        }

        log_info("[SYNC] TONE SYNC");
        bt_tws_play_tone_at_same_time(SYNC_TONE_PHONE_CONNECTED, 1200);
    }

    return 1;
}

void bt_tws_phone_disconnected()
{
    gtws.state &= ~BT_TWS_PHONE_CONNECTED;

    printf("bt_tws_phone_disconnected: %x\n", gtws.state);

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        tws_api_sync_call_by_uuid('T', SYNC_CMD_SHUT_DOWN_TIME, 400);//同步关机时间
    }

    bt_user_priv_var.auto_connection_counter = 0;
    if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_connect_and_connectable_switch();
    }
}

void bt_tws_phone_page_timeout()
{
    printf("bt_tws_phone_page_timeout: %x\n", gtws.state);

    bt_tws_phone_disconnected();
}


void bt_tws_phone_connect_timeout()
{
    log_d("bt_tws_phone_connect_timeout: %x, %d\n", gtws.state, gtws.pair_timer);

    gtws.state &= ~BT_TWS_PHONE_CONNECTED;

    /*
     * 跟手机超时断开,如果对耳未连接则优先连接对耳
     */
    if (gtws.state & (BT_TWS_UNPAIRED | BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_connect_and_connectable_switch();
    } else {
        bt_tws_connect_sibling(CONFIG_TWS_CONNECT_SIBLING_TIMEOUT);
    }
}

void bt_get_vm_mac_addr(u8 *addr);

void bt_page_scan_for_test(u8 inquiry_en)
{
    u8 local_addr[6];

    int state = tws_api_get_tws_state();

    log_info("\n\n\n\n -------------bt test page scan\n");

    bt_tws_delete_pair_timer();

    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

    tws_api_detach(TWS_DETACH_BY_POWEROFF);

    user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

    if (0 == get_total_connect_dev()) {
        log_info("<<<<<no phone connect,instant page_scan,inquiry_scan:%x>>>>>\n", inquiry_en);
        bt_get_vm_mac_addr(local_addr);

        //log_info("local_adr\n");
        //put_buf(local_addr,6);
        lmp_hci_write_local_address(local_addr);
        if (inquiry_en) {
            user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        }
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }

    gtws.state = 0;
}

int bt_tws_poweroff()
{
    log_info("bt_tws_poweroff\n");

    bt_tws_delete_pair_timer();

    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

    tws_api_detach(TWS_DETACH_BY_POWEROFF);

    tws_profile_exit();

    if (tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED) {
        return 1;
    }

    return 0;
}

void tws_page_scan_deal_by_esco(u8 esco_flag)
{
    if (gtws.state & BT_TWS_UNPAIRED) {
        return;
    }

    if (esco_flag) {
        bt_tws_delete_pair_timer();
        gtws.state &= ~BT_TWS_CONNECT_SIBLING;
        tws_api_cancle_create_connection();
        tws_api_connect_in_esco();
        puts("close scan\n");
    }

    if (!esco_flag && !(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        puts("open scan22\n");
        tws_api_cancle_connect_in_esco();
        tws_api_wait_connection(0);
    }
}

/*
 * 解除配对，清掉对方地址信息和本地声道信息
 */
void bt_tws_remove_pairs()
{
    u8 mac_addr[6];
    char channel = 'U';
    char tws_channel = 0;
    int role = 0;

    gtws.state &= ~BT_TWS_REMOVE_PAIRS;

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
    syscfg_write(VM_TWS_ROLE, &role, 1);
#endif

    memset(mac_addr, 0xFF, 6);
    syscfg_write(CFG_TWS_COMMON_ADDR, mac_addr, 6);
    syscfg_write(CFG_TWS_REMOTE_ADDR, mac_addr, 6);
#if CONFIG_TWS_USE_COMMMON_ADDR
    bt_reset_and_get_mac_addr(mac_addr);
    delete_last_device_from_vm();
#else
    syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
#endif
    lmp_hci_write_local_address(mac_addr);

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT)

#if CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &tws_channel, 1);
#endif
    if ((tws_channel != 'L') && (tws_channel != 'R')) {
        syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        tws_api_set_local_channel(channel);
    }
#endif

    tws_api_clear_connect_aa();

    bt_tws_wait_pair_and_phone_connect();

}

#if TCFG_ADSP_UART_ENABLE
extern void adsp_deal_sibling_uart_command(u8 type, u8 cmd);
void tws_sync_uart_command(u8 type, u8 cmd)
{
    struct tws_sync_info_t sync_adsp_uart_command;
    sync_adsp_uart_command.type = TWS_SYNC_ADSP_UART_CMD;
    sync_adsp_uart_command.u.adsp_cmd[0] = type;
    sync_adsp_uart_command.u.adsp_cmd[1] = cmd;
    tws_api_send_data_to_sibling((u8 *)&sync_adsp_uart_command, sizeof(sync_adsp_uart_command));
}
#endif

extern void set_tws_sibling_bat_level(u16 level);


#define TWS_FUNC_ID_VOL_SYNC    TWS_FUNC_ID('V', 'O', 'L', 'S')

static void bt_tws_vol_sync(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, data[0], 1);
        app_audio_set_volume(APP_AUDIO_STATE_CALL, data[1], 1);
        r_printf("vol_sync: %d, %d\n", data[0], data[1]);
    }

}


REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = TWS_FUNC_ID_VOL_SYNC,
    .func    = bt_tws_vol_sync,
};

void bt_tws_sync_volume()
{
    u8 data[2];

    data[0] = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    data[1] = app_audio_get_volume(APP_AUDIO_STATE_CALL);

    tws_api_send_data_to_slave(data, 2, TWS_FUNC_ID_VOL_SYNC);
}

#if TCFG_AUDIO_ANC_ENABLE
#define TWS_FUNC_ID_ANC_SYNC    TWS_FUNC_ID('A', 'N', 'C', 'S')
static void bt_tws_anc_sync(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        //r_printf("[slave]anc_sync: %d, %d\n", data[0], data[1]);
        anc_mode_sync(data[0]);
    }
}

REGISTER_TWS_FUNC_STUB(app_anc_sync_stub) = {
    .func_id = TWS_FUNC_ID_ANC_SYNC,
    .func    = bt_tws_anc_sync,
};

void bt_tws_sync_anc(void)
{
    u8 data[2];
    data[0] = anc_mode_get();
    data[1] = 0;
    //r_printf("[master]anc_sync: %d, %d\n", data[0], data[1]);
    tws_api_send_data_to_slave(data, 2, TWS_FUNC_ID_ANC_SYNC);
}
#endif/*TCFG_AUDIO_ANC_ENABLE*/

void tws_conn_auto_test(void *p)
{
    cpu_reset();
}


#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
int tws_host_get_local_role()
{
    int role = 0;

    if (syscfg_read(VM_TWS_ROLE, &role, 1) == 1) {
        if (role == TWS_ROLE_SLAVE) {


            //printf("\n\n\nrole is slave\n\n\n");

            return TWS_ROLE_SLAVE;
        }
    }
    return TWS_ROLE_MASTER;
}
#endif


/*
 * tws事件状态处理函数
 */
int bt_tws_connction_status_event_handler(struct bt_event *evt)
{
    u8 ret = 0;
    u8 addr[4][6];
    int state;
    int pair_suss = 0;
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];
    u16 random_num = 0;
    char channel = 0;
    u8 mac_addr[6];

    log_info("tws-user: role= %d, phone_link_connection %d, reason=%d,event= %d\n",
             role, phone_link_connection, reason, evt->event);

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        log_info(" TWS_EVENT_CONNECTED \n");
        tws_event_connected(evt);
        ret = 1;
        break;

    case TWS_EVENT_SEARCH_TIMEOUT:
        log_info(" TWS_EVENT_SEARCH_TIMEOUT \n");
        tws_event_search_timeout(evt);
        ret = 1;
        break;
    case TWS_EVENT_CONNECTION_TIMEOUT:
        log_info(" TWS_EVENT_CONNECTION_TIMEOUT \n");
        tws_event_connection_timeout(evt);
        ret = 1;
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        log_info(" TWS_EVENT_CONNECTION_DETACH \n");
        tws_event_connection_detach(evt);
        ret = 1;
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        log_info(" TWS_EVENT_PHONE_LINK_DETACH \n");
        tws_event_phone_link_detach(evt);
        ret = 1;
        break;
    case TWS_EVENT_REMOVE_PAIRS:
        log_info(" TWS_EVENT_REMOVE_PAIRS \n");
        tws_event_remove_pairs(evt);
        ret = 1;
        break;
    case TWS_EVENT_SYNC_FUN_CMD:
        log_info(" TWS_EVENT_SYNC_FUN_CMD \n");
        tws_event_sync_fun_cmd(evt);
        ret = 1;
        break;
    case TWS_EVENT_SYNC_FUN_TRANID:
        log_info(" TWS_EVENT_SYNC_FUN_TRANID \n");
        tws_event_sync_fun_tranid(evt);
        ret = 1;
        break;
#if (TCFG_DEC2TWS_ENABLE)
    case TWS_EVENT_LOCAL_MEDIA_START:
        log_info(" TWS_EVENT_LOCAL_MEDIA_START \n");
        tws_event_local_media_start(evt);
        ret = 1;
        break;
    case TWS_EVENT_LOCAL_MEDIA_STOP:
        log_info(" TWS_EVENT_LOCAL_MEDIA_STOP \n");
        tws_event_local_media_stop(evt);
        ret = 1;
        break;
#endif
    case TWS_EVENT_ROLE_SWITCH:
        log_info(" TWS_EVENT_ROLE_SWITCH \n");
        tws_event_role_switch(evt);
        ret = 1;
        break;
    case TWS_EVENT_ESCO_ADD_CONNECT:
        log_info(" TWS_EVENT_ESCO_ADD_CONNECT \n");
        tws_event_esco_add_connect(evt);
        ret = 1;
        break;

    case TWS_EVENT_MODE_CHANGE:
        log_info("TWS_EVENT_MODE_CHANGE : %d", evt->args[0]);
        tws_event_mode_change_connect(evt);
        ret = 1;
        break;

    case TWS_EVENT_TONE_TEST:
        log_info("TWS_EVENT_TEST : %d", evt->args[0]);
        int tone_play_index(u8 index, u8 preemption);
        tone_play_index(evt->args[0], 1);
        ret = 1;
        break;
    }
    return ret;
}

void tws_cancle_all_noconn()
{

    bt_tws_delete_pair_timer();
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
}


bool get_tws_sibling_connect_state(void)
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return TRUE;
    }
    return FALSE;
}


bool get_tws_phone_connect_state(void)
{
    if (gtws.state & BT_TWS_PHONE_CONNECTED) {
        return TRUE;
    }
    return FALSE;
}



#define TWS_SNIFF_CNT_TIME      5000    //ms

static void bt_tws_enter_sniff(void *parm)
{
    gtws.sniff_timer = 0;
    extern void tws_tx_sniff_req(void);
    tws_tx_sniff_req();
}

void tws_sniff_controle_check_reset(void)
{
    if (gtws.sniff_timer) {
        sys_timer_modify(gtws.sniff_timer, TWS_SNIFF_CNT_TIME);
    }
}

void tws_sniff_controle_check_enable(void)
{
#if (CONFIG_BT_TWS_SNIFF == 0)
    return;
#endif

    if (update_check_sniff_en() == 0) {
        return;
    }

    if (gtws.sniff_timer == 0) {
        gtws.sniff_timer = sys_timeout_add(NULL, bt_tws_enter_sniff, TWS_SNIFF_CNT_TIME);
    }
}

void tws_sniff_controle_check_disable(void)
{
#if (CONFIG_BT_TWS_SNIFF == 0)
    return;
#endif

    if (gtws.sniff_timer) {
        sys_timeout_del(gtws.sniff_timer);
        gtws.sniff_timer = 0;
    }
}

extern void tws_link_check_for_ble_link_switch(void);
static u16 ble_try_switch_timer;
void tws_phone_ble_link_try_switch(void)
{
    //等待退出tws sniff
    if (!tws_in_sniff_state()) {
        //开始启动ble controller检查,满足切换条件进行链路切换;
        tws_link_check_for_ble_link_switch();
        sys_timeout_del(ble_try_switch_timer);
        ble_try_switch_timer = 0;
    }
}

void bt_tws_ble_link_switch(void)
{
#if CONFIG_BT_TWS_SNIFF
    tws_sniff_controle_check_disable();
    tws_tx_unsniff_req();
#endif
    ble_try_switch_timer = sys_timer_add(NULL, tws_phone_ble_link_try_switch, 20);
}

//未连接手机时的TWS主从切换
void bt_tws_role_switch()
{
    if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        return;
    }
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    puts("bt_tws_role_switch\n");
    bt_tws_delete_pair_timer();
    tws_api_cancle_wait_pair();
#if CONFIG_BT_TWS_SNIFF
    tws_sniff_controle_check_disable();
    tws_tx_unsniff_req();
    for (int i = 0; i < 20; i++) {
        if (!tws_in_sniff_state()) {
            tws_api_role_switch();
            break;
        }
        os_time_dly(4);
    }
#else
    tws_api_role_switch();
#endif
}

#endif
#endif
