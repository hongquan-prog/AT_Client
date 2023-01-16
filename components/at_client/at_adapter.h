/*
 * Copyright (c) 2022-2022, lihongquan
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-22     lihongquan   port to esp32
 */
#ifndef __AT_ADAPTER__
#define __AT_ADAPTER__

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "at_def.h"
#include "esp_log.h"

#ifndef rt_strcmp
#define rt_strcmp                       strcmp
#endif

#ifndef rt_memcpy
#define rt_memcpy                       memcpy
#endif

#ifndef rt_memcmp
#define rt_memcmp                       memcmp
#endif

#ifndef rt_memset
#define rt_memset                       memset
#endif

#ifndef rt_strlen
#define rt_strlen                       strlen
#endif

#ifndef rt_strncmp
#define rt_strncmp                      strncmp
#endif

#ifndef rt_strstr
#define rt_strstr                       strstr
#endif

#ifndef rt_snprintf
#define rt_snprintf                     snprintf
#endif

#ifndef rt_kprintf
#define rt_kprintf                      printf
#endif

#ifndef rt_malloc
#define rt_malloc                       malloc
#endif

#ifndef rt_free
#define rt_free                         free
#endif

#ifndef rt_calloc
#define rt_calloc                       calloc
#endif

#ifndef rt_realloc
#define rt_realloc                      realloc
#endif

#ifndef rt_tick_from_millisecond
#define rt_tick_from_millisecond        pdMS_TO_TICKS
#endif

#ifndef rt_tick_get
#define rt_tick_get                     xTaskGetTickCount
#endif

#ifndef rt_mutex_take
#define rt_mutex_take(sem, timeout)     ((pdTRUE == xSemaphoreTake((sem), (timeout))) ? (RT_EOK) : (RT_ETIMEOUT))
#endif

#ifndef rt_mutex_release
#define rt_mutex_release                xSemaphoreGive
#endif

#ifndef rt_mutex_delete
#define rt_mutex_delete                 vSemaphoreDelete
#endif

#ifndef rt_sem_take
#define rt_sem_take                     rt_mutex_take
#endif

#ifndef rt_sem_release
#define rt_sem_release                  xSemaphoreGive
#endif

#ifndef rt_sem_delete
#define rt_sem_delete                   vSemaphoreDelete
#endif

#ifndef rt_thread_startup
#define rt_thread_startup(arg)
#endif

#ifndef LOG_E
#define LOG_E(...)                      ESP_LOGE(LOG_TAG, ##__VA_ARGS__)
#endif 

#ifndef LOG_W
#define LOG_W(...)                      ESP_LOGW(LOG_TAG, ##__VA_ARGS__)
#endif 

#ifndef LOG_I
#define LOG_I(...)                      ESP_LOGI(LOG_TAG, ##__VA_ARGS__)
#endif 

#ifndef LOG_D
#define LOG_D(...)                      ESP_LOGD(LOG_TAG, ##__VA_ARGS__)
#endif 

rt_mutex_t rt_mutex_create (const char *name, rt_uint8_t flag);
rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag);
rt_err_t rt_sem_control(rt_sem_t sem, int cmd, void *arg);
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *),
                             void *parameter, rt_uint32_t stack_size,
                             rt_uint8_t priority, rt_uint32_t tick);

#endif
