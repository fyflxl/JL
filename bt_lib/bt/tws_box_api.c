
static void user_set_tws_box_mode(u8 mode)
{

}

extern void sys_enter_soft_poweroff(void *priv);

static u8 tws_going_poweroff = 0;

u8 is_tws_going_poweroff()
{
    return 0;
}

static void tws_poweroff_same_time_tone_end(void *priv, int flag)
{
    //提示音结束， 才进入关机流程
    u32 index = (u32)priv;
    switch (index) {
    case IDEX_TONE_POWER_OFF:
        ///提示音播放结束， 启动播放器播放
        sys_enter_soft_poweroff(NULL);
        break;
    default:
        printf("%s callback err!!!!!!\n", __FUNCTION__);
        break;
    }
}

static void tws_poweroff_same_time_deal(void)
{
    ///先播放提示音， 保证同时播放

    tws_going_poweroff  = 1;

    int ret = tone_play_with_callback_by_name(tone_table[IDEX_TONE_POWER_OFF], 1,
              tws_poweroff_same_time_tone_end, (void *)IDEX_TONE_POWER_OFF);
    if (ret) {
        sys_enter_soft_poweroff(NULL);
    }
}

static void tws_event_cmd_max_vol()
{

#if TCFG_MAX_VOL_PROMPT
    tone_play_by_path(tone_table[IDEX_TONE_MAX_VOL], 0);
#endif
}

