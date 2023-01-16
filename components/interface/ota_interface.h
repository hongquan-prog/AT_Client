#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void ota_inface_t;

typedef enum
{
    OTA_STATUS_UNINITIALIZED,
    OTA_STATUS_IDLE,
    OTA_STATUS_RUNNING
} ota_status_def;

typedef enum
{
    OTA_EVT_DOWNLOAD_FAIL,
    OTA_EVT_DOWNLOAD_SUCCESS,
    OTA_EVT_NONE
} ota_event_def;

typedef struct
{
    void *param;
    void (*func)(void *param, ota_event_def event);
} ota_event_cb_t;

typedef struct
{
    void *user_data;
    void (*register_callback)(ota_event_cb_t *cb);
    bool (*init)(void);
    bool (*start)(const char *url, bool ignore_version_check);
    void (*restart)(void);
} ota_drv_t;

void ota_register_callback(ota_inface_t *obj, ota_event_cb_t *cb);
bool ota_init(ota_inface_t *obj);
bool ota_start(ota_inface_t *obj, const char *url, bool ignore_version_check);
void ota_restart(ota_inface_t *obj);