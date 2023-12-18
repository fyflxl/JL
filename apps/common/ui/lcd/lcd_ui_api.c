#include "app_config.h"
#include "includes.h"
#include "ui/ui_api.h"
#include "ui/ui.h"
#include "res/mem_var.h"
#include "typedef.h"
#include "clock_cfg.h"
#include "app_task.h"
#include "btstack/avctp_user.h"
#include "key_event_deal.h"
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
#include "ui/ui_page_switch.h"
#endif

#if (TCFG_LUA_ENABLE)
#include "lua/lua.h"
#include "lua_ui/lua_sys.h"
#endif /* #if (TCFG_LUA_ENABLE) */

#if (TCFG_UI_ENABLE && (TCFG_SPI_LCD_ENABLE || TCFG_LCD_OLED_ENABLE) && (!TCFG_SIMPLE_LCD_ENABLE))

#include "ui/ui_sys_param.h"
#include "ui/res_config.h"

#include "lcd_ui_return_page.c"

#define CURR_WINDOW_MAIN (0)

#define UI_NO_ARG (-1)
#define UI_TASK_NAME "ui"
static DEFINE_SPINLOCK(lock);

enum {
    UI_MSG_OTHER,
    UI_MSG_KEY,
    UI_MSG_TOUCH,
    UI_MSG_SHOW,
    UI_MSG_HIDE,
    UI_TIME_TOUCH_RESUME,
    UI_TIME_TOUCH_SUPEND,
    UI_MSG_EXIT,
};


struct ui_server_env {
    u8 init: 1;
    u8 key_lock : 1;
    OS_SEM start_sem;
    void (*touch_event_call)(void *priv);
    int touch_event_interval;
    u16 timer;
    u16 shut_down_time;
};

void lcd_bl_open();
void ui_backlight_open(u8 recover_cur_page);
void ui_backlight_close(void);
int lcd_sleep_ctrl(u8 enter);
int lcd_sleep_status();
u8 ui_auto_shut_down_disable(void);

void imb_task_protect(int enable);

extern void wdt_clr(void);
extern u16 get_backlight_time_item(void);
/* extern u8 get_standby_watch_en(); */

static struct ui_server_env __ui_display = {0};
static u8 lcd_bl_idle_flag = 0;
static u32 mem_id[8] = {0};


void ui_auto_shut_down_re_run(void);

/* static void ui_auto_shut_down_restart(void); */
__attribute__((weak)) u8 get_standby_watch_en()
{
    return 0;
}

//进入屏保
__attribute__((weak)) void ui_screen_saver(void *p)
{
    ui_backlight_close();
}

//退出屏保
__attribute__((weak)) void ui_screen_recover(u8 recover_cur_page)
{
    ui_backlight_open(recover_cur_page);
}

__attribute__((weak)) u8 get_screen_saver_status()
{
    return lcd_sleep_status();
}

__attribute__((weak)) void set_screen_saver_status(u8 status)
{

}

__attribute__((weak)) void auto_goto_dial_timer_del()
{

}

__attribute__((weak)) void ui_sysinfo_init()
{

}

__attribute__((weak)) void sys_param_init(void)
{

}

__attribute__((weak)) void lcd_bl_open()
{

}

//=================================================================================//
// @brief: 对id进行remap 操作，在失败时候可以映射到内置flash的升级界面
//=================================================================================//
__attribute__((weak)) int ui_id2remap(int id)
{
    return id;
}

u32 *get_ui_mem_id()
{
    return mem_id;
}

u32 get_ui_mem_id_size()
{
    return sizeof(mem_id) / sizeof(mem_id[0]);
}



u8 get_ui_init_status()
{
    return __ui_display.init;
}

int key_is_ui_takeover()
{
    return __ui_display.key_lock;
}

void key_ui_takeover(u8 on)
{
    __ui_display.key_lock = !!on;
}

static int post_ui_msg(int *msg, u8 len)
{
    int err = 0;
    int count = 0;
    if (!__ui_display.init) {
        return -1;
    }
__retry:
    err =  os_taskq_post_type(UI_TASK_NAME, msg[0], len - 1, &msg[1]);

    if (cpu_in_irq() || cpu_irq_disabled()) {
        return err;
    }

    if (err) {
        if (!strcmp(os_current_task(), UI_TASK_NAME)) {
            return err;
        }
        if (count > 20) {
            return -1;
        }
        count++;
        os_time_dly(1);
        goto __retry;
    }
    return err;
}


