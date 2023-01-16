#include "mc665_http.h"
#include "esp_log.h"
#include <string.h>

#define HTTP_CONNECT_SUCCESS_BIT BIT0
#define HTTP_CONNECT_FAIL_BIT BIT1
#define HTTP_RECV_RESP_BIT BIT2
#define HTTP_RECV_DATA_BIT BIT3

static const char *TAG = "mc665_http";
static struct at_urc s_urc_table[1] = {0};
static const char *s_http_param_table[MC665_HTTP_PARAM_NUM] = {
    "URL",
    "UAGENT",
    "ACCEPT",
    "CONTYPE",
    "RESPONSEHEADER",
    "MODE",
    "REDIR",
    "RANGE",
    "IPV6"};

static void private_mc665_http_set_event_bits(mc665_http_drv_t *obj, uint32_t bits)
{
    if (obj->event)
    {
        xEventGroupSetBits(obj->event, bits);
    }
}

void mc665_http_handler(struct at_client *client, const char *data, rt_size_t size, void *param)
{
    char expr[40] = {0};
    char temp[40] = {0};
    mc665_http_drv_t *obj = (mc665_http_drv_t *)param;

    if (!obj)
    {
        return;
    }

    /* 获取:前的字符串，最长39字符 */
    client->recv_line_buf[size - 1] = '\0';
    ESP_LOGD(TAG, "%s", client->recv_line_buf);

    snprintf(expr, sizeof(expr), "%%%d[^:]", sizeof(temp) - 1);
    sscanf(client->recv_line_buf, expr, temp);

    /* +HTTPREAD: <reslength>\r\nData\r\n\r\nOK\r\n */
    if (!strncmp(temp, "+HTTPREAD", sizeof("+HTTPREAD")))
    {
        int data_len = 0;
        int total_len = 0;

        /* 读取数据长度 */
        if (1 == sscanf(client->recv_line_buf, "%*s%d", &data_len))
        {
            /* 计算总共需要接收的长度 */
            total_len = data_len + sizeof("\r\n\r\nOK\r\n") - 1;

            if (total_len > client->recv_bufsz)
            {
                ESP_LOGE(TAG, "Payload is too long, please readjust receive buf size(current:%d, actual:%d)!", client->recv_bufsz, total_len);
            }
            else
            {
                int recv_len = at_client_obj_recv(client, client->recv_line_buf, total_len, 20);
                client->recv_line_buf[recv_len - 1] = '\0';

                if (recv_len == total_len)
                {
                    if (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY))
                    {
                        if (NULL == obj->msg)
                        {
                            ESP_LOGE(TAG, "Received buf is NULL");
                        }
                        else
                        {
                            if (data_len > obj->msg->buf_size)
                            {
                                ESP_LOGE(TAG, "Overflowed: buf size:%d, data size:%d", obj->msg->buf_size, data_len);
                            }
                            else
                            {
                                memcpy(obj->msg->buf, client->recv_line_buf, data_len);
                                obj->msg->read_len = data_len;
                                private_mc665_http_set_event_bits(obj, HTTP_RECV_DATA_BIT);
                            }
                        }
                        xSemaphoreGive(obj->mutex);
                    }
                }
                else
                {
                    ESP_LOGD(TAG, "%s\r\n", client->recv_line_buf);
                    ESP_LOGE(TAG, "http read failed!");
                }
            }
        }
    }
    /* +HTTP: <status> */
    else if (!strncmp(temp, "+HTTP", sizeof("+HTTP")))
    {
        int status = MC665_HTTP_CONNECT_FAIL;

        if (1 == sscanf(client->recv_line_buf, "%*s%d", &status))
        {
            if (MC665_HTTP_CONNECT_SUCCESS == status)
            {
                private_mc665_http_set_event_bits(obj, HTTP_CONNECT_SUCCESS_BIT);
            }
            else
            {
                private_mc665_http_set_event_bits(obj, HTTP_CONNECT_FAIL_BIT);
            }
        }
    }
    /* +HTTPRES: <mode>,<reply>,<length> */
    else if (!strncmp(temp, "+HTTPRES", sizeof("+HTTPRES")))
    {
        if (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY))
        {
            if (3 == sscanf(client->recv_line_buf, "%*s%d,%d,%d", &obj->resp.mode, &obj->resp.reply, &obj->resp.length))
            {
                private_mc665_http_set_event_bits(obj, HTTP_RECV_RESP_BIT);
            }

            xSemaphoreGive(obj->mutex);
        }
    }
}

