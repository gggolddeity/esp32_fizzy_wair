//
// Created by deity on 10.07.2025.
//
#pragma once

#ifndef SD_CARD_LOGIC_H
#define SD_CARD_LOGIC_H
#include "esp_err.h"

esp_err_t init_card(const char *TAG);
esp_err_t cleanup_sd_card(const char *TAG);

#endif //SD_CARD_LOGIC_H