//=================================================================================//
// @brief: 显示主页 应用于非ui线程显示主页使用
//有针对ui线程进行处理，允许用于ui线程等同ui_show使用
//=================================================================================//
int ui_show_main(int id)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    printf("__func__ %s %x\n", __func__, rets);

    OS_SEM sem;// = zalloc(sizeof(OS_SEM));
    int msg[8];
    int i;


    if (id <= 0 && id > -(int)(sizeof(mem_id) / sizeof(mem_id[0]))) {
        if (mem_id[7 + id]) {
            id = mem_id[7 + id];
        } else {
            printf("[ui_show_main] id %d is invalid.\n", id);
            return 0;
        }
    }

    //else {
    if (mem_id[7] != id) {
        for (i = 1; i <= 7; i++) {
            mem_id[i - 1] = mem_id[i];
        }
        mem_id[7] = id;
    }
    //}



    printf("[ui_show_main] id = 0x%x\n", id);

    auto_goto_dial_timer_del();
    /* if (!strcmp(os_current_task(), UI_TASK_NAME)) { */
    /*     ui_show(id); */
    /*     return 0; */
    /* } */
    u8 need_pend = 0;
    if (!(cpu_in_irq() || cpu_irq_disabled())) {
        if (strcmp(os_current_task(), UI_TASK_NAME)) {
            need_pend = 1;
        }
    }
    msg[0] = UI_MSG_SHOW;
    msg[1] = id;
    msg[2] = 0;

    if (need_pend) {
        os_sem_create(&sem, 0);
        msg[2] = (int)&sem;
    }

    if (!post_ui_msg(msg, 3)) {
        if (need_pend) {
            os_sem_pend(&sem, 0);
        }
    }


    if (get_screen_saver_status()) {
        printf("screen-saver ... ui_show_main_now\n");
        ui_screen_recover(0);
        lcd_bl_idle_flag = 0;
    }


    return 0;
}


//=================================================================================//
// @brief: 关闭主页 应用于非ui线程关闭使用
//有针对ui线程进行处理，允许用于ui线程等同ui_hide使用
//=================================================================================//
int ui_hide_main(int id)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    printf("__func__ %s %x\n", __func__, rets);


    OS_SEM sem;// = zalloc(sizeof(OS_SEM));
    int msg[8];

    u8 need_pend = 0;;
    if (!(cpu_in_irq() || cpu_irq_disabled())) {
        if (strcmp(os_current_task(), UI_TASK_NAME)) {
            need_pend = 1;
        }
    }
    /* if (!strcmp(os_current_task(), UI_TASK_NAME)) { */
    /*     if (CURR_WINDOW_MAIN == id) { */
    /*         id = ui_get_current_window_id(); */
    /*     } */
    /*     ui_hide(id); */
    /*     return 0; */
    /* } */
    msg[0] = UI_MSG_HIDE;
    msg[1] = id;
    msg[2] = 0;

    if (need_pend) {
        os_sem_create(&sem, 0);
        msg[2] = (int)&sem;
    }

    if (!post_ui_msg(msg, 3)) {
        if (need_pend) {
            os_sem_pend(&sem, 0);
        }
    }
    return 0;
}

//=================================================================================//
// @brief: 关闭当前主页 灵活使用自动判断当前主页进行关闭
//用户可以不用关心当前打开的主页,特别适用于一个任务使用了多个主页的场景
//=================================================================================//
int ui_hide_curr_main()
{
    return ui_hide_main(CURR_WINDOW_MAIN);
}


//=================================================================================//
// @brief: 应用往ui发送消息，ui主页需要注册回调函数关闭当前主页
// //消息发送方法demo： UI_MSG_POST("test1:a=%4,test2:bcd=%4,test3:efgh=%4,test4:hijkl=%4", 1,2,3,4);
// 往test1、test2、test3、test4发送了字符为a、bcd、efgh、hijkl，附带变量为1、2、3、4
// 每次可以只发一个消息，也可以不带数据例如:UI_MSG_POST("test1")
//=================================================================================//
int ui_server_msg_post(const char *msg, ...)
{
    int ret = 0;
    int argv[9];
    argv[0] = UI_MSG_OTHER;
    argv[1] = (int)msg;
    va_list argptr;
    va_start(argptr, msg);
    for (int i = 2; i < 7; i++) {
        argv[i] = va_arg(argptr, int);
    }
    va_end(argptr);
    return post_ui_msg(argv, 9);
}

//=================================================================================//
// @brief: 应用往ui发送key消息，由ui控件分配
//=================================================================================//
volatile u16 touch_msg_counter = 0;
int ui_key_msg_post(int key)
{
    u8 count = 0;
    int msg[8];

    if (key >= 0x80) {
        printf("key need <= 0x80");
        return -1;
    }

    msg[0] = UI_MSG_KEY;
    msg[1] = key;
    /* touch_msg_counter++; */
    return post_ui_msg(msg, 2);
}


