#include "mc665.h"
#include "esp_log.h"
#include <string.h>

#define MC665_RECV_BUF_SIZE 1024
#define MC665_RECV_TIMEOUT 10000

#define MC665_SIM_READY_BIT BIT0
#define MC665_SIM_DROP_BIT BIT1

static const char *TAG = "mc665";
static struct at_urc s_urc_table[4] = {0};

static void private_mc665_set_event_bits(mc665_drv_t *obj, uint32_t bits)
{
    if (obj->event)
    {
        xEventGroupSetBits(obj->event, bits);
    }
}

static void private_mc665_error_handler(struct at_client *client, const char *data, rt_size_t size, void *param)
{
    client->recv_line_buf[size - 1] = '\0';
    ESP_LOGE(TAG, "%s", client->recv_line_buf);
}

static void private_mc665_sim_handler(struct at_client *client, const char *data, rt_size_t size, void *param)
{
    client->recv_line_buf[size - 1] = '\0';
    mc665_drv_t *obj = (mc665_drv_t *)param;

    // +SIM READY
    if (!strncmp(data, "+SIM READY", sizeof("+SIM READY") - 1))
    {
        private_mc665_set_event_bits(obj, MC665_SIM_READY_BIT);
    }
    // +SIM DROP
    else if (!strncmp(data, "+SIM DROP", sizeof("+SIM DROP") - 1))
    {
        private_mc665_set_event_bits(obj, MC665_SIM_DROP_BIT);
    }
    // +SIM: Removed
    else if (!strncmp(data, "+SIM: Removed", sizeof("+SIM: Removed") - 1))
    {
        private_mc665_set_event_bits(obj, MC665_SIM_DROP_BIT);
    }
    else
    {
        ESP_LOGW(TAG, "%s", client->recv_line_buf);
    }
}

