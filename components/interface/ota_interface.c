#include "ota_interface.h"

#define DEBUG_ASSERT(VALUE) \
    while (!(VALUE))        \
    {                       \
    }

void ota_register_callback(ota_inface_t *p, ota_event_cb_t *cb)
{
    DEBUG_ASSERT(((ota_drv_t *)p)->register_callback);
    ((ota_drv_t *)p)->register_callback(cb);
}

bool ota_init(ota_inface_t *p)
{
    DEBUG_ASSERT(((ota_drv_t *)p)->init);
    return ((ota_drv_t *)p)->init();
}

bool ota_start(ota_inface_t *p, const char *url, bool ignore_version_check)
{
    DEBUG_ASSERT(((ota_drv_t *)p)->start);
    return ((ota_drv_t *)p)->start(url, ignore_version_check);
}

void ota_restart(ota_inface_t *p)
{
    DEBUG_ASSERT(((ota_drv_t *)p)->restart);
    ((ota_drv_t *)p)->restart();
}