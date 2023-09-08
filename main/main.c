/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <lwip/inet.h>
#include <esp_check.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "dns_server.h"
#include "captive_portal.h"
#include "app_wifi.h"

#include "gui/ui_main.h"
#include "bsp/tft-feather.h"

static const char *TAG = "ESP-FOLLOWME2";


static void wifi_credential_reset(void *handle, void *arg)
{
    ESP_LOGW(TAG, "WiFi credential reset");

    esp_wifi_restore();
    esp_restart();
}

void wifi_task(void *args)
{
    /* Initialize Wi-Fi.
     */
//    app_wifi_init();

    /* Start the Wi-Fi. */
    wifi_init_softap();
//    esp_err_t err = app_wifi_start();
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "Could not start Wifi");
//    }

    vTaskDelete(NULL);
}

void main_init(void)
{
    gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL<<GPIO_NUM_21),
            .intr_type = GPIO_INTR_DISABLE,
            .pull_down_en = 0,
            .pull_up_en = 0,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "configure GPIO for TFT_I2C_POWER failed");
    }

    err = gpio_set_level(GPIO_NUM_21, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_level GPIO for TFT_I2C_POWER failed");
    }
}

void app_main(void)
{
    main_init();

    esp_log_level_set("TFT-FEATHER", ESP_LOG_VERBOSE);
    esp_log_level_set("ledc", ESP_LOG_VERBOSE);
    esp_log_level_set("lcd_panel.st7789", ESP_LOG_VERBOSE);
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    BaseType_t ret_val = xTaskCreatePinnedToCore(wifi_task, "Wifi Task", 4 * 1024, NULL, 1, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret_val) ? ESP_OK : ESP_FAIL);

    // Start the server for the first time
//    start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
//    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
//    start_dns_server(&config);

    bsp_display_start();

    ESP_LOGI(TAG, "GUI start");
    bsp_display_backlight_on();
    ESP_ERROR_CHECK(ui_main_start());
}