//=================================================================================//
// @brief: 应用往ui发送触摸消息，由ui控件分配
//=================================================================================//
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
int ui_touch_msg_post(struct touch_event *event)
{
    int msg[8];
    int i = 0;
    int err;

    ui_auto_shut_down_re_run();
    if (get_screen_saver_status()) {
        ui_screen_recover(1);
        return 0;
    }
    msg[0] = UI_MSG_TOUCH;
    msg[1] = (int)NULL;
    memcpy(&msg[2], event, sizeof(struct touch_event));

    while (1) {
        spin_lock(&lock);
        touch_msg_counter++;
        spin_unlock(&lock);
        err = post_ui_msg(msg, sizeof(struct touch_event) / 4 + 2);
        if (err) {
            printf("post_ui_msg err! %d, %d, %d\n", event->event, event->x, event->y);
            spin_lock(&lock);
            touch_msg_counter--;
            spin_unlock(&lock);

            /* if ((event->event == ELM_EVENT_TOUCH_DOWN) || (event->event == ELM_EVENT_TOUCH_UP)) { */
            /*     os_time_dly(1); */
            /*     continue; */
            /* } */

            return -1;
        }
        break;
    }

    return 0;
}
#else
int ui_touch_msg_post(struct touch_event *event)
{
    int msg[8];
    int i = 0;
    int err;

    ui_auto_shut_down_re_run();
    if (get_screen_saver_status()) {
        ui_screen_recover(1);
        return 0;
    } else {
        ui_auto_shut_down_re_run();
    }
    msg[0] = UI_MSG_TOUCH;
    msg[1] = (int)NULL;
    memcpy(&msg[2], event, sizeof(struct touch_event));
    if (touch_msg_counter < 16) {
        spin_lock(&lock);
        touch_msg_counter++;
        spin_unlock(&lock);
        err = post_ui_msg(msg, sizeof(struct touch_event) / 4 + 2);
        if (err) {
            printf("post_ui_msg err! %d, %d, %d\n", event->event, event->x, event->y);
            spin_lock(&lock);
            touch_msg_counter--;
            spin_unlock(&lock);
            return -1;
        }
    } else {
        printf("touch msg drop! %d, %d, %d\n", event->event, event->x, event->y);
        return -1;
    }
    return 0;
}
#endif

//=================================================================================//
// @brief: 应用往ui发送触摸消息，由ui控件分配
//=================================================================================//
int ui_touch_msg_post_withcallback(struct touch_event *event, void (*cb)(u8 finish))
{
    int msg[8];
    int i = 0;
    int err;

    msg[0] = UI_MSG_TOUCH;
    msg[1] = (int)cb;
    memcpy(&msg[2], event, sizeof(struct touch_event));
    if (touch_msg_counter < 16) {
        spin_lock(&lock);
        touch_msg_counter++;
        spin_unlock(&lock);
        err = post_ui_msg(msg, sizeof(struct touch_event) / 4 + 2);
        if (err) {
            printf("post_ui_msg err! %d, %d, %d\n", event->event, event->x, event->y);
            spin_lock(&lock);
            touch_msg_counter--;
            spin_unlock(&lock);
            return -1;
        }
    } else {
        printf("touch msg drop! %d, %d, %d\n", event->event, event->x, event->y);
        return -1;
    }
    return 0;
}



const char *str_substr_iter(const char *str, char delim, int *iter)
{
    const char *substr;
    ASSERT(str != NULL);
    substr = str + *iter;
    if (*substr == '\0') {
        return NULL;
    }
    for (str = substr; *str != '\0'; str++) {
        (*iter)++;
        if (*str == delim) {
            break;
        }
    }
    return substr;
}


static int do_msg_handler(const char *msg, va_list *pargptr, int (*handler)(const char *, u32))
{
    int ret = 0;
    int width;
    int step = 0;
    u32 arg = 0x5678;
    int m[16];
    char *t = (char *)&m[3];
    va_list argptr = *pargptr;

    if (*msg == '\0') {
        handler((const char *)' ', 0);
        return 0;
    }

    while (*msg && *msg != ',') {
        switch (step) {
        case 0:
            if (*msg == ':') {
                step = 1;
            }
            break;
        case 1:
            switch (*msg) {
            case '%':
                msg++;
                if (*msg >= '0' && *msg <= '9') {
                    if (*msg == '1') {
                        arg = va_arg(argptr, int) & 0xff;
                    } else if (*msg == '2') {
                        arg = va_arg(argptr, int) & 0xffff;
                    } else if (*msg == '4') {
                        arg = va_arg(argptr, int);
                    }
                } else if (*msg == 'p') {
                    arg = va_arg(argptr, int);
                }
                m[2] = arg;
                handler((char *)&m[3], m[2]);
                t = (char *)&m[3];
                step = 0;
                break;
            case '=':
                *t = '\0';
                break;
            case ' ':
                break;
            default:
                *t++ = *msg;
                break;
            }
            break;
        }
        msg++;
    }
    *pargptr = argptr;
    return ret;
}

#if (TCFG_LUA_ENABLE)

