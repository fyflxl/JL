/*
 * Copyright 2020 Unisound AI Technology Co., Ltd.
 * Author: Hao Peng
 * All Rights Reserved.
 */

#ifndef UAL_AIK_H_
#define UAL_AIK_H_

#include "app_config.h"

#if TCFG_UNISOUND_ENABLE

#if defined(_WIN32) || defined(_WIN64)
#define _WINDOWS
#endif

#ifndef _WINDOWS
#define AIK_EXPORT __attribute__((visibility("default")))
#else
#ifdef DLL_EXPORT
#define AIK_EXPORT __declspec(dllexport)
#else
#define AIK_EXPORT __declspec(dllimport)
#endif
#endif

#include "./ual_aik_event.h"
#include "./ual_aik_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @Description: 事件上报回调，由用户实现，可以获取到 AIK
 * 的各种状态和数据
 * @Input params: event: 事件 id, 见 aik-event.h
 *                args: 参考ual-aik-event.h中各事件对应的结构体
 * @Output params:
 * @Return:
 */
typedef void (*EventCallback)(AikEvent event, void *args);

/*
 * @Description: 数据上报回调，由用户实现，可以获取 AIK 处理后的
 *               数据（如降噪数据）
 * @Input params: event: 事件 id，见 aik-event.h
 *                data: 数据流
 *                size: data 长度
 *                index: 数据块编号
 * @Output params:
 * @Return:
 */
typedef void (*DataCallback)(AikEvent event, void *data, int size, int index);

/*
 * @Description: 设置AIS打印等级，默认为Debug
 * @Input params: level: 打印等级
 * @Output params:
 * @Return:
 */
AIK_EXPORT void UalAikSetLogLevel(int level);

/*
 * @Description: 初始化
 * @Input params: event_callback: 事件上报回调
 * @Output params:
 * @Return: 成功：0
 *          失败：其他
 */
AIK_EXPORT int UalAikInit(EventCallback event_callback);

/*
 * @Description: 设置用户输入参数（如动态修改的唤醒词语法）
 * @Input params: id: AIK_ID_XXX (ual-aik-id.h)
 *                args: 参数
 * @Output params:
 * @Return: 成功：UAL_AIK_OK
 *          失败：UAL_AIK_ERR
 */
AIK_EXPORT int UalAikSet(int id, void *args);

/*
 * @Description: 获取参数
 * @Input params:
 *                id: AIK_ID_XXX (ual-aik-id.h)
 * @Output params: args: 参数值 (ual-aik-args.h)
 * @Return: 成功：UAL_AIK_OK
 *          失败：UAL_AIK_ERR
 */
AIK_EXPORT int UalAikGet(int id, void *args);

/*
 * @Description: 启动
 * @Input params: mode: 运行模式
 * @Output params:
 * @Return: 成功：UAL_AIK_OK
 *          失败：UAL_AIK_ERR
 */
AIK_EXPORT int UalAikStart(AikMode mode);

/*
 * @Description: 处理数据 (Slave 模式专用）
 * @Input params: data: 数据
 *                size: 数据长度
 * @Output params:
 * @Return:
 */
AIK_EXPORT int UalAikUpdate(void *data, int size);

/*
 * @Description: 停止，用于打断
 * @Input params:  mode: 运行模式（用来区分打断对象）
 * @Output params:
 * @Return: 成功：AIK_OK
 *          失败：AIK_FAILED
 */
AIK_EXPORT int UalAikStop(AikMode mode);

/*
 * @Description: 释放资源
 * @Input params:
 * @Output params:
 * @Return:
 */
AIK_EXPORT void UalAikRelease();

/*
 * @Description: 获取 AIK 版本号
 * @Input params:
 * @Output params:
 * @Return:
 */
AIK_EXPORT const char *UalAikGetVersion();

#ifdef __cplusplus
}
#endif

#endif  //TCFG_VOICE_BOX_ENABLE

#endif  // UAL_AIK_H_
