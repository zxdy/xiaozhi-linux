// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2008-2023 100askTeam : Dongshan WEI <weidongshan@100ask.net>
 * Discourse:  https://forums.100ask.net
 */

/*  Copyright (C) 2008-2023 深圳百问网科技有限公司
 *  All rights reserved
 *
 * 免责声明: 百问网编写的文档, 仅供学员学习使用, 可以转发或引用(请保留作者信息),禁止用于商业用途！
 * 免责声明: 百问网编写的程序, 用于商业用途请遵循GPL许可, 百问网不承担任何后果！
 *
 * 本程序遵循GPL V3协议, 请遵循协议
 * 百问网学习平台   : https://www.100ask.net
 * 百问网交流社区   : https://forums.100ask.net
 * 百问网官方B站    : https://space.bilibili.com/275908810
 * 本程序所用开发板 : Linux开发板
 * 百问网官方淘宝   : https://100ask.taobao.com
 * 联系我们(E-mail) : weidongshan@100ask.net
 *
 *          版权所有，盗版必究。
 *
 * 修改历史     版本号           作者        修改内容
 *-----------------------------------------------------
 * 2025.03.20      v01         百问科技      创建文件
 *-----------------------------------------------------
 */
#ifndef __GPIO_BUTTON_H
#define __GPIO_BUTTON_H

#include <pthread.h>

/* 按钮按下时的回调函数类型 */
typedef void (*gpio_button_press_cb_t)(void);

/**
 * 注册按钮按下时的回调函数
 * 必须在 create_gpio_button_thread 之前调用
 *
 * @param cb 按钮每次按下时调用的回调
 */
void gpio_button_set_press_callback(gpio_button_press_cb_t cb);

/**
 * 创建 GPIO 按钮监控线程
 *
 * 该线程监控指定 GPIO 引脚的下降沿中断事件，
 * 每次检测到按钮按下时调用已注册的回调函数
 *
 * @param gpio_pin GPIO 引脚号
 * @return 成功返回线程 ID，失败返回 0
 */
pthread_t create_gpio_button_thread(int gpio_pin);

/**
 * 停止 GPIO 按钮监控线程
 */
void stop_gpio_button_thread(void);

#endif /* __GPIO_BUTTON_H */