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

#include "at.h"
#include "at_uart_drv.h"

static const char *TAG = "main";
static com_drv_t at_uart_drv = {0};

void at_modbule_init(void)
{
	at_uart_drv_get(&at_uart_drv);
	at_client_init(&at_uart_drv, 128);

	at_response_t resp = at_create_resp(256, 0, 3000);
	
	if (0 == at_exec_cmd(resp, "AT+QHTTPCFG=\"contextid\",1"))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}
	if (0 == at_exec_cmd(resp, "AT+QHTTPCFG=\"responseheader\",1 "))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}
	if (0 == at_exec_cmd(resp, "AT+QIACT?"))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}
	if (0 == at_exec_cmd(resp, "AT+QICSGP=1,1,\"UNINET\",\"\",\"\",1"))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}
	if (0 == at_exec_cmd(resp, "AT+QIACT=1"))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}
	if (0 == at_exec_cmd(resp, "AT+QIACT?"))
	{
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 1));
		ESP_LOGI(TAG, "%s", at_resp_get_line(resp, 2));
	}

	at_delete_resp(resp);
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
