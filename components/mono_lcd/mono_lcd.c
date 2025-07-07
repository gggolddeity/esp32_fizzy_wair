//
// Created by deity on 07.07.2025.
//

#include "mono_lcd.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "fonts6x8.h"
#include <string.h>

static const char *TAG = "mono_lcd";

#define I2C_NUM         I2C_NUM_0
#define I2C_SDA_GPIO    21
#define I2C_SCL_GPIO    22
#define I2C_FREQ_HZ     400000
#define DEV_ADDR        0x3C
#define SCREEN_W        128
#define SCREEN_H        64
#define PAGE_H           8
#define FONT_W           6
#define FONT_H           8
#define LINE_BYTES     (SCREEN_W)
#define MAX_PAGES      (SCREEN_H / PAGE_H)

static uint8_t fb[SCREEN_W * SCREEN_H / 8];
static esp_lcd_panel_handle_t panel;

// send one page (8-pixel rows)
static esp_err_t flush_page(int page)
{
    return esp_lcd_panel_draw_bitmap(panel,
                                     0, page*PAGE_H,
                                     SCREEN_W,(page+1)*PAGE_H,
                                     fb + page*LINE_BYTES);
}

// draw a word buffer to fb at given page,x
static void draw_word(int page, int x, const char *word, int len)
{
    uint8_t *dst = fb + page*LINE_BYTES + x;
    for (int i=0; i<len; ++i) {
        const uint8_t *glyph = get_char_data((uint8_t)word[i]);
        memcpy(dst, glyph, FONT_W);
        dst += FONT_W;
        flush_page(page);
    }
}

// main draw function
esp_err_t mono_lcd_draw_text(const char *str)
{
    int cur_x=0, cur_page=0;
    const char *p = str;

    memset(fb,0,sizeof fb);
    for (; *p && cur_page<MAX_PAGES;) {
        // skip spaces
        while (*p==' ') {
            if (cur_x>LINE_BYTES-FONT_W) { cur_x=0; cur_page++; if (cur_page>=MAX_PAGES) return ESP_FAIL; }
            memset(fb+cur_page*LINE_BYTES+cur_x,0,FONT_W);
            flush_page(cur_page);
            cur_x+=FONT_W; p++;
        }
        // collect word
        char buf[16]; int len=0;
        while (*p && *p!=' ' && len<16) buf[len++] = *p++;
        if (len==0) continue;
        int wbytes = len*FONT_W;
        if (cur_x>LINE_BYTES-wbytes) { cur_x=0; cur_page++; if (cur_page>=MAX_PAGES) return ESP_FAIL; }
        draw_word(cur_page, cur_x, buf, len);
        cur_x += wbytes;
    }
    return ESP_OK;
}

esp_err_t mono_lcd_clear(void)
{
    memset(fb,0,sizeof fb);
    for (int page=0; page<MAX_PAGES; ++page) {
        esp_err_t ret = flush_page(page);
        if (ret!=ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t mono_lcd_init(void)
{
    // I2C master init
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .flags.enable_internal_pullup = true,
        .intr_priority = 0,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err!=ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        return err;
    }

    // panel IO
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = DEV_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    err = esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io);
    if (err!=ESP_OK) return err;

    // panel
    esp_lcd_panel_dev_config_t panel_cfg = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    err = esp_lcd_new_panel_ssd1306(io, &panel_cfg, &panel);
    if (err!=ESP_OK) return err;

    // init & turn on
    esp_lcd_panel_init(panel);
    esp_lcd_panel_disp_on_off(panel, true);

    // clear on start
    return mono_lcd_clear();
}