static void private_mc665_task(void *argument)
{
    bool ret = false;
    EventBits_t event = 0;
    int bit_error_rate = 0;
    int signal_intensity = 0;
    mc665_drv_t *obj = (mc665_drv_t *)argument;

    /* 等待SIM卡就绪 */
    event = xEventGroupWaitBits(obj->event, MC665_SIM_READY_BIT, pdTRUE, pdFALSE, 30000);
    obj->status = (event & MC665_SIM_READY_BIT) ? (MC665_STATUS_WAIT_CONNECTED) : (MC665_STATUS_DISCONNECTED);

    for (;;)
    {
        event = xEventGroupWaitBits(obj->event, MC665_SIM_READY_BIT | MC665_SIM_DROP_BIT, pdTRUE, pdFALSE, 1);
        if (event & MC665_SIM_READY_BIT)
        {
            ESP_LOGI(TAG, "MC665 sim inserted");
            obj->status = MC665_STATUS_WAIT_CONNECTED;
        }
        else if (event & MC665_SIM_DROP_BIT)
        {
            obj->status = MC665_STATUS_DISCONNECTED;
            ESP_LOGE(TAG, "MC665 sim removed");

            if (obj->event_cb.func)
            {
                obj->event_cb.func(obj->event_cb.param, MC665_EVT_NETWORK_DISCONNECTED);
            }
        }

        switch (obj->status)
        {
        case MC665_STATUS_WAIT_CONNECTED:
            if (mc665_detect(obj) && mc665_enable_rf(obj))
            {
                ESP_LOGI(TAG, "MC665 detected");
                obj->status = MC665_STATUS_WAIT_CARD_READY;
            }
            else
            {
                ESP_LOGW(TAG, "MC665 is not exist");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            break;
        case MC665_STATUS_WAIT_CARD_READY:
            if (mc665_read_pin(obj))
            {
                ESP_LOGI(TAG, "MC665 PIN ready");
                obj->status = MC665_STATUS_SET_APN;
            }
            else
            {
                ESP_LOGW(TAG, "MC665 wait PIN ready");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            break;
        case MC665_STATUS_SET_APN:
            if (mc665_set_apn(obj) && mc665_get_csq(obj, &signal_intensity, &bit_error_rate))
            {
                ESP_LOGI(TAG, "MC665 apn set success");
                obj->status = MC665_STATUS_SEARCH_NETWORK;
            }
            else
            {
                ESP_LOGW(TAG, "MC665 apn set fail");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            break;
        case MC665_STATUS_SEARCH_NETWORK:
            if (mc665_get_operator_info(obj, NULL, 0, NULL))
            {
                ESP_LOGI(TAG, "MC665 network searched");
                obj->status = MC665_STATUS_WAIT_GPRS_ENABLE;
            }
            else
            {
                ESP_LOGW(TAG, "MC665 network search timeout");
                vTaskDelay(pdMS_TO_TICKS(15000));
            }
            break;
        case MC665_STATUS_WAIT_GPRS_ENABLE:
            ret = mc665_ps_is_registered(obj);
            ret = ret && mc665_lte_is_registered(obj);

            if (ret)
            {
                ESP_LOGI(TAG, "MC665 enable GPRS");
                obj->status = MC665_STATUS_REQUEST_IP;
            }
            else
            {
                ESP_LOGW(TAG, "MC665 enable GPRS fail");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            break;
        case MC665_STATUS_REQUEST_IP:
            if (mc665_ip_is_available(obj))
            {
                ESP_LOGI(TAG, "MC665 ready!");
                obj->status = MC665_STATUS_READY;

                if (obj->event_cb.func)
                {
                    obj->event_cb.func(obj->event_cb.param, MC665_EVT_GOT_IP);
                }
            }
            else
            {
                ESP_LOGW(TAG, "MC665 request ip fail");
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            break;
        case MC665_STATUS_DISCONNECTED:
        case MC665_STATUS_READY:
            // To do:定时器查询网络状态，获取网络变化信息
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        default:
            break;
        }
    }
}

void mc665_register_callback(mc665_drv_t *obj, mc665_event_cb_t *cb)
{
    if (cb && obj)
    {
        obj->event_cb = *cb;
    }
}

bool mc665_init(mc665_drv_t *obj)
{
    bool ret = false;

    if (!obj)
    {
        ESP_LOGE(TAG, "obj is NULL");
        goto __exit;
    }

    /* 初始化URC */
    s_urc_table[0].cmd_prefix = "+CME ERROR:";
    s_urc_table[0].cmd_suffix = "\r\n";
    s_urc_table[0].func = private_mc665_error_handler;
    s_urc_table[0].param = obj;
    s_urc_table[1].cmd_prefix = "+ESMCAUSE:";
    s_urc_table[1].cmd_suffix = "\r\n";
    s_urc_table[1].func = private_mc665_error_handler;
    s_urc_table[1].param = obj;
    s_urc_table[2].cmd_prefix = "+CMS ERROR:";
    s_urc_table[2].cmd_suffix = "\r\n";
    s_urc_table[2].func = private_mc665_error_handler;
    s_urc_table[2].param = obj;
    s_urc_table[3].cmd_prefix = "+SIM";
    s_urc_table[3].cmd_suffix = "\r\n";
    s_urc_table[3].func = private_mc665_sim_handler;
    s_urc_table[3].param = obj;
    if (at_set_urc_table(s_urc_table, 4))
    {
        ESP_LOGE(TAG, "at client urc_table initial fail");
        goto __exit;
    }

    /* 为接收AT指令的应答分配内存 */
    obj->resp = at_create_resp(MC665_RECV_BUF_SIZE, 0, MC665_RECV_TIMEOUT);
    if (!obj->resp)
    {
        ESP_LOGE(TAG, "Create at client response object failed!");
        goto __exit;
    }

    obj->event = xEventGroupCreate();
    if (!obj->event)
    {
        ESP_LOGE(TAG, "Create event_group object failed!");
        goto __exit;
    }

    obj->mutex = xSemaphoreCreateMutex();
    if (!obj->mutex)
    {
        ESP_LOGE(TAG, "Create mutex object failed!");
        goto __exit;
    }

    xTaskCreate(private_mc665_task, "mc665_task", 1024 * 4, obj, 6, &obj->task);
    if (!obj->task)
    {
        ESP_LOGE(TAG, "Create mc665_task object failed! memory not enough");
        goto __exit;
    }

    ret = true;
    ESP_LOGI(TAG, "MC665 resource alloc success");

__exit:

    if (!ret)
    {
        if (obj->task)
        {
            vTaskDelete(obj->task);
            obj->task = NULL;
        }

        if (obj->resp)
        {
            at_delete_resp(obj->resp);
            obj->resp = false;
        }

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
    }

    return ret;
}

bool mc665_take_lock(mc665_drv_t *obj)
{
    return (pdTRUE == xSemaphoreTake(obj->mutex, portMAX_DELAY));
}

void mc665_release_lock(mc665_drv_t *obj)
{
    xSemaphoreGive(obj->mutex);
}

// 检查设备是否存在
bool mc665_detect(mc665_drv_t *obj)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        ret = (0 == at_client_wait_connect(1000));
        mc665_release_lock(obj);
    }

    return ret;
}

// 关闭回显
bool mc665_disable_echo(mc665_drv_t *obj)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        ret = (0 == at_exec_cmd(obj->resp, "ATE0"));
        mc665_release_lock(obj);
    }

    return ret;
}

// 设置搜网顺序LTE优先
bool mc665_set_network_search_priority(mc665_drv_t *obj)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        ret = (0 == at_exec_cmd(obj->resp, "AT+GTRAT=10,3,0"));
        mc665_release_lock(obj);
    }

    return ret;
}

