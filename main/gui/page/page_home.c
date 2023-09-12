//
// Created by Hessian on 2023/9/10.
//

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include "page_home.h"
#include "app_wifi.h"

static const char *TAG = "PAGE_HOME";

LV_FONT_DECLARE(font_OPPOSans_L_16);
static const lv_font_t *main_font = &font_OPPOSans_L_16;

static lv_obj_t *lab_net_state = NULL;

static void update_text()
{
    wifi_mode_t mode;

    if (lab_net_state == NULL) {
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));

    if (mode != WIFI_MODE_STA) {
        lv_label_set_text_fmt(lab_net_state, "#F2C161 连接下方的AP配网#\n#CCCCCC %s#\n#999999 密码# #CCCCCC %s#", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else {
        char wifi_ssid[64];
        app_wifi_get_wifi_ssid(wifi_ssid, 64);

        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_default_netif(), &ip_info);

        char ip_addr[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip_addr, 16);

        if (app_wifi_is_connected()) {
            lv_label_set_text_fmt(lab_net_state, "#999999 已连接# #99ff99 %s#\n#999999 IP地址# #99ff99 %s#\n#666666 长按Boot0重置WiFi#", wifi_ssid, ip_addr);
        } else {
            lv_label_set_text_fmt(lab_net_state, "#F2C161 正在连接...#\n#8561F2 %s#", wifi_ssid);
        }
    }

    lv_obj_align_to(lab_net_state, lv_obj_get_parent(lab_net_state), LV_ALIGN_TOP_MID, 0, -10);
}


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "event_handler start");
    update_text();
    ESP_LOGI(TAG, "event_handler end");
}

void page_home_render(lv_obj_t *parent)
{
    ESP_LOGD(TAG, "render start");

    if (parent == NULL) {
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, NULL));

    if (lab_net_state == NULL) {
        lab_net_state = lv_label_create(parent);
        lv_label_set_recolor(lab_net_state, true);
        lv_obj_set_style_text_font(lab_net_state, main_font, LV_PART_MAIN);
    }
//    lv_obj_set_style_text_font(lab_net_state, &main_font, LV_PART_MAIN);
//    lv_obj_align(lab_net_state, LV_ALIGN_CENTER, 0, 0);

    update_text();

    ESP_LOGD(TAG, "render end");
}

void page_home_destroy()
{
    ESP_LOGI(TAG, "destroyer start");
    if (lab_net_state != NULL) {
        lv_obj_del(lab_net_state);
        lab_net_state = NULL;
    }
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler));
    ESP_LOGD(TAG, "destroy end");
}