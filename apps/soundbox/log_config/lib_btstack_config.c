/*********************************************************************************************
    *   Filename        : btstack_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-16 11:49

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "btcontroller_config.h"
#include "bt_common.h"

/**
 * @brief Bluetooth Stack Module
 */

#ifdef CONFIG_SOUNDBOX_FLASH_256K
const int CONFIG_BTSTACK_BIG_FLASH_ENABLE     = 0;
#else
const int CONFIG_BTSTACK_BIG_FLASH_ENABLE     = 1;
#endif


#if TCFG_BT_SUPPORT_AAC
const int CONFIG_BTSTACK_SUPPORT_AAC    = 1;
#else
const int CONFIG_BTSTACK_SUPPORT_AAC    = 0;
#endif

//协议栈接收到命令是否自动退出sniff
const int config_btstask_auto_exit_sniff = 1;



#if RCSP_MODE
const int config_rcsp_stack_enable = 1;
#else
const int config_rcsp_stack_enable = 0;
#endif

#if TRANS_MULTI_BLE_EN
//le 配置,可以优化代码和RAM
const int config_le_hci_connection_num = TRANS_MULTI_BLE_CONNECT_NUMS;//支持同时连接个数
const int config_le_sm_support_enable  = TRANS_MULTI_BLE_SM_ENABLE;   //是否支持加密配对
const int config_le_gatt_server_num    = TRANS_MULTI_BLE_SLAVE_NUMS;  //支持server角色个数
const int config_le_gatt_client_num    = TRANS_MULTI_BLE_MASTER_NUMS; //支持client角色个数

#else

//le 配置,可以优化代码和RAM
const int config_le_hci_connection_num = 1;//支持同时连接个数
const int config_le_sm_support_enable = 0; //是否支持加密配对
const int config_le_gatt_server_num = 1;   //支持server角色个数
const int config_le_gatt_client_num = 1;   //支持client角色个数
#endif

const int config_le_sm_sub_sc_enable = 0;            /*SC*/
const int config_le_sm_sub_sc_bridge_edr_enable = 0; /**/



