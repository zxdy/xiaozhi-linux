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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#include "gpio_button.h"

static gpio_button_press_cb_t g_press_cb = NULL;
static volatile int g_gpio_running = 1;
static pthread_t g_gpio_thread_id = 0;

void gpio_button_set_press_callback(gpio_button_press_cb_t cb)
{
    g_press_cb = cb;
}

/**
 * 导出 GPIO 引脚
 *
 * @param gpio_pin GPIO 引脚号
 * @return 成功返回 0，失败返回 -1
 */
static int gpio_export(int gpio_pin) {
    int fd;
    char buf[64];
    ssize_t len;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open /sys/class/gpio/export");
        return -1;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio_pin);
    if (write(fd, buf, len) < 0) {
        // GPIO 可能已经导出，不算错误
        fprintf(stderr, "GPIO %d may already be exported\n", gpio_pin);
    }
    close(fd);

    // 等待 sysfs 文件创建完成
    usleep(100000);  // 100ms

    return 0;
}

/**
 * 设置 GPIO 引脚方向为输入
 *
 * @param gpio_pin GPIO 引脚号
 * @return 成功返回 0，失败返回 -1
 */
static int gpio_set_direction_input(int gpio_pin) {
    int fd;
    char path[64];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio direction file");
        return -1;
    }

    if (write(fd, "in", 2) < 0) {
        perror("Failed to set gpio direction to input");
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

/**
 * 设置 GPIO 边沿触发方式
 *
 * @param gpio_pin GPIO 引脚号
 * @param edge 触发方式: "rising", "falling", "both", "none"
 * @return 成功返回 0，失败返回 -1
 */
static int gpio_set_edge(int gpio_pin, const char *edge) {
    int fd;
    char path[64];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", gpio_pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio edge file");
        return -1;
    }

    if (write(fd, edge, strlen(edge)) < 0) {
        perror("Failed to set gpio edge");
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

/**
 * 打开 GPIO value 文件
 *
 * @param gpio_pin GPIO 引脚号
 * @return 成功返回文件描述符，失败返回 -1
 */
static int gpio_open_value(int gpio_pin) {
    int fd;
    char path[64];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pin);
    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open gpio value file");
        return -1;
    }

    return fd;
}

/**
 * GPIO 按钮监控线程主函数
 *
 * @param arg GPIO 引脚号指针
 * @return 线程退出返回 NULL
 */
static void* gpio_button_thread_func(void* arg) {
    int gpio_pin = *(int*)arg;
    int fd;
    struct pollfd pfd;
    char buf[8];
    int ret;

    printf("GPIO button thread started, monitoring GPIO %d\n", gpio_pin);

    // 导出 GPIO
    if (gpio_export(gpio_pin) < 0) {
        fprintf(stderr, "Failed to export GPIO %d\n", gpio_pin);
        return NULL;
    }

    // 设置为输入
    if (gpio_set_direction_input(gpio_pin) < 0) {
        fprintf(stderr, "Failed to set GPIO %d as input\n", gpio_pin);
        return NULL;
    }

    // 设置下降沿触发
    if (gpio_set_edge(gpio_pin, "falling") < 0) {
        fprintf(stderr, "Failed to set GPIO %d edge to falling\n", gpio_pin);
        return NULL;
    }

    // 打开 value 文件
    fd = gpio_open_value(gpio_pin);
    if (fd < 0) {
        fprintf(stderr, "Failed to open GPIO %d value file\n", gpio_pin);
        return NULL;
    }

    // 先读取一次当前值，清除任何待处理的事件
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, sizeof(buf));

    // 配置 poll 结构
    pfd.fd = fd;
    pfd.events = POLLPRI;  // 边沿中断事件
    pfd.revents = 0;

    printf("GPIO %d monitoring ready, waiting for button press...\n", gpio_pin);

    // 监控循环
    while (g_gpio_running) {
        // 等待中断事件，超时 1 秒（用于检查 g_gpio_running）
        ret = poll(&pfd, 1, 1000);

        if (ret < 0) {
            perror("poll error");
            continue;
        }

        if (ret == 0) {
            // 超时，继续循环检查 g_gpio_running
            continue;
        }

        if (pfd.revents & POLLPRI) {
            // 检测到边沿中断，读取当前值
            lseek(fd, 0, SEEK_SET);
            ret = read(fd, buf, sizeof(buf) - 1);
            if (ret > 0) {
                buf[ret] = '\0';
                printf("GPIO %d interrupt detected, value=%s\n", gpio_pin, buf);

                // 通知上层处理按钮逻辑
                if (g_press_cb) {
                    g_press_cb();
                }
            }
        }
    }

    // 清理
    close(fd);
    printf("GPIO button thread exited\n");

    return NULL;
}

/**
 * 创建 GPIO 按钮监控线程
 *
 * @param gpio_pin GPIO 引脚号
 * @return 成功返回线程 ID，失败返回 0
 */
pthread_t create_gpio_button_thread(int gpio_pin) {
    static int gpio_pin_static;  // 静态变量保存引脚号，避免线程访问局部变量

    gpio_pin_static = gpio_pin;
    g_gpio_running = 1;

    int err = pthread_create(&g_gpio_thread_id, NULL, gpio_button_thread_func, &gpio_pin_static);
    if (err) {
        fprintf(stderr, "Failed to create GPIO button thread\n");
        return 0;
    }

    return g_gpio_thread_id;
}

/**
 * 停止 GPIO 按钮监控线程
 */
void stop_gpio_button_thread(void) {
    if (g_gpio_thread_id != 0) {
        g_gpio_running = 0;
        pthread_join(g_gpio_thread_id, NULL);
        g_gpio_thread_id = 0;
    }
}