static int private_mc665_http_parse_line(char *data, int length)
{
    for (int i = 0; i < length; i++)
    {
        if ('\n' == data[i])
        {
            data[i] = 0;
            return i;
        }
    }

    return length;
}

int mc665_read_content_length(mc665_http_drv_t *obj, int max_length)
{
    int temp = 0;
    int ret = -1;
    char *data = NULL;
    int read_offset = 0;
    int line_offset = 0;
    char *line_buf = NULL;

    line_buf = malloc(max_length);
    data = malloc(max_length);

    if (!line_buf || !data)
    {
        ESP_LOGE(TAG, "No memory to parse http line");
        goto __exit;
    }

    while ((read_offset < 2048) && (mc665_http_read_data(obj, read_offset, max_length, data, 5000) == max_length))
    {
        temp = private_mc665_http_parse_line(data, max_length);

        /* 根据是否查找到换行符，设置下一次读取的位置 */
        read_offset = read_offset + temp;
        (temp != max_length) ? (read_offset = read_offset + 1) : (0);

        if (-1 != line_offset)
        {
            if ((line_offset + temp) < max_length)
            {
                memcpy(line_buf + line_offset, data, temp);
                line_offset += temp;
                line_buf[line_offset] = 0;
            }
            else
            {
                line_offset = -1;
                ESP_LOGW(TAG, "http data is overflow");
            }
        }
        /* -1代表出现较长的单行数据，在接收到新的一行之前，该行数据将被忽略 */
        else if (temp != max_length)
        {
            line_offset = 0;
        }

        if ((max_length != temp) && line_offset)
        {
            /* 将header转换成小写（HTTP不区分大小写） */
            for (int i = 0; (i < line_offset) && (':' != line_buf[i]); i++)
            {
                if (('A' <= line_buf[i]) && ('Z' >= line_buf[i]))
                {
                    line_buf[i] += 32;
                }
            }

            if (!memcmp(line_buf, "content-length:", sizeof("content-length:") - 1))
            {
                sscanf(line_buf, "content-length: %d", &ret);
                break;
            }

            line_offset = 0;
        }
    }

__exit:

    if (line_buf)
    {
        free(line_buf);
    }

    if (data)
    {
        free(data);
    }

    return ret;
}

bool mc665_http_init(mc665_http_drv_t *obj)
{
    bool ret = false;

    if (!obj->drv)
    {
        ESP_LOGE(TAG, "The MC665 driver is NULL! please pass the mc665_drv_t pointer and retry!");
        goto __exit;
    }

    /* 初始化URC */
    s_urc_table[0].cmd_prefix = "+HTTP";
    s_urc_table[0].cmd_suffix = "\r\n";
    s_urc_table[0].func = mc665_http_handler;
    s_urc_table[0].param = obj;
    if (at_set_urc_table(s_urc_table, 1))
    {
        ESP_LOGE(TAG, "at client urc_table initial fail");
        goto __exit;
    }

    obj->event = xEventGroupCreate();
    if (!obj->event)
    {
        ESP_LOGE(TAG, "Create event_group object failed! memory not enough");
        goto __exit;
    }

    obj->mutex = xSemaphoreCreateMutex();
    if (!obj->mutex)
    {
        ESP_LOGE(TAG, "Create mutex object failed! memory not enough");
        goto __exit;
    }

    ret = true;
    ESP_LOGI(TAG, "MC665 http resource alloc success");

__exit:

    if (!ret)
    {
        if (obj->event)
        {
            vEventGroupDelete(obj->event);
            obj->event = NULL;
        }

        if (obj->mutex)
        {
            vSemaphoreDelete(obj->mutex);
            obj->mutex = NULL;
        }

        ESP_LOGE(TAG, "MC665 http resource alloc fail");
    }

    return ret;
}

