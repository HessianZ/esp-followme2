/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include <esp_err.h>
#include <esp_wifi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[64];
} WifiInfo;

void app_wifi_init();
esp_err_t app_wifi_start(void);
char *app_wifi_get_prov_payload(void);
bool app_wifi_is_connected(void);
esp_err_t app_wifi_get_wifi_ssid(char *ssid, size_t len);

/**
 * 扫描wifi ssid
 * @param number 最大扫描数量
 * @param ap_info 扫描结果
 * @param ap_count 扫描到的数量
 */
esp_err_t wifi_scan(uint16_t number, wifi_ap_record_t *ap_info, uint16_t *ap_count);

#ifdef __cplusplus
}
#endif
