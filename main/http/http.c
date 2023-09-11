#include <esp_types.h>
#include <string.h>
#include <inttypes.h>
#include <esp_http_client.h>
#include <sys/param.h>
#include "esp_log.h"
#include "nvs.h"

#include "esp_tls.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"
#include "http.h"
#include "json_parser.h"

#define BILIBILI_UID "10442962"
#define BILIBILI_FANS_URL "https://api.bilibili.com/x/relation/stat?vmid=" BILIBILI_UID "&jsonp=jsonp"

// 101280401 = 梅州
#define CITY_CODE "101280401"
#define WEATHER_URL "http://d1.weather.com.cn/weather_index/" CITY_CODE ".html"
#define WEATHER_REFERER "http://www.weather.com.cn/"

#define MAX_HTTP_OUTPUT_BUFFER 5192

static const char *TAG = "http";


#define USER_AGENT "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.37"


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    const int buffer_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(buffer_len);
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (buffer_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

esp_err_t http_get_weather(weather_result_t *result)
{
    ESP_LOGI(TAG, "Start http_get_weather ...");

    char *response = calloc(8192, sizeof(char));
    esp_http_client_config_t config = {
            .url = WEATHER_URL,
            .event_handler = _http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .buffer_size = 8192 * sizeof(char),
            .user_data = response,
            .user_agent = USER_AGENT
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Referer", WEATHER_REFERER);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int64_t len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                 esp_http_client_get_status_code(client),
                 len
        );
        response[len] = '\0';
        ESP_LOGI(TAG, "Response: %s", response);
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    char *json = NULL;
    jparse_ctx_t *jctx = NULL;
    char* pStart = strstr(response, "var dataSK =");
    if (pStart == NULL) {
        ESP_LOGE(TAG, "Parse weather failed: %s", response);
        goto exception;
    }

    pStart += 12;
    char* pEnd = strstr(pStart, "};");
    pEnd += 1;
    int len = pEnd - pStart;
    json = malloc(len + 1);
    memcpy(json, pStart, len);
    json[len] = '\0';
    free(response);
    response = NULL;

    jctx = (jparse_ctx_t *)malloc(sizeof(jparse_ctx_t));
    int ret = json_parse_start(jctx, json, strlen(json));
    if (ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_parse_start failed\n");
        goto exception;
    }

    if (json_obj_get_string(jctx, "cityname", &result->city, 10) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: weather\n");
        goto exception;
    }
    if (json_obj_get_string(jctx, "temp", &result->temp, 5) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: weather\n");
        goto exception;
    }
    if (json_obj_get_string(jctx, "SD", &result->humi, 5) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: humi\n");
        goto exception;
    }
    if (json_obj_get_string(jctx, "weather", &result->weather, 12) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: weather\n");
        goto exception;
    }
    if (json_obj_get_string(jctx, "WD", &result->wind, 12) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: wind\n");
        goto exception;
    }
    if (json_obj_get_string(jctx, "WS", &result->windSpeed, 10) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: windSpeed\n");
        goto exception;
    }
    strstr(result->humi, "%")[0] = '\0';
    strstr(result->windSpeed, "级")[0] = '\0';

    json_parse_end(jctx);
    free(json);
    json = NULL;


    return ESP_OK;

    exception:
    ESP_LOGW(TAG, "http_get_weather exception");
    if (response != NULL) {
        free(response);
    }
    if (jctx != NULL) {
        json_parse_end(jctx);
    }
    if (json != NULL) {
        free(json);
    }

    return ESP_FAIL;
}

int http_get_bilibili_fans()
{
    int fans_num = -1;
    ESP_LOGI(TAG, "Start http_get_bilibili_fans");

    char *json = malloc(2048);
    esp_http_client_config_t config = {
            .url = BILIBILI_FANS_URL,
            .event_handler = _http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .user_data = json,
            .user_agent = USER_AGENT
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client)
        );
        strstr(json, "}}")[2] = '\0';
        ESP_LOGI(TAG, "Response: %s", json);
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    jparse_ctx_t *jctx = (jparse_ctx_t *)malloc(sizeof(jparse_ctx_t));
    int ret = json_parse_start(jctx, json, strlen(json));
    if (ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_parse_start failed\n");
        return -1;
    }

    if (json_obj_get_object(jctx, "data")!= OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_object failed: data\n");
        return -1;
    }

    if (json_obj_get_int(jctx, "follower", &fans_num) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_obj_get_int failed: follower\n");
        return -1;
    }

    ESP_LOGI(TAG, "follower: %d\n", fans_num);

    if (jctx != NULL) {
        json_parse_end(jctx);
    }
    if (json != NULL) {
        free(json);
    }

    ESP_LOGI(TAG, "BILIBILI_FANS: %d", fans_num);

    return fans_num;
}
