
# make 编译并下载
# make VERBOSE=1 显示编译详细过程
# make clean 清除编译临时文件
#
# 注意： Linux 下编译方式：
#     1. 从 http://pkgman.jieliapp.com/doc/all 处找到下载链接
#     2. 下载后，解压到 /opt/jieli 目录下，保证
#       /opt/jieli/common/bin/clang 存在（注意目录层次）
#     3. 确认 ulimit -n 的结果足够大（建议大于8096），否则链接可能会因为打开文件太多而失败
#       可以通过 ulimit -n 8096 来设置一个较大的值
#

# 工具路径设置
ifeq ($(OS), Windows_NT)
# Windows 下工具链位置
TOOL_DIR := C:/JL/pi32/bin
CC    := clang.exe
CXX   := clang.exe
LD    := pi32v2-lto-wrapper.exe
AR    := pi32v2-lto-ar.exe
MKDIR := mkdir_win -p
RM    := rm -rf

SYS_LIB_DIR := C:/JL/pi32/pi32v2-lib/r3-large
SYS_INC_DIR := C:/JL/pi32/pi32v2-include
EXT_CFLAGS  := # Windows 下不需要 -D__SHELL__
export PATH:=$(TOOL_DIR);$(PATH)

## 后处理脚本
FIXBAT          := tools\utils\fixbat.exe # 用于处理 utf8->gbk 编码问题
POST_SCRIPT     := cpu\br28\tools\download.bat
RUN_POST_SCRIPT := $(POST_SCRIPT)
else
# Linux 下工具链位置
TOOL_DIR := /opt/jieli/pi32v2/bin
CC    := clang
CXX   := clang++
LD    := lto-wrapper
AR    := lto-ar
MKDIR := mkdir -p
RM    := rm -rf
export OBJDUMP := $(TOOL_DIR)/objdump
export OBJCOPY := $(TOOL_DIR)/objcopy
export OBJSIZEDUMP := $(TOOL_DIR)/objsizedump

SYS_LIB_DIR := $(TOOL_DIR)/../lib/r3-large
SYS_INC_DIR := $(TOOL_DIR)/../include
EXT_CFLAGS  := -D__SHELL__ # Linux 下需要这个保证正确处理 download.c
export PATH:=$(TOOL_DIR):$(PATH)

## 后处理脚本
FIXBAT          := touch # Linux下不需要处理 bat 编码问题
POST_SCRIPT     := cpu/br28/tools/download.sh
RUN_POST_SCRIPT := bash $(POST_SCRIPT)
endif

CC  := $(TOOL_DIR)/$(CC)
CXX := $(TOOL_DIR)/$(CXX)
LD  := $(TOOL_DIR)/$(LD)
AR  := $(TOOL_DIR)/$(AR)
# 输出文件设置
OUT_ELF   := cpu/br28/tools/sdk.elf
OBJ_FILE  := $(OUT_ELF).objs.txt
# 编译路径设置
BUILD_DIR := objs

# 编译参数设置
CFLAGS := \
	-target pi32v2 \
	-mcpu=r3 \
	-integrated-as \
	-flto \
	-Wuninitialized \
	-Wno-invalid-noreturn \
	-fno-common \
	-integrated-as \
	-Oz \
	-g \
	-flto \
	-fallow-pointer-null \
	-fprefer-gnu-section \
	-Wno-shift-negative-value \
	-Wundef \
	-Wframe-larger-than=256 \
	-Wincompatible-pointer-types \
	-Wreturn-type \
	-Wimplicit-function-declaration \
	-mllvm -pi32v2-large-program=true \
	-fms-extensions \
	-fdiscrete-bitfield-abi \
	-w \


# C++额外的编译参数
CXXFLAGS :=


# 宏定义
DEFINES := \
	-DSUPPORT_MS_EXTENSIONS \
	-DCONFIG_RELEASE_ENABLE \
	-DCONFIG_CPU_BR28 \
	-DCONFIG_PRINT_IN_MASK \
	-DCONFIG_NEW_BREDR_ENABLE \
	-DCONFIG_NEW_MODEM_ENABLE \
	-DCONFIG_NEW_TWS_FORWARD_ENABLE \
	-DCONFIG_UCOS_ENABLE \
	-DCONFIG_EQ_SUPPORT_ASYNC \
	-DEQ_CORE_V1 \
	-DCONFIG_AAC_CODEC_FFT_USE_MUTEX \
	-DCONFIG_DNS_ENABLE \
	-DCONFIG_DMS_MALLOC \
	-DCONFIG_MMU_ENABLE \
	-DCONFIG_SBC_CODEC_HW \
	-DCONFIG_MSBC_CODEC_HW \
	-DCONFIG_AEC_M=256 \
	-DCONFIG_SUPPORT_WIFI_DETECT \
	-DCONFIG_AUDIO_ONCHIP \
	-DCONFIG_MEDIA_DEVELOP_ENABLE \
	-DCONFIG_MIXER_CYCLIC \
	-DCONFIG_SOUND_PLATFORM_ENABLE=1 \
	-DCONFIG_SUPPORT_EX_TWS_ADJUST \
	-D__GCC_PI32V2__ \
	-DCONFIG_NEW_ECC_ENABLE \
	-DCONFIG_MAC_ADDR_GET_USED_SFC \
	-DSUPPORT_BLUETOOTH_PROFILE_RELEASE \
	-DCONFIG_SOUNDBOX \
	-DCONFIG_OS_AFFINITY_ENABLE \
	-DEVENT_HANDLER_NUM_CONFIG=2 \
	-DEVENT_TOUCH_ENABLE_CONFIG=0 \
	-DEVENT_POOL_SIZE_CONFIG=256 \
	-DCONFIG_EVENT_KEY_MAP_ENABLE=0 \
	-DTIMER_POOL_NUM_CONFIG=15 \
	-DAPP_ASYNC_POOL_NUM_CONFIG=0 \
	-DVFS_ENABLE=1 \
	-DUSE_SDFILE_NEW=1 \
	-DSDFILE_STORAGE=1 \
	-DVFS_FILE_POOL_NUM_CONFIG=1 \
	-DFS_VERSION=0x020001 \
	-DFATFS_VERSION=0x020101 \
	-DSDFILE_VERSION=0x020000 \
	-DVM_MAX_PAGE_ALIGN_SIZE_CONFIG=64*1024 \
	-DVM_MAX_SECTOR_ALIGN_SIZE_CONFIG=64*1024 \
	-DVM_ITEM_MAX_NUM=256 \
	-DCONFIG_TWS_ENABLE \
	-DCONFIG_SOUNDBOX_CASE_ENABLE \
	-DCONFIG_NEW_CFG_TOOL_ENABLE \
	-DCONFIG_LITE_AEC_ENABLE=0 \
	-DAUDIO_REC_POOL_NUM=1 \
	-DAUDIO_DEC_POOL_NUM=3 \
	-DCONFIG_LMP_CONN_SUSPEND_ENABLE \
	-DCONFIG_BTCTRLER_TASK_DEL_ENABLE \
	-DCONFIG_LINK_DISTURB_SCAN_ENABLE=0 \
	-DCONFIG_UPDATA_ENABLE \
	-DCONFIG_OTA_UPDATA_ENABLE \
	-DCONFIG_EFFECT_CORE_V2_ENABLE \
	-DCONFIG_ITEM_FORMAT_VM \
	-D__LD__ \


