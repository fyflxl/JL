#ifndef __RES_CONFIG_H__
#define __RES_CONFIG_H__


#define EXTERN_PATH "storage/virfat_flash/C/"
#define INTERN_PATH "mnt/sdfile/res/"
// #define RES_PATH   INTERN_PATH	// br27 内置falsh
#define RES_PATH   EXTERN_PATH	// br28 外挂falsh

#if (TCFG_UI_ENABLE)

#define FONT_PATH		RES_PATH"font/"
#define UPGRADE_PATH	INTERN_PATH"ui_upgrade/"

#define UI_STY_CHECK_PATH     \
    RES_PATH"JL/JL.sty",      \
    RES_PATH"watch/watch.sty",\
    RES_PATH"watch1/watch1.sty",\
    RES_PATH"watch2/watch2.sty",\
    RES_PATH"watch3/watch3.sty",\
    RES_PATH"watch4/watch4.sty",\
    RES_PATH"watch5/watch5.sty",

#define UI_RES_CHECK_PATH  \
    RES_PATH"JL/JL.res",   \
    RES_PATH"watch/watch.res",\
    RES_PATH"watch1/watch1.res",\
    RES_PATH"watch2/watch2.res",\
    RES_PATH"watch3/watch3.res",\
    RES_PATH"watch4/watch4.res",\
    RES_PATH"watch5/watch5.res",\

#define UI_STR_CHECK_PATH      \
    RES_PATH"JL/JL.str",       \
    RES_PATH"watch/watch.str", \
    RES_PATH"watch1/watch1.str",\
    RES_PATH"watch2/watch2.str",\
    RES_PATH"watch3/watch3.str",\
    RES_PATH"watch4/watch4.str",\
    RES_PATH"watch5/watch5.str",


#define UI_STY_WATCH_PATH     \
    RES_PATH"watch/watch.sty",\
    RES_PATH"watch1/watch1.sty",\
    RES_PATH"watch2/watch2.sty",\
    RES_PATH"watch3/watch3.sty",\
    RES_PATH"watch4/watch4.sty",\
    RES_PATH"watch5/watch5.sty",


#define UI_USED_DOUBLE_BUFFER   1//使用双buf推屏
#define UI_WATCH_RES_ENABLE     0//表盘功能
#define UI_UPGRADE_RES_ENABLE   1//升级界面功能


#else

#define FONT_PATH   RES_PATH

#define UI_STY_CHECK_PATH     \
    RES_PATH"JL.sty",

#define UI_RES_CHECK_PATH  \
    RES_PATH"JL.res",

#define UI_STR_CHECK_PATH      \
    RES_PATH"JL.str",

#define UI_WATCH_RES_ENABLE    0//表盘功能

#endif




#endif
