/*
 * Copyright (c) 2022-2022, lihongquan
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-22     lihongquan   port to esp32
 */
#ifndef __AT_DEF__
#define __AT_DEF__

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "com_interface.h"

typedef com_inface_t*                   rt_device_t;
typedef unsigned int                    rt_size_t;
typedef unsigned int                    rt_off_t;
typedef char                            rt_int8_t;
typedef short                           rt_int16_t;
typedef int                             rt_int32_t;
typedef unsigned char                   rt_uint8_t;
typedef unsigned short                  rt_uint16_t;
typedef unsigned int                    rt_uint32_t;
typedef unsigned int                    rt_tick_t;
typedef int                             rt_err_t;
typedef SemaphoreHandle_t               rt_mutex_t;
typedef SemaphoreHandle_t               rt_sem_t;
typedef unsigned char                   rt_bool_t;
typedef TaskHandle_t                    rt_thread_t;

#define RT_FALSE                        0
#define RT_TRUE                         1
#define RT_NULL                         NULL
#define RT_WAITING_FOREVER              portMAX_DELAY

#define RT_ASSERT                       assert
#define RT_WEAK                         __attribute__((weak))

/* In FreeRTOS, those flag is invalid */
#define RT_IPC_CMD_RESET                0
#define RT_IPC_FLAG_PRIO                0
#define RT_IPC_FLAG_FIFO                0


#define RT_THREAD_PRIORITY_MAX          configMAX_PRIORITIES
#define RT_NAME_MAX                     32

/* RT-Thread error code definitions */
#define RT_EOK                          0               /**< There is no error */
#define RT_ERROR                        1               /**< A generic error happens */
#define RT_ETIMEOUT                     2               /**< Timed out */
#define RT_EFULL                        3               /**< The resource is full */
#define RT_EEMPTY                       4               /**< The resource is empty */
#define RT_ENOMEM                       5               /**< No memory */
#define RT_ENOSYS                       6               /**< No system */
#define RT_EBUSY                        7               /**< Busy */
#define RT_EIO                          8               /**< IO error */
#define RT_EINTR                        9               /**< Interrupted system call */
#define RT_EINVAL                       10              /**< Invalid argument */

#endif
