#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mono_lcd.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "sd_card_logic.h"
#include "ble.h"
#include "soc/uart_struct.h"

#define MOUNT_POINT "/sdcard"

// Пины для подключения SD карты
#define PIN_MISO    GPIO_NUM_19
#define PIN_MOSI    GPIO_NUM_23
#define PIN_CLK     GPIO_NUM_18
#define PIN_CS      GPIO_NUM_5

const char* TAG = "fizzy_wair";
TaskHandle_t  MAIN_CYCLE_DESC = NULL;

typedef enum {
    DISP_TEXT,
    DISP_ICON,
    DISP_CLEAR,
} disp_msg_type_t;

typedef struct {
    disp_msg_type_t type;
    union {
        struct { char text[32]; } txt;
        struct { uint8_t icon_id; } icon;
    } payload;
} disp_msg_t;

QueueHandle_t q;

const char* texts[] = {
    "[NEXT YEAR] Gas has been smoked again",
    NULL
};

void IRAM_ATTR uart_isr(void)
{
    uint8_t byte = UART0.fifo.rw_byte;
    xQueueSendFromISR(q, &byte, NULL);
}

bool check_file_exists_std(const char* filename) {
    struct stat st;
    return (stat(filename, &st) == 0);
}

long get_file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    if (size < 0) {
        perror("ftell");
    }
    fclose(f);

    ESP_LOGI(TAG, "File size: %ld", size);
    return size;
}

void main_cycle(void *pvParameters) {
    ESP_ERROR_CHECK(mono_lcd_init());
    for (size_t i = 0; texts[i] != NULL; i++) {
        mono_lcd_draw_text(texts[i]);
        vTaskDelay(pdMS_TO_TICKS(2000));
        mono_lcd_clear();
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_err_t ret = init_card(TAG);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось инициализировать SD карту");
        ESP_LOGE(TAG, "Проверьте:");
        ESP_LOGE(TAG, "1. Правильность подключения проводов");
        ESP_LOGE(TAG, "2. Качество SD карты (используйте известные бренды)");
        ESP_LOGE(TAG, "3. Форматирование в FAT32");
        ESP_LOGE(TAG, "4. Наличие подтягивающих резисторов");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(5000));

    const char *filename = "/sdcard/test.txt";

    if (check_file_exists_std(filename)) {
        ESP_LOGI(TAG, "Reading file");

        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return;
        }

        char line[512];
        while (fgets(line, sizeof(line), f) != NULL) {
            char *pos = strchr(line, '\n');
            if (pos) *pos = '\0';

            mono_lcd_clear();
            mono_lcd_draw_text(line);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        fclose(f);
    } else {
        FILE *f = fopen(filename, "wb");
        if (f == NULL) {
            perror("fopen");
            ESP_LOGE(TAG, "Failed to open file");
            return;
        }
        fprintf(f, "Hello, world!\n");
        fclose(f);
        ESP_LOGI(TAG, "File written");
    }

    ESP_LOGI(TAG, "Тестирование завершено");
    ESP_LOGI(TAG, "Перезагрузка через 5 секунд...");

    vTaskDelay(pdMS_TO_TICKS(5000));

    disp_msg_t msg;

    while (xQueueReceive(q, &msg, portMAX_DELAY) == pdPASS)
    {
        switch (msg.type) {
            case DISP_TEXT:
                mono_lcd_clear();
                mono_lcd_draw_text(msg.payload.txt.text);

                long old_size = get_file_size(filename);
                ESP_LOGI(TAG, "OLD File size: %ld", old_size);

                FILE *f = fopen(filename, "ab");

                // todo; mv to var впадлу щас(
                size_t written = fwrite(msg.payload.txt.text, 1, strlen(msg.payload.txt.text), f);
                if (written != strlen(msg.payload.txt.text)) {
                    perror("fwrite");
                }
                fwrite("\n", 1, 1, f);
                fclose(f);

                long new_size = get_file_size(filename);
                if (new_size != old_size + (long)strlen(msg.payload.txt.text) + 1) { // +1 для '\n'
                    ESP_LOGE(TAG, "Записано не так много, как ожидалось");
                }

                ESP_LOGI(TAG, "NEW File size: %ld", new_size);

                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            default:
                mono_lcd_clear();
                break;
        }
    }

    cleanup_sd_card(TAG);
}

void app_main(void)
{
    q = xQueueCreate(10, sizeof(disp_msg_t));

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    xTaskCreate(
        main_cycle,
        "check_sd_card",
        4096,
        NULL,
        10,
        &MAIN_CYCLE_DESC
    );

    bt_app_gatt_start(q);
}
