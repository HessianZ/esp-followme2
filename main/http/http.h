//
// Created by Hessian on 2023/7/19.
//

#ifndef FACTORY_DEMO_HTTP_H
#define FACTORY_DEMO_HTTP_H

typedef struct {
    char city[10];
    char temp[5];
    char weather[12];
    char humi[5];
    char wind[12];
    char windSpeed[10];
} weather_result_t;


int http_get_bilibili_fans();
esp_err_t http_get_weather(weather_result_t *result);


#endif //FACTORY_DEMO_HTTP_H