static int do_msg_handler_by_lua(const char *msg, va_list *pargptr, LuaRegHandler_def *hdl)
{
    int ret = 0;
    int width;
    int step = 0;
    u32 arg = 0x5678;
    int m[16];
    char *t = (char *)&m[3];
    va_list argptr = *pargptr;

    if (*msg == '\0') {
        /* handler((const char *)' ', 0); */
        lua_pushstring(hdl->lua, (const char *)' ');
        lua_pushinteger(hdl->lua, 0);
        hdl->arg_number = 2;
        callLuaHandler(hdl);
        return 0;
    }

    while (*msg && *msg != ',') {
        switch (step) {
        case 0:
            if (*msg == ':') {
                step = 1;
            }
            break;
        case 1:
            switch (*msg) {
            case '%':
                msg++;
                if (*msg >= '0' && *msg <= '9') {
                    if (*msg == '1') {
                        arg = va_arg(argptr, int) & 0xff;
                    } else if (*msg == '2') {
                        arg = va_arg(argptr, int) & 0xffff;
                    } else if (*msg == '4') {
                        arg = va_arg(argptr, int);
                    }
                } else if (*msg == 'p') {
                    arg = va_arg(argptr, int);
                }
                m[2] = arg;
                /* printf("put:%s,%d \n", (char *)&m[3], m[2]); */
                /* handler((char *)&m[3], m[2]); */
                lua_pushstring(hdl->lua, (const char *)&m[3]);
                lua_pushinteger(hdl->lua, m[2]);
                hdl->arg_number = 2;
                callLuaHandler(hdl);
                t = (char *)&m[3];
                step = 0;
                break;
            case '=':
                *t = '\0';
                break;
            case ' ':
                break;
            default:
                *t++ = *msg;
                break;
            }
            break;
        }
        msg++;
    }
    *pargptr = argptr;
    return ret;
}

#endif /* #if (TCFG_LUA_ENABLE) */

int ui_message_handler(int id, const char *msg, va_list argptr)
{
#if (TCFG_LUA_ENABLE)
    int iter = 0;
    const char *str;
    /* printf("\n\n msg:%s \n", msg); */
    while ((str = str_substr_iter(msg, ',', &iter)) != NULL) {
        int str_len = 0;
        LuaRegHandler_def *hdl = getLuaHandler(str, &str_len);
        /* printf("str:%s, len:%d \n", str, str_len); */
        if (hdl) {
            do_msg_handler_by_lua(str + str_len, &argptr, hdl);
        }
    }
#else /* #if (TCFG_LUA_ENABLE) */
    int iter = 0;
    const char *str;
    const struct uimsg_handl *handler;
    struct window *window = (struct window *)ui_core_get_element_by_id(id);
    if (!window || !window->private_data) {
        return -EINVAL;
    }
    handler = (const struct uimsg_handl *)window->private_data;
    while ((str = str_substr_iter(msg, ',', &iter)) != NULL) {
        for (; handler->msg != NULL; handler++) {
            if (!memcmp(str, handler->msg, strlen(handler->msg))) {
                do_msg_handler(str + strlen(handler->msg), &argptr, handler->handler);
                break;
            }
        }
    }
#endif /* #if (TCFG_LUA_ENABLE) */

    return 0;
}

void ui_touch_timer_delete()
{
    if (!__ui_display.init) {
        return;
    }

    int msg[8];
    msg[0] = UI_TIME_TOUCH_SUPEND;
    post_ui_msg(msg, 1);

#if UI_WATCH_RES_ENABLE
    local_irq_disable();
    if (!__ui_display.timer) {
        local_irq_enable();
        return;
    }
    local_irq_enable();
#endif

}


void ui_touch_timer_start()
{
    if (!__ui_display.init) {
        return;
    }

    int msg[8];
    msg[0] = UI_TIME_TOUCH_RESUME;
    post_ui_msg(msg, 1);

    local_irq_disable();
    if (__ui_display.timer) {
        local_irq_enable();
        return;
    }
    local_irq_enable();
}

void ui_set_touch_event(void (*touch_event)(void *priv), int interval)
{
    __ui_display.touch_event_call = touch_event;
    __ui_display.touch_event_interval = interval;
}


int ui_auto_shut_down_timer = 0;
static u8 open_flag = 0;
static u8 lcd_need_keep_open = 0;
u8 get_ui_open_flag()
{
    return open_flag;
}

void set_ui_open_flag(u8 flag)
{
    open_flag = flag;
}

void set_lcd_keep_open_flag(u8 flag)
{
    lcd_need_keep_open = flag;
}

void ui_backlight_open(u8 recover_cur_page)
{

    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));

    lcd_bl_idle_flag = 0;
    if (!__ui_display.init) {
        return;
    }

    if (open_flag) {
        return;
    }

    printf("__func__ %s %x %x %x\n", __func__, rets, get_screen_saver_status(), recover_cur_page);
    if (get_screen_saver_status()) {

        open_flag = 1;


        /* UI_MSG_POST("ui_lp_cb:exit=%4", 1); */
        /* sys_key_event_enable();//关闭按键 */
        ui_auto_shut_down_enable();
        //lcd_sleep_ctrl(0);//屏幕退出低功耗
#if UI_WATCH_RES_ENABLE
        if (recover_cur_page) {
            ui_show_main(0);//恢复当前ui
        }
#endif
        ui_touch_timer_start();
    }
}