// 查询模块射频功能设置，第一个参数非 1 需要设置+CFUN
bool mc665_rf_is_enabled(mc665_drv_t *obj)
{
    int state = 0;
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CFUN?"))
        {
            ret = ((obj->resp->line_counts >= 2) && (1 == at_resp_parse_line_args(obj->resp, 2, "+CFUN: %d", &state)) && (1 == state));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 设置为正常模式,，开启射频功能
bool mc665_enable_rf(mc665_drv_t *obj)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        ret = (0 == at_exec_cmd(obj->resp, "AT+CFUN=1"));
        mc665_release_lock(obj);
    }

    return ret;
}

// 查询PIN设置
bool mc665_read_pin(mc665_drv_t *obj)
{
    bool ret = false;
    const char *line = NULL;

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CPIN?"))
        {
            line = at_resp_get_line(obj->resp, 2);
            ret = (line && strstr(line, "+CPIN: READY"));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 读取IMSI
bool mc665_read_imsi(mc665_drv_t *obj, void *buf, uint32_t len)
{
    bool ret = false;
    char expr[40] = {0};
    char ismi[30] = {0};

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CIMI?"))
        {
            snprintf(expr, sizeof(expr), "+CIMI: %%%ds", sizeof(ismi) - 1);
            ret = ((obj->resp->line_counts >= 2) && (1 == at_resp_parse_line_args(obj->resp, 2, expr, ismi)));

            if (ret && buf && (len > strlen(ismi)))
            {
                memcpy(buf, ismi, strlen(ismi));
            }
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 设置APN
bool mc665_set_apn(mc665_drv_t *obj)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        ret = (0 == at_exec_cmd(obj->resp, "AT+CGDCONT=1,\"IP\""));
        mc665_release_lock(obj);
    }

    return ret;
}

// 读取信号强度
bool mc665_get_csq(mc665_drv_t *obj, int *signal_intensity, int *bit_error_rate)
{
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        if (signal_intensity && bit_error_rate && (0 == at_exec_cmd(obj->resp, "AT+CSQ?")))
        {
            ret = ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, "+CSQ: %d,%d", signal_intensity, bit_error_rate)));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 查询网络自动注册情况,act值参考mc665_act_def
bool mc665_get_operator_info(mc665_drv_t *obj, void *buf, uint32_t len, int *act)
{
    int temp_act = 0;
    bool ret = false;
    char operator[20] = {0};
    char expr[40] = {0};

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+COPS?"))
        {
            snprintf(expr, sizeof(expr), "+COPS: %%*d,%%*d,\"%%%d[^\"]\",%%d", sizeof(operator) - 1);
            ret = ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, expr, operator, & temp_act)));

            if (ret)
            {
                if (buf && (len > strlen(operator)))
                {
                    memcpy(buf, operator, strlen(operator));
                }

                if (act)
                {
                    *act = temp_act;
                }
            }
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 查询GPRS是否注册
bool mc665_ps_is_registered(mc665_drv_t *obj)
{
    int temp = 0;
    int status = 0;
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CGREG?"))
        {
            ret = ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, "+CGREG: %d,%d", &temp, &status)));
            ret = ret && ((1 == status) || (5 == status));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 查询EPS是否注册（查询4G数据业务可用状态）
