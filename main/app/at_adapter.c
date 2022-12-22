/*
 * Copyright (c) 2022-2022, lihongquan
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-22     lihongquan   port to esp32
 */
#include "at_adapter.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

rt_err_t rt_sem_control(rt_sem_t sem, int cmd, void *arg)
{
    if ((RT_IPC_CMD_RESET == cmd) && uxSemaphoreGetCount(sem))
    {
        xSemaphoreTake(sem, 0);
    }

    return RT_EOK;
}

rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag)
{
    return xSemaphoreCreateMutex();
}

rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag)
{
    if (value > 1)
    {
        return xSemaphoreCreateCounting(value, 0);
    }
    else
    {
        return xSemaphoreCreateBinary();
    }
}

rt_thread_t rt_thread_create(const char *name,
                             void (*entry)(void *),
                             void *parameter,
                             rt_uint32_t stack_size,
                             rt_uint8_t priority,
                             rt_uint32_t tick)
{
    rt_thread_t ret = NULL;
    
    xTaskCreate(entry, name, stack_size, parameter, priority, &ret);

    return ret;
}