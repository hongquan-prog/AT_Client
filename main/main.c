/*
 * Copyright (c) 2022-2022, lihongquan
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-22     lihongquan   port to esp32
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "at.h"
#include "at_uart_drv.h"

#include "mc665.h"
#include "mc665_mqtt.h"
#include "mc665_http.h"
#include "mc665_ota.h"

#define MC665_PWR_PIN (GPIO_NUM_21)
#define MC665_RST_PIN (GPIO_NUM_19)

static const char *TAG = "main";
static com_drv_t   at_uart_drv = {0};
static mc665_drv_t mc665_drv = {0};
static mqtt_drv_t   mqtt_drv = {0};
static ota_drv_t   ota_drv = {0};


void mc665_event_callback(void *param, mc665_event_def event)
{
	switch (event)
	{
	case MC665_EVT_GOT_IP:
		mqtt_open(&mqtt_drv);
		break;
	case MC665_EVT_NETWORK_DISCONNECTED:
		break;
	default:
		break;
	}
}

void ota_callback(void *param, ota_event_def event)
{
	if (OTA_EVT_DOWNLOAD_FAIL == event)
	{

	}
	else if (OTA_EVT_DOWNLOAD_SUCCESS == event)
	{
		ESP_LOGI(TAG, "Prepare to restart system!");
		vTaskDelay(1000);
		ota_restart(param);
	}
}

static void mqtt_event_callback(void *args, mqtt_event_def event, mqtt_msg_t *pmsg)
{
    switch(event)
    {
    case MQTT_EVT_CONNECTED:
        break;
    case MQTT_EVT_DISCONNECTED:
        ESP_LOGW(TAG, "mqtt disconnected");
        break;
    case MQTT_EVT_SUBSCRIBED:
    case MQTT_EVT_UNSUBSCRIBED:
    case MQTT_EVT_PUBLISHED:
    case MQTT_EVT_BEFORE_CONNECT:
        break;
    case MQTT_EVT_DATA:
        ESP_LOGE(TAG, "mqtt data");
        break;
    case MQTT_EVT_ERROR:
        ESP_LOGW(TAG, "mqtt error");
        break;
    default:
        ESP_LOGW(TAG, "Unexpected mqtt event(%d)", event);
        return;
    }    
}

void at_modbule_init(void)
{
	mqtt_cfg_t mqtt_cfg = {
		.host = "1.14.62.200"
	};

	mc665_event_cb_t mc665_cb = {
		.param = &mc665_drv,
		.func = mc665_event_callback};

	ota_event_cb_t ota_cb = {
		.param = &ota_drv,
		.func = ota_callback};

	mqtt_event_cb_t mqtt_cb = {
		.func = mqtt_event_callback,
		.param = &mqtt_drv
        };

	at_uart_drv_get(&at_uart_drv);
	at_client_init(&at_uart_drv, 128);

	/* 复位模组 */
    gpio_reset_pin(MC665_PWR_PIN);
    gpio_set_direction(MC665_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(MC665_RST_PIN);
    gpio_set_direction(MC665_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MC665_PWR_PIN, 0);
    gpio_set_level(MC665_RST_PIN, 0);

	/* 使能设备电源 */
    gpio_set_level(MC665_PWR_PIN, 1);
    gpio_set_level(MC665_RST_PIN, 0);

	mc665_register_callback(&mc665_drv, &mc665_cb);
    mc665_init(&mc665_drv);
	
	ota_drv.user_data = &mc665_drv;
	mc665_ota_drv_get(&ota_drv);
	ota_register_callback(&ota_drv, &ota_cb);
	ota_init(&ota_drv);

	mqtt_drv.user_data = &mc665_drv;
	mc665_mqtt_drv_get(&mqtt_drv);
	mqtt_register_callback(&mqtt_drv, &mqtt_cb);
	mqtt_init(&mqtt_drv, &mqtt_cfg);
}

void app_main()
{
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(err);

	at_modbule_init();
}
