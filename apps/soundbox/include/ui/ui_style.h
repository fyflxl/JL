#ifndef UI_STYLE_H
#define UI_STYLE_H

//#include "ui/style_led7.h"//led7



#if(CONFIG_UI_STYLE == STYLE_JL_WTACH)
/* #include "ui/style_jl01.h"//彩屏//  */
// #define ID_WINDOW_MAIN         PAGE_3
// #define ID_WINDOW_BT           PAGE_0
// #define ID_WINDOW_MUSIC        PAGE_1
// #define ID_WINDOW_LINEIN       PAGE_1
// #define ID_WINDOW_FM           PAGE_2
// #define ID_WINDOW_CLOCK        PAGE_0
// #define ID_WINDOW_BT_MENU      PAGE_3
// #define ID_WINDOW_POWER_ON     PAGE_4
// #define ID_WINDOW_POWER_OFF    PAGE_5
/* #define CONFIG_UI_STYLE_JL_ENABLE */


#include "ui/style_JL.h"
#include "ui/style_DIAL.h"
#include "ui/style_upgrade.h"
#include "ui/style_sidebar.h"
//#include "ui/style_WATCH.h"
//#include "ui/style_WATCH1.h"
//#include "ui/style_WATCH2.h"
//#include "ui/style_WATCH3.h"
//#include "ui/style_WATCH4.h"
//#include "ui/style_WATCH5.h"

#ifdef PAGE_0
#undef PAGE_0
#define PAGE_0 DIAL_PAGE_0
#endif

#define ID_WINDOW_LINEIN       			PAGE_1
#define ID_WINDOW_FM           			PAGE_2

#define ID_WINDOW_BT                    PAGE_7
#define ID_WINDOW_CLOCK                 PAGE_0
#define ID_WINDOW_ACTIVERECORD          PAGE_1
#define ID_WINDOW_SLEEP                 PAGE_2
#define ID_WINDOW_MAIN                  PAGE_3
#define ID_WINDOW_POWER_ON              PAGE_4
#define ID_WINDOW_POWER_OFF             PAGE_5
#define ID_WINDOW_VMENU                 PAGE_7
#define ID_WINDOW_PHONE                 PAGE_6
#define ID_WINDOW_MUSIC                 PAGE_10
#define ID_WINDOW_IDLE                  PAGE_15
#define ID_WINDOW_PHONEBOOK             PAGE_19
#define ID_WINDOW_PHONEBOOK_SYNC        PAGE_20
#define ID_WINDOW_MUSIC_SET             PAGE_21
#define ID_WINDOW_CALLRECORD            PAGE_22
#define ID_WINDOW_PAGE                  PAGE_23
#define ID_WINDOW_MUSIC_BROWER          PAGE_28
#define ID_WINDOW_PC                    PAGE_32
#define ID_WINDOW_STOPWATCH             PAGE_33
#define ID_WINDOW_CALCULAGRAPH          PAGE_34
#define ID_WINDOW_SET                   PAGE_41
#define ID_WINDOW_ALARM                 PAGE_51
#define ID_WINDOW_FLASHLIGHT            PAGE_52
#define ID_WINDOW_FINDPHONE             PAGE_53
#define ID_WINDOW_TRAIN                 PAGE_54
#define ID_WINDOW_SPORT_SHOW            PAGE_55
#define ID_WINDOW_BREATH_TRAIN          PAGE_57
#define ID_WINDOW_ALTIMETER				PAGE_63
#define ID_WINDOW_BARO                  PAGE_64
#define ID_WINDOW_WEATHER               PAGE_66
#define ID_WINDOW_PRESSURE              PAGE_67
#define ID_WINDOW_MESS                  PAGE_74
#define ID_WINDOW_HEART                 PAGE_77
#define ID_WINDOW_SPORT_RECORD          PAGE_75
#define ID_WINDOW_BLOOD_OXYGEN          PAGE_78
#define ID_WINDOW_SPORT_INFO            PAGE_79
#define ID_WINDOW_SPORT_CTRL			PAGE_80
#define ID_WINDOW_DETECTION				PAGE_82
#define ID_WINDOW_FALL					PAGE_81
#define ID_WINDOW_SHUTDOWN_OR_RESET     0
// #define ID_WINDOW_BT                 PAGE_17
// #define ID_WINDOW_BT_MENU            PAGE_3
// #define ID_WINDOW_TARGET             PAGE_54
#define ID_WINDOW_TRAIN_STATUS			0
#define ID_WINDOW_UPGRADE			    PAGE_0

#define ID_WINDOW_ALARM_RING_START			    PAGE_71
#define ID_WINDOW_ALARM_RING_STOP			    PAGE_72
#define ID_WINDOW_ALARM_RING_SOON			    PAGE_73

#define CONFIG_UI_STYLE_JL_ENABLE



#endif



#if(CONFIG_UI_STYLE == STYLE_JL_SOUNDBOX)
#include "ui/style_jl02.h"//点阵//
#define ID_WINDOW_MAIN     PAGE_0
#define ID_WINDOW_BT       PAGE_1
#define ID_WINDOW_FM       PAGE_2
#define ID_WINDOW_CLOCK    PAGE_3
#define ID_WINDOW_MUSIC    PAGE_4
#define ID_WINDOW_LINEIN   PAGE_9
#define ID_WINDOW_POWER_ON     PAGE_5
#define ID_WINDOW_POWER_OFF    PAGE_6
#define ID_WINDOW_SYS      PAGE_7
#define ID_WINDOW_PC       PAGE_8
#define ID_WINDOW_IDLE       (-1)
#define ID_WINDOW_REC      PAGE_10
#endif



#if(CONFIG_UI_STYLE == STYLE_JL_LED7)//led7 显示
#define ID_WINDOW_BT           UI_BT_MENU_MAIN
#define ID_WINDOW_FM           UI_FM_MENU_MAIN
#define ID_WINDOW_CLOCK        UI_RTC_MENU_MAIN
#define ID_WINDOW_MUSIC        UI_MUSIC_MENU_MAIN
#define ID_WINDOW_LINEIN       UI_AUX_MENU_MAIN
#define ID_WINDOW_PC           UI_PC_MENU_MAIN
#define ID_WINDOW_REC          UI_RECORD_MENU_MAIN
#define ID_WINDOW_POWER_ON     UI_IDLE_MENU_MAIN
#define ID_WINDOW_POWER_OFF    UI_IDLE_MENU_MAIN
#define ID_WINDOW_SPDIF        UI_IDLE_MENU_MAIN
#define ID_WINDOW_IDLE         UI_IDLE_MENU_MAIN
#endif

#if(CONFIG_UI_STYLE == STYLE_UI_SIMPLE)//lcd 去架构
#define ID_WINDOW_BT           (0)
#define ID_WINDOW_FM           (0)
#define ID_WINDOW_CLOCK        (0)
#define ID_WINDOW_MUSIC        (0)
#define ID_WINDOW_LINEIN       (0)
#define ID_WINDOW_PC           (0)
#define ID_WINDOW_REC          (0)
#define ID_WINDOW_POWER_ON     (0)
#define ID_WINDOW_POWER_OFF    (0)
#define ID_WINDOW_SPDIF        (0)
#define ID_WINDOW_IDLE         (0)
#endif



#endif