DEFINES += $(EXT_CFLAGS) # 额外的一些定义

# 头文件搜索路径
INCLUDES := \
	-Iinclude_lib \
	-Iinclude_lib/driver \
	-Iinclude_lib/driver/device \
	-Iinclude_lib/driver/cpu/br28 \
	-Iinclude_lib/system \
	-Iinclude_lib/system/generic \
	-Iinclude_lib/system/device \
	-Iinclude_lib/system/fs \
	-Iinclude_lib/system/ui \
	-Iinclude_lib/btctrler \
	-Iinclude_lib/btctrler/port/br28 \
	-Iinclude_lib/update \
	-Iinclude_lib/agreement \
	-Iinclude_lib/btstack/third_party/common \
	-Iinclude_lib/btstack/third_party/rcsp \
	-Iinclude_lib/media/media_develop \
	-Iinclude_lib/media/media_develop/media \
	-Iinclude_lib/media/media_develop/media/cpu/br28 \
	-Iinclude_lib/media/media_develop/media/cpu/br28/asm \
	-Iapps/soundbox/include \
	-Iapps/soundbox/include/task_manager \
	-Iapps/soundbox/include/task_manager/bt \
	-Iapps/soundbox/include/user_api \
	-Iapps/common \
	-Iapps/common/device \
	-Iapps/common/audio \
	-Iapps/common/audio/le_audio \
	-Iapps/common/audio/stream \
	-Iapps/common/power_manage \
	-Iapps/common/charge_box \
	-Iapps/common/third_party_profile/common \
	-Iapps/common/third_party_profile/interface \
	-Iapps/common/third_party_profile/jieli \
	-Iapps/common/third_party_profile/jieli/trans_data_demo \
	-Iapps/common/third_party_profile/jieli/online_db \
	-Iapps/common/dev_manager \
	-Iapps/common/file_operate \
	-Iapps/common/music \
	-Iapps/common/include \
	-Iapps/common/config/include \
	-Iapps/soundbox/board/br28 \
	-Icpu/br28 \
	-Icpu/br28/audio_common \
	-Icpu/br28/audio_dec \
	-Icpu/br28/audio_enc \
	-Icpu/br28/audio_effect \
	-Icpu/br28/audio_mic \
	-Icpu/br28/localtws \
	-Iinclude_lib/btstack/third_party/baidu \
	-Iinclude_lib/btstack \
	-Iinclude_lib/media \
	-Iapps/common/device/usb \
	-Iapps/common/device/usb/device \
	-Iapps/common/device/usb/host \
	-Iapps/soundbox/include/ui/color_led \
	-Iapps/common/third_party_profile/jieli/rcsp \
	-Iapps/common/third_party_profile/jieli/rcsp/bt_trans_data \
	-Iapps/common/third_party_profile/jieli/rcsp/rcsp_functions \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/include \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_misc_setting \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_update \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/external_flash \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sensors_info_opt \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt \
	-Iapps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/browser \
	-Iinclude_lib/btstack/third_party/broadcast \
	-Iapps/common/wireless_dev_manager \
	-Iapps/soundbox/include/broadcast \
	-Iinclude_lib/system/ui/ui/cpu/br28 \
	-Iinclude_lib/system/math/cpu/br28 \
	-Icpu/br28/ui_driver \
	-Icpu/br28/audio_way \
	-I$(SYS_INC_DIR) \


