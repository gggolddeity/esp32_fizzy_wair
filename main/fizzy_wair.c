#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mono_lcd.h"

const char* texts[] = {
    "Sergant McDonalds, We've been smoking zaza for some time",
    "IMHO, we'd stop doing it daily",
    "What you think about it?",
    "{Studidly} Yeah, let's stop",
    "[NEXT DAY] Gas has been smoked again",
    NULL
};

/*====================================================================*/
void app_main(void)
{
    ESP_ERROR_CHECK(mono_lcd_init());
    for (int i = 0; texts[i] != NULL; i++) {
        mono_lcd_draw_text(texts[i]);
        vTaskDelay(pdMS_TO_TICKS(2000));
        mono_lcd_clear();
    }

    esp_restart();
}