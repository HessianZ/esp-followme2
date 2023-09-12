//
// Created by Hessian on 2023/9/6.
//

#include <esp_log.h>
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <ctype.h>
#include <sys/stat.h>
#include <esp_check.h>
#include "captive_portal.h"
#include "app_wifi.h"

static const char *TAG = "CAPTIVE_PORTAL";

#define HTML_BUF_SIZE 2048

static httpd_handle_t server = NULL;

bool file_ext_cmp(const char *filename, const char *extension) {
    // 获取文件名中最后一个点的位置
    const char *dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        // 比较扩展名是否匹配
        return strcmp(dot + 1, extension) == 0;
    }

    return false; // 没有扩展名或者不匹配
}

static esp_err_t send_file_response(httpd_req_t *req, char* filename)
{
    esp_err_t ret = ESP_OK;
    FILE *fp = NULL;
    char *buf = NULL;
    size_t read_len = 0;

    fp = fopen(filename, "rb");

    ESP_GOTO_ON_FALSE(fp != NULL, ESP_ERR_NOT_FOUND, err, TAG, "Failed to open file %s", filename);

    buf = calloc(HTML_BUF_SIZE, sizeof(char));

    if (NULL == buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for buf");
        return ESP_ERR_NO_MEM;
    }

    if (file_ext_cmp(filename, "js")) {
        httpd_resp_set_type(req, "text/javascript");
    } else if (file_ext_cmp(filename, "css")) {
        httpd_resp_set_type(req, "text/css");
    } else {
        httpd_resp_set_type(req, "text/html");
    }

    while ((read_len = fread(buf, sizeof(char), HTML_BUF_SIZE, fp)) > 0) {
        httpd_resp_send_chunk(req, buf, read_len);
    }

    ESP_GOTO_ON_FALSE(feof(fp), ESP_FAIL, err, TAG, "Failed to read file %s error: %d", filename, ferror(fp));

    // chunks send finish
    httpd_resp_send_chunk(req, NULL, 0);

    err:
    if (fp != NULL) {
        fclose(fp);
    }
    if (buf != NULL) {
        free(buf);
    }

    if (ret == ESP_ERR_NOT_FOUND) {
        ret = httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    }

    return ret;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    uint16_t number = 10;
    uint16_t size = number * sizeof(wifi_ap_record_t);
    wifi_ap_record_t *ap_info = malloc(number * sizeof(wifi_ap_record_t));
    uint16_t ap_count = 0;
    char *resp = NULL;
    char json_obj[128];
    uint16_t resp_buf_len;

    memset(ap_info, 0, size);

    ESP_GOTO_ON_ERROR(wifi_scan(number, ap_info, &ap_count), end, TAG, "wifi_scan failed");

    httpd_resp_set_type(req, "text/html");

    if (ap_count > 0) {
        resp_buf_len = ap_count * (sizeof(ap_info->ssid) + 22);
        resp = malloc(resp_buf_len);
        memset(resp, 0, resp_buf_len);

        resp[0] = '[';

        int resp_len = 1;
        for (int i = 0; (i < number) && (i < ap_count); i++) {
            resp_len += sprintf(json_obj, "{\"ssid\":\"%s\", \"rssi\":%d}", ap_info[i].ssid, ap_info[i].rssi);
            if (i > 0) {
                strcat(resp, ",");
                resp_len += 1;
            }
            strcat(resp, json_obj);
        }
        strcat(resp, "]");
//        resp[resp_len+1] = '\0';
        resp_len += 1;
        ESP_LOGD(TAG, "resp: %s", resp);
        ESP_RETURN_ON_ERROR(httpd_resp_send(req, resp, resp_len), TAG, "send response failed");
        free(resp);
    } else {
        ESP_RETURN_ON_ERROR(httpd_resp_sendstr(req, "[]"), TAG, "send response failed");
    }

    end:
    free(ap_info);

    return ret;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/config");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_sendstr(req, "Redirect to the captive portal");

    ESP_LOGI(TAG, "Redirecting to /config");
    return ESP_OK;
}

void url_decode(char *src) {
    char *dst = src;
    char a, b;

    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((uint8_t)a) && isxdigit((uint8_t)b))) {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// HTTP Save Handler
static esp_err_t config_get_handler(httpd_req_t *req)
{
    // default response
    char index_filename[] = CONFIG_BSP_SPIFFS_MOUNT_POINT "/index.html";
    return send_file_response(req, index_filename);
}

static void async_wifi_connect(void* arg)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    // wifi connect will be auto called after start
    // see app_wifi event handler function.
    // ESP_ERROR_CHECK(esp_wifi_connect());

    vTaskDelete(NULL);
}

static esp_err_t save_get_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    char *buf = NULL;
    size_t buf_len;

    // request with params
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = calloc(buf_len, sizeof(char));
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[65] = {0};
            char wifi_ssid[33] = {0};
            char wifi_password[65] = {0};

            wifi_config_t wifi_cfg = {};

            if (httpd_query_key_value(buf, "ssid", param, sizeof(param)) == ESP_OK) {
                url_decode(param);
                ESP_LOGI(TAG, "ssid=%s", param);
                strncpy(wifi_ssid, param, sizeof(wifi_ssid));
            }

            if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK) {
                url_decode(param);
                ESP_LOGI(TAG, "password=%s", param);
                strncpy(wifi_password, param, sizeof(wifi_password));
            }

            if (strlen(wifi_ssid) > 0 && strlen(wifi_password)) {
                ESP_LOGI(TAG, "WiFi settings accepted!");
                strncpy((char *)wifi_cfg.sta.ssid, wifi_ssid, sizeof(wifi_cfg.sta.ssid));
                strncpy((char *)wifi_cfg.sta.password, wifi_password, sizeof(wifi_cfg.sta.password));

                httpd_resp_set_type(req, "text/html");
                if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) == ESP_OK &&
                    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
                    ESP_LOGI(TAG, "WiFi settings applied and stored to flash");

                    ret = httpd_resp_sendstr(req, "ok");

                    xTaskCreate(async_wifi_connect, "app_wifi_connect", 4096, NULL, 5, NULL);

                } else {
                    ESP_LOGE(TAG, "Failed to set WiFi config to flash");
                    ret = httpd_resp_sendstr(req, "Failed to configure WiFi settings");
                }
            } else {
                ESP_LOGW(TAG, "WiFi settings rejected!");
                ret = ESP_ERR_INVALID_ARG;
            }
        } else {
            ret = ESP_ERR_INVALID_ARG;
        }
        free(buf);
    } else {
        ret = ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_ERR_INVALID_ARG) {
        ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "参数错误");
    }

    return ret;
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

static const httpd_uri_t save_action = {
        .uri = "/save",
        .method = HTTP_GET,
        .handler = save_get_handler
};

static const httpd_uri_t wifi_scan_action = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_get_handler
};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    char filename[HTTPD_MAX_URI_LEN+10];
    sprintf(filename, CONFIG_BSP_SPIFFS_MOUNT_POINT "%s", req->uri);

    return send_file_response(req, filename);
}

httpd_handle_t start_captive_portal(void)
{
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
        httpd_register_uri_handler(server, &save_action);
        httpd_register_uri_handler(server, &wifi_scan_action);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

esp_err_t stop_captive_portal(void)
{
    return httpd_stop(server);
}