# 需要编译的 .c 文件
c_SRC_FILES := \
	apps/common/audio/amplitude_statistic.c \
	apps/common/audio/audio_digital_vol.c \
	apps/common/audio/audio_export_demo.c \
	apps/common/audio/audio_utils.c \
	apps/common/audio/decode/audio_key_tone.c \
	apps/common/audio/decode/decode.c \
	apps/common/audio/demo/audio_mic_enc_dec_demo.c \
	apps/common/audio/encode/encode_write_file.c \
	apps/common/audio/le_audio/broadcast_audio_sync.c \
	apps/common/audio/le_audio/le_audio_stream.c \
	apps/common/audio/le_audio/le_audio_test.c \
	apps/common/audio/sine_make.c \
	apps/common/audio/stream/stream_entry.c \
	apps/common/audio/stream/stream_src.c \
	apps/common/audio/stream/stream_sync.c \
	apps/common/audio/uartPcmSender.c \
	apps/common/audio/wm8978/iic.c \
	apps/common/audio/wm8978/wm8978.c \
	apps/common/bt_common/bt_test_api.c \
	apps/common/charge_box/chargeIc_manage.c \
	apps/common/charge_box/chgbox_box.c \
	apps/common/charge_box/chgbox_ctrl.c \
	apps/common/charge_box/chgbox_det.c \
	apps/common/charge_box/chgbox_handshake.c \
	apps/common/charge_box/chgbox_ui.c \
	apps/common/charge_box/chgbox_ui_drv_pwmled.c \
	apps/common/charge_box/chgbox_ui_drv_timer.c \
	apps/common/charge_box/chgbox_wireless.c \
	apps/common/config/app_config.c \
	apps/common/config/bt_profile_config.c \
	apps/common/config/ci_transport_uart.c \
	apps/common/config/new_cfg_tool.c \
	apps/common/debug/debug.c \
	apps/common/debug/debug_lite.c \
	apps/common/dev_manager/dev_manager.c \
	apps/common/dev_manager/dev_reg.c \
	apps/common/dev_manager/dev_update.c \
	apps/common/device/bmp280/bmp280.c \
	apps/common/device/detection.c \
	apps/common/device/fm/bk1080/Bk1080.c \
	apps/common/device/fm/fm_inside/fm_inside.c \
	apps/common/device/fm/fm_manage.c \
	apps/common/device/fm/qn8035/QN8035.c \
	apps/common/device/fm/rda5807/RDA5807.c \
	apps/common/device/fm_emitter/ac3433/ac3433.c \
	apps/common/device/fm_emitter/fm_emitter_manage.c \
	apps/common/device/fm_emitter/fm_inside/fm_emitter_inside.c \
	apps/common/device/fm_emitter/qn8007/qn8007.c \
	apps/common/device/fm_emitter/qn8027/qn8027.c \
	apps/common/device/gSensor/SC7A20.c \
	apps/common/device/gSensor/da230.c \
	apps/common/device/gSensor/gSensor_manage.c \
	apps/common/device/gps/atk1218_bd.c \
	apps/common/device/key/adkey.c \
	apps/common/device/key/adkey_rtcvdd.c \
	apps/common/device/key/ctmu_touch_key.c \
	apps/common/device/key/iokey.c \
	apps/common/device/key/irkey.c \
	apps/common/device/key/key_driver.c \
	apps/common/device/key/rdec_key.c \
	apps/common/device/key/slidekey.c \
	apps/common/device/key/touch_key.c \
	apps/common/device/max30102/algorithm.c \
	apps/common/device/max30102/max30102.c \
	apps/common/device/norflash/norflash.c \
	apps/common/device/qmc5883/qmc5883.c \
	apps/common/device/usb/device/cdc.c \
	apps/common/device/usb/device/custom_hid.c \
	apps/common/device/usb/device/descriptor.c \
	apps/common/device/usb/device/hid.c \
	apps/common/device/usb/device/msd.c \
	apps/common/device/usb/device/msd_upgrade.c \
	apps/common/device/usb/device/rcsp_hid_inter.c \
	apps/common/device/usb/device/task_pc.c \
	apps/common/device/usb/device/uac1.c \
	apps/common/device/usb/device/uac_stream.c \
	apps/common/device/usb/device/usb_device.c \
	apps/common/device/usb/device/user_setup.c \
	apps/common/device/usb/host/adb.c \
	apps/common/device/usb/host/aoa.c \
	apps/common/device/usb/host/apple_mfi.c \
	apps/common/device/usb/host/audio.c \
	apps/common/device/usb/host/audio_demo.c \
	apps/common/device/usb/host/hid.c \
	apps/common/device/usb/host/usb_bulk_transfer.c \
	apps/common/device/usb/host/usb_ctrl_transfer.c \
	apps/common/device/usb/host/usb_host.c \
	apps/common/device/usb/host/usb_storage.c \
	apps/common/device/usb/usb_config.c \
	apps/common/device/usb/usb_host_config.c \
	apps/common/fat_nor/cfg_private.c \
	apps/common/fat_nor/nandflash_ftl.c \
	apps/common/fat_nor/nor_fs.c \
	apps/common/fat_nor/phone_rec_fs.c \
	apps/common/fat_nor/virfat_flash.c \
	apps/common/file_operate/file_api.c \
	apps/common/file_operate/file_bs_deal.c \
	apps/common/file_operate/file_manager.c \
	apps/common/iap/iAP_des.c \
	apps/common/iap/iAP_device.c \
	apps/common/iap/iAP_iic.c \
	apps/common/music/breakpoint.c \
	apps/common/music/general_player.c \
	apps/common/music/music_decrypt.c \
	apps/common/music/music_id3.c \
	apps/common/music/music_player.c \
	apps/common/rec_nor/nor_interface.c \
	apps/common/rec_nor/nor_rec_fs.c \
	apps/common/temp_trim/adc_dtemp_alog.c \
	apps/common/temp_trim/dtemp_pll_trim.c \
	apps/common/third_party_profile/common/3th_profile_api.c \
	apps/common/third_party_profile/common/custom_cfg.c \
	apps/common/third_party_profile/common/mic_rec.c \
	apps/common/third_party_profile/interface/app_protocol_api.c \
	apps/common/third_party_profile/interface/app_protocol_common.c \
	apps/common/third_party_profile/interface/app_protocol_dma.c \
	apps/common/third_party_profile/interface/app_protocol_gma.c \
	apps/common/third_party_profile/interface/app_protocol_mma.c \
	apps/common/third_party_profile/interface/app_protocol_ota.c \
	apps/common/third_party_profile/interface/app_protocol_tme.c \
	apps/common/third_party_profile/jieli/hid_user.c \
	apps/common/third_party_profile/jieli/le_client_demo.c \
	apps/common/third_party_profile/jieli/multi_demo/le_multi_client.c \
	apps/common/third_party_profile/jieli/multi_demo/le_multi_common.c \
	apps/common/third_party_profile/jieli/multi_demo/le_multi_trans.c \
	apps/common/third_party_profile/jieli/online_db/online_db_deal.c \
	apps/common/third_party_profile/jieli/online_db/spp_online_db.c \
	apps/common/third_party_profile/jieli/rcsp/ble_rcsp_module.c \
	apps/common/third_party_profile/jieli/rcsp/bt_trans_data/ble_rcsp_adv.c \
	apps/common/third_party_profile/jieli/rcsp/bt_trans_data/ble_rcsp_multi_client.c \
	apps/common/third_party_profile/jieli/rcsp/bt_trans_data/ble_rcsp_multi_common.c \
	apps/common/third_party_profile/jieli/rcsp/bt_trans_data/ble_rcsp_multi_trans.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/cmd_recieve.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/cmd_recieve_no_respone.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/cmd_respone.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/data_recieve.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/data_recieve_no_respone.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/data_respone.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/browser/rcsp_browser.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd/rcsp_bt_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd/rcsp_fm_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd/rcsp_linein_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd/rcsp_music_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/func_cmd/rcsp_rtc_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/rcsp_feature.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/device_info/rcsp_function.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/external_flash/rcsp_extra_flash_cmd.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/external_flash/rcsp_extra_flash_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/dev_format.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_bluk_trans_prepare.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_delete.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_env_prepare.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_simple_transfer.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_trans_back.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/file_transfer/file_transfer.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_adv_bluetooth.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_anc_voice.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_anc_voice_key.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_bt_name_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_hearing_aid_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_key_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_led_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_mic_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_time_stamp_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/adv_work_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_color_led_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_eq_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_high_low_vol_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_karaoke_eq_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_karaoke_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_misc_setting/rcsp_misc_drc_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_misc_setting/rcsp_misc_reverbration_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_misc_setting/rcsp_misc_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_music_info_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting/rcsp_vol_setting.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_setting_opt/rcsp_setting_sync.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_switch_device.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_update/rcsp_ch_loader_download.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_update/rcsp_update.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/rcsp_update/rcsp_update_tws.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/nfc_data_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sensor_log_notify.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sensors_data_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_func.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_bt_disconn.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_continuous_heart_rate.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_exercise_heart_rate.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_fall_detection.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_personal_info.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_pressure_detection.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_raise_wrist.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_sedentary.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_sensor_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_data_info_opt/sport_info_sleep_detection.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_info_opt.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_info_sync.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sport_info_vm.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_air_pressure.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_altitude.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_blood_oxygen.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_exercise_recovery_time.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_exercise_steps.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_heart_rate.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_max_oxygen_uptake.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_pressure_detection.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_sports_information.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/function_modules/sensors_data_opt/sports_data_opt/sport_data_training_load.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/rcsp_cmd_user.c \
	apps/common/third_party_profile/jieli/rcsp/cmd_data_deal/rcsp_command.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp_functions/rcsp_bt_manage.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp_functions/rcsp_config.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp_functions/rcsp_event.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp_functions/rcsp_manage.c \
	apps/common/third_party_profile/jieli/rcsp/rcsp_functions/rcsp_task.c \
	apps/common/third_party_profile/jieli/trans_data_demo/le_trans_data.c \
	apps/common/third_party_profile/jieli/trans_data_demo/spp_trans_data.c \
	apps/common/ui/interface/ui_platform.c \
	apps/common/ui/interface/ui_pushScreen_manager.c \
	apps/common/ui/interface/ui_resources_manager.c \
	apps/common/ui/interface/ui_synthesis_manager.c \
	apps/common/ui/interface/ui_synthesis_oled.c \
	apps/common/ui/interface/watch_bgp.c \
	apps/common/ui/lcd/lcd_ui_api.c \
	apps/common/ui/lcd1602/lcd1602a.c \
	apps/common/ui/lcd_drive/lcd_mcu/lcd_mcu_JD5858_360x360.c \
	apps/common/ui/lcd_drive/lcd_rgb/lcd_rgb_st7789v_240x240.c \
	apps/common/ui/lcd_drive/lcd_spi/lcd_spi_rm69330_454x454.c \
	apps/common/ui/lcd_drive/lcd_spi/lcd_spi_sh8601a_454x454.c \
	apps/common/ui/lcd_drive/lcd_spi/lcd_spi_st7789v_240x240.c \
	apps/common/ui/lcd_drive/lcd_spi/oled_spi_ssd1306_128x64.c \
	apps/common/ui/lcd_drive/spi_oled_drive.c \
	apps/common/ui/lcd_simple/lcd_simple_api.c \
	apps/common/ui/lcd_simple/ui.c \
	apps/common/ui/lcd_simple/ui_mainmenu.c \
	apps/common/ui/led7/led7_ui_api.c \
	apps/common/update/norflash_ufw_update.c \
	apps/common/update/norflash_update.c \
	apps/common/update/testbox_update.c \
	apps/common/update/uart_update.c \
	apps/common/update/uart_update_master.c \
	apps/common/update/update.c \
	apps/common/wireless_dev_manager/wireless_dev_manager.c \
	apps/soundbox/aec/br28/audio_aec.c \
	apps/soundbox/aec/br28/audio_aec_demo.c \
	apps/soundbox/aec/br28/audio_aec_online.c \
	apps/soundbox/app_main.c \
	apps/soundbox/board/br28/board_jl7012f_demo/board_jl7012f_demo.c \
	apps/soundbox/board/br28/board_jl7012f_demo/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl7012f_demo/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl7012f_demo/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl7012f_demo/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl7012f_demo/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_a90/board_jl701n_a90.c \
	apps/soundbox/board/br28/board_jl701n_a90/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_a90/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_a90/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_a90/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_a90/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/board_jl701n_btemitter.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_btemitter/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_demo/board_jl701n_demo.c \
	apps/soundbox/board/br28/board_jl701n_demo/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_demo/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_demo/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_demo/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_demo/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_k8957/board_jl701n_k8957.c \
	apps/soundbox/board/br28/board_jl701n_k8957/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_k8957/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_k8957/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_k8957/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_k8957/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/board_jl701n_smartbox.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_smartbox/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/board_jl701n_tws_box.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_tws_box/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_unisound/board_jl701n_unisound.c \
	apps/soundbox/board/br28/board_jl701n_unisound/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_unisound/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_unisound/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_unisound/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_unisound/key_table/touch_key_table.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/board_jl701n_wireless_1tN.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/key_table/adkey_table.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/key_table/iokey_table.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/key_table/irkey_table.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/key_table/rdec_key_table.c \
	apps/soundbox/board/br28/board_jl701n_wireless_1tN/key_table/touch_key_table.c \
	apps/soundbox/broadcast/app_broadcast.c \
	apps/soundbox/broadcast/broadcast.c \
	apps/soundbox/broadcast/broadcast_test.c \
	apps/soundbox/common/dev_status.c \
	apps/soundbox/common/init.c \
	apps/soundbox/common/task_table.c \
	apps/soundbox/common/tone_table.c \
	apps/soundbox/common/user_cfg_new.c \
	apps/soundbox/font/fontinit.c \
	apps/soundbox/log_config/app_config.c \
	apps/soundbox/log_config/lib_btctrler_config.c \
	apps/soundbox/log_config/lib_btstack_config.c \
	apps/soundbox/log_config/lib_driver_config.c \
	apps/soundbox/log_config/lib_media_config.c \
	apps/soundbox/log_config/lib_system_config.c \
	apps/soundbox/log_config/lib_update_config.c \
	apps/soundbox/power_manage/app_charge.c \
	apps/soundbox/power_manage/app_chargestore.c \
	apps/soundbox/power_manage/app_power_manage.c \
	apps/soundbox/soundcard/lamp.c \
	apps/soundbox/soundcard/notice.c \
	apps/soundbox/soundcard/peripheral.c \
	apps/soundbox/soundcard/soundcard.c \
	apps/soundbox/task_manager/app_common.c \
	apps/soundbox/task_manager/app_task_switch.c \
	apps/soundbox/task_manager/broadcast_mic/broadcast_mic.c \
	apps/soundbox/task_manager/broadcast_mic/mic_api.c \
	apps/soundbox/task_manager/bt/bt.c \
	apps/soundbox/task_manager/bt/bt_ble.c \
	apps/soundbox/task_manager/bt/bt_emitter.c \
	apps/soundbox/task_manager/bt/bt_event_fun.c \
	apps/soundbox/task_manager/bt/bt_key_fun.c \
	apps/soundbox/task_manager/bt/bt_product_test.c \
	apps/soundbox/task_manager/bt/bt_switch_fun.c \
	apps/soundbox/task_manager/bt/bt_tws.c \
	apps/soundbox/task_manager/bt/bt_tws_new.c \
	apps/soundbox/task_manager/bt/bt_vartual_fast_connect.c \
	apps/soundbox/task_manager/bt/vol_sync.c \
	apps/soundbox/task_manager/fm/fm.c \
	apps/soundbox/task_manager/fm/fm_api.c \
	apps/soundbox/task_manager/fm/fm_rw.c \
	apps/soundbox/task_manager/idle/idle.c \
	apps/soundbox/task_manager/linein/linein.c \
	apps/soundbox/task_manager/linein/linein_api.c \
	apps/soundbox/task_manager/linein/linein_dev.c \
	apps/soundbox/task_manager/music/music.c \
	apps/soundbox/task_manager/pc/pc.c \
	apps/soundbox/task_manager/power_off/power_off.c \
	apps/soundbox/task_manager/power_on/power_on.c \
	apps/soundbox/task_manager/record/record.c \
	apps/soundbox/task_manager/rtc/alarm_api.c \
	apps/soundbox/task_manager/rtc/alarm_user.c \
	apps/soundbox/task_manager/rtc/rtc.c \
	apps/soundbox/task_manager/rtc/virtual_rtc.c \
	apps/soundbox/task_manager/sleep/sleep.c \
	apps/soundbox/task_manager/spdif/hdmi_cec_drv.c \
	apps/soundbox/task_manager/spdif/spdif.c \
	apps/soundbox/task_manager/task_key.c \
	apps/soundbox/task_manager/unisound/unisound_ais100.c \
	apps/soundbox/task_manager/unisound/unisound_mic.c \
	apps/soundbox/third_party_profile/ancs_client_demo/ancs_client_demo.c \
	apps/soundbox/third_party_profile/app_protocol_deal.c \
	apps/soundbox/third_party_profile/trans_data_demo/trans_data_demo.c \
	apps/soundbox/ui/color_led/color_led_app.c \
	apps/soundbox/ui/color_led/color_led_table.c \
	apps/soundbox/ui/color_led/driver/color_led.c \
	apps/soundbox/ui/color_led/driver/color_led_driver.c \
	apps/soundbox/ui/lcd/STYLE_02/bt_action.c \
	apps/soundbox/ui/lcd/STYLE_02/clock_action.c \
	apps/soundbox/ui/lcd/STYLE_02/file_brower.c \
	apps/soundbox/ui/lcd/STYLE_02/fm_action.c \
	apps/soundbox/ui/lcd/STYLE_02/linein_action.c \
	apps/soundbox/ui/lcd/STYLE_02/music_action.c \
	apps/soundbox/ui/lcd/STYLE_02/pc_action.c \
	apps/soundbox/ui/lcd/STYLE_02/record_action.c \
	apps/soundbox/ui/lcd/STYLE_02/system_action.c \
	apps/soundbox/ui/lcd/lyrics_api.c \
	apps/soundbox/ui/lcd/ui_sys_param_api.c \
	apps/soundbox/ui/lcd_simple/my_demo.c \
	apps/soundbox/ui/led/pwm_led_api.c \
	apps/soundbox/ui/led/pwm_led_para_table.c \
	apps/soundbox/ui/led7/ui_bt.c \
	apps/soundbox/ui/led7/ui_common.c \
	apps/soundbox/ui/led7/ui_fm.c \
	apps/soundbox/ui/led7/ui_fm_emitter.c \
	apps/soundbox/ui/led7/ui_idle.c \
	apps/soundbox/ui/led7/ui_linein.c \
	apps/soundbox/ui/led7/ui_music.c \
	apps/soundbox/ui/led7/ui_pc.c \
	apps/soundbox/ui/led7/ui_record.c \
	apps/soundbox/ui/led7/ui_rtc.c \
	apps/soundbox/user_api/app_pwmled_api.c \
	apps/soundbox/user_api/app_record_api.c \
	apps/soundbox/user_api/app_special_play_api.c \
	apps/soundbox/user_api/app_status_api.c \
	apps/soundbox/user_api/dev_multiplex_api.c \
	apps/soundbox/user_api/product_info_api.c \
	apps/soundbox/version.c \
	apps/soundbox/wireless/ble/big.c \
	cpu/br28/adc_api.c \
	cpu/br28/aec_tool.c \
	cpu/br28/audio_codec_clock.c \
	cpu/br28/audio_common/app_audio.c \
	cpu/br28/audio_common/sound_device_driver.c \
	cpu/br28/audio_common/sound_pcm_demo.c \
	cpu/br28/audio_dec/audio_dec.c \
	cpu/br28/audio_dec/audio_dec_bt.c \
	cpu/br28/audio_dec/audio_dec_file.c \
	cpu/br28/audio_dec/audio_dec_fm.c \
	cpu/br28/audio_dec/audio_dec_linein.c \
	cpu/br28/audio_dec/audio_dec_midi_ctrl.c \
	cpu/br28/audio_dec/audio_dec_midi_file.c \
	cpu/br28/audio_dec/audio_dec_pc.c \
	cpu/br28/audio_dec/audio_dec_record.c \
	cpu/br28/audio_dec/audio_dec_spdif.c \
	cpu/br28/audio_dec/audio_dec_tone.c \
	cpu/br28/audio_dec/audio_spectrum.c \
	cpu/br28/audio_dec/audio_sync.c \
	cpu/br28/audio_dec/audio_usb_mic.c \
	cpu/br28/audio_dec/broadcast_dec.c \
	cpu/br28/audio_dec/lfwordana_enc_api.c \
	cpu/br28/audio_dec/tone_player.c \
	cpu/br28/audio_demo/audio_adc_demo.c \
	cpu/br28/audio_demo/audio_dac_demo.c \
	cpu/br28/audio_demo/audio_fft_demo.c \
	cpu/br28/audio_demo/audio_matrix_demo.c \
	cpu/br28/audio_demo/audio_multiple_mic_rec_demo.c \
	cpu/br28/audio_effect/audio_autotune_demo.c \
	cpu/br28/audio_effect/audio_ch_swap_demo.c \
	cpu/br28/audio_effect/audio_dynamic_eq_demo.c \
	cpu/br28/audio_effect/audio_eff_default_parm.c \
	cpu/br28/audio_effect/audio_eq_drc_demo.c \
	cpu/br28/audio_effect/audio_gain_process_demo.c \
	cpu/br28/audio_effect/audio_noise_gate_demo.c \
	cpu/br28/audio_effect/audio_sound_track_2_p_x.c \
	cpu/br28/audio_effect/audio_surround_demo.c \
	cpu/br28/audio_effect/audio_vbass_demo.c \
	cpu/br28/audio_effect/audio_voice_changer_demo.c \
	cpu/br28/audio_effect/effects_adj.c \
	cpu/br28/audio_effect/eq_config.c \
	cpu/br28/audio_enc/audio_adc_demo.c \
	cpu/br28/audio_enc/audio_enc.c \
	cpu/br28/audio_enc/audio_enc_file.c \
	cpu/br28/audio_enc/audio_enc_recoder.c \
	cpu/br28/audio_enc/audio_mic_codec.c \
	cpu/br28/audio_enc/audio_recorder_mix.c \
	cpu/br28/audio_enc/audio_sbc_codec.c \
	cpu/br28/audio_enc/broadcast_enc.c \
	cpu/br28/audio_general.c \
	cpu/br28/audio_link.c \
	cpu/br28/audio_mic/effect_linein.c \
	cpu/br28/audio_mic/effect_parm.c \
	cpu/br28/audio_mic/effect_reg.c \
	cpu/br28/audio_mic/loud_speaker.c \
	cpu/br28/audio_mic/mic_effect.c \
	cpu/br28/audio_mic/mic_stream.c \
	cpu/br28/audio_mic/simpleAGC.c \
	cpu/br28/audio_mic_capless.c \
	cpu/br28/audio_way/audio_app_stream.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_controller.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_dma.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_hw.c \
	cpu/br28/audio_way/audio_way.c \
	cpu/br28/audio_way/audio_way_bt_emitter.c \
	cpu/br28/audio_way/audio_way_dac.c \
	cpu/br28/charge.c \
	cpu/br28/chargestore.c \
	cpu/br28/clock_manager.c \
	cpu/br28/hw_fft.c \
	cpu/br28/iic_hw.c \
	cpu/br28/iic_soft.c \
	cpu/br28/irflt.c \
	cpu/br28/localtws/localtws.c \
	cpu/br28/localtws/localtws_dec.c \
	cpu/br28/lp_touch_key.c \
	cpu/br28/lp_touch_key_tool.c \
	cpu/br28/lua_port_api.c \
	cpu/br28/mcpwm.c \
	cpu/br28/overlay_code.c \
	cpu/br28/plcnt.c \
	cpu/br28/power/power_app.c \
	cpu/br28/power/power_check.c \
	cpu/br28/power/power_port.c \
	cpu/br28/power/power_trim.c \
	cpu/br28/psram.c \
	cpu/br28/pwm_led.c \
	cpu/br28/rdec.c \
	cpu/br28/setup.c \
	cpu/br28/sound_iis.c \
	cpu/br28/spi.c \
	cpu/br28/uart_dev.c \
	cpu/br28/ui_driver/LED_1888/LED1888.c \
	cpu/br28/ui_driver/lcd_seg/lcd_seg3x9_driver.c \
	cpu/br28/ui_driver/led7/led7_driver.c \
	cpu/br28/ui_driver/led7_timer.c \
	cpu/br28/ui_driver/ui_common.c \
	cpu/br28/umidigi_chargestore.c \


