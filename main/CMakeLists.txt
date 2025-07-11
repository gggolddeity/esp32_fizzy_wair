idf_component_register(SRCS "fizzy_wair.c"
                    INCLUDE_DIRS "."
                    REQUIRES
        esp_lcd
        driver
        wpa_supplicant
        mono_lcd
        ble
        sd_card_logic
        vfs
        sdmmc
        fatfs
        spi_flash
        esp_common
        esp_timer
        soc
        log
        nvs_flash
)

