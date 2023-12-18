#include "ui/ui.h"
#include "app_config.h"
#include "ui/ui_style.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "key_event_deal.h"
#include "asm/charge.h"
#include "audio_config.h"
#include "asm/charge.h"
#include "app_power_manage.h"

#if (TCFG_UI_ENABLE&&(CONFIG_UI_STYLE == STYLE_JL_SOUNDBOX))

#define STYLE_NAME  JL

extern void power_off_deal(struct sys_event *event, u8 step);
/************************************************************
                        录音模式主页窗口控件
************************************************************/
static int pc_mode_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        puts("\n***pc_mode_onchange***\n");
        break;
    case ON_CHANGE_RELEASE:
        puts("\n***pc_mode_release***\n");
        break;
    default:
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(ID_WINDOW_PC)
.onchange = pc_mode_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


/************************************************************
                         录音模式布局控件按键事件
 ************************************************************/
static int pc_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_text *text = (struct ui_text *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_SHOW_PROBE:
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

static int pc_layout_onkey(void *ctr, struct element_key_event *e)
{
    printf("pc_layout_onkey %d\n", e->value);
    switch (e->value) {
    case KEY_MENU:
        break;
    case KEY_OK:
        app_task_put_key_msg(KEY_MUSIC_PP, 0);
        break;
    case KEY_UP:
        app_task_put_key_msg(KEY_MUSIC_PREV, 0);
        break;
    case KEY_DOWN:
        app_task_put_key_msg(KEY_MUSIC_NEXT, 0);
        break;
    case KEY_VOLUME_INC:
    case KEY_VOLUME_DEC:
        ui_show(PC_VOL_LAYOUT);
        break;
    case KEY_MODE:
        UI_HIDE_CURR_WINDOW();
        ui_show_main(ID_WINDOW_SYS);
        break;
    case KEY_POWER_START:
    case KEY_POWER:
        power_off_deal(NULL, e->value - KEY_POWER_START);
        break;
    default:
        return false;
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(PC_LAYOUT)
.onchange = pc_layout_onchange,
 .onkey = pc_layout_onkey,
  .ontouch = NULL,
};


/************************************************************
                         音量设置布局控件
 ************************************************************/
static u16 vol_timer = 0;
static void pc_vol_lay_timeout(void *p)
{
    int id = (int)(p);
    if (ui_get_disp_status_by_id(id) == TRUE) {
        ui_hide(id);
    }
    vol_timer = 0;
}


static void vol_init(int id)
{
    struct unumber num;
    num.type = TYPE_NUM;
    num.numbs =  1;
    num.number[0] = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    ui_number_update_by_id(PC_VOL_NUM, &num);
}

static int pc_vol_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct layout *layout = (struct layout *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        if (!vol_timer) {
            vol_timer  = sys_timer_add((void *)layout->elm.id, pc_vol_lay_timeout, 3000);
        }
        break;

    case ON_CHANGE_FIRST_SHOW:
        ui_set_call(vol_init, 0);
        break;

    case ON_CHANGE_SHOW_POST:
        break;

    case ON_CHANGE_RELEASE:
        if (vol_timer) {
            sys_timeout_del(vol_timer);
            vol_timer = 0;
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}


static int pc_vol_layout_onkey(void *ctr, struct element_key_event *e)
{
    printf("pc_vol_layout_onkey %d\n", e->value);
    struct unumber num;
    u8 vol;

    if (vol_timer) {
        sys_timer_modify(vol_timer, 3000);
    }


    switch (e->value) {
    case KEY_MENU:
        break;

    case KEY_UP:
    case KEY_VOLUME_INC:
        app_task_put_key_msg(KEY_VOL_UP, 0);
        vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        num.type = TYPE_NUM;
        num.numbs =  1;
        num.number[0] = vol;
        ui_number_update_by_id(PC_VOL_NUM, &num);
        break;
    case KEY_DOWN:
    case KEY_VOLUME_DEC:
        app_task_put_key_msg(KEY_VOL_DOWN, 0);
        vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        num.type = TYPE_NUM;
        num.numbs =  1;
        num.number[0] = vol;
        ui_number_update_by_id(PC_VOL_NUM, &num);
        break;
    default:
        return false;
        break;
    }
    return true;
}

REGISTER_UI_EVENT_HANDLER(PC_VOL_LAYOUT)
.onchange = pc_vol_layout_onchange,
 .onkey = pc_vol_layout_onkey,
  .ontouch = NULL,
};

/************************************************************
                         电池控件事件
 ************************************************************/

static void battery_timer(void *priv)
{
    int  incharge = 0;//充电标志
    int id = (int)(priv);
    static u8 percent = 0;
    static u8 percent_last = 0;
    if (1 || get_charge_online_flag()) { //充电时候图标动态效果 get_charge_online_flag()在内置充电的时候获取是否正在充电,外置充电根据电路来编写接口
        if (percent > get_vbat_percent()) {
            percent = 0;
        }
        ui_battery_set_level_by_id(id, percent, incharge); //充电标志,ui可以显示不一样的图标
        percent += 20;
    } else {

        percent = get_vbat_percent();
        if (percent != percent_last) {
            ui_battery_set_level_by_id(id, percent, incharge); //充电标志,ui可以显示不一样的图标,需要工具设置
            percent_last = percent;
        }

    }
}

static int battery_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_battery *battery = (struct ui_battery *)ctr;
    static u32 timer = 0;
    int  incharge = 0;//充电标志

    switch (e) {
    case ON_CHANGE_INIT:
        ui_battery_set_level(battery, get_vbat_percent(), incharge);
        if (!timer) {
            timer = sys_timer_add((void *)battery->elm.id, battery_timer, 1 * 1000); //传入的id就是BT_BAT
        }
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (timer) {
            sys_timer_del(timer);
            timer = 0;
        }
        break;
    default:
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(PC_BAT)
.onchange = battery_onchange,
 .ontouch = NULL,
};


#endif