# 需要编译的 .S 文件
S_SRC_FILES := \
	apps/soundbox/sdk_version.z.S \


# 需要编译的 .s 文件
s_SRC_FILES :=


# 需要编译的 .cpp 文件
cpp_SRC_FILES :=


# 需要编译的 .cc 文件
cc_SRC_FILES :=


# 需要编译的 .cxx 文件
cxx_SRC_FILES :=


# 链接参数
LFLAGS := \
	--plugin-opt=-pi32v2-always-use-itblock=false \
	--plugin-opt=-enable-ipra=true \
	--plugin-opt=-pi32v2-merge-max-offset=4096 \
	--plugin-opt=-pi32v2-enable-simd=true \
	--plugin-opt=mcpu=r3 \
	--plugin-opt=-global-merge-on-const \
	--plugin-opt=-inline-threshold=5 \
	--plugin-opt=-inline-max-allocated-size=32 \
	--plugin-opt=-inline-normal-into-special-section=true \
	--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
	--plugin-opt=save-temps \
	--plugin-opt=-pi32v2-enable-rep-memop \
	--plugin-opt=-warn-stack-size=256 \
	--sort-common \
	--plugin-opt=-used-symbol-file=cpu/br28/sdk_used_list.used \
	--plugin-opt=-pi32v2-large-program=true \
	--gc-sections \
	--start-group \
	cpu/br28/liba/cpu.a \
	cpu/br28/liba/system.a \
	cpu/br28/liba/btstack.a \
	cpu/br28/liba/rcsp_stack.a \
	cpu/br28/liba/tme_stack.a \
	cpu/br28/liba/btctrler.a \
	cpu/br28/liba/aec.a \
	cpu/br28/liba/lib_dns.a \
	cpu/br28/liba/libjlsp.a \
	cpu/br28/liba/media.a \
	cpu/br28/liba/libepmotion.a \
	cpu/br28/liba/libAptFilt_pi32v2_OnChip.a \
	cpu/br28/liba/libEchoSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libNoiseSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libSplittingFilter_pi32v2_OnChip.a \
	cpu/br28/liba/libSMS_TDE_pi32v2_OnChip.a \
	cpu/br28/liba/libDelayEstimate_pi32v2_OnChip.a \
	cpu/br28/liba/libAdaptiveEchoSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libOpcore_maskrom_pi32v2_OnChip.a \
	cpu/br28/liba/lib_resample_cal.a \
	cpu/br28/liba/lib_resample_fast_cal.a \
	cpu/br28/liba/lib_esco_repair.a \
	cpu/br28/liba/sbc_eng_lib.a \
	cpu/br28/liba/mp3_dec_lib.a \
	cpu/br28/liba/mp3tsy_dec_lib.a \
	cpu/br28/liba/mp3_decstream_lib.a \
	cpu/br28/liba/m4a_tws_lib.a \
	cpu/br28/liba/wav_dec_lib.a \
	cpu/br28/liba/wma_dec_lib.a \
	cpu/br28/liba/flac_dec_lib.a \
	cpu/br28/liba/ape_dec_lib.a \
	cpu/br28/liba/dts_dec_lib.a \
	cpu/br28/liba/amr_dec_lib.a \
	cpu/br28/liba/agreement.a \
	cpu/br28/liba/media_app.a \
	cpu/br28/liba/jla_dec_lib.a \
	cpu/br28/liba/opus_dec_lib.a \
	cpu/br28/liba/SpectrumShow.a \
	cpu/br28/liba/lib_midi_dec.a \
	cpu/br28/liba/bt_hash_enc.a \
	cpu/br28/liba/lib_word_ana.a \
	cpu/br28/liba/drc.a \
	cpu/br28/liba/DynamicEQ.a \
	cpu/br28/liba/lib_voiceChager.a \
	cpu/br28/liba/VirtualBass.a \
	cpu/br28/liba/lib_sur_cal.a \
	cpu/br28/liba/loudness.a \
	cpu/br28/liba/howling.a \
	cpu/br28/liba/noisegate.a \
	cpu/br28/liba/lib_howlings_phf.a \
	cpu/br28/liba/crossover_coff.a \
	cpu/br28/liba/lib_reverb_cal.a \
	cpu/br28/liba/lib_pitch_speed.a \
	cpu/br28/liba/lfaudio_plc_lib.a \
	cpu/br28/liba/libFFT_pi32v2_OnChip.a \
	cpu/br28/liba/wtg_dec_lib.a \
	cpu/br28/liba/bfilterfun_lib.a \
	cpu/br28/liba/crypto_toolbox_Osize.a \
	cpu/br28/liba/unisound/libaik.a \
	cpu/br28/liba/unisound/libkws.a \
	cpu/br28/liba/unisound/libssp.a \
	cpu/br28/liba/unisound/libssp_1mic.a \
	cpu/br28/liba/unisound/libssp_2mic_aec.a \
	cpu/br28/liba/unisound/libssp_2mic_mlp_aec.a \
	cpu/br28/liba/unisound/libual-osal.a \
	cpu/br28/liba/unisound/libumd.a \
	cpu/br28/liba/mp3_enc_lib.a \
	cpu/br28/liba/lib_mp2_code.a \
	cpu/br28/liba/lib_adpcm_code.a \
	cpu/br28/liba/lib_adpcm_ima_code.a \
	cpu/br28/liba/speex_enc_lib.a \
	cpu/br28/liba/amr_enc_lib.a \
	cpu/br28/liba/jla_codec_lib.a \
	cpu/br28/liba/opus_enc_lib.a \
	cpu/br28/liba/update.a \
	cpu/br28/liba/ui_dot.a \
	cpu/br28/liba/ui_cpu.a \
	cpu/br28/liba/ui_draw.a \
	cpu/br28/liba/font.a \
	cpu/br28/liba/res.a \
	--end-group \
	-Tcpu/br28/sdk.ld \
	-M=cpu/br28/tools/sdk.map \
	--plugin-opt=mcpu=r3 \
	--plugin-opt=-mattr=+fprev1 \


