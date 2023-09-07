//
// Created by Hessian on 2023/9/6.
//

#include <esp_log.h>
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include "captive_portal.h"

static const char *TAG = "CAPTIVE_PORTAL";

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");


static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/config");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to /config");
    return ESP_OK;
}

// HTTP Save Handler
static esp_err_t config_get_handler(httpd_req_t *req)
{
    const char page[] = "<form action=\"/config\" method=\"get\"><br><br>\n"
                        "SSID:  <input type=\"text\" id=\"ssid\" name=\"ssid\"><br><br>\n"
                        "Password:  <input type=\"text\" id=\"password\" name=\"password\"><br><br>\n"
                        "  <input type=\"submit\" value=\"Connect\">"
                        "</form>";
    char  *buf = NULL;
    size_t buf_len;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            wifi_config_t wifi_cfg = {};

            if (httpd_query_key_value(buf, "ssid", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "ssid=%s", param);
                strncpy((char *)wifi_cfg.sta.ssid, param, sizeof(wifi_cfg.sta.ssid));
            }
            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "password=%s", param);
                strncpy((char *)wifi_cfg.sta.password, param, sizeof(wifi_cfg.sta.password));
            }

            if (strlen((char *)wifi_cfg.sta.ssid) > 0 && strlen((char *)wifi_cfg.sta.password)) {
                httpd_resp_set_type(req, "text/html");
                esp_wifi_set_mode(WIFI_MODE_STA);
                if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) == ESP_OK &&
                    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
                    const char wifi_configured[] = "<h1>Connecting...</h1>";
                    ESP_LOGI(TAG, "WiFi settings accepted!");
                    httpd_resp_send(req, wifi_configured, strlen(wifi_configured));
                } else {
                    const char wifi_config_failed[] = "<h1>Failed to configure WiFi settings</h1>";
                    ESP_LOGE(TAG, "Failed to set WiFi config to flash");
                    httpd_resp_send(req, wifi_config_failed, strlen(wifi_config_failed));
                }

                free(buf);
//                if (s_flags) {
//                    xEventGroupSetBits(*s_flags, s_success_bit);
//                }
                return ESP_OK;
            }
        }
        free(buf);
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, sizeof(page));

    return ESP_OK;
}

static const httpd_uri_t root_action = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
};

static const httpd_uri_t generate_204_action = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = root_get_handler
};

static const httpd_uri_t generate204_action = {
        .uri = "/generate204",
        .method = HTTP_GET,
        .handler = root_get_handler
};

static const httpd_uri_t config_action = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = config_get_handler
};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "404 Not Found");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Page not found", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = CONFIG_LWIP_MAX_SOCKETS - 3;
    config.lru_purge_enable = true;
    config.max_resp_headers = 20;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root_action);
        httpd_register_uri_handler(server, &generate204_action);
        httpd_register_uri_handler(server, &generate_204_action);
        httpd_register_uri_handler(server, &config_action);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}