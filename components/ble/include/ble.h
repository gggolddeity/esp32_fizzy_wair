//
// Created by deity on 10.07.2025.
//
#pragma once

#include "esp_err.h"
#ifndef BLE_H
#define BLE_H

void bt_app_gatt_start(QueueHandle_t q);
void bt_app_gap_start_up(void);

#endif //BLE_H
