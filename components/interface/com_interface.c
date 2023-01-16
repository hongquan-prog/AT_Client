#include "com_interface.h"

#define DEBUG_ASSERT(VALUE) \
    while (!(VALUE))        \
    {                       \
    }

void com_init(com_inface_t *p)
{
    DEBUG_ASSERT(((com_drv_t *)p)->init);
    ((com_drv_t *)p)->init();
}

const char *com_device_name(com_inface_t *p)
{
    return (p) ? (((com_drv_t *)p)->name) : ("");
}

bool com_available(com_inface_t *p)
{
    DEBUG_ASSERT(((com_drv_t *)p)->available);
    return ((com_drv_t *)p)->available();
}

int com_read(com_inface_t *p, void *buf, uint32_t length, uint32_t timeout_ms)
{
    DEBUG_ASSERT(((com_drv_t *)p)->read);
    return ((com_drv_t *)p)->read(buf, length, timeout_ms);
}

int com_write(com_inface_t *p, const void *src, uint32_t size)
{
    DEBUG_ASSERT(((com_drv_t *)p)->write);
    return ((com_drv_t *)p)->write(src, size);
}

void com_flush(com_inface_t *p)
{
    DEBUG_ASSERT(((com_drv_t *)p)->flush);
    ((com_drv_t *)p)->flush();
}