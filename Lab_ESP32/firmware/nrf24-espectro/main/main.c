#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Prueba de vida del nRF24L01+ por SPI2 (FSPI nativo), modo 0, 1 MHz.
// Solo lectura/escritura de registros: la radio queda en power-down
// (PWR_UP=0, CE=0), es decir, NO transmite nada. Regla dura del proyecto.

static const char *TAG = "nrf24";

// Pines según Lab_ESP32/PINOUT.md
#define PIN_CE   9
#define PIN_CSN  10
#define PIN_MOSI 11
#define PIN_SCK  12
#define PIN_MISO 13

// Registro del nRF24L01+
#define NRF_REG_CONFIG   0x00
#define NRF_REG_EN_AA    0x01
#define NRF_REG_SETUP_AW 0x03
#define NRF_REG_RF_CH    0x05
#define NRF_REG_RF_SETUP 0x06
#define NRF_REG_STATUS   0x07

#define NRF_CMD_W_REG 0x20

static spi_device_handle_t spi;

// SPI full-duplex: el byte de respuesta del chip viene en rx[0] (STATUS).
// Devuelve STATUS; los datos van por rdata (o NULL).
static uint8_t nrf_xfer(uint8_t cmd, const uint8_t *wdata, uint8_t *rdata, uint8_t len)
{
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    tx[0] = cmd;
    if (wdata != NULL) {
        memcpy(&tx[1], wdata, len);
    }
    memset(&rx[1], 0xFF, len); // clock out del chip para leer

    spi_transaction_t t = {
        .length = 8 * (len + 1),
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));

    if (rdata != NULL) {
        memcpy(rdata, &rx[1], len);
    }
    return rx[0];
}

static uint8_t nrf_read_reg(uint8_t reg)
{
    uint8_t v = 0;
    nrf_xfer(reg & 0x1F, NULL, &v, 1); // R_REGISTER
    return v;
}

static uint8_t nrf_write_reg(uint8_t reg, uint8_t val)
{
    return nrf_xfer((reg & 0x1F) | NRF_CMD_W_REG, &val, NULL, 1);
}

void app_main(void)
{
    // CE como GPIO de salida en bajo: radio quieta, en power-down
    gpio_config_t ce_cfg = {
        .pin_bit_mask = 1ULL << PIN_CE,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&ce_cfg));
    gpio_set_level(PIN_CE, 0);

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .mode = 0,            // CPOL=0, CPHA=0
        .clock_speed_hz = 1000000, // 1 MHz: cables Dupont de 20 cm
        .spics_io_num = PIN_CSN,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &spi));

    // El nRF24 necesita ~100 ms tras alimentarse antes de hablar por SPI
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "=== prueba de vida nRF24L01+ (SPI2, modo 0, 1 MHz) ===");

    uint8_t status = nrf_read_reg(NRF_REG_STATUS);
    uint8_t config = nrf_read_reg(NRF_REG_CONFIG);
    uint8_t en_aa = nrf_read_reg(NRF_REG_EN_AA);
    uint8_t setup_aw = nrf_read_reg(NRF_REG_SETUP_AW);
    uint8_t rf_ch = nrf_read_reg(NRF_REG_RF_CH);
    uint8_t rf_setup = nrf_read_reg(NRF_REG_RF_SETUP);

    ESP_LOGI(TAG, "STATUS   = 0x%02X", status);
    ESP_LOGI(TAG, "CONFIG   = 0x%02X (esperado 0x08)", config);
    ESP_LOGI(TAG, "EN_AA    = 0x%02X (esperado 0x3F)", en_aa);
    ESP_LOGI(TAG, "SETUP_AW = 0x%02X (esperado 0x03)", setup_aw);
    ESP_LOGI(TAG, "RF_CH    = 0x%02X (esperado 0x02)", rf_ch);
    ESP_LOGI(TAG, "RF_SETUP = 0x%02X (esperado 0x0E)", rf_setup);

    if (config == 0x00 && en_aa == 0x00 && rf_setup == 0x00) {
        ESP_LOGW(TAG, "TODO EN 0x00: chip sin alimentacion o MISO (GPIO13) sin conectar");
    } else if (config == 0xFF && en_aa == 0xFF && rf_setup == 0xFF) {
        ESP_LOGW(TAG, "TODO EN 0xFF: lineas SPI flotando, revisar GND comun y CSN (GPIO10)");
    }

    // Prueba de escritura real: RF_CH = 0x4C, lectura de vuelta y restaurar
    ESP_LOGI(TAG, "--- prueba de escritura RF_CH ---");
    nrf_write_reg(NRF_REG_RF_CH, 0x4C);
    uint8_t back = nrf_read_reg(NRF_REG_RF_CH);
    ESP_LOGI(TAG, "escribi 0x4C, lei 0x%02X", back);

    nrf_write_reg(NRF_REG_RF_CH, rf_ch);
    uint8_t restored = nrf_read_reg(NRF_REG_RF_CH);
    ESP_LOGI(TAG, "restaure 0x%02X, lei 0x%02X", rf_ch, restored);

    bool ok = (back == 0x4C) && (restored == rf_ch);
    if (ok) {
        ESP_LOGI(TAG, "PRUEBA DE VIDA: PASO - el SPI lee y escribe de verdad");
    } else {
        ESP_LOGE(TAG, "PRUEBA DE VIDA: FALLO - back=0x%02X restored=0x%02X", back, restored);
    }

    // Latido lento: confirma que la app sigue viva sin ensuciar el log
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "vivo | STATUS=0x%02X", nrf_read_reg(NRF_REG_STATUS));
    }
}
