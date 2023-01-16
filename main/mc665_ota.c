#include "mc665.h"
#include "mc665_ota.h"
#include "mc665_http.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include <string.h>
#include <stdlib.h>

/* 单次接收OTA数据包的长度（小于AT客户端行缓存） */
#define MC665_OTA_PACKT_SIZE 512

typedef struct
{
    /* 忽略软件版本检查 */
    bool ignore_version_check;
    mc665_drv_t *drv;
    mc665_http_drv_t http;
    ota_event_cb_t event_cb;
    ota_status_def status;
} private_mc665_ota_drv_t;

static const char *TAG = "mc665_ota_drv";
static private_mc665_ota_drv_t s_ota_drv = {
    .status = OTA_STATUS_UNINITIALIZED};

static void private_mc665_ota_register_callback(ota_event_cb_t *cb)
{
    s_ota_drv.event_cb = *cb;
}

static bool private_mc665_ota_init(void)
{
    bool ret = false;

    if (!s_ota_drv.drv)
    {
        ESP_LOGE(TAG, "The MC665 driver is NULL! please pass the mc665_drv_t pointer and retry!");
        goto __exit;
    }

    s_ota_drv.http.drv = s_ota_drv.drv;
    if (!mc665_http_init(&s_ota_drv.http))
    {
        ESP_LOGE(TAG, "The MC665 http resource init fail");
        goto __exit;
    }

    ret = true;
    ESP_LOGI(TAG, "MC665 ota init success!");
    s_ota_drv.status = OTA_STATUS_IDLE;

__exit:

    return ret;
}

static bool private_mc665_ota_validate_image_header(int file_offset)
{
    bool ret = false;
    char *data = NULL;
    esp_app_desc_t new_app_info = {0};
    esp_app_desc_t running_app_info = {0};
    const int header_size = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    
    data = (char *)malloc(header_size);
    if (!data)
    {
        ESP_LOGE(TAG, "No memory to recv http data");
        goto __exit;
    }

    if (mc665_http_read_data(&s_ota_drv.http, file_offset, header_size, data, 5000) == header_size)
    {
        memcpy(&new_app_info, data + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));
        ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
    }
    else
    {
        ESP_LOGW(TAG, "New app header read fail");
        goto __exit;
    }

    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Current running firmware version: %s", running_app_info.version);
    }

    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
    {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        goto __exit;
    }
    
    ret = true;

__exit:

    if (data)
    {
        free(data);
    }

    return ret;
}