void tws_event_cmd_mode_change(void)
{
    if (get_bt_init_status() && (bt_get_task_state() == 0)) {
#if TWFG_APP_POWERON_IGNORE_DEV
        if ((timer_get_ms() - app_var.start_time) > TWFG_APP_POWERON_IGNORE_DEV)
#endif//TWFG_APP_POWERON_IGNORE_DEV

        {
            printf("KEY_CHANGE_MODE\n");
            app_task_switch_next();
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 手机链接上 local tws设置
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_local_connected(u8 state, u8 role)
{
    //state&TWS_STA_LOCAL_TWS_OPEN 对方是否开启local_tws
    log_info("tws_conn_state=0x%x\n", state);

    u8 cur_task = app_check_curr_task(APP_BT_TASK);

    if (cur_task) {
        TWS_LOCAL_IN_BT();
    } else {
        TWS_LOCAL_OUT_BT();
    }

    if (state & TWS_STA_LOCAL_TWS_OPEN) {
        TWS_REMOTE_OUT_BT();
    } else {
        TWS_REMOTE_IN_BT();
    }


    if (state & TWS_STA_LOCAL_TWS_OPEN) {
#if (TCFG_DEC2TWS_ENABLE)
        user_set_tws_box_mode(1);
#endif
        clear_current_poweron_memory_search_index(0);
        bt_user_priv_var.auto_connection_counter = 0;
        if (!cur_task) {
            gtws.device_role = TWS_ACTIVE_DEIVCE;
            log_debug("\n    ------  tws active device 0 ------   \n");
        } else {
            gtws.device_role = TWS_UNACTIVE_DEIVCE;
            log_debug("\n    ------  tws unactive device0 ------   \n");
        }
    } else if (!cur_task) {
        clear_current_poweron_memory_search_index(0);
        bt_user_priv_var.auto_connection_counter = 0;
        gtws.device_role = TWS_ACTIVE_DEIVCE;
        log_debug("\n    ------  tws active device 1 ------   \n");
    } else {
        if (role == TWS_ROLE_MASTER) {
            gtws.device_role = TWS_ACTIVE_DEIVCE;
            log_debug("\n    ------  tws active device 2 ------   \n");
        } else {
            gtws.device_role = TWS_UNACTIVE_DEIVCE;
            log_debug("\n    ------  tws unactive device2 ------   \n");
        }

    }
}
static void bt_tws_box_sync(void *_data, u16 len, bool rx)
{
#if (TCFG_DEC2TWS_ENABLE)
    if (rx) {
        u8 *data = (u8 *)_data;
        if (data[0] == TWS_BOX_NOTICE_A2DP_BACK_TO_BT_MODE
            || data[0] == TWS_BOX_A2DP_BACK_TO_BT_MODE_START) {
            tws_local_back_to_bt_mode(data[0], data[1]);
            /* u32 bt_tmr = 0; */
            /* memcpy(&bt_tmr, &data[2], 4); */
            /* a2dp_dec_output_set_start_bt_time(bt_tmr); */
        }
    }
#endif
}

REGISTER_TWS_FUNC_STUB(app_box_sync_stub) = {
    .func_id = TWS_FUNC_ID_BOX_SYNC,
    .func    = bt_tws_box_sync,
};


void tws_user_sync_box(u8 cmd, u8 value)
{
#if (TCFG_DEC2TWS_ENABLE)
    u8 data[6];
    data[0] = cmd;
    data[1] = value;
    u32 bt_tmr = audio_bt_time_read(NULL, NULL) + 1000;
    memcpy(&data[2], &bt_tmr, 4);
    tws_api_send_data_to_sibling(data, sizeof(data), TWS_FUNC_ID_BOX_SYNC);
    /* if (cmd == TWS_BOX_NOTICE_A2DP_BACK_TO_BT_MODE */
    /* || cmd == TWS_BOX_A2DP_BACK_TO_BT_MODE_START) { */
    /* a2dp_dec_output_set_start_bt_time(bt_tmr); */
    /* } */
#else
    u8 data[2];

    data[0] = cmd;
    data[1] = value;
    tws_api_send_data_to_sibling(data, 2, TWS_FUNC_ID_BOX_SYNC);
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 判断tws是否都在蓝牙模式
   @param    无
   @return   1:在蓝牙模式    0:不在蓝牙模式
   @note
*/
/*----------------------------------------------------------------------------*/
u8 is_tws_all_in_bt()
{
#if (TCFG_DEC2TWS_ENABLE)
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if ((gtws.bt_task & 0x03) == 0x03) {
            return 1;
        }
        return 0;
    }
#endif
    return 1;
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 判断tws是否为活动端
   @param    无
   @return   1:活动端    0:非活动端
   @note     音源输入端为活动端
*/
/*----------------------------------------------------------------------------*/
u8 is_tws_active_device(void)
{
    /* log_info("\n    ------  tws device ------   %x \n",gtws.device_role); */
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (gtws.device_role == TWS_ACTIVE_DEIVCE) {
            return 1;
        }
        return 0;
    }
    return 1;
}

static void tws_event_cmd_earphone_charge_start()
{
    if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
        /* g_printf("TWS MUSIC PLAY"); */
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    }
}

static void tws_event_cmd_led_tws_conn_status()
{
    pwm_led_mode_set(PWM_LED_ALL_OFF);
    ui_update_status(STATUS_BT_TWS_CONN);
}

static void tws_event_cmd_led_phone_conn_status(int err)
{
    if (err != -TWS_SYNC_CALL_RX) {
        pwm_led_mode_set(PWM_LED_ALL_OFF);
        ui_update_status(STATUS_BT_CONN);
    }
}

static void tws_event_cmd_led_phone_disconn_status()
{
    ui_update_status(STATUS_BT_TWS_CONN);
    if (!app_var.goto_poweroff_flag) { /*关机不播断开提示音*/
        if (is_tws_all_in_bt()) {
            tone_play_by_path(tone_table[IDEX_TONE_BT_DISCONN], 1);
        }
    }
}

static void tws_event_cmd_phone_disconn_tone()
{
#if TCFG_DEC2TWS_ENABLE
    return;
#endif
    tone_play_by_path(tone_table[IDEX_TONE_TWS_CONN], 1);
}

static void tws_event_cmd_phone_conn_tone(int err)
{
    int state;
    if (err != -TWS_SYNC_CALL_RX) {
        state = tws_api_get_tws_state();
        if (state & (TWS_STA_SBC_OPEN | TWS_STA_ESCO_OPEN) ||
            (get_call_status() != BT_CALL_HANGUP)) {
            return;
        }
        tone_play_by_path(tone_table[IDEX_TONE_BT_CONN], 1);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 拨号
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_event_cmd_phone_num_tone()
{
    if (bt_user_priv_var.phone_con_sync_num_ring) {
        return;
    }
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        if (bt_user_priv_var.phone_timer_id) {
            sys_timeout_del(bt_user_priv_var.phone_timer_id);
            bt_user_priv_var.phone_timer_id = 0;
        }
    }
    if (bt_user_priv_var.phone_ring_flag) {
        phone_num_play_timer(NULL);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 铃声
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_event_cmd_phone_ring_tone()
{
    if (bt_user_priv_var.phone_ring_flag) {
        if (bt_user_priv_var.phone_con_sync_ring) {
            if (phone_ring_play_start()) {
                bt_user_priv_var.phone_con_sync_ring = 0;
                if (tws_api_get_role() == TWS_ROLE_MASTER) {
                    //播完一声来电铃声之后,关闭aec、msbc_dec之后再次同步播来电铃声
                    /* bt_tws_api_push_cmd(SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 2 + 400); */
                }
            }
        } else {
            phone_ring_play_start();
        }
    }
}

static void tws_event_cmd_sync_num_ring_tone()
{
    if (bt_user_priv_var.phone_ring_flag) {
        if (phone_ring_play_start()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                //播完一声来电铃声之后,关闭aec、msbc_dec之后再次同步播来电铃声
                bt_user_priv_var.phone_con_sync_ring = 0;
            }
            bt_user_priv_var.phone_con_sync_num_ring = 1;

        }
    }
}

/*
 * 提示音同步播放
 */
static void play_tone_at_same_time(int tone_name, int err)
{
    STATUS *p_tone = get_tone_config();

    switch (tone_name) {
    case SYNC_TONE_TWS_CONNECTED:
        tws_event_cmd_phone_disconn_tone();
        break;
    case SYNC_TONE_PHONE_CONNECTED:
        tws_event_cmd_phone_conn_tone(err);
        break;
    case SYNC_TONE_PHONE_DISCONNECTED:
        /* 关机不播断开提示音 */
        /* if (!app_var.goto_poweroff_flag) { */
        /*     tone_play_index(p_tone->bt_disconnect, 1); */
        /* } */
        /* ui_update_status(STATUS_BT_TWS_CONN); */
        break;
    case SYNC_TONE_MAX_VOL:
        tws_event_cmd_max_vol();
        break;
    case SYNC_TONE_POWER_OFF:
        /* y_printf("SYNC_TONE_POWER_OFF\n"); */
        /* if (app_var.goto_poweroff_flag == 1) { */
        /*     app_var.goto_poweroff_flag = 2; */
        /*     tone_play_index(p_tone->power_off, 0); */
        /* } */
        /* bt_tws_poweroff(); */
        break;
    case SYNC_TONE_PHONE_NUM:
        tws_event_cmd_phone_num_tone();
        break;
    case SYNC_TONE_PHONE_RING:
        tws_event_cmd_phone_ring_tone();
        break;
    case SYNC_TONE_PHONE_NUM_RING:
        tws_event_cmd_sync_num_ring_tone();
        break;
    }
}

TWS_SYNC_CALL_REGISTER(tws_tone_play) = {
    .uuid = 0x123A9E50,
    .task_name = "app_core",
    .func = play_tone_at_same_time,
};

void bt_tws_play_tone_at_same_time(int tone_name, int msec)
{
    tws_api_sync_call_by_uuid(0x123A9E50, tone_name, msec);
}

/*
 * LED状态同步
 */
static void led_state_sync_handler(int state, int err)
{
    switch (state) {
    case SYNC_LED_STA_TWS_CONN:
        tws_event_cmd_led_tws_conn_status();
        break;
    case SYNC_LED_STA_PHONE_CONN:
        tws_event_cmd_led_phone_conn_status(err);
        break;
    case SYNC_LED_STA_PHONE_DISCONN:
        tws_event_cmd_led_phone_disconn_status();
        break;
    }
}

TWS_SYNC_CALL_REGISTER(tws_led_sync) = {
    .uuid = 0x2A1E3095,
    .task_name = "app_core",
    .func = led_state_sync_handler,
};

void bt_tws_led_state_sync(int led_state, int msec)
{
    tws_api_sync_call_by_uuid(0x2A1E3095, led_state, msec);
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws tws本地解码状态设置
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_event_sync_fun_tranid(struct bt_event *evt)
{
    int reason = evt->args[2];
    if (reason == SYNC_CMD_BOX_EXIT_BT) {
#if (TCFG_DEC2TWS_ENABLE)
        user_set_tws_box_mode(1);
#endif
        log_debug("\n    ------   TWS_UNACTIVE_DEIVCE \n");
        TWS_LOCAL_IN_BT();
        TWS_REMOTE_OUT_BT();
        gtws.device_role =  TWS_UNACTIVE_DEIVCE;
        tws_local_back_to_bt_mode(TWS_BOX_EXIT_BT, gtws.device_role);
    } else if (reason == SYNC_CMD_BOX_INIT_EXIT_BT) {
        TWS_LOCAL_OUT_BT();
        TWS_REMOTE_IN_BT();
#if (TCFG_DEC2TWS_ENABLE)
        user_set_tws_box_mode(2);
#endif
        log_debug("\n    ------   TWS_ACTIVE_DEIVCE \n");
        gtws.device_role =  TWS_ACTIVE_DEIVCE;
        tws_local_back_to_bt_mode(TWS_BOX_EXIT_BT, gtws.device_role);
    } else if (reason == SYNC_CMD_BOX_ENTER_BT) {
        log_debug("----  SYNC_CMD_BOX_ENTER_BT");
        TWS_LOCAL_IN_BT();
        TWS_REMOTE_IN_BT();
#if (TCFG_DEC2TWS_ENABLE)
        user_set_tws_box_mode(0);
#endif
        tws_local_back_to_bt_mode(TWS_BOX_ENTER_BT, gtws.device_role);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws tws本地解码启动
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_event_local_media_start(struct bt_event *evt)
{
    if (false == app_check_curr_task(APP_BT_TASK)) {
        /* log_debug("is not bt mode \n"); */
        return;
    }
    if (a2dp_dec_close()) {
        /* tws_api_local_media_trans_clear(); */
    }
    localtws_dec_open(evt->value);
}


static void tws_event_local_media_stop(struct bt_event *evt)
{
    if (false == app_check_curr_task(APP_BT_TASK)) {
        /* log_debug("is not bt mode \n"); */
        return;
    }
    if (localtws_dec_close(0)) {
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 主从同步调用函数处理
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static void tws_event_sync_fun_cmd(struct bt_event *evt)
{
    int reason = evt->args[2];
    log_d("tws_event_sync_fun_cmd: %x\n", reason);
    switch (reason) {
    case SYNC_CMD_SHUT_DOWN_TIME:
        sys_auto_shut_down_disable();
        sys_auto_shut_down_enable();
        break;
    case SYNC_CMD_POWER_OFF_TOGETHER:
        tws_poweroff_same_time_deal();
        break;
    case SYNC_CMD_MAX_VOL:
        tws_event_cmd_max_vol();
        break;
    case SYNC_CMD_MODE_BT :
    case SYNC_CMD_MODE_MUSIC :
    case SYNC_CMD_MODE_LINEIN:
    case SYNC_CMD_MODE_FM:
    case SYNC_CMD_MODE_PC:
    case SYNC_CMD_MODE_ENC:
    case SYNC_CMD_MODE_RTC:
    case SYNC_CMD_MODE_SPDIF:
        break;

    case SYNC_CMD_LOW_LATENCY_ENABLE:
        earphone_a2dp_codec_set_low_latency_mode(1, 600);
        break;
    case SYNC_CMD_LOW_LATENCY_DISABLE:
        earphone_a2dp_codec_set_low_latency_mode(0, 600);
        break;
    case SYNC_CMD_MODE_CHANGE:
        tws_event_cmd_mode_change();
        break;
    default :
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws tws链接上
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_connected(struct bt_event *evt)
{
    u8 addr[4][6];
    int state;
    int pair_suss = 0;
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];
    char channel = 0;
    log_info("-----  tws_event_pair_suss: %x\n", gtws.state);

    clr_tws_local_back_role();


#if CONFIG_TWS_DISCONN_NO_RECONN
    tws_detach_by_remove_key = 0;
#endif


#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
    bt_set_pair_code_en(0);
#endif

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
    lmp_hci_write_local_address(bt_get_mac_addr());
#endif

    syscfg_read(CFG_TWS_REMOTE_ADDR, addr[0], 6);
    syscfg_read(CFG_TWS_COMMON_ADDR, addr[1], 6);
    tws_api_get_sibling_addr(addr[2]);
    tws_api_get_local_addr(addr[3]);

    /*
     * 记录对方地址
     */
    if (memcmp(addr[0], addr[2], 6)) {
        syscfg_write(CFG_TWS_REMOTE_ADDR, addr[2], 6);
        pair_suss = 1;
        log_info("rec tws addr\n");
    }
    if (memcmp(addr[1], addr[3], 6)) {
        syscfg_write(CFG_TWS_COMMON_ADDR, addr[3], 6);
        pair_suss = 1;
        log_info("rec comm addr\n");
    }

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
    syscfg_write(VM_TWS_ROLE, &role, 1);
#endif

    if (pair_suss) {

        gtws.state = BT_TWS_PAIRED;

#if CONFIG_TWS_USE_COMMMON_ADDR
        extern void bt_update_mac_addr(u8 * addr);
        bt_update_mac_addr((void *)addr[3]);
#endif
    }

    /*
     * 记录左右声道
     */
    channel = tws_api_get_local_channel();
    if (channel != bt_tws_get_local_channel()) {
        syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
    }
    log_debug("tws_local_channel: %c\n", channel);

    /* BT_STATE_TWS_CONNECTED(pair_suss, addr[3]); */

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
    if (tws_auto_pair_enable) {
        tws_auto_pair_enable = 0;
        tws_le_acc_generation_init();
    }
#endif

#ifdef CONFIG_NEW_BREDR_ENABLE
    if (channel == 'L') {
        syscfg_read(CFG_TWS_LOCAL_ADDR, addr[2], 6);
    }
    tws_api_set_quick_connect_addr(addr[2]);
#else
    tws_api_set_connect_aa(channel);
    tws_remote_state_clear();
#endif
    if (reason & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN)) {
        if (role == TWS_ROLE_SLAVE) {
            gtws.state |= BT_TWS_AUDIO_PLAYING;
        }
    }

    if (!phone_link_connection ||
        (reason & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN))) {
        pwm_led_clk_set(PWM_LED_CLK_BTOSC_24M);
    }
    /*
     * TWS连接成功, 主机尝试回连手机
     */
    if (gtws.connect_time) {
        if (role == TWS_ROLE_MASTER) {
            bt_user_priv_var.auto_connection_counter = gtws.connect_time;
        }
        gtws.connect_time = 0;
    }

    gtws.state &= ~BT_TWS_TIMEOUT;
    gtws.state |= BT_TWS_SIBLING_CONNECTED;

    state = evt->args[2];
    if (role == TWS_ROLE_MASTER) {
        if (!phone_link_connection) {
            //还没连上手机
            log_info("[SYNC] LED SYNC");
            bt_tws_led_state_sync(SYNC_LED_STA_TWS_CONN, 400);
        } else {
            bt_tws_led_state_sync(SYNC_LED_STA_PHONE_CONN, 400);
        }
        if (bt_get_exit_flag() || ((get_call_status() == BT_CALL_HANGUP) && !(state & TWS_STA_SBC_OPEN))) {  //通话状态不播提示音
            log_info("[SYNC] TONE SYNC");
            bt_tws_play_tone_at_same_time(SYNC_TONE_TWS_CONNECTED, 400);
        }
#if BT_SYNC_PHONE_RING
        if (get_call_status() == BT_CALL_INCOMING) {
            if (bt_user_priv_var.phone_ring_flag && !bt_user_priv_var.inband_ringtone) {
                log_debug("<<<<<<<<<phone_con_sync_num_ring \n");
                bt_user_priv_var.phone_con_sync_ring = 1;//主机来电过程，从机连接加入
#if BT_PHONE_NUMBER
                bt_user_priv_var.phone_con_sync_num_ring = 1;//主机来电过程，从机连接加入
                bt_tws_sync_phone_num(NULL);
#endif
            }
        }
#endif
        bt_tws_sync_volume();
    }
#if TCFG_CHARGESTORE_ENABLE
    chargestore_sync_chg_level();//同步充电舱电量
#endif
    tws_sync_bat_level(); //同步电量到对耳
    bt_tws_delete_pair_timer();

#if (TCFG_DEC2TWS_ENABLE)
    tws_local_connected(state, role);
#endif

    if (phone_link_connection == 0) {   ///没有连接手机

        if (role == TWS_ROLE_MASTER) {
            bt_tws_connect_and_connectable_switch();
        } else {   //从机关闭各种搜索链接
            tws_api_cancle_wait_pair();
        }
        sys_auto_shut_down_disable();
        sys_auto_shut_down_enable();
    }

    /* tws_connect_ble_adv(phone_link_connection); */

#if RCSP_MODE
    extern void sync_setting_by_time_stamp(void);
    sync_setting_by_time_stamp();
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 发起配对超时, 等待手机连接
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_search_timeout(struct bt_event *evt)
{
    gtws.state &= ~BT_TWS_SEARCH_SIBLING;


#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
    bt_set_pair_code_en(0);
#endif


#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
    tws_api_set_local_channel(bt_tws_get_local_channel());
    lmp_hci_write_local_address(gtws.addr);
#endif
    bt_tws_connect_and_connectable_switch();

}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws TWS连接超时
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_connection_timeout(struct bt_event *evt)
{
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];
#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status()) {
        return;
    }
#endif

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
    bt_ble_icon_slave_en(0);
#endif

    clr_tws_local_back_role();

    if (gtws.state & BT_TWS_UNPAIRED) {
        /*
         * 配对超时
         */
#if CONFIG_TWS_PAIR_BY_BOTH_SIDES
        bt_set_pair_code_en(0);
#endif


        gtws.state &= ~BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
        tws_api_set_local_channel(bt_tws_get_local_channel());
        lmp_hci_write_local_address(gtws.addr);
#endif
        bt_tws_connect_and_connectable_switch();
    } else {
        if (phone_link_connection) {   ///如果手机连接上，就开启等待链接
            bt_tws_wait_sibling_connect(0);
        } else {
            if (gtws.state & BT_TWS_TIMEOUT) {
                gtws.state &= ~BT_TWS_TIMEOUT;
                gtws.connect_time = bt_user_priv_var.auto_connection_counter;
                bt_user_priv_var.auto_connection_counter = 0;
            }
            bt_tws_connect_and_connectable_switch();
        }
    }

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
    u8 mac_addr[6];
    syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
    lmp_hci_write_local_address(mac_addr);
#endif

}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 断开链接
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_connection_detach(struct bt_event *evt)
{
    u8 mac_addr[6];
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];

    clr_tws_local_back_role();

#if (TCFG_DEC2TWS_ENABLE)
    if (app_check_curr_task(APP_BT_TASK)) {
        //tws断开时清除local_tws标志，防止tws重新配对后状态错误
        user_set_tws_box_mode(0);
        // close localtws dec
        localtws_dec_close(0);
    }
#endif

    gtws.device_role = TWS_ACTIVE_DEIVCE;
    app_power_set_tws_sibling_bat_level(0xff, 0xff);

#if TCFG_CHARGESTORE_ENABLE
    chargestore_set_sibling_chg_lev(0xff);
#endif

    if ((!a2dp_get_status()) && (get_call_status() == BT_CALL_HANGUP)) {  //如果当前正在播歌，切回RC会导致下次从机开机回连后灯状态不同步
        pwm_led_clk_set((!TCFG_LOWPOWER_BTOSC_DISABLE) ? PWM_LED_CLK_RC32K : PWM_LED_CLK_BTOSC_24M);
        //pwm_led_clk_set(PWM_LED_CLK_RC32K);
    }

#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status()) {
        return;
    }
#endif

    if (phone_link_connection) {
        ui_update_status(STATUS_BT_CONN);
        extern void power_event_to_user(u8 event);
        power_event_to_user(POWER_EVENT_POWER_CHANGE);
        user_send_cmd_prepare(USER_CTRL_HFP_CMD_UPDATE_BATTARY, 0, NULL);           //对耳断开后如果手机还连着，主动推一次电量给手机
    } else {
        ui_update_status(STATUS_BT_TWS_DISCONN);
    }

    if ((gtws.state & BT_TWS_SIBLING_CONNECTED) && (!app_var.goto_poweroff_flag)) {
#if CONFIG_TWS_POWEROFF_SAME_TIME
        extern u8 poweroff_sametime_flag;
        if (!app_var.goto_poweroff_flag && !poweroff_sametime_flag && !phone_link_connection && (reason != TWS_DETACH_BY_REMOVE_PAIRS)) { /*关机不播断开提示音*/
            tone_play_by_path(tone_table[IDEX_TONE_TWS_DISCONN], 1);
        }
#else
        if (!app_var.goto_poweroff_flag && !phone_link_connection && (reason != TWS_DETACH_BY_REMOVE_PAIRS)) {
            tone_play_by_path(tone_table[IDEX_TONE_TWS_DISCONN], 1);
        }
#endif
        gtws.state &= ~BT_TWS_SIBLING_CONNECTED;

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
        syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
        lmp_hci_write_local_address(mac_addr);
#endif
    }

    if (reason == TWS_DETACH_BY_REMOVE_PAIRS) {
        gtws.state = BT_TWS_UNPAIRED;
        log_debug("<<<<<<<<<<<<<<<<<<<<<<tws detach by remove pairs>>>>>>>>>>>>>>>>>>>\n");

        if (!phone_link_connection) {
            /* bt_open_tws_wait_pair(); */
        } else {
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
            /* bt_open_tws_wait_pair(); */
#endif
        }

        return;
    }

    if (get_esco_coder_busy_flag()) {
        tws_api_connect_in_esco();
        return;
    }

    //非测试盒在仓测试，直连蓝牙
    if (reason == TWS_DETACH_BY_TESTBOX_CON) {
        log_debug(">>>>>>>>>>>>>>>>TWS_DETACH_BY_TESTBOX_CON<<<<<<<<<<<<<<<<<<<\n");
        gtws.state &= ~BT_TWS_PAIRED;
        gtws.state |= BT_TWS_UNPAIRED;
        //syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
        if (!phone_link_connection) {
#if CONFIG_TWS_USE_COMMMON_ADDR
            get_random_number(mac_addr, 6);
            lmp_hci_write_local_address(mac_addr);
#endif

            bt_tws_wait_pair_and_phone_connect();
        }
        return;
    }

#if CONFIG_TWS_DISCONN_NO_RECONN
    if (reason == TWS_DETACH_BY_REMOVE_NO_RECONN) {
        tws_detach_by_remove_key = 1;
        if ((tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED)) {
            bt_tws_wait_sibling_connect(0);
        } else {
            connect_and_connectable_switch_by_noreconn(0);
        }

        return;
    }
#endif

    if (phone_link_connection) {
        bt_tws_wait_sibling_connect(0);
    } else {
        if (reason == TWS_DETACH_BY_SUPER_TIMEOUT) {
            if (gtws.state & BT_TWS_REMOVE_PAIRS) {
                bt_tws_remove_pairs();
                return;
            }
            if (role == TWS_ROLE_SLAVE) {
                gtws.state |= BT_TWS_TIMEOUT;
                gtws.connect_time = bt_user_priv_var.auto_connection_counter;
                bt_user_priv_var.auto_connection_counter = 0;
            }
            bt_tws_connect_sibling(6);
        } else {
            bt_tws_connect_and_connectable_switch();
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws 手机断开链接
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_phone_link_detach(struct bt_event *evt)
{
    /*
     * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
     */
    int reason = evt->args[2];
    if (app_var.goto_poweroff_flag) {
        return;
    }

#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status()) {
        return;
    }
#endif

    sys_auto_shut_down_enable();

    if ((reason != 0x08) && (reason != 0x0b))   {
        bt_user_priv_var.auto_connection_counter = 0;
    } else {
        if (reason == 0x0b) {
            bt_user_priv_var.auto_connection_counter = (8 * 1000);
        }
    }


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
    if (reason == 0x0b) {
        //CONNECTION ALREADY EXISTS
        ble_icon_contrl.miss_flag = 1;
    } else {
        ble_icon_contrl.miss_flag = 0;
    }
#endif

    bt_tws_connect_and_connectable_switch();

    tws_sniff_controle_check_enable();

}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙tws tws 清除配对信息
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void tws_event_remove_pairs(struct bt_event *evt)
{
    tone_play_by_path(tone_table[IDEX_TONE_TWS_DISCONN], 1);
    bt_tws_remove_pairs();
    app_power_set_tws_sibling_bat_level(0xff, 0xff);
#if TCFG_CHARGESTORE_ENABLE
    chargestore_set_sibling_chg_lev(0xff);
#endif
    pwm_led_clk_set((!TCFG_LOWPOWER_BTOSC_DISABLE) ? PWM_LED_CLK_RC32K : PWM_LED_CLK_BTOSC_24M);
}

static void tws_event_role_switch(struct bt_event *evt)
{
    int role = evt->args[0];
    if (!(tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED)) {
        bt_tws_connect_and_connectable_switch();
        tws_sniff_controle_check_enable();
    }
    if (role == TWS_ROLE_MASTER) {

    } else {

    }
}

static void tws_event_esco_add_connect(struct bt_event *evt)
{
    int role = evt->args[0];
    if (role == TWS_ROLE_MASTER) {
        bt_tws_sync_volume();
    }
}

static void tws_event_mode_change_connect(struct bt_event *evt)
{
    if (evt->args[0] == 0) {
        if (0 == (tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED)) {
            tws_sniff_controle_check_enable();
        }
    }
}