void ui_backlight_close(void)
{

    if (!__ui_display.init) {
        return;
    }


    open_flag = 0;

    if (!lcd_sleep_status()) {
        /* UI_MSG_POST("ui_lp_cb:enter=%4", 0); */
        /* sys_key_event_disable();//关闭按键 */
#if UI_WATCH_RES_ENABLE
        ui_hide_curr_main();//关闭当前页面
#endif
        ui_auto_shut_down_disable();
        ui_touch_timer_delete();
    }
    lcd_bl_idle_flag = 1;
}

#if TCFG_UI_SHUT_DOWN_TIME
void ui_set_shut_down_time(u16 time)
{
    __ui_display.shut_down_time = time;
}
int ui_get_shut_down_time()
{
    return  __ui_display.shut_down_time;
}
#endif



void ui_auto_shut_down_enable(void)
{
#if TCFG_UI_SHUT_DOWN_TIME
    if (!__ui_display.init) {
        return;
    }

    /* set_lcd_keep_open_flag(0); */
    if (get_call_status() != BT_CALL_HANGUP) {
        return;
    }

    if (lcd_need_keep_open) {
        return;
    }

    if (ui_auto_shut_down_timer == 0) {
        if (__ui_display.shut_down_time == 0) {
            __ui_display.shut_down_time = 10;
        }
        if (__ui_display.shut_down_time > 20) {
            /* __ui_display.shut_down_time = 20; */
        }
        g_printf("ui shut down time:%d", __ui_display.shut_down_time);
        ui_auto_shut_down_timer = sys_timeout_add(NULL, ui_screen_saver, __ui_display.shut_down_time * 1000);
    }
#endif
}

u8 ui_auto_shut_down_disable(void)
{
#if TCFG_UI_SHUT_DOWN_TIME
    /* set_lcd_keep_open_flag(1); */
    if (ui_auto_shut_down_timer) {
        sys_timeout_del(ui_auto_shut_down_timer);
        ui_auto_shut_down_timer = 0;
        return true;
    }
#endif
    return false;
}

void ui_auto_shut_down_modify(void)
{
#if TCFG_UI_SHUT_DOWN_TIME
    if (ui_auto_shut_down_timer) {
        sys_timer_modify(ui_auto_shut_down_timer, __ui_display.shut_down_time * 1000);
    }
#endif
}

void ui_auto_shut_down_re_run(void)
{
#if TCFG_UI_SHUT_DOWN_TIME
    if (ui_auto_shut_down_timer) {
        sys_timer_re_run(ui_auto_shut_down_timer);
    }
#endif
}

static u8 lcd_bl_idle_query(void)
{
    return lcd_bl_idle_flag;
}
REGISTER_LP_TARGET(lcd_backlight_lp_target) = {
    .name = "lcd_backlight",
    .is_idle = lcd_bl_idle_query,
};


#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
int g_cur_left;
volatile static int scroll_time = 0;
__attribute__((weak)) void page_auto_scroll(void *priv)
{
    scroll_time++;
    if (scroll_time >= 5) {
        printf("Attention : wait TOUCH_UP timeout, TOUCH_UP may be lost!\n");
        struct element_touch_event *e = (struct element_touch_event *)priv;
        e->event = ELM_EVENT_TOUCH_UP;
        ui_card_ontouch(e);
        scroll_time = 0;
    }
}

