#pragma once

#include "at.h"
#include "mqtt_interface.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* access technology selected */
typedef enum
{
    MC665_ACT_GSM,
    MC665_ACT_GSM_COMPACT,
    MC665_ACT_UTRAN,
    MC665_ACT_GSM_wEGPRS,
    MC665_ACT_UTRAN_wHSDPA,
    MC665_ACT_UTRAN_wHSUPA,
    MC665_ACT_UTRAN_wHSDPA_AND_HSUPA,
    MC665_ACT_EUTRAN,
    MC665_ACT_CDMA,
    MC665_ACT_CDMAEVDO,
    MC665_ACT_EVDO,
    MC665_ACT_eMTC,
    MC665_ACT_NBIoT
} mc665_act_def;

typedef enum
{
    MC665_ERROR_MQTT_UNACCEPT_PROTOCOL = 700,
    MC665_ERROR_MQTT_ID_REJECTED,
    MC665_ERROR_MQTT_SERVER_UNAVAILABLE,
    MC665_ERROR_MQTT_BAD_USERNAME_OR_PASSWORD,
    MC665_ERROR_MQTT_NOT_AUTHORIZED
} mc665_error_def;

typedef enum
{
    MC665_EVT_GOT_IP,
    MC665_EVT_NETWORK_DISCONNECTED
} mc665_event_def;

typedef enum
{
    MC665_STATUS_WAIT_CONNECTED,
    MC665_STATUS_WAIT_CARD_READY,
    MC665_STATUS_SET_APN,
    MC665_STATUS_SEARCH_NETWORK,
    MC665_STATUS_WAIT_GPRS_ENABLE,
    MC665_STATUS_REQUEST_IP,
    MC665_STATUS_DISCONNECTED,
    MC665_STATUS_READY
} mc665_status_def;

typedef struct
{
    void *param;
    void (*func)(void *param, mc665_event_def event);
} mc665_event_cb_t;

typedef struct
{
    at_response_t resp;
    TaskHandle_t task;
    mc665_status_def status;
    EventGroupHandle_t event;
    /* 用于保护多条AT指令执行过程不被干扰 */
    SemaphoreHandle_t  mutex;
    mc665_event_cb_t event_cb;
} mc665_drv_t;

void mc665_register_callback(mc665_drv_t *obj, mc665_event_cb_t *cb);
bool mc665_init(mc665_drv_t *obj);
bool mc665_take_lock(mc665_drv_t *obj);
void mc665_release_lock(mc665_drv_t *obj);
bool mc665_detect(mc665_drv_t *obj);
bool mc665_disable_echo(mc665_drv_t *obj);
bool mc665_set_network_search_priority(mc665_drv_t *obj);
bool mc665_rf_is_enabled(mc665_drv_t *obj);
bool mc665_enable_rf(mc665_drv_t *obj);
bool mc665_read_pin(mc665_drv_t *obj);
bool mc665_set_apn(mc665_drv_t *obj);
bool mc665_read_imsi(mc665_drv_t *obj, void *buf, uint32_t len);
bool mc665_get_csq(mc665_drv_t *obj, int *signal_intensity, int *bit_error_rate);
bool mc665_get_operator_info(mc665_drv_t *obj, void *operator, uint32_t len, int *act);
bool mc665_ps_is_registered(mc665_drv_t *obj);
bool mc665_lte_is_registered(mc665_drv_t *obj);
bool mc665_cs_is_registered(mc665_drv_t *obj);
bool mc665_request_ip(mc665_drv_t *obj);
bool mc665_is_get_ip(mc665_drv_t *obj, void *ip, uint32_t len);
bool mc665_ip_is_available(mc665_drv_t *obj);