#include "app_config.h"
#include "key_event_deal.h"
static int auto_test_timer1;
static int auto_test_timer2;
static int auto_test_timer3;
static int auto_test_timer4;
void broadcast_auto_test1(void *p)
{
    static u8 sw_flag = 0;
    if (!auto_test_timer1) {
        return;
    }
    if (!sw_flag) {
        sys_timer_modify(auto_test_timer1, 5 * 1000);
        sw_flag = 1;
    } else {
        sys_timer_modify(auto_test_timer1, 3 * 1000);
        sw_flag = 0;
    }
    app_task_put_key_msg(KEY_BROADCAST_SW, 0);
}
void broadcast_auto_test1_init(void)
{
    if (!auto_test_timer1) {
        auto_test_timer1 = sys_timer_add(NULL, broadcast_auto_test1, 10 * 1000);
    }
}

void broadcast_auto_test2(void *p)
{
    if (!auto_test_timer2) {
        return;
    }
    u32 rand_time = (rand32() % 20);
    if (rand_time < 2) {
        rand_time = 2;
    }
    sys_timer_modify(auto_test_timer2, rand_time * 1000);
    app_task_put_key_msg(KEY_BROADCAST_SW, 0);
}
void broadcast_auto_test2_init(void)
{
    if (!auto_test_timer2) {
        auto_test_timer2 = sys_timer_add(NULL, broadcast_auto_test2, 10 * 1000);
    }
}

void broadcast_auto_test3(void *p)
{
    static u8 sw_flag = 0;
    if (!auto_test_timer3) {
        return;
    }

    u32 rand_time = (rand32() % 15);
    if (rand_time < 2) {
        rand_time = 2;
    }
    sys_timer_modify(auto_test_timer3, rand_time * 1000);
    app_task_put_key_msg(KEY_CHANGE_MODE, 0);
}
void broadcast_auto_test3_init(void)
{
    if (!auto_test_timer3) {
        auto_test_timer3 = sys_timer_add(NULL, broadcast_auto_test3, 10 * 1000);
    }
}

void broadcast_auto_test4(void *p)
{
    static u8 sw_flag = 0;
    if (!auto_test_timer4) {
        return;
    }

    u32 rand_time = (rand32() % 15);
    if (rand_time < 2) {
        rand_time = 2;
    }
    sys_timer_modify(auto_test_timer4, rand_time * 1000);
    app_task_put_key_msg(KEY_MUSIC_PP, 0);
}
void broadcast_auto_test4_init(void)
{
    if (!auto_test_timer4) {
        auto_test_timer4 = sys_timer_add(NULL, broadcast_auto_test4, 10 * 1000);
    }
}