LIBPATHS := \
	-L$(SYS_LIB_DIR) \


LIBS := \
	$(SYS_LIB_DIR)/libm.a \
	$(SYS_LIB_DIR)/libc.a \
	$(SYS_LIB_DIR)/libm.a \
	$(SYS_LIB_DIR)/libcompiler-rt.a \



c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)
S_OBJS    := $(S_SRC_FILES:%.S=%.S.o)
s_OBJS    := $(s_SRC_FILES:%.s=%.s.o)
cpp_OBJS  := $(cpp_SRC_FILES:%.cpp=%.cpp.o)
cxx_OBJS  := $(cxx_SRC_FILES:%.cxx=%.cxx.o)
cc_OBJS   := $(cc_SRC_FILES:%.cc=%.cc.o)

OBJS      := $(c_OBJS) $(S_OBJS) $(s_OBJS) $(cpp_OBJS) $(cxx_OBJS) $(cc_OBJS)
DEP_FILES := $(OBJS:%.o=%.d)


OBJS      := $(addprefix $(BUILD_DIR)/, $(OBJS))
DEP_FILES := $(addprefix $(BUILD_DIR)/, $(DEP_FILES))


VERBOSE ?= 0
ifeq ($(VERBOSE), 1)
QUITE :=
else
QUITE := @
endif

