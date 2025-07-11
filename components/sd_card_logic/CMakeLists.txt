idf_component_register(
        SRCS sd_card_logic.c
        INCLUDE_DIRS .
        REQUIRES vfs
        sdmmc
        fatfs
        spi_flash
)