int ui_card_ontouch(struct element_touch_event *e)
{
    static struct position pos = {0};
    int xoffset, yoffset;
    struct element *p;
    static u8 move_en = 1;
    static int slide_en = 0;
    static int slide_cnt = 0;
    static int startx = 0, starty = 0, endx = 0, endy = 0;
    struct ui_page *page;
    static int first_result = -1;
    static int result = -1;
    static int page_in_touchdown = 0;
    static u16 ui_move_timer = 0;


    int mode = 0;
    struct element *curr_elm = 0;

    //printf("ui_card_ontouch %d, (%d, %d)\n", e->event, e->pos.x, e->pos.y);
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        page_in_touchdown = ui_get_current_window_id();
        page = ui_page_search(ui_get_current_window_id());
        if (!page) {
            break;
        }
        if (slide_en) {
            ui_page_move_auto(ui_get_current_window_id(), mode);
            slide_en = false;
        }

        list_for_each_child_element(p, ui_core_get_root()) {
            ASSERT(p->id == ui_get_current_window_id());
        }
        pos.x = e->pos.x;
        pos.y = e->pos.y;
        startx = e->pos.x;
        starty = e->pos.y;
        first_result = -1;
        result = -1;

        curr_elm = ui_core_get_element_by_id(ui_get_current_window_id());
        g_cur_left = curr_elm->css.left;

        putchar('D');
        break;

    case ELM_EVENT_TOUCH_MOVE:
        page = ui_page_search(ui_get_current_window_id());
        if (!page) {
            break;
        }
        if (page_in_touchdown != page->id) {
            return 0;
        }
        xoffset = e->pos.x - pos.x;
        yoffset = e->pos.y - pos.y;
        pos.x = e->pos.x;
        pos.y = e->pos.y;
        putchar('M');

        result = get_direction(startx, starty, e->pos.x, e->pos.y);
        if ((first_result == -1) && (result > 0)) {
            first_result = result;
        }

        move_en = ui_page_move_callback_run();

        if ((move_en != 0) && (xoffset != 0) && ((result == DIRECTION_LEFT) || (result == DIRECTION_RIGHT)) && ((first_result == DIRECTION_LEFT) || (first_result == DIRECTION_RIGHT))) {
            ui_page_move(ui_get_current_window_id(), xoffset, yoffset, mode);
            slide_en = true;
            /* if (!ui_move_timer) { */
            /* ui_move_timer = sys_timer_add(e, page_auto_scroll, 20); */
            /* } */
            scroll_time = 0;
            return 0;
        }
        break;

    case ELM_EVENT_TOUCH_L_MOVE:
    case ELM_EVENT_TOUCH_R_MOVE:
        /* case ELM_EVENT_TOUCH_U_MOVE: */
        /* case ELM_EVENT_TOUCH_D_MOVE: */
        if (!ENABLE_LUA_VIRTUAL_MACHINE) {
            page = ui_page_search(ui_get_current_window_id());
            if (page) {
                return 0;
            }
        }
        break;

    case ELM_EVENT_TOUCH_UP:
        page = ui_page_search(ui_get_current_window_id());
        if (!page) {
            break;
        }

        putchar('U');
        if (slide_en) {
            /* if (ui_move_timer) { */
            /* sys_timer_del(ui_move_timer); */
            /* ui_move_timer = 0; */
            /* } */
            ui_page_move_auto(ui_get_current_window_id(), mode);
            slide_en = false;
            return 0;
        }
        break;
    }

    if (slide_en) {
        /* printf("slide_en is true!!!\n"); */
        return 0;
    }

    return ui_event_ontouch(e);
}

#endif

struct ui_msg {
    struct list_head entry;
    struct touch_event touch;
    void(*msg_hook)(u8);
};

struct ui_msg msg_handle;

