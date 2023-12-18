/*********************************************************************************************
    *   Filename        : lib_system_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-18 15:22

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/

#include "app_config.h"
#include "system/includes.h"


///打印是否时间打印信息
const int config_printf_time         = 1;

///异常中断，asser打印开启
#ifdef CONFIG_RELEASE_ENABLE
const int config_asser         = 0;
#else
const int config_asser         = 1;
#endif

const int config_system_info   = 2;


//================================================//
//                 异常信息记录使能               //
//================================================//
//注意: 当config_asser变量为0时才有效.
#if	(defined CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE)
const int config_debug_exception_record = (!config_asser) && 1; 	//异常记录功能总开关
const int config_debug_exception_record_dump_info = (!config_asser) && 1; 		//小机上电输出异常信息使能
const int config_debug_exception_record_p11 = (!config_asser) && 1; 			//P11异常信息使能
const int config_debug_exception_record_stack = (!config_asser) && 1; 			//堆栈异常信息使能
const int config_debug_exception_record_ret_instrcion = (!config_asser) && 1; 	//指令数据异常信息使能
#else /* #if	(define CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE) */
const int config_debug_exception_record = 0; 				//异常记录功能总开关
const int config_debug_exception_record_dump_info = 0; 		//小机上电输出异常信息使能
const int config_debug_exception_record_p11 = 0; 			//P11异常信息使能
const int config_debug_exception_record_stack = 0; 			//堆栈异常信息使能
const int config_debug_exception_record_ret_instrcion = 0; 	//指令数据异常信息使能
#endif /* #if (define CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE) */


//================================================//
//                  SDFILE 精简使能               //
//================================================//
#ifdef CONFIG_SOUNDBOX_FLASH_256K
const int SDFILE_VFS_REDUCE_ENABLE = 1;
#else
const int SDFILE_VFS_REDUCE_ENABLE = 0;
#endif

//================================================//
//                  FLASH FAT管理使能             //
//================================================//
#ifdef  TCFG_VIRFAT_FLASH_SIMPLE
const int VIRFAT_FLASH_ENABLE = 1;
#else
const int VIRFAT_FLASH_ENABLE = 0;
#endif

//================================================//
//                  dev使用异步读使能             //
//================================================//
#ifdef TCFG_DEVICE_BULK_READ_ASYNC_ENABLE
const int device_bulk_read_async_enable = 1;
#else
const int device_bulk_read_async_enable = 0;
#endif

//================================================//
//                  UI 							  //
//================================================//
const int ENABLE_LUA_VIRTUAL_MACHINE = 0;

//================================================//
//              IMD是否使用JL UI  				  //
//================================================//
#if (TCFG_UI_ENABLE)
const int ENABLE_JL_UI_FRAME = 1;
#else
const int ENABLE_JL_UI_FRAME = 0;
#endif

//================================================//
//              UI版本和CPU类型  				  //
// 使用一个8bit值表示
// 	高四位表示UI类型，0表示点阵屏，1表示彩屏
// 	低四位表示CPU，0表示BR23和BR27，1表示BR28
//================================================//
#if ((defined CONFIG_CPU_BR23) || (defined CONFIG_CPU_BR27))
#if (defined TCFG_DOT_LCD_ENABLE)
const int JLUI_TYPE_AND_VERSION = 0x00;
#else
const int JLUI_TYPE_AND_VERSION = 0x10;
#endif

#else
#if (defined TCFG_DOT_LCD_ENABLE)
const int JLUI_TYPE_AND_VERSION = 0x01;

#else
const int JLUI_TYPE_AND_VERSION = 0x11;
#endif
#endif


/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_d_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

//FreeRTOS
const char log_tag_const_v_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_w_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_d_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
