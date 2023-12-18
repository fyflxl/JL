/*************************************************************
   此文件函数主要是 broadcast mic 模式按键处理和事件处理

	void app_bc_mic_task
   broadcast mic模式主函数

	static int linein_sys_event_handler(struct sys_event *event)
   linein模式系统事件所有处理入口

	static void linein_task_close(void)
	linein模式退出

**************************************************************/

#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"

#include "app_config.h"
#include "app_task.h"

#include "media/includes.h"
#include "tone_player.h"

#include "app_charge.h"
#include "app_main.h"
#include "app_online_cfg.h"
#include "app_power_manage.h"

#include "key_event_deal.h"
#include "user_cfg.h"
#include "clock_cfg.h"

#include "app_broadcast.h"


#if TCFG_APP_BC_MIC_EN

/* u8 local_mic_is_on = 0;		//该值为1说明当前本地mic打开的状态 */

//*----------------------------------------------------------------------------*/
/**@brief    broadcast mic 入口
   @param    无
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void broadcast_mic_app_init(void)
{
    sys_key_event_enable();
    clock_idle(REC_IDLE_CLOCK);		//暂时设置为录音的时钟
#if TCFG_BROADCAST_ENABLE
#if TCFG_APP_BC_MIC_EN
    extern void broadcast_mic_hdl_register(void);
    broadcast_mic_hdl_register();
#endif
#endif
}

//*----------------------------------------------------------------------------*/
/**@brief    broadcast mic 提示音播放结束回调函数
   @param    无
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void  bc_mic_tone_play_end_callback(void *priv, int flag)
{
    u32 index = (u32)priv;
    if (APP_BC_MIC_TASK != app_get_curr_task()) {
        printf("\n-- error: curr task isn't APP_BC_MIC_TASK!\n");
        return;
    }
    switch (index) {
    case IDEX_TONE_LINEIN:
        app_task_put_key_msg(KEY_MIC_START, 0);
        break;
    default:
        break;
    }
}

//*----------------------------------------------------------------------------*/
/**@brief   bc mic 按键消息入口
   @param    无
   @return   1、消息已经处理，不需要发送到common  0、消息发送到common处理
   @note
*/
/*----------------------------------------------------------------------------*/
static int bc_mic_key_msg_deal(struct sys_event *event)
{
    int result;
    int ret = true;
    int err = 0;
    u8 vol;
    int key_event = event->u.key.event;
    int key_value = event->u.key.value;
    switch (key_event) {
    case KEY_MIC_START:
#if TCFG_BROADCAST_ENABLE
        result = app_broadcast_deal(BROADCAST_APP_MODE_ENTER);
        if (result <= 0)
#endif
        {
            extern int mic_start(void);
            u8 ret = 0;
            ret = mic_start();	//打开本地mic
            if (ret != true) {
                printf("\n-- mic start failed!!\n");
            }
        }
        break;
    default:
        ret = false;
        break;
    }
    return ret;
}



//*----------------------------------------------------------------------------*/
/**@brief    bc mic 模式活跃状态 所有消息入口
   @param    无
   @return   1、当前消息已经处理，不需要发送comomon 0、当前消息不是linein处理的，发送到common统一处理
   @note
*/
/*----------------------------------------------------------------------------*/
static int bc_mic_sys_event_handler(struct sys_event *event)
{
    int ret = TRUE;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return bc_mic_key_msg_deal(event);
        break;
    default:
        return false;
    }
    return false;
}

static void bc_mic_task_close(void)
{
#if TCFG_BROADCAST_ENABLE
    app_broadcast_deal(BROADCAST_APP_MODE_EXIT);
#endif
    extern void mic_stop(void);
    mic_stop();
}

//*----------------------------------------------------------------------------*/
/**@brief    broadcast mic 任务主体
   @param    无
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void app_bc_mic_task()
{
    int res;
    int msg[32];
    broadcast_mic_app_init();
    tone_play_with_callback_by_name(tone_table[IDEX_TONE_NONE], 1, bc_mic_tone_play_end_callback, (void *)IDEX_TONE_LINEIN);

    while (1) {
        app_task_get_msg(msg, ARRAY_SIZE(msg), 1);
        switch (msg[0]) {
        case APP_MSG_SYS_EVENT:
            if (bc_mic_sys_event_handler((struct sys_event *)(&msg[1])) == false) {
                app_default_event_deal((struct sys_event *)(&msg[1]));
            }
            break;
        default:
            break;
        }

        if (app_task_exitting()) {
            bc_mic_task_close();
            return;
        }
    }
}

#else
void app_bc_mic_task(void)
{

}

#endif /* TCFG_APP_BC_MIC_EN */


