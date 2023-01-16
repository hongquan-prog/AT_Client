#include "mqtt_interface.h"

#define DEBUG_ASSERT(VALUE) \
    while (!(VALUE))        \
    {                       \
    }

void mqtt_register_callback(mqtt_inface_t *p, mqtt_event_cb_t *cb)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->register_callback);
    ((mqtt_drv_t *)p)->register_callback(cb);
}

bool mqtt_init(mqtt_inface_t *p, mqtt_cfg_t *cfg)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->init);
    return ((mqtt_drv_t *)p)->init(cfg);
}

bool mqtt_open(mqtt_inface_t *p)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->open);
    return ((mqtt_drv_t *)p)->open();
}

bool mqtt_subscribe(mqtt_inface_t *p, const char *topic, int qos)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->subscribe);
    return ((mqtt_drv_t *)p)->subscribe(topic, qos);
}

bool mqtt_unsubscribe(mqtt_inface_t *p, const char *topic)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->unsubscribe);
    return ((mqtt_drv_t *)p)->unsubscribe(topic);
}

bool mqtt_publish(mqtt_inface_t *p, const char *topic, const char *data, int len, int qos, int retain)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->publish);
    return ((mqtt_drv_t *)p)->publish(topic, data, len, qos, retain);
}

bool mqtt_close(mqtt_inface_t *p)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->close);
    return ((mqtt_drv_t *)p)->close();
}

mqtt_err_def mqtt_error_code(mqtt_inface_t *p)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->error_code);
    return ((mqtt_drv_t *)p)->error_code();
}

void mqtt_delete(mqtt_inface_t *p)
{
    DEBUG_ASSERT(((mqtt_drv_t *)p)->delete);
    ((mqtt_drv_t *)p)->delete();
}