static void ui_task(void *p)
{
    int msg[32];
    int ret;
    int bl_ctrl;
    struct element_key_event e = {0};

#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
    ui_page_init();
#if (TCFG_LUA_ENABLE)
    /* lua状态机初始化 */
    extern void lua_late_init(void);
    lua_late_init();
#endif /* #if (TCFG_LUA_ENABLE) */
    /* 左右滑动抬手后自动滑屏的参数调节 */
    ui_page_set_param(2000, // 判断滑动距离超过 n/10000 坐标后才滑动
                      2000, // 每次自动滑动 n/10000 屏幕的距离
                      1); // 滑动的间隔 n*10 ms
#endif

    os_time_dly(2);

    //extern void ui_core_event_debug();
    //ui_core_event_debug();

#if TCFG_NOR_VM
#include "ui_vm/ui_vm.h"
    /* flash_message_init("ui_vm", 30); */
    /* flash_weather_init("ui_vm"); */
    extern void flash_message_cfg_init();
    flash_message_cfg_init();
#endif

    ui_framework_init(p);
    sys_param_init();
    ui_sysinfo_init();


#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
    struct ui_style style;
    style.file = RES_PATH"JL/JL.sty";
    ret =  ui_set_style_file(&style);
    if (ret) {
        printf("ui task fail!\n");
        return;
    }
#endif
    __ui_display.init = 1;
    os_sem_post(&(__ui_display.start_sem));
    struct touch_event *touch;
    struct element_touch_event t;

    if (__ui_display.touch_event_call && __ui_display.touch_event_interval) {
        __ui_display.timer = sys_timer_add((void *)NULL, __ui_display.touch_event_call, __ui_display.touch_event_interval); //注册按键扫描定时器
    }

    INIT_LIST_HEAD(&msg_handle.entry);

    u8 touch_move_msg = 0;
    struct ui_msg ui_msg_move;
    u8 msg_filter_en = false;
    u8 tmsg_cnt = 0;
    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (ret != OS_TASKQ) {
            continue;
        }
        if (tmsg_cnt++ > 30) {
            tmsg_cnt = 0;
            wdt_clr();
        }

        msg_filter_en = false;
        if (msg[0] == UI_MSG_TOUCH) {
            touch = (struct touch_event *)&msg[2];
            if (touch->event == ELM_EVENT_TOUCH_MOVE) {
                touch_move_msg++;
                ui_msg_move.msg_hook = (void (*)(u8))msg[1];
                memcpy((u8 *)&ui_msg_move.touch, (u8 *)&msg[2], sizeof(struct touch_event));
            } else {
                if (touch_move_msg) {
                    struct ui_msg *ui_msg = (struct ui_msg *)malloc(sizeof(struct ui_msg));
                    ui_msg->msg_hook = ui_msg_move.msg_hook;
                    memcpy((u8 *)&ui_msg->touch, (u8 *)&ui_msg_move.touch, sizeof(struct touch_event));
                    list_add_tail(&ui_msg->entry, &msg_handle.entry);
                    touch_move_msg = 0;
                }
                struct ui_msg *ui_msg = (struct ui_msg *)malloc(sizeof(struct ui_msg));
                ui_msg->msg_hook = (void (*)(u8))msg[1];
                memcpy((u8 *)&ui_msg->touch, (u8 *)&msg[2], sizeof(struct touch_event));
                list_add_tail(&ui_msg->entry, &msg_handle.entry);
            }
            spin_lock(&lock);
            touch_msg_counter--;
            spin_unlock(&lock);
            if (touch_msg_counter) {
                continue;
            } else {
                if (touch_move_msg) {
                    struct ui_msg *ui_msg = (struct ui_msg *)malloc(sizeof(struct ui_msg));
                    ui_msg->msg_hook = ui_msg_move.msg_hook;
                    memcpy((u8 *)&ui_msg->touch, (u8 *)&ui_msg_move.touch, sizeof(struct touch_event));
                    list_add_tail(&ui_msg->entry, &msg_handle.entry);
                    touch_move_msg = 0;
                }

                while (!list_empty(&msg_handle.entry)) {
                    struct ui_msg *ui_msg = list_first_entry(&msg_handle.entry, struct ui_msg, entry);
                    touch = (struct touch_event *)&ui_msg->touch;
                    t.event = touch->event;
                    t.pos.x = touch->x;
                    t.pos.y = touch->y;
                    if (msg_filter_en && ((t.event == ELM_EVENT_TOUCH_DOWN)
                                          || (t.event == ELM_EVENT_TOUCH_MOVE)
                                          || (t.event == ELM_EVENT_TOUCH_L_MOVE)
                                          || (t.event == ELM_EVENT_TOUCH_R_MOVE)
                                          || (t.event == ELM_EVENT_TOUCH_UP)
                                         )) {
                        list_del(&ui_msg->entry);
                        free(ui_msg);
                        continue;
                    }

                    if (tmsg_cnt++ > 5) {
                        tmsg_cnt = 0;
                        wdt_clr();
                    }
                    if (ui_msg->msg_hook) {
                        ((void(*)(u8))ui_msg->msg_hook)(0);
                    }
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
                    ui_card_ontouch(&t);
#else
                    ui_event_ontouch(&t);
#endif
                    if (ui_msg->msg_hook) {
                        ((void(*)(u8))ui_msg->msg_hook)(1);
                    }
                    list_del(&ui_msg->entry);
                    free(ui_msg);
                    if (t.event == ELM_EVENT_TOUCH_UP) {
                        msg_filter_en = true;
                    }
                }
                continue;
            }
        } else {//其他消息直接处理
            if (touch_move_msg) {
                struct ui_msg *ui_msg = (struct ui_msg *)malloc(sizeof(struct ui_msg));
                ui_msg->msg_hook = ui_msg_move.msg_hook;
                memcpy((u8 *)&ui_msg->touch, (u8 *)&ui_msg_move.touch, sizeof(struct touch_event));
                list_add_tail(&ui_msg->entry, &msg_handle.entry);
                touch_move_msg = 0;
            }
            while (!list_empty(&msg_handle.entry)) {
                struct ui_msg *ui_msg = list_first_entry(&msg_handle.entry, struct ui_msg, entry);
                touch = (struct touch_event *)&ui_msg->touch;
                t.event = touch->event;
                t.pos.x = touch->x;
                t.pos.y = touch->y;

                if (tmsg_cnt++ > 5) {
                    tmsg_cnt = 0;
                    wdt_clr();
                }
                if (ui_msg->msg_hook) {
                    ((void(*)(u8))ui_msg->msg_hook)(0);
                }
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
                ui_card_ontouch(&t);
#else
                ui_event_ontouch(&t);
#endif
                if (ui_msg->msg_hook) {
                    ((void(*)(u8))ui_msg->msg_hook)(1);
                }
                list_del(&ui_msg->entry);
                free(ui_msg);
            }
        }

        switch (msg[0]) { //action
        case UI_MSG_EXIT:
            os_sem_post((OS_SEM *)msg[1]);
            os_time_dly(10000);
            break;
        case UI_MSG_OTHER:
            ui_message_handler(ui_get_current_window_id(), (const char *)msg[1], (void *)&msg[2]);
            break;
        case UI_MSG_KEY:
            e.value = msg[1];
            ui_event_onkey(&e);
            break;
        case UI_MSG_TOUCH:
            touch = (struct touch_event *)&msg[2];
            t.event = touch->event;
            t.pos.x = touch->x;
            t.pos.y = touch->y;
            /* printf("event = %d %d %d \n", t.event, t.pos.x, t.pos.y); */
            if (msg[1]) {
                ((void(*)(u8))msg[1])(0);
            }
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
            ui_card_ontouch(&t);
#else
            ui_event_ontouch(&t);
#endif
            if (msg[1]) {
                ((void(*)(u8))msg[1])(1);
            }

            break;
        case UI_MSG_SHOW:
            bl_ctrl = false;
            if (lcd_sleep_status()) {
                lcd_sleep_ctrl(false);//退出低功耗
                bl_ctrl = true;
            }

            if ((ui_id2type(msg[1]) == CTRL_TYPE_WINDOW) && (ui_get_current_window_id() > 0)) {
                printf("ui = %x show repeat.....\n", ui_get_current_window_id());
                ui_hide(ui_get_current_window_id());
            }


            msg[1] = ui_id2remap(msg[1]);

#ifdef CONFIG_WATCH_CASE_ENABLE
            UI_WINDOW_BACK_PUSH(msg[1]);
            if (ui_page_search(msg[1])) {
                UI_WINDOW_BACK_CLEAN(); // 卡片
            }
#endif

            ui_show(msg[1]);
            if (msg[2]) {
                os_sem_post((OS_SEM *)msg[2]);
            }
            if (bl_ctrl) {
                lcd_bl_open();
            }
            break;
        case UI_MSG_HIDE:
            if (CURR_WINDOW_MAIN == msg[1]) {
                ui_hide(ui_get_current_window_id());
            } else {
                ui_hide(msg[1]);
            }

            if (msg[2]) {
                os_sem_post((OS_SEM *)msg[2]);
            }
            break;
        case UI_TIME_TOUCH_RESUME:
#if (TCFG_UI_ENABLE&&(CONFIG_UI_STYLE == STYLE_JL_SOUNDBOX))
            if (lcd_sleep_status()) {
                set_backlight_time(get_backlight_time_item());//防止在息屏状态下跳转到PC模式后不亮屏
            }
#endif
            if (lcd_sleep_status()) {
                lcd_sleep_ctrl(false);
            }
            set_screen_saver_status(0);
            if (__ui_display.timer) {
                break;
            }
            if (__ui_display.touch_event_call && __ui_display.touch_event_interval) {
                __ui_display.timer = sys_timer_add((void *)NULL, __ui_display.touch_event_call, __ui_display.touch_event_interval); //注册按键扫描定时器
            }
            break;
        case UI_TIME_TOUCH_SUPEND:
            if (__ui_display.timer) {
                sys_timer_del(__ui_display.timer);
                __ui_display.timer = 0;
            }
            if (!get_standby_watch_en()) {
                lcd_sleep_ctrl(true);
            }
            set_screen_saver_status(1);
            break;
        default:
            break;
        }
    }
}


