#include "mc665.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "mc665_mqtt.h"
#include <string.h>

#define MQTT_CLIENT_ID 1
#define MQTT_CONNECTED_BIT BIT0
#define MQTT_DISCONNECTED_BIT BIT1
#define MQTT_SUBSCRIBE_BIT BIT2
#define MQTT_PUBLISH_BIT BIT3
#define MQTT_UNSUBSCRIBE_BIT BIT4
#define MQTT_CLOSE_BIT BIT5

typedef struct
{
    mqtt_msg_t msg;
    mqtt_event_def event;
    struct at_client *client;
} mc665_mqtt_msg_t;

typedef struct
{
    mc665_drv_t *drv;
    mqtt_cfg_t cfg;
    mqtt_event_cb_t event_cb;
    EventGroupHandle_t event;
    TaskHandle_t task;
    QueueHandle_t queue;
} mc665_mqtt_drv_t;

static const char *TAG = "mc665_mqtt";
static struct at_urc s_urc_table[1] = {0};
static mc665_mqtt_drv_t s_mqtt_drv = {NULL};

static void private_mc665_mqtt_set_event_bits(uint32_t bits)
{
    if (s_mqtt_drv.event)
    {
        xEventGroupSetBits(s_mqtt_drv.event, bits);
    }
}

