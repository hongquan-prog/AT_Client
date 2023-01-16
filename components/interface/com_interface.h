#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void com_inface_t;

typedef struct
{
    const char *name;
    void (*init)(void);
    bool (*available)(void);
    int (*read)(void *buf, uint32_t length, uint32_t timeout_ms);
    int (*write)(const void *src, uint32_t size);
    void (*flush)(void);
} com_drv_t;

void com_init(com_inface_t *p);
const char *com_device_name(com_inface_t *p);
bool com_available(com_inface_t *p);
int com_read(com_inface_t *p, void *buf, uint32_t length, uint32_t timeout_ms);
int com_write(com_inface_t *p, const void *src, uint32_t size);
void com_flush(com_inface_t *p);
