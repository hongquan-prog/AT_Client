#pragma once

#include "mc665.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

typedef enum
{
    MC665_HTTP_PARAM_URL,
    MC665_HTTP_PARAM_USER_AGENT,
    MC665_HTTP_PARAM_ACCEPT,
    MC665_HTTP_PARAM_CONTENT_TYPE,
    MC665_HTTP_PARAM_RESPONSEHEADER,
    MC665_HTTP_PARAM_MODE,
    MC665_HTTP_PARAM_REDIR,
    MC665_HTTP_PARAM_CONTENT_RANGE,
    MC665_HTTP_PARAM_IPV6,
    MC665_HTTP_PARAM_NUM
} mc665_http_param_def;

typedef enum 
{
    MC665_HTTP_MODE_GET,
    MC665_HTTP_MODE_POST,
    MC665_HTTP_MODE_HEAD
} mc665_http_mode_def;

typedef enum 
{
    MC665_HTTP_REPLY_OK = 200,
    MC665_HTTP_REPLY_NOT_FOUND = 404
} mc665_http_reply_def;

typedef enum 
{
    MC665_HTTP_CONNECT_FAIL,
    MC665_HTTP_CONNECT_SUCCESS,
    MC665_HTTP_CONNECT_TIMEOUT
} mc665_http_connect_status_def;

typedef enum 
{
    MC665_HTTP_EVT_NONE,
    MC665_HTTP_EVT_CONNECT_FAIL,
    MC665_HTTP_EVT_CONNECT_SUCCESS,
    MC665_HTTP_EVT_RECV_DATA,
    MC665_HTTP_EVT_RECV_RESP,
} mc665_http_event_def;

typedef struct 
{
    char *buf;
    int buf_size;
    int read_len;
} mc665_http_data_t;

typedef struct 
{
    int mode;
    int reply;
    int length;
} mc665_http_resp_t;

typedef struct
{
    mc665_drv_t *drv;
    mc665_http_data_t *msg;
    SemaphoreHandle_t mutex;
    mc665_http_resp_t resp;
    EventGroupHandle_t event;
} mc665_http_drv_t;

bool mc665_http_init(mc665_http_drv_t *obj);
int mc665_read_content_length(mc665_http_drv_t *obj, int max_length);
bool mc665_http_set_param(mc665_http_drv_t *obj, mc665_http_param_def header, const char *value);
mc665_http_connect_status_def mc665_http_read_status(mc665_http_drv_t *obj, uint32_t timeout);
bool mc665_http_read_resp(mc665_http_drv_t *obj, mc665_http_resp_t *resp, uint32_t timeout);
int mc665_http_read_data(mc665_http_drv_t *obj, int offset, int length, void *buf, uint32_t timeout);
bool mc665_http_post(mc665_http_drv_t *obj, char *data, int len);
bool mc665_http_start(mc665_http_drv_t *obj, mc665_http_mode_def mode, int timeout);