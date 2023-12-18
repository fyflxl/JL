#ifndef __BUFFER_MANAGER_H__
#define __BUFFER_MANAGER_H__


#include "list.h"


// 缓存buf的状态
typedef enum buffer_status_def {
    BUFFER_STATUS_INIT,		// 初始
    BUFFER_STATUS_IDLE,		// 空闲
    BUFFER_STATUS_LOCK,		// 锁定
    BUFFER_STATUS_PENDING,	// 等待
    BUFFER_STATUS_INUSED,	// 使用
} BUFFER_STATUS;


// buffer管理句柄定义
// typedef struct buffer_handler_def {
// struct list_head head;
// void *buffer_baddr;
// int buffer_size;
// int block_number;
// } BUFFER_MANAGER;

// buffer管理句柄定义
typedef struct list_head BUFFER_MANAGER;


// buffer管理原生API，不建议直接调用原生API，
// 防止后续库升级导致调用方法改变，使用时请调用重封装的快捷调用
int buffer_manager_set_default_handler(void *buffer_hdl);

void *buffer_manager_init_handler(void *buf, unsigned int size, int block_num);

void buffer_manager_free_handler(void *buffer_hdl);

int buffer_manager_set_buf_status(void *buffer_hdl, void *buf, int status);

int buffer_manager_get_buf_status(void *buffer_hdl, void *buf);

void *buffer_manager_get_buf_by_status(void *buffer_hdl, int status);



/* buffer管理API快捷调用 */
// 初始化buffer管理，创建buffer管理句柄
#define buffer_manager_init(buf, size, block_num) \
	buffer_manager_init_handler(buf, size, block_num)

// 释放buffer管理资源和句柄
#define buffer_manager_free(buffer_handler) \
	buffer_manager_free_handler(buffer_handler)

// 设置指定buffer的状态
#define set_buffer_status(buffer_handler, buf_addr, status) \
	buffer_manager_set_buf_status(buffer_handler, buf_addr, status)

// 获取指定buffer的状态
#define get_buffer_status(buffer_handler, buf_addr) \
	buffer_manager_get_buf_status(buffer_handler, buf_addr)

// 获取指定状态的buffer
#define get_buffer_by_status(buffer_handler, status) \
	buffer_manager_get_buf_by_status(buffer_handler, status)

// 设置默认句柄，可以设置一个默认句柄，该句柄将作为buffer管理的静态变量，外部无需每次操作都传入该句柄
// 需要注意的时，buffer_manager_free时，可以传入NULL来释放默认句柄，但大buffer还需外面主动释放
// 即：buffer管理只做状态管理，和自身资源的申请、释放，不管被管理的buffer申请和释放
#define set_default_buffer_handler(buffer_handler) \
	buffer_manager_set_default_handler(buffer_handler)




/* 快捷获取指定状态的buffer */
// 获取空闲状态的buffer
#define get_idle_buffer(buffer_handler) \
	get_buffer_by_status(buffer_handler, BUFFER_STATUS_IDLE)

// 获取锁定状态的buffer
#define get_lock_buffer(buffer_handler) \
	get_buffer_by_status(buffer_handler, BUFFER_STATUS_LOCK)

// 获取等待状态的buffer
#define get_pending_buffer(buffer_handler) \
	get_buffer_by_status(buffer_handler, BUFFER_STATUS_PENDING)

// 获取使用状态的buffer
#define get_inused_buffer(buffer_handler) \
	get_buffer_by_status(buffer_handler, BUFFER_STATUS_INUSED)




/* 快捷设置指定buffer的状态 */
// 设置buffer状态为空闲
#define set_buffer_idle(buffer_handler, buf) \
	set_buffer_status(buffer_handler, buf, BUFFER_STATUS_IDLE)

// 设置buffer状态为锁定
#define set_buffer_lock(buffer_handler, buf) \
	set_buffer_status(buffer_handler, buf, BUFFER_STATUS_LOCK)

// 设置buffer状态为等待
#define set_buffer_pending(buffer_handler, buf) \
	set_buffer_status(buffer_handler, buf, BUFFER_STATUS_PENDING)

// 设置buffer状态为使用
#define set_buffer_inused(buffer_handler, buf) \
	set_buffer_status(buffer_handler, buf, BUFFER_STATUS_INUSED)







#if 0
// 旧版API
void lcd_buffer_set_param(struct imd_param *param);
u8 *lcd_buffer_init(u8 index, u8 *baddr, u32 size);
void lcd_buffer_release(u8 index);
u8 *lcd_buffer_get(u8 index, u8 *pre_baddr);
u8 lcd_buffer_pending(u8 index, u8 *buffer);
void lcd_buffer_idle(u8 index);
bool is_imd_buf_using(u8 index, u8 *lcd_buf);
u8 *lcd_buffer_check(u8 index, u8 *lcd_buf);
u8 *lcd_buffer_next(u8 index);
#endif


#endif