bool mc665_lte_is_registered(mc665_drv_t *obj)
{
    int temp = 0;
    int status = 0;
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CEREG?"))
        {
            ret = ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, "+CEREG: %d,%d", &temp, &status)));
            ret = ret && ((1 == status) || (5 == status));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 查询CS域是否注册 (语言业务)
bool mc665_cs_is_registered(mc665_drv_t *obj)
{
    int temp = 0;
    int status = 0;
    bool ret = false;

    if (mc665_take_lock(obj))
    {
        if (0 == at_exec_cmd(obj->resp, "AT+CREG?"))
        {
            ret = ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, "+CREG: %d,%d", &temp, &status)));
            ret = ret && ((1 == status) || (5 == status));
        }

        mc665_release_lock(obj);
    }

    return ret;
}

// 尝试请求运营商分配IP
bool mc665_request_ip(mc665_drv_t *obj)
{
    bool ret = false;
    char ip[20] = {0};
    char expr[40] = {0};

    if (mc665_take_lock(obj))
    {
        at_resp_set_info(obj->resp, MC665_RECV_BUF_SIZE, 4, 30000);

        if (0 == at_exec_cmd(obj->resp, "AT+MIPCALL=1"))
        {
            snprintf(expr, sizeof(expr), "+MIPCALL: %%%ds", sizeof(ip) - 1);
            ret = ((obj->resp->line_counts >= 4) && (1 == at_resp_parse_line_args(obj->resp, 4, expr, ip)));
        }

        at_resp_set_info(obj->resp, MC665_RECV_BUF_SIZE, 0, MC665_RECV_TIMEOUT);

        mc665_release_lock(obj);
    }

    return ret;
}

// 查询当前IP
bool mc665_is_get_ip(mc665_drv_t *obj, void *buf, uint32_t len)
{
    bool ret = false;
    int requested = 0;
    char ip[20] = {0};
    char expr[40] = {0};

    if (mc665_take_lock(obj))
    {
        at_resp_set_info(obj->resp, MC665_RECV_BUF_SIZE, 0, 30000);

        if (0 == at_exec_cmd(obj->resp, "AT+MIPCALL?"))
        {
            snprintf(expr, sizeof(expr), "+MIPCALL: %%d,%%%ds", sizeof(ip) - 1);

            if ((obj->resp->line_counts >= 2) && (2 == at_resp_parse_line_args(obj->resp, 2, expr, &requested, ip) && requested))
            {
                ret = true;

                if (buf && len > strlen(ip))
                {
                    memcpy(buf, ip, strlen(ip) + 1);
                }
            }
        }

        at_resp_set_info(obj->resp, MC665_RECV_BUF_SIZE, 0, 10000);
        mc665_release_lock(obj);
    }

    return ret;
}

bool mc665_ip_is_available(mc665_drv_t *obj)
{
    bool ret = mc665_is_get_ip(obj, NULL, 0);

    if (!ret)
    {
        ret = mc665_request_ip(obj);
    }

    return ret;
}