extern int ui_file_check_valid();
extern int ui_upgrade_file_check_valid();

int lcd_ui_init(void *arg)
{
    int err = 0;
    clock_add_set(DEC_UI_CLK);

#ifdef  CONFIG_WATCH_CASE_ENABLE
    mem_var_init(3 * 1024, false);
#else
    mem_var_init(0, false);
#endif

#if(CONFIG_UI_STYLE == STYLE_JL_WTACH_NEW)
#ifdef CONFIG_RELEASE_ENABLE
    imb_task_protect(false);
#else
    imb_task_protect(false);
#endif
#endif

    printf("open_resouece_start...\n");
    if (ui_file_check_valid()) {
        printf("ui_file_check_valid fail!!!\n");
        if (ui_upgrade_file_check_valid()) {
            printf("ui_upgrade_file_check_valid fail!!!\n");
            return -1;
        }
    }

    os_sem_create(&(__ui_display.start_sem), 0);
    err = task_create(ui_task, arg, UI_TASK_NAME);
    os_sem_pend(&(__ui_display.start_sem), 0);
    return err;
}

#else

int key_is_ui_takeover()
{
    return 0;
}

void key_ui_takeover(u8 on)
{

}

int ui_key_msg_post(int key)
{
    return 0;
}


int ui_touch_msg_post(struct touch_event *event)
{
    return 0;
}

int ui_touch_msg_post_withcallback(struct touch_event *event, void (*cb)(u8 finish))
{
    return 0;
}

void ui_touch_timer_delete()
{

}

void ui_touch_timer_start()
{

}

void ui_backlight_ctrl(u8 lcd_open_flag)
{

}


#endif

