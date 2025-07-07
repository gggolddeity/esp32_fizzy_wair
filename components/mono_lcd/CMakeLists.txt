idf_component_register(
        SRCS "mono_lcd.c"
        INCLUDE_DIRS .
        REQUIRES esp_lcd i2cdev
)