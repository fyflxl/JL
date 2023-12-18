#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 *  板级配置选择
*/


#include "media/audio_def.h"

#include "board_demo_cfg.h"


//*********************************************************************************//
//                                 编译警告                                         //
//*********************************************************************************//
#if ((TCFG_COMM_TYPE == TCFG_UART_COMM) && TCFG_ONLINE_ENABLE) && (TCFG_PC_ENABLE || TCFG_UDISK_ENABLE)
#error "eq online adjust enable, plaease close usb marco"
#endif// ((TCFG_ONLINE_ENABLE) && (TCFG_PC_ENABLE || TCFG_UDISK_ENABLE))

#if ((TCFG_COMM_TYPE == TCFG_UART_COMM) && TCFG_CFG_TOOL_ENABLE ) && TCFG_PC_ENABLE
#error "soundbox tool enable, plaease close pc marco"
#endif// (TCFG_CFG_TOOL_ENABLE) && (TCFG_PC_ENABLE)

#if TCFG_UI_ENABLE
#if ((TCFG_SPI_LCD_ENABLE &&  TCFG_CODE_FLASH_ENABLE) && (TCFG_FLASH_DEV_SPI_HW_NUM == TCFG_TFT_LCD_DEV_SPI_HW_NUM))
#error "flash spi port == lcd spi port, please close one !!!"
#endif//((TCFG_SPI_LCD_ENABLE &&  TCFG_CODE_FLASH_ENABLE) && (TCFG_FLASH_DEV_SPI_HW_NUM == TCFG_TFT_LCD_DEV_SPI_HW_NUM))
#endif//TCFG_UI_ENABLE

#if((TRANS_DATA_EN + DUEROS_DMA_EN + ANCS_CLIENT_EN) > 1)
#error "they can not enable at the same time,just select one!!!"
#endif//(TRANS_DATA_EN && DUEROS_DMA_EN)

#if (TCFG_DEC2TWS_ENABLE && (TCFG_MIC_EFFECT_ENABLE ||TCFG_APP_RECORD_EN || TCFG_APP_RTC_EN ||TCFG_DRC_ENABLE))
#error "对箱支持音源转发，请关闭混响录音等功能 !!!"
#endif// (TCFG_DEC2TWS_ENABLE && (TCFG_MIC_EFFECT_ENABLE ||TCFG_APP_RECORD_EN || TCFG_APP_RTC_EN ||TCFG_DRC_ENABLE))

#if (TCFG_MIC_EFFECT_ENABLE && (TCFG_DEC_APE_ENABLE || TCFG_DEC_FLAC_ENABLE || TCFG_DEC_DTS_ENABLE))
// #error "无损格式+混响暂时不支持同时打开 !!!"
#endif//(TCFG_MIC_EFFECT_ENABLE && (TCFG_DEC_APE_ENABLE || TCFG_DEC_FLAC_ENABLE || TCFG_DEC_DTS_ENABLE))

#if (TCFG_REVERB_DODGE_EN && (USER_DIGITAL_VOLUME_ADJUST_ENABLE == 0))
#error "使用闪避功能，打开自定义数字音量调节 !!!"
#endif

#if ((TCFG_NORFLASH_DEV_ENABLE || TCFG_NOR_FS_ENABLE) &&  TCFG_UI_ENABLE)
#error "引脚复用问题，使用norflash需要关闭UI ！！！"
#endif

#if ((TCFG_APP_RECORD_EN) && (TCFG_USER_TWS_ENABLE))
#error "TWS 暂不支持录音功能"
#endif

#if ((AUDIO_EQUALLOUDNESS_CONFIG) && (SYS_VOL_TYPE == VOL_TYPE_ANALOG))
#error "开启等响度 需要使用数字音量"
#endif

#if ((TCFG_APP_FM_EN) && (TCFG_MIC_EFFECT_ENABLE) && (RECORDER_MIX_EN))
#error "FM模式下开混响暂不支持混合录音"
#endif

#if AUDIO_EQUALLOUDNESS_CONFIG
#if (TCFG_EQ_ENABLE == 0)
#error "开启等响度 需要打开EQ总使能"
#endif
#if (TCFG_EQ_ENABLE && TCFG_BT_MUSIC_EQ_ENABLE)
#error "开启等响度 需要打开EQ总使能，关闭其他模式的eq,例如关闭蓝牙播歌eq"
#endif
#endif
#if TCFG_MIC_DODGE_EN
#if !SYS_DIGVOL_GROUP_EN
#error "请使能软件数字音量SYS_DIGVOL_GROUP_EN"
#endif
#endif

#if TCFG_DYNAMIC_EQ_ENABLE && !TCFG_DRC_ENABLE //动态eq使能后，需接入drc进行幅度控制
#error "动态eq使能后，需使能TCFG_DRC_ENABLE接入drc进行幅度控制"
#endif
#if TCFG_DYNAMIC_EQ_ENABLE && !TCFG_AUDIO_MDRC_ENABLE
#error "动态eq使能后，需使能TCFG_AUDIO_MDRC_ENABLE 为1接入多带drc进行幅度控制"
#endif

#if SOUND_TRACK_2_P_X_CH_CONFIG && !TCFG_EQ_DIVIDE_ENABLE
#error  "2.1/2.2声道，需使能宏TCFG_EQ_DIVIDE_ENABLE"
#endif
#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ANALOG)
#error "AC701N Linein 仅支持数字通路"
#endif
#if (AUDIO_OUT_WAY_TYPE == AUDIO_WAY_TYPE_IIS) && (SYS_VOL_TYPE != VOL_TYPE_DIGITAL)
#error "iis输出使用 数字音量"
#endif

///<<<<所有宏定义不要在编译警告后面定义！！！！！！！！！！！！！！！！！！！！！！！！！！
///<<<<所有宏定义不要在编译警告后面定义！！！！！！！！！！！！！！！！！！！！！！！！！！







#endif