static void private_mc665_ota_task(void *pvParameter)
{
    esp_err_t err;
    bool ret = false;
    char *data = NULL;
    int total_len = 0;
    int content_len = 0;
    int read_offset = 0;
    int received_file_length = 0;
    mc665_http_resp_t resp = {0};
    esp_ota_handle_t update_handle = {0};
    const esp_partition_t *update_partition = NULL;

    /* 等待http连接 */
    mc665_http_start(&s_ota_drv.http, MC665_HTTP_MODE_GET, 60);
    if (MC665_HTTP_CONNECT_SUCCESS == mc665_http_read_status(&s_ota_drv.http, 60000))
    {
        ESP_LOGI(TAG, "http connect success!");
    }
    else
    {
        ESP_LOGE(TAG, "http connect failed!");
        goto __exit;
    }

    /* 读取http应答 */
    if (mc665_http_read_resp(&s_ota_drv.http, &resp, 60000))
    {
        read_offset = 0;
        total_len = resp.length;
        ESP_LOGI(TAG, "http file size:%d", total_len);
    }
    else
    {
        ESP_LOGE(TAG, "http receive response timeout!");
        goto __exit;
    }

    /* 读取文件大小并计算出文件起始位置 */
    content_len = mc665_read_content_length(&s_ota_drv.http, 100);
    if (content_len > 0)
    {
        read_offset = total_len - content_len;
    }
    else
    {
        ESP_LOGE(TAG, "http content length read fail");
        goto __exit;
    }

    /* 读取下一个需要升级的分区，并验证app版本 */
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (NULL == update_partition)
    {
        ESP_LOGE(TAG, "esp ota partition read fail");
        goto __exit;
    }

    /* 验证版本是否和当前不一致 */
    if (!private_mc665_ota_validate_image_header(read_offset))
    {
        if (false == s_ota_drv.ignore_version_check)
        {
            goto __exit;
        }
        ESP_LOGW(TAG, "ignore app version check");
    }

    /* 开启ota */
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK)
    {
        esp_ota_abort(update_handle);
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto __exit;
    }

    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    /* 为读取OTA数据分配内存 */
    data = malloc(MC665_OTA_PACKT_SIZE);
    if (!data)
    {  
        ESP_LOGE(TAG, "No memory to recv http ota data");
        goto __exit;
    }

    for (; read_offset < total_len;)
    {
        /* 计算读取长度 */
        int read_len = total_len - read_offset;
        (read_len >= MC665_OTA_PACKT_SIZE) ? (read_len = MC665_OTA_PACKT_SIZE) : (0);

        /* 使用http读取文件 */
        if (mc665_http_read_data(&s_ota_drv.http, read_offset, read_len, data, 5000) == read_len)
        {
            read_offset += read_len;
            err = esp_ota_write(update_handle, (const void *)data, read_len);

            if (err != ESP_OK)
            {
                esp_ota_abort(update_handle);
                goto __exit;
            }

            received_file_length += read_len;
            ESP_LOGD(TAG, "Written image length %d", received_file_length);
        }
        else
        {
            esp_ota_abort(update_handle);
            ESP_LOGE(TAG, "Error: http data read error");
            goto __exit;
        }
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }

        goto __exit;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        goto __exit;
    }

    ret = true;
    ESP_LOGI(TAG, "MC665_HTTPS_OTA upgrade successful.");

__exit:

    s_ota_drv.status = OTA_STATUS_IDLE;
    if (s_ota_drv.event_cb.func)
    {
        s_ota_drv.event_cb.func(s_ota_drv.event_cb.param, (ret) ? (OTA_EVT_DOWNLOAD_SUCCESS) : (OTA_EVT_DOWNLOAD_FAIL));
    }

    if (data)
    {
        free(data);
    }

    vTaskDelete(NULL);
}

static bool private_mc665_ota_start(const char *url, bool ignore_version_check)
{
    bool ret = false;

    if (OTA_STATUS_IDLE == s_ota_drv.status)
    {
        ret = mc665_http_set_param(&s_ota_drv.http, MC665_HTTP_PARAM_URL, url);
        ret = ret && mc665_http_set_param(&s_ota_drv.http, MC665_HTTP_PARAM_USER_AGENT, "fibocom");
        ret = ret && mc665_http_set_param(&s_ota_drv.http, MC665_HTTP_PARAM_RESPONSEHEADER, "0");
        ret = ret && mc665_http_set_param(&s_ota_drv.http, MC665_HTTP_PARAM_REDIR, "1");
        ret = ret && (pdTRUE == xTaskCreate(private_mc665_ota_task, "mc665_ota_task", 1024 * 3, NULL, 5, NULL));
        (ret) ? (s_ota_drv.status = OTA_STATUS_RUNNING) : (0);

        if (ret)
        {
            s_ota_drv.ignore_version_check = ignore_version_check;
            ESP_LOGI(TAG, "MC665 ota task start");
        }
        else
        {
            ESP_LOGE(TAG, "MC665 ota task create fail");
        }
    }
    else
    {
        ESP_LOGE(TAG, "MC665 ota task is running");
    }

    return ret;
}

static void private_mc665_ota_restart(void)
{
    esp_restart();
}

void mc665_ota_drv_get(ota_drv_t *drv)
{
    if (drv)
    {
        drv->register_callback = private_mc665_ota_register_callback;
        drv->init = private_mc665_ota_init;
        drv->restart = private_mc665_ota_restart;
        drv->start = private_mc665_ota_start;

        if (!drv->user_data)
        {
            ESP_LOGE(TAG, "The user_data is NULL! please pass the mc665_drv_t pointer and retry!");
        }

        s_ota_drv.drv = drv->user_data;
    }
}