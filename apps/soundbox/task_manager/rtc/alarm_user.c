#include "system/includes.h"
#include "rtc/alarm.h"
#include "common/app_common.h"
#include "system/timer.h"
#include "app_main.h"
#include "tone_player.h"
#include "app_task.h"
#include "btstack/avctp_user.h"


#if TCFG_APP_RTC_EN

#define ALARM_RING_MAX 50
volatile u16 g_alarm_ring_cnt = 0;
static u16 g_alarm_dly_tmr = 0;
static u16 g_ring_playing_timer = 0;

extern u8 get_switch_to_rtc_reason();

/*************************************************************
   此文件函数主要是用户主要修改的文件

void set_rtc_default_time(struct sys_time *t)
设置默认时间的函数

void alm_wakeup_isr(void)
闹钟到达的函数

void alarm_event_handler(struct sys_event *event, void *priv)
监听按键消息接口,应用于随意按键停止闹钟


int alarm_sys_event_handler(struct sys_event *event)
闹钟到达响应接口

void alarm_ring_start()
闹钟铃声播放接口


void alarm_stop(void)
闹钟铃声停止播放接口
**************************************************************/

static u8 rtc_can_run(void)
{
    u8 call_status = get_call_status();
    if ((call_status == BT_CALL_ACTIVE) ||
        (call_status == BT_CALL_OUTGOING) ||
        (call_status == BT_CALL_ALERT) ||
        (call_status == BT_CALL_INCOMING)) {
        //通话过程不允许开rtc
        return false;
    }

    return true;
}

void set_rtc_default_time(struct sys_time *t)
{
    t->year = 2019;
    t->month = 5;
    t->day = 5;
    t->hour = 18;
    t->min = 18;
    t->sec = 18;
}


__attribute__((weak))
u8 rtc_app_alarm_ring_play(u8 alarm_state)
{
    return 0;
}


void alm_wakeup_isr(void)
{
    if (!is_sys_time_online()) {
        alarm_active_flag_set(true);
    } else {
        alarm_active_flag_set(true);
        struct sys_event e;
        e.type = SYS_DEVICE_EVENT;
        e.arg  = (void *)DEVICE_EVENT_FROM_ALM;
        e.u.dev.event = DEVICE_EVENT_IN;
        e.u.dev.value = 0;
        sys_event_notify(&e);
    }
}

void alarm_ring_cnt_clear(void)
{
    g_alarm_ring_cnt = 0;
}

void alarm_stop(void)
{
    u32 rets_addr;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    printf("ALARM_STOP : 0x%x !!!\n", rets_addr);
    alarm_active_flag_set(0);
    alarm_ring_cnt_clear();
    rtc_app_alarm_ring_play(0);
}

void alarm_play_timer_del(void)
{
    if (g_ring_playing_timer) {
        sys_timeout_del(g_ring_playing_timer);
        g_ring_playing_timer = 0;
    }
}

static void  __alarm_ring_play(void *p)
{
    if (g_alarm_ring_cnt > 0) {
        u8 app = app_get_curr_task();
        if (app != APP_RTC_TASK) {
            alarm_stop();
            printf("...not in rtc task\n");
            return;
        }

        if (!tone_get_status()) {
            if (!rtc_app_alarm_ring_play(1)) {
                tone_play_by_path(tone_table[IDEX_TONE_NORMAL], 0);
                g_alarm_ring_cnt--;
            }
        }
        g_ring_playing_timer = sys_timeout_add(NULL, __alarm_ring_play, 500);
    } else {
        if (alarm_active_flag_get()) {
            alarm_stop();
            if (get_switch_to_rtc_reason() && (app_task_switch_back() <= 0)) {
                sys_enter_soft_poweroff();
            }
        }
    }
}

void alarm_ring_start()
{
    if (g_alarm_ring_cnt == 0) {
        g_alarm_ring_cnt = ALARM_RING_MAX;
        sys_timeout_add(NULL, __alarm_ring_play, 500);
    }
}

void alarm_event_handler(struct sys_event *event, void *priv)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        if (alarm_active_flag_get()) {
#if	(defined(SMARTBOX_ALARM_EX) && (SMARTBOX_ALARM_EX))
            extern u8 index_flag;
            index_flag = -1;
#endif
            alarm_stop();
            event->consumed = 1;//接管按键消息,app应用不会收到消息
            if (get_switch_to_rtc_reason() && (app_task_switch_back() <= 0)) {
                sys_enter_soft_poweroff();
            }
        }
        break;
    default:
        break;
    }
}

SYS_EVENT_HANDLER(SYS_KEY_EVENT, alarm_event_handler, 2);

static void alarm_dly_play_deal(u32 event_arg)
{
    u8 alm_flag = alarm_active_flag_get();
    if (rtc_can_run() || (!alm_flag)) {
        sys_timer_del(g_alarm_dly_tmr);
        g_alarm_dly_tmr = NULL;
        if (alm_flag) {
            app_task_switch_to(APP_RTC_TASK);
        }
#if (RCSP_MODE)
        u8 app  = APP_RTC_TASK ;
        extern void rcsp_update_dev_state(u32 event, void *param);
        /* printf("%s alarm_event:%x",__func__,param); */
        rcsp_update_dev_state(event_arg, &app);
#endif
    }
}

int alarm_sys_event_handler(struct sys_event *event)
{
    struct application *cur;
    if ((u32)event->arg == DEVICE_EVENT_FROM_ALM) {
        if (event->u.dev.event == DEVICE_EVENT_IN) {
            printf("alarm_sys_event_handler\n");
            alarm_update_info_after_isr();

            u8 app = app_get_curr_task();
            if (app == APP_RTC_TASK) {
                alarm_ring_start();
                return false;
            }

            if (!rtc_can_run()) {
                if (g_alarm_dly_tmr == 0) {
                    g_alarm_dly_tmr = sys_timer_add((event->arg), alarm_dly_play_deal, 500);
                }
                return false;
            }
            return true;
        }
    }

    return false;
}

#endif