static void private_mc665_mqtt_handler(struct at_client *client, const char *data, rt_size_t size, void *param)
{
    (void)param;
    char expr[40] = {0};
    char temp[40] = {0};
    mc665_mqtt_msg_t urc = {.event = MQTT_EVT_NONE};

    /* 获取:前的字符串，最长39字符 */
    client->recv_line_buf[size - 1] = '\0';
    ESP_LOGD(TAG, "%s", client->recv_line_buf);

    snprintf(expr, sizeof(expr), "%%%d[^:]", sizeof(temp) - 1);
    sscanf(client->recv_line_buf, expr, temp);

    /* +MQTTMSGI: <Client id>,<Qos>,<tlength/Topicid>,<plength>
           +MQTTREAD=<Client id>
           接收到该指令后通过 at_client_send 发送 MQTTREAD 读取payload
           计算出所需要接收的字节数后使用at_client_recv进行接收 */
    if (!strncmp(temp, "+MQTTMSGI", sizeof("+MQTTMSGI")))
    {
        if (sscanf(client->recv_line_buf, "%*s%*d,%*d,%d,%d", &urc.msg.topic_len, &urc.msg.data_len))
        {
            int cmd_len = 0;
            int topic_pos = 0;
            int total_len = 0;
            int payload_pos = 0;

            /* +MQTTREAD: <Client id>,<Qos>,<tlength>,<plength>,<Topic/Topic id>,<Payload> */
            topic_pos = snprintf(client->recv_line_buf, client->recv_bufsz, "+MQTTREAD: 1,0,%d,%d,\"", urc.msg.topic_len, urc.msg.data_len);
            cmd_len = snprintf(client->recv_line_buf, client->recv_bufsz, "AT+MQTTREAD=%d\r\n", MQTT_CLIENT_ID);
            topic_pos = topic_pos + cmd_len;
            payload_pos = topic_pos + urc.msg.topic_len + sizeof("\",\"") - 1;
            total_len = payload_pos + urc.msg.data_len + sizeof("\"\r\n\r\nOK\r\n") - 1;

            if (total_len > client->recv_bufsz)
            {
                ESP_LOGE(TAG, "Payload is too long, please readjust receive buf size(current:%d, actual:%d)!", client->recv_bufsz, total_len);
            }
            else
            {
                at_client_obj_send(client, client->recv_line_buf, topic_pos - cmd_len);
                int recv_len = at_client_obj_recv(client, client->recv_line_buf, total_len, 20);

                if (recv_len == total_len)
                {
                    client->recv_line_buf[total_len - 1] = '\0';

                    if (3 == sscanf(client->recv_line_buf + cmd_len, "%*s%*d,%d,%d,%d", &urc.msg.qos, &urc.msg.topic_len, &urc.msg.data_len))
                    {
                        if (urc.msg.topic || urc.msg.data)
                        {
                            free(urc.msg.topic);
                            free(urc.msg.data);
                            urc.msg.topic = NULL;
                            urc.msg.data = NULL;
                            ESP_LOGE(TAG, "Unhandled mqtt msg");
                        }

                        urc.msg.topic = malloc(urc.msg.topic_len + 1);
                        urc.msg.data = malloc(urc.msg.data_len + 1);

                        if (urc.msg.topic && urc.msg.data)
                        {
                            urc.event = MQTT_EVT_DATA;
                            client->recv_line_buf[topic_pos + urc.msg.topic_len] = 0;
                            memcpy(urc.msg.topic, &client->recv_line_buf[topic_pos], urc.msg.topic_len + 1);
                            client->recv_line_buf[payload_pos + urc.msg.data_len] = 0;
                            memcpy(urc.msg.data, &client->recv_line_buf[payload_pos], urc.msg.data_len + 1);
                        }
                        else
                        {
                            free(urc.msg.topic);
                            free(urc.msg.data);
                            urc.msg.topic = NULL;
                            urc.msg.data = NULL;
                        }
                    }
                }
                else
                {
                    client->recv_line_buf[recv_len - 1] = '\0';
                    ESP_LOGD(TAG, "%s\r\n", client->recv_line_buf);
                    ESP_LOGE(TAG, "MQTT message read failed!");
                }
            }
        }
    }
    /* +MQTTPUB: <Client id>,<Status> */
    else if (!strncmp(temp, "+MQTTPUB", sizeof("+MQTTPUB")))
    {
        urc.event = MQTT_EVT_PUBLISHED;
        private_mc665_mqtt_set_event_bits(MQTT_PUBLISH_BIT);
    }
    /* +MQTTSUB: <Client id>,<Status> */
    else if (!strncmp(temp, "+MQTTSUB", sizeof("+MQTTSUB")))
    {
        urc.event = MQTT_EVT_SUBSCRIBED;
        private_mc665_mqtt_set_event_bits(MQTT_SUBSCRIBE_BIT);
    }
    /* +MQTTOPEN: <Client id>,<Status> */
    else if (!strncmp(temp, "+MQTTOPEN", sizeof("+MQTTOPEN")))
    {
        urc.event = MQTT_EVT_CONNECTED;
        private_mc665_mqtt_set_event_bits(MQTT_CONNECTED_BIT);
    }
    /* +MQTTUNSUB: <Client id>,<Status> */
    else if (!strncmp(temp, "+MQTTUNSUB", sizeof("+MQTTUNSUB")))
    {
        urc.event = MQTT_EVT_UNSUBSCRIBED;
        private_mc665_mqtt_set_event_bits(MQTT_UNSUBSCRIBE_BIT);
    }
    /* +MQTTBREAK: <Client id>,<cause> */
    else if (!strncmp(temp, "+MQTTBREAK", sizeof("+MQTTBREAK")))
    {
        urc.event = MQTT_EVT_DISCONNECTED;
        private_mc665_mqtt_set_event_bits(MQTT_DISCONNECTED_BIT);
    }
    /* +MQTTCLOSE: <Client id>,<Status> */
    else if (!strncmp(temp, "+MQTTCLOSE", sizeof("+MQTTCLOSE")))
    {
        private_mc665_mqtt_set_event_bits(MQTT_CLOSE_BIT);
    }

    if (s_mqtt_drv.queue && MQTT_EVT_NONE != urc.event)
    {
        if (xQueueSend(s_mqtt_drv.queue, &urc, portMAX_DELAY) != pdTRUE)
        {
            ESP_LOGE(TAG, "MC665 mqtt queue is full, please readjust the queue size");
            free(urc.msg.topic);
            free(urc.msg.data);
            urc.msg.topic = NULL;
            urc.msg.data = NULL;
        }
    }
}

static void private_mc665_mqtt_task(void *argument)
{
    mc665_mqtt_msg_t urc = {0};

    for (;;)
    {
        /* 等待URC消息 */
        xQueueReceive(s_mqtt_drv.queue, &urc, portMAX_DELAY);

        /* 通知应用程序进行处理 */
        if (s_mqtt_drv.event_cb.func)
        {
            s_mqtt_drv.event_cb.func(s_mqtt_drv.event_cb.param, urc.event, (MQTT_EVT_DATA == urc.event) ? (&urc.msg) : (NULL));
        }

        /* 垃圾回收 */
        if (MQTT_EVT_DATA == urc.event)
        {
            free(urc.msg.topic);
            free(urc.msg.data);
            urc.msg.topic = NULL;
            urc.msg.data = NULL;
        }
    }
}

