//
// Created by Hessian on 2023/9/12.
//

#ifndef ESP_FOLLOWME2_PAGE_LED_H
#define ESP_FOLLOWME2_PAGE_LED_H

#include "lvgl.h"

void page_led_render(lv_obj_t *parent);
void page_led_destroy();
void page_led_next_color();

#endif //ESP_FOLLOWME2_PAGE_LED_H