bool mc665_http_set_param(mc665_http_drv_t *obj, mc665_http_param_def header, const char *value)
{
    bool ret = false;

    if ((header < MC665_HTTP_PARAM_NUM) && (mc665_take_lock(obj->drv)))
    {
        ret = (0 == at_exec_cmd(obj->drv->resp, "AT+HTTPSET=\"%s\",\"%s\"", s_http_param_table[header], value));
        mc665_release_lock(obj->drv);
    }

    return ret;
}

bool mc665_http_read_resp(mc665_http_drv_t *obj, mc665_http_resp_t *resp, uint32_t timeout)
{
    bool ret = false;

    if (HTTP_RECV_RESP_BIT & xEventGroupWaitBits(obj->event, HTTP_RECV_RESP_BIT, pdTRUE, pdFALSE, timeout))
    {
        if (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY))
        {
            ret = true;
            *resp = obj->resp;
            xSemaphoreGive(obj->mutex);
        }
    }

    return ret;
}

mc665_http_connect_status_def mc665_http_read_status(mc665_http_drv_t *obj, uint32_t timeout)
{
    EventBits_t event = 0;
    mc665_http_connect_status_def ret = MC665_HTTP_CONNECT_TIMEOUT;

    event = xEventGroupWaitBits(obj->event, HTTP_CONNECT_SUCCESS_BIT | HTTP_CONNECT_FAIL_BIT, pdTRUE, pdFALSE, timeout);

    if (HTTP_CONNECT_SUCCESS_BIT & event)
    {
        ret = MC665_HTTP_CONNECT_SUCCESS;
    }
    else if (HTTP_CONNECT_FAIL_BIT & event)
    {
        ret = MC665_HTTP_CONNECT_FAIL;
    }

    return ret;
}

int mc665_http_read_data(mc665_http_drv_t *obj, int offset, int length, void *buf, uint32_t timeout)
{
    int expr_len = 0;
    char expr[40] = {0};
    mc665_http_data_t msg = {
        .buf = buf,
        .buf_size = length,
        .read_len = 0};

    expr_len = snprintf(expr, sizeof(expr), "AT+HTTPREAD=%d,%d\r\n", offset, length);

    if (mc665_take_lock(obj->drv))
    {
        /* 设置接收缓冲 */
        if (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY))
        {
            obj->msg = &msg;
            xSemaphoreGive(obj->mutex);
        }

        at_client_send(expr, expr_len);
        xEventGroupWaitBits(obj->event, HTTP_RECV_DATA_BIT, pdTRUE, pdFALSE, timeout);

        /* 清除接收缓冲 */
        if (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY))
        {
            obj->msg = NULL;
            xSemaphoreGive(obj->mutex);
        }

        mc665_release_lock(obj->drv);
    }

    return msg.read_len;
}

bool mc665_http_post(mc665_http_drv_t *obj, char *data, int len)
{
    bool ret = false;

    if (mc665_take_lock(obj->drv))
    {
        at_set_end_sign('>');
        ret = (0 == at_exec_cmd(obj->drv->resp, "AT+HTTPDATA=%d", len));
        at_set_end_sign(0);

        if (ret)
        {
            at_client_send(data, len);
            ret = (0 == at_exec_cmd(obj->drv->resp, ""));
        }

        mc665_release_lock(obj->drv);
    }

    return ret;
}

bool mc665_http_start(mc665_http_drv_t *obj, mc665_http_mode_def mode, int timeout)
{
    bool ret = false;

    if (mc665_take_lock(obj->drv))
    {
        ret = (0 == at_exec_cmd(obj->drv->resp, "AT+HTTPACT=%d,%d", mode, timeout));
        mc665_release_lock(obj->drv);
    }

    return ret;
}