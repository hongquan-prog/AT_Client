#include "driver/uart.h"
#include "driver/gpio.h"
#include "at_uart_drv.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/select.h>

#define AT_UART             UART_NUM_1
#define AT_UART_RX_BUF_SIZE (1024)
#define AT_UART_BAUD_RATE   (115200)
#define AT_UART_TX_PIN      (GPIO_NUM_23)
#define AT_UART_RX_PIN      (GPIO_NUM_22)

typedef struct
{
    int fd;
} at_uart_drv_t;

static const char *TAG = "at_uart_drv";
static at_uart_drv_t at_uart_drv = {-1};

static void at_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = AT_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    ESP_ERROR_CHECK(uart_param_config(AT_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(AT_UART, AT_UART_TX_PIN, AT_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(AT_UART, AT_UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    at_uart_drv.fd = open("/dev/uart/1", O_RDWR);

    if (at_uart_drv.fd == -1)
    {
        ESP_LOGE(TAG, "Cannot open UART");
        return;
    }

    esp_vfs_dev_uart_use_driver(1);
}

bool at_uart_available(void)
{
    size_t size = 0;
    bool ret = false;

    if ((-1 != at_uart_drv.fd) && (ESP_OK == uart_get_buffered_data_len(AT_UART, &size)))
    {
        ret = (size > 0);
    }

    return ret;
}

static int at_uart_write(const void *src, uint32_t size)
{
    return (-1 != at_uart_drv.fd) ? (uart_write_bytes(AT_UART, src, size)) :(0);
}

static int at_uart_read(void *buf, uint32_t length, uint32_t timeout_ms)
{
    int ret = 0;
    fd_set read_set = {0};
    struct timeval tv = {0};

    if (-1 != at_uart_drv.fd)
    {
        if (portMAX_DELAY != timeout_ms)
        {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
        }
        
        FD_ZERO(&read_set);
        FD_SET(at_uart_drv.fd, &read_set);
        ret = select(at_uart_drv.fd + 1, &read_set, NULL, NULL, (portMAX_DELAY != timeout_ms) ? (&tv) : (NULL));

        if ((ret > 0) && FD_ISSET(at_uart_drv.fd, &read_set))
        {
            // 使用read时，0xd会被被替换，所以此处使用uart_read_bytes
            return uart_read_bytes(AT_UART, buf, length, 0);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(timeout_ms));
        return 0;
    }
}

static void at_uart_flush_input(void)
{
    uart_flush_input(AT_UART);
}

void at_uart_drv_get(com_drv_t *drv)
{
    if (drv)
    {
        drv->name = "uart1";
        drv->init = at_uart_init;
        drv->read = at_uart_read;
        drv->write = at_uart_write;
        drv->flush = at_uart_flush_input;
        drv->available = at_uart_available;
    }
}