# 一些旧的 make 不支持 file 函数，需要 make 的时候指定 LINK_AT=0 make
LINK_AT ?= 1

# 表示下面的不是一个文件的名字，无论是否存在 all, clean, pre_build 这样的文件
# 还是要执行命令
# see: https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean pre_build

# 不要使用 make 预设置的规则
# see: https://www.gnu.org/software/make/manual/html_node/Suffix-Rules.html
.SUFFIXES:

all: pre_build $(OUT_ELF)
	$(info +POST-BUILD)
	$(QUITE) $(RUN_POST_SCRIPT) sdk

pre_build:
	$(info +PRE-BUILD)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -E -P cpu/br28/sdk_used_list.c -o cpu/br28/sdk_used_list.used
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -E -P cpu/br28/sdk_ld.c -o cpu/br28/sdk.ld
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -E -P cpu/br28/tools/download.c -o $(POST_SCRIPT)
	$(QUITE) $(FIXBAT) $(POST_SCRIPT)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -E -P cpu/br28/tools/isd_config_rule.c -o cpu/br28/tools/isd_config.ini

clean:
	$(QUITE) $(RM) $(OUT_ELF)
	$(QUITE) $(RM) $(BUILD_DIR)



ifeq ($(LINK_AT), 1)
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(file >$(OBJ_FILE), $(OBJS))
	$(QUITE) $(LD) -o $(OUT_ELF) @$(OBJ_FILE) $(LFLAGS) $(LIBPATHS) $(LIBS)
else
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(QUITE) $(LD) -o $(OUT_ELF) $(OBJS) $(LFLAGS) $(LIBPATHS) $(LIBS)
endif


$(BUILD_DIR)/%.c.o : %.c
	$(info +CC $<)
	@$(MKDIR) $(@D)
	@$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.S.o : %.S
	$(info +AS $<)
	@$(MKDIR) $(@D)
	@$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.s.o : %.s
	$(info +AS $<)
	@$(MKDIR) $(@D)
	@$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.cpp.o : %.cpp
	$(info +CXX $<)
	@$(MKDIR) $(@D)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.cxx.o : %.cxx
	$(info +CXX $<)
	@$(MKDIR) $(@D)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.cc.o : %.cc
	$(info +CXX $<)
	@$(MKDIR) $(@D)
	@$(CXX) $(CFLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

-include $(DEP_FILES)