static bool private_mc665_mqtt_close(void)
{
    bool ret = false;

    if (mc665_take_lock(s_mqtt_drv.drv))
    {
        ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTCLOSE=%d\"", MQTT_CLIENT_ID));
        mc665_release_lock(s_mqtt_drv.drv);
        ret = ret && (MQTT_CLOSE_BIT & xEventGroupWaitBits(s_mqtt_drv.event,
                                                 MQTT_CLOSE_BIT | MQTT_DISCONNECTED_BIT,
                                                 pdTRUE, pdFALSE, pdMS_TO_TICKS(5000)));
    }

    return ret;
}

static void private_mc665_mqtt_delete(void)
{
    private_mc665_mqtt_close();

    if (s_mqtt_drv.task)
    {
        vTaskDelete(s_mqtt_drv.task);
        s_mqtt_drv.task = NULL;
    }

    if (s_mqtt_drv.event)
    {
        vEventGroupDelete(s_mqtt_drv.event);
        s_mqtt_drv.event = NULL;
    }

    if (s_mqtt_drv.queue)
    {
        vQueueDelete(s_mqtt_drv.queue);
        s_mqtt_drv.queue = NULL;
    }

    if (s_mqtt_drv.cfg.uri)
    {
        free((void *)s_mqtt_drv.cfg.uri);
        s_mqtt_drv.cfg.uri = NULL;
    }

    if (s_mqtt_drv.cfg.client_id)
    {
        free((void *)s_mqtt_drv.cfg.client_id);
        s_mqtt_drv.cfg.client_id = NULL;
    }
}

static bool private_mc665_mqtt_init(mqtt_cfg_t *cfg)
{
    bool ret = false;
    mc665_drv_t *obj = s_mqtt_drv.drv;

    if (!obj)
    {
        ESP_LOGE(TAG, "The MC665 driver is NULL! please pass the mc665_drv_t pointer and retry!");
        goto __exit;
    }

    /* 初始化URC */
    s_urc_table[0].cmd_prefix = "+MQTT";
    s_urc_table[0].cmd_suffix = "\r\n";
    s_urc_table[0].func = private_mc665_mqtt_handler;
    if (at_set_urc_table(s_urc_table, 1))
    {
        ESP_LOGE(TAG, "at client urc_table initial fail");
        goto __exit;
    }

    s_mqtt_drv.event = xEventGroupCreate();
    if (!s_mqtt_drv.event)
    {
        ESP_LOGE(TAG, "Create event_group object failed! memory not enough");
        goto __exit;
    }

    s_mqtt_drv.cfg.port = (0 == cfg->port) ? (1883) : (cfg->port);
    s_mqtt_drv.cfg.client_id = (cfg->client_id) ? (strdup(cfg->client_id)) : (NULL);
    s_mqtt_drv.cfg.uri = (cfg->uri) ? (strdup(cfg->uri)) : (NULL);
    (!s_mqtt_drv.cfg.uri && cfg->host) ? (s_mqtt_drv.cfg.uri = strdup(cfg->host)) : (0);
    if (!s_mqtt_drv.cfg.uri)
    {
        ESP_LOGE(TAG, "URL duplicate failed!");
        goto __exit;
    }

    s_mqtt_drv.queue = xQueueCreate(10, sizeof(mc665_mqtt_msg_t));
    if (!s_mqtt_drv.queue)
    {
        ESP_LOGE(TAG, "mc665_urc_queue create failed! memory not enough");
        goto __exit;
    }

    xTaskCreate(private_mc665_mqtt_task, "mc665_mqtt_task", 1024 * 4, NULL, 6, &s_mqtt_drv.task);
    if (!s_mqtt_drv.task)
    {
        ESP_LOGE(TAG, "mc665_mqtt_task create failed! memory not enough");
        goto __exit;
    }

    ret = true;
    ESP_LOGI(TAG, "MC665 mqtt resorce allocated!");

__exit:

    if (!ret)
    {
        private_mc665_mqtt_delete();
    }

    return ret;
}

