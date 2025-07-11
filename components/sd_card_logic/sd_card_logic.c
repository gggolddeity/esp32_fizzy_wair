//
// Created by deity on 10.07.2025.
//
#include "sd_card_logic.h"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_protocol_defs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"


#define MOUNT_POINT "/sdcard"

static bool sd_card_mounted = false;
static sdmmc_card_t *card = NULL;

// Пины для подключения SD карты
#define PIN_MISO    GPIO_NUM_19
#define PIN_MOSI    GPIO_NUM_23
#define PIN_CLK     GPIO_NUM_18
#define PIN_CS      GPIO_NUM_5

esp_err_t init_card(const char *TAG) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = PIN_MISO,
        .sclk_io_num     = PIN_CLK,
        .quadwp_io_num   = -1,            // не используется
        .quadhd_io_num   = -1,            // не используется
        .max_transfer_sz = 4000,          // достаточно 4 КБ
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* ---------- 2.  Параметры устройства SD ---------- */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = SPI2_HOST;
    slot_config.gpio_cd = GPIO_NUM_NC;    // Детектор карты не используется
    slot_config.gpio_wp = GPIO_NUM_NC;    // Защита от записи не используется

    /* ---------- 3.  Параметры монтирования ---------- */
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = true,
        .max_files              = 5,
        .allocation_unit_size   = 0,
    };
    // Конфигурация хоста
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 7500;  // Начинаем с 20MHz

    ESP_LOGI(TAG, "Монтирование SD карты...");
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_cfg, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Не удалось смонтировать файловую систему");
            ESP_LOGE(TAG, "Возможно, карта не отформатирована в FAT32");
        } else {
            ESP_LOGE(TAG, "Ошибка инициализации карты: %s", esp_err_to_name(ret));
            ESP_LOGE(TAG, "Проверьте подключение и подтягивающие резисторы");
        }
        return ret;
    }

    sd_card_mounted = true;

    // Информация о карте
    ESP_LOGI(TAG, "✓ SD карта успешно смонтирована");
    ESP_LOGI(TAG, "Название: %s", card->cid.name);
    ESP_LOGI(TAG, "Тип: %s", (card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "Скорость: %d kHz", card->real_freq_khz);
    ESP_LOGI(TAG, "Размер: %.2f MB", (card->csd.capacity * 512ULL) / (1024.0 * 1024.0));

    return ESP_OK;
}

esp_err_t cleanup_sd_card(const char *TAG)
{
    if (sd_card_mounted) {
        ESP_LOGI(TAG, "Размонтирование SD карты...");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        sd_card_mounted = false;
        ESP_LOGI(TAG, "✓ SD карта размонтирована");
    }

    return ESP_OK;
}
