//
// Created by deity on 07.07.2025.
//

#ifndef LCD_H
#define LCD_H

#include <esp_err.h>

/**
 * @brief Initialize the SSD1306 display over I2C
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mono_lcd_init(void);

/**
 * @brief Clear entire display
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mono_lcd_clear(void);

/**
 * @brief Draw a null-terminated ASCII string, wrapping words
 * @param text    input string (max total length fits display)
 * @return ESP_OK on success, or ESP_FAIL if text too long
 */
esp_err_t mono_lcd_draw_text(const char *text);


#endif //LCD_H