static bool private_mc665_mqtt_open(void)
{
    bool ret = false;

    if (s_mqtt_drv.cfg.uri)
    {
        if (mc665_take_lock(s_mqtt_drv.drv))
        {
            if (s_mqtt_drv.cfg.client_id)
            {
                ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTUSER=%d,\"\",\"\",\"%s\"", MQTT_CLIENT_ID, s_mqtt_drv.cfg.client_id));
            }
            else
            {
                ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTUSER=%d,\"\",\"\"", MQTT_CLIENT_ID));
            }

            /* 0: Default value. Report the content of the topic and payload directly by the MQTTMSG command.
            1: Report the length of the topic and payload by the MQTTMSGI command */
            ret = ret && (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTCONF=1"));
            ret = ret && (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTOPEN=%d,\"%s\",%d,0,60", MQTT_CLIENT_ID, s_mqtt_drv.cfg.uri, s_mqtt_drv.cfg.port));
            mc665_release_lock(s_mqtt_drv.drv);

            if (ret)
            {
                if (MQTT_CONNECTED_BIT & xEventGroupWaitBits(s_mqtt_drv.event,
                                                             MQTT_CONNECTED_BIT | MQTT_DISCONNECTED_BIT,
                                                             pdTRUE, pdFALSE, pdMS_TO_TICKS(30000)))
                {
                    ESP_LOGI(TAG, "MC665 mqtt connect success!");
                }
                else
                {
                    ret = false;
                    ESP_LOGE(TAG, "MC665 mqtt connect failed!");
                }
            }
        }
    }

    return ret;
}

static bool private_mc665_mqtt_subscribe(const char *topic, int qos)
{
    bool ret = false;

    if (mc665_take_lock(s_mqtt_drv.drv))
    {
        ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTSUB=%d,\"%s\",%d", MQTT_CLIENT_ID, topic, qos));
        mc665_release_lock(s_mqtt_drv.drv);
        ret = ret && (MQTT_SUBSCRIBE_BIT & xEventGroupWaitBits(s_mqtt_drv.event,
                                                               MQTT_SUBSCRIBE_BIT | MQTT_DISCONNECTED_BIT,
                                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(5000)));
    }

    return ret;
}

static bool private_mc665_mqtt_unsubscribe(const char *topic)
{
    bool ret = false;

    if (mc665_take_lock(s_mqtt_drv.drv))
    {
        ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTUNSUB=%d,\"%s\"", MQTT_CLIENT_ID, topic));
        mc665_release_lock(s_mqtt_drv.drv);
        ret = ret && (MQTT_UNSUBSCRIBE_BIT & xEventGroupWaitBits(s_mqtt_drv.event,
                                                                 MQTT_UNSUBSCRIBE_BIT | MQTT_DISCONNECTED_BIT,
                                                                 pdTRUE, pdFALSE, pdMS_TO_TICKS(5000)));
    }

    return ret;
}

static bool private_mc665_mqtt_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    bool ret = false;

    if (mc665_take_lock(s_mqtt_drv.drv))
    {
        at_set_end_sign('>');
        ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, "AT+MQTTPUB=%d,\"%s\",%d,%d,%d", MQTT_CLIENT_ID, topic, qos, retain, len));
        at_set_end_sign(0);

        if (ret)
        {
            at_client_send(data, len);
            ret = (0 == at_exec_cmd(s_mqtt_drv.drv->resp, ""));
        }

        mc665_release_lock(s_mqtt_drv.drv);
        ret = ret && (MQTT_PUBLISH_BIT & xEventGroupWaitBits(s_mqtt_drv.event,
                                                             MQTT_PUBLISH_BIT | MQTT_DISCONNECTED_BIT,
                                                             pdTRUE, pdFALSE, pdMS_TO_TICKS(5000)));
    }

    return ret;
}

static mqtt_err_def private_mc665_mqtt_error_code(void)
{
    return MQTT_ERR_NONE;
}

static void private_mc665_mqtt_register_callback(mqtt_event_cb_t *cb)
{
    s_mqtt_drv.event_cb = *cb;
}

void mc665_mqtt_drv_get(mqtt_drv_t *drv)
{
    if (drv)
    {
        drv->close = private_mc665_mqtt_close;
        drv->delete = private_mc665_mqtt_delete;
        drv->init = private_mc665_mqtt_init;
        drv->open = private_mc665_mqtt_open;
        drv->publish = private_mc665_mqtt_publish;
        drv->subscribe = private_mc665_mqtt_subscribe;
        drv->unsubscribe = private_mc665_mqtt_unsubscribe;
        drv->error_code = private_mc665_mqtt_error_code;
        drv->register_callback = private_mc665_mqtt_register_callback;

        if (!drv->user_data)
        {
            ESP_LOGE(TAG, "The user_data is NULL! please pass the mc665_drv_t pointer and retry!");
        }

        s_mqtt_drv.drv = drv->user_data;
    }
}