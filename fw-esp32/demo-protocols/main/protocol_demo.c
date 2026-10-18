/* Combined external protocol stimulus for the Zybo logic analyzer.
 *
 * All demo pins are on the ESP32-S3-DevKitC-1 J1 header, the same header row
 * as GPIO17. The analyzer can capture all signals together and decode three
 * protocols in parallel:
 *
 *   UART  GPIO17 TX            115200 8N1, repeating "LA-S3\n"
 *   SPI   SCK GPIO12, MOSI GPIO11, CS GPIO10
 *                              mode 0, 8-bit, 10 kHz, repeating 4C 41
 *   I2C   master SCL GPIO9, SDA GPIO8
 *         slave  SCL GPIO4, SDA GPIO5 (wire 9-4 and 8-5 externally)
 *                              address 0x50, 10 kHz; master writes 4C 41,
 *                              then reads two bytes 41 33 from the slave
 *
 * The two I2C controllers make the demo self-contained: no external sensor is
 * required. The analyzer taps the master-side SCL/SDA bus and common ground.
 * This is stimulus-only firmware; it replaces the ESP32 gateway application.
 */
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "LA_PROTO_DEMO";

#define UART_PORT UART_NUM_1
#define UART_TX_GPIO 17
#define UART_RX_GPIO 16
#define UART_BAUD 115200

#define SPI_HOST SPI2_HOST
#define SPI_SCK_GPIO 12
#define SPI_MOSI_GPIO 11
#define SPI_CS_GPIO 10
#define SPI_HZ 10000

#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_MASTER_SCL_GPIO 9
#define I2C_MASTER_SDA_GPIO 8
#define I2C_SLAVE_PORT I2C_NUM_1
#define I2C_SLAVE_SCL_GPIO 4
#define I2C_SLAVE_SDA_GPIO 5
#define I2C_HZ 10000
#define I2C_SLAVE_ADDR 0x50

/* Eight pending read-request tokens. Full policy: drop the newest ISR token;
 * never block in the callback. A missed request is visible as a master timeout. */
static QueueHandle_t s_slave_events;
static i2c_slave_dev_handle_t s_slave;

