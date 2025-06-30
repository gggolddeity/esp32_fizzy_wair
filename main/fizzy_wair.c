#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#include "fonts6x8.h"

/*----------------------------------------------------------------------
 *  SSD1306 128×64, «page» mode (1 page = 8 vertical pixels)
 *  FRAMEBUFFER LAYOUT:
 *      fb[page * BYTES_PER_LINE + column]
 *  where page ∈ [0..7], column ∈ [0..127].
 *  Each byte stores a vertical column 8‑pixels high, LSBit = top pixel.
 *----------------------------------------------------------------------*/

#define I2C_NUM               I2C_NUM_0
#define I2C_SDA_GPIO          21
#define I2C_SCL_GPIO          22
#define I2C_FREQ_HZ           400000
#define DEV_ADDR              0x3C
#define EXAMPLE_LCD_CMD_BITS  8

#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT          64
#define PAGE_HEIGHT             8
#define FONT_WIDTH              6   // 6×8 шрифт из fonts6x8.h
#define FONT_HEIGHT             8

#define BYTES_PER_LINE   (SCREEN_WIDTH)                // 128
#define MAX_PAGES        (SCREEN_HEIGHT / PAGE_HEIGHT) // 8

static uint8_t fb[SCREEN_WIDTH * SCREEN_HEIGHT / 8];  // 1024‑byte framebuffer
static esp_lcd_panel_handle_t g_panel = NULL;         // глобальный дескриптор панели

/*--------------------------------------------------------------------*/
typedef struct {
    int  length;            // количество символов (≤16)
    char data[16];          // ASCII‑коды подряд без NUL
} word_buffer;

static const char *TAG = "SSD1306_DEMO";

/*------------------------------------------------------------------*/
static void i2c_scan(i2c_master_bus_handle_t bus)
{
    ESP_LOGI(TAG, "Scanning I²C bus…");
    esp_err_t ret = i2c_master_probe(bus, DEV_ADDR, 1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Device 0x%02X: %s", DEV_ADDR, (ret == ESP_OK) ? "FOUND" : "MISSING");
}

/*--------------------------------------------------------------------
 *  Отправляем одну PAGE (8 строк) на дисплей. Это быстрее, чем всегда
 *  слать все 1024 байта, и достаточно для «печатного» эффекта.
 *--------------------------------------------------------------------*/
static void flush_page(int page)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        g_panel,
        0,                       /* x0  */
        page * PAGE_HEIGHT,      /* y0  */
        SCREEN_WIDTH,            /* x1  */
        (page + 1) * PAGE_HEIGHT,/* y1  */
        fb + page * BYTES_PER_LINE));
}

/*--------------------------------------------------------------------
 *  Нарисовать одно слово (wb) начиная с (page, x) в байтовом буфере fb.
 *--------------------------------------------------------------------*/
static void draw_word_to_fb(int page, int x, const word_buffer *wb)
{
    uint8_t *dst = fb + page * BYTES_PER_LINE + x;

    for (int i = 0; i < wb->length; ++i) {
        const uint8_t *glyph = get_char_data(wb->data[i]);
        memcpy(dst, glyph, FONT_WIDTH);
        dst += FONT_WIDTH;

        /* сразу обновляем эту колонку на экране для плавной анимации */
        flush_page(page);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/*--------------------------------------------------------------------
 *  Вывести строку str с переносом слов. Возвращает 0 или -1 (не влезло).
 *--------------------------------------------------------------------*/
static int draw_string_to_fb(int start_x, const char *str)
{
    int cur_x    = start_x; // байтова позиция по X
    int cur_page = 0;       // страница 0‥7

    word_buffer wb = { .length = 0 };

    for (size_t i = 0, len = strlen(str); i < len && cur_page < MAX_PAGES;) {
        /* ----------- пропускаем пробелы (рисуем пустые колонки) */
        while (i < len && str[i] == ' ') {
            if (cur_x > BYTES_PER_LINE - FONT_WIDTH) {
                cur_x = 0;
                if (++cur_page >= MAX_PAGES) return -1;
            }
            memset(fb + cur_page * BYTES_PER_LINE + cur_x, 0, FONT_WIDTH);
            flush_page(cur_page);
            vTaskDelay(pdMS_TO_TICKS(30));
            cur_x += FONT_WIDTH;
            ++i;
        }

        /* -------- читаем очередное слово (до 16 символов) */
        wb.length = 0;
        while (i < len && str[i] != ' ' && wb.length < 16) {
            wb.data[wb.length++] = str[i++];
        }
        if (wb.length == 0) continue; // может быть длиннее 16 — отрежем

        int word_bytes = wb.length * FONT_WIDTH;
        if (cur_x > BYTES_PER_LINE - word_bytes) {
            cur_x = 0;
            if (++cur_page >= MAX_PAGES) return -1;
        }

        draw_word_to_fb(cur_page, cur_x, &wb);
        cur_x += word_bytes;
    }
    return 0;
}

void clear_before_new_text(void) {
    memset(fb, 0, sizeof fb);
    esp_lcd_panel_draw_bitmap(g_panel, 0, 0,
                          SCREEN_WIDTH, SCREEN_HEIGHT, fb);
}

/*====================================================================*/
void app_main(void)
{
    /* --------------------------- I²C init ------------------------ */
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
        .i2c_port              = I2C_NUM,
        .sda_io_num            = I2C_SDA_GPIO,
        .scl_io_num            = I2C_SCL_GPIO,
        .flags.enable_internal_pullup = true,
        .intr_priority         = 0
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
    i2c_scan(i2c_bus);

    /* --------------------- LCD panel init ------------------------ */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr            = DEV_ADDR,
        .scl_speed_hz        = I2C_FREQ_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset       = 6,
        .lcd_cmd_bits        = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits      = EXAMPLE_LCD_CMD_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_cfg, &io_handle));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_cfg, &g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));

    /* ------------------------- демо‑вывод ------------------------ */
    memset(fb, 0, sizeof fb);

    const char *demo = "WHO'S THAT JIGGY NINJA WITH THE GOLD LINKS";
    if (draw_string_to_fb(0, demo) != 0) {
        ESP_LOGW(TAG, "Text is too long for the display");
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    clear_before_new_text();
    const char *demo2 = "BUTT PLUGG NIGGAS HATE PLUG WIGGAS";
    if (draw_string_to_fb(0, demo2) != 0) {
        ESP_LOGW(TAG, "Text is too long for the display");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    clear_before_new_text();

    vTaskDelay(pdMS_TO_TICKS(2000));

    esp_restart();
}