static bool IRAM_ATTR slave_request_cb(i2c_slave_dev_handle_t handle,
                                       const i2c_slave_request_event_data_t *event,
                                       void *arg)
{
    (void)handle;
    (void)event;
    (void)arg;
    BaseType_t woken = pdFALSE;
    uint8_t token = 1;
    xQueueSendFromISR(s_slave_events, &token, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR slave_receive_cb(i2c_slave_dev_handle_t handle,
                                       const i2c_slave_rx_done_event_data_t *event,
                                       void *arg)
{
    (void)handle;
    (void)event;
    (void)arg;
    return false;
}

static void uart_task(void *arg)
{
    (void)arg;
    const uart_config_t config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    static const char message[] = "LA-S3\n";
    unsigned long batches = 0;
    TickType_t last_report = xTaskGetTickCount();

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART ready TX=GPIO%d %d 8N1 payload=4C 41 2D 53 33 0A",
             UART_TX_GPIO, UART_BAUD);

    for (;;) {
        int written = uart_write_bytes(UART_PORT, message, sizeof(message) - 1);
        if (written != (int)sizeof(message) - 1) {
            ESP_LOGW(TAG, "UART short write %d", written);
        }
        (void)uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
        ++batches;
        vTaskDelay(pdMS_TO_TICKS(10));
        TickType_t now = xTaskGetTickCount();
        if (now - last_report >= pdMS_TO_TICKS(1000)) {
            ESP_LOGI(TAG, "UART running batches=%lu", batches);
            last_report = now;
        }
    }
}

static void spi_task(void *arg)
{
    (void)arg;
    spi_bus_config_t bus = {
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = -1,
        .sclk_io_num = SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8,
    };
    spi_device_interface_config_t device = {
        .clock_speed_hz = SPI_HZ,
        .mode = 0,
        .spics_io_num = SPI_CS_GPIO,
        .queue_size = 1,
    };
    spi_device_handle_t handle = NULL;
    static const uint8_t payload[2] = {0x4C, 0x41};
    spi_transaction_t transaction = {0};

    esp_err_t err = spi_bus_initialize(SPI_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    err = spi_bus_add_device(SPI_HOST, &device, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    transaction.length = sizeof(payload) * 8;
    transaction.tx_buffer = payload;
    ESP_LOGI(TAG, "SPI ready SCK=GPIO%d MOSI=GPIO%d CS=GPIO%d mode=0 %dHz payload=4C 41",
             SPI_SCK_GPIO, SPI_MOSI_GPIO, SPI_CS_GPIO, SPI_HZ);

    for (;;) {
        err = spi_device_transmit(handle, &transaction);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SPI transaction failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void i2c_slave_task(void *arg)
{
    (void)arg;
    static const uint8_t response[2] = {0x41, 0x33};
    uint8_t token;

    for (;;) {
        if (xQueueReceive(s_slave_events, &token, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        uint32_t written = 0;
        esp_err_t err = i2c_slave_write(s_slave, response, sizeof(response),
                                        &written, 1000);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2C slave write failed: %s", esp_err_to_name(err));
        }
    }
}

static void i2c_master_task(void *arg)
{
    i2c_master_dev_handle_t device = (i2c_master_dev_handle_t)arg;
    static const uint8_t write_payload[2] = {0x4C, 0x41};
    uint8_t register_pointer = 0x4C;
    uint8_t read_payload[2] = {0};
    unsigned long failures = 0;
    unsigned long successes = 0;
    TickType_t last_report = xTaskGetTickCount();

    for (;;) {
        esp_err_t err = i2c_master_transmit(device, write_payload,
                                            sizeof(write_payload), 1000);
        if (err != ESP_OK) {
            ++failures;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        err = i2c_master_transmit_receive(device, &register_pointer,
                                          sizeof(register_pointer),
                                          read_payload, sizeof(read_payload), 1000);
        if (err != ESP_OK) {
            ++failures;
        } else {
            ++successes;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        TickType_t now = xTaskGetTickCount();
        if (now - last_report >= pdMS_TO_TICKS(1000)) {
            if (failures != 0) {
                ESP_LOGW(TAG, "I2C ok=%lu failed=%lu (check GPIO9-4 and GPIO8-5 loopback)",
                         successes, failures);
            } else {
                ESP_LOGI(TAG, "I2C running ok=%lu write=4C 41 read=%02X %02X",
                         successes, read_payload[0], read_payload[1]);
            }
            failures = 0;
            last_report = now;
        }
    }
}

void app_main(void)
{
    s_slave_events = xQueueCreate(8, sizeof(uint8_t));
    if (s_slave_events == NULL) {
        ESP_LOGE(TAG, "Cannot create I2C slave event queue");
        return;
    }

    i2c_slave_config_t slave_config = {
        .i2c_port = I2C_SLAVE_PORT,
        .sda_io_num = I2C_SLAVE_SDA_GPIO,
        .scl_io_num = I2C_SLAVE_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = 32,
        .receive_buf_depth = 32,
        .slave_addr = I2C_SLAVE_ADDR,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .flags.enable_internal_pullup = 1,
    };
    ESP_ERROR_CHECK(i2c_new_slave_device(&slave_config, &s_slave));
    i2c_slave_event_callbacks_t callbacks = {
        .on_request = slave_request_cb,
        .on_receive = slave_receive_cb,
    };
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(s_slave, &callbacks, NULL));
    xTaskCreate(i2c_slave_task, "i2c_slave", 3072, NULL, 5, NULL);

    i2c_master_bus_config_t master_config = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = I2C_MASTER_SDA_GPIO,
        .scl_io_num = I2C_MASTER_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_dev_handle_t device = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&master_config, &bus));
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_SLAVE_ADDR,
        .scl_speed_hz = I2C_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device_config, &device));
    ESP_LOGI(TAG, "I2C ready master SCL=GPIO%d SDA=GPIO%d slave SCL=GPIO%d SDA=GPIO%d "
                  "addr=0x%02X %dHz (wire 9-4 and 8-5)",
             I2C_MASTER_SCL_GPIO, I2C_MASTER_SDA_GPIO,
             I2C_SLAVE_SCL_GPIO, I2C_SLAVE_SDA_GPIO, I2C_SLAVE_ADDR, I2C_HZ);

    xTaskCreate(i2c_master_task, "i2c_master", 4096, device, 5, NULL);
    xTaskCreate(spi_task, "spi", 4096, NULL, 5, NULL);
    xTaskCreate(uart_task, "uart", 4096, NULL, 5, NULL);
}
