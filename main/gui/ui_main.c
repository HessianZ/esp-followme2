/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <sys/time.h>
#include <esp_wifi.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lv_symbol_extra_def.h"
#include "app_wifi.h"
#include "ui_main.h"
#include "esp_lvgl_port.h"
#include "bsp/tft-feather.h"
#include "page/page_home.h"
#include "http.h"
#include "page_led.h"

#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8
#define LCD_LEDC_CH            1

static const char *TAG = "ui_main";

LV_FONT_DECLARE(font_icon_16);
LV_FONT_DECLARE(font_OPPOSans_L_16);

static const lv_font_t *main_font = &font_OPPOSans_L_16;

static int g_last_index = -1;
static int g_item_index = 0;
static lv_group_t *g_btn_op_group = NULL;
static button_style_t g_btn_styles;
static lv_obj_t *g_page_body = NULL;

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t g_guisemaphore;
static lv_obj_t *g_status_bar = NULL;
static lv_obj_t *lab_time = NULL;
static lv_obj_t *lab_weather = NULL;
static lv_obj_t *g_lab_wifi = NULL;

static void ui_main_menu(int32_t index_id);

void ui_acquire(void)
{
    bsp_display_lock(0);
}

void ui_release(void)
{
    bsp_display_unlock();
}

static void ui_button_style_init(void)
{
    /*Init the style for the default state*/

    lv_style_init(&g_btn_styles.style);

    lv_style_set_radius(&g_btn_styles.style, 5);

    // lv_style_set_bg_opa(&g_btn_styles.style, LV_OPA_100);
    lv_style_set_bg_color(&g_btn_styles.style, lv_color_make(255, 255, 255));
    // lv_style_set_bg_grad_color(&g_btn_styles.style, lv_color_make(255, 255, 255));
    // lv_style_set_bg_grad_dir(&g_btn_styles.style, LV_GRAD_DIR_VER);

    lv_style_set_border_opa(&g_btn_styles.style, LV_OPA_30);
    lv_style_set_border_width(&g_btn_styles.style, 2);
    lv_style_set_border_color(&g_btn_styles.style, lv_palette_main(LV_PALETTE_GREY));

    lv_style_set_shadow_width(&g_btn_styles.style, 7);
    lv_style_set_shadow_color(&g_btn_styles.style, lv_color_make(0, 0, 0));
    lv_style_set_shadow_ofs_x(&g_btn_styles.style, 0);
    lv_style_set_shadow_ofs_y(&g_btn_styles.style, 0);

    // lv_style_set_pad_all(&g_btn_styles.style, 10);

    // lv_style_set_outline_width(&g_btn_styles.style, 1);
    // lv_style_set_outline_opa(&g_btn_styles.style, LV_OPA_COVER);
    // lv_style_set_outline_color(&g_btn_styles.style, lv_palette_main(LV_PALETTE_RED));


    // lv_style_set_text_color(&g_btn_styles.style, lv_color_white());
    // lv_style_set_pad_all(&g_btn_styles.style, 10);

    /*Init the pressed style*/

    lv_style_init(&g_btn_styles.style_pr);

    lv_style_set_border_opa(&g_btn_styles.style_pr, LV_OPA_40);
    lv_style_set_border_width(&g_btn_styles.style_pr, 2);
    lv_style_set_border_color(&g_btn_styles.style_pr, lv_palette_main(LV_PALETTE_GREY));


    lv_style_init(&g_btn_styles.style_focus);
    lv_style_set_outline_color(&g_btn_styles.style_focus, lv_color_make(255, 0, 0));

    lv_style_init(&g_btn_styles.style_focus_no_outline);
    lv_style_set_outline_width(&g_btn_styles.style_focus_no_outline, 0);
}

button_style_t *ui_button_styles(void)
{
    return &g_btn_styles;
}

lv_group_t *ui_get_btn_op_group(void)
{
    return g_btn_op_group;
}

static void ui_status_bar_set_visible(bool visible)
{
    if (visible) {
        // update all state
        ui_main_status_bar_set_wifi(app_wifi_is_connected());
        lv_obj_clear_flag(g_status_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_status_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *ui_main_get_status_bar(void)
{
    return g_status_bar;
}

void ui_main_status_bar_set_wifi(bool is_connected)
{
    if (g_lab_wifi) {
        lv_label_set_text_static(g_lab_wifi, is_connected ? LV_SYMBOL_WIFI : LV_SYMBOL_EXTRA_WIFI_OFF);
    }
}


enum {
    PAGE_HOME,
    PAGE_LED,
//    PAGE_ABOUT_US_INDEX,
    PAGE_COUNT
} page_index_t;

typedef struct {
    void (*render)(lv_obj_t *);
    void (*destroy)();
} PageDef;

static PageDef pages[] = {
        {page_home_render, page_home_destroy},
        {page_led_render, page_led_destroy},
};

static lv_obj_t *g_container = NULL;
static lv_obj_t *g_led_item[PAGE_COUNT];


void menu_new_item_select(int new_index)
{
    lv_led_off(g_led_item[g_item_index]);
    g_item_index = new_index % PAGE_COUNT;

    if (g_last_index == g_item_index) {
        return;
    }

    if (g_last_index >= 0) {
        pages[g_last_index].destroy();
    }

    g_last_index = g_item_index;

    ESP_LOGI(TAG, "slected:%d", g_item_index);
    lv_led_on(g_led_item[g_item_index]);

    // 清空容器
    lv_obj_clean(g_container);

    pages[g_item_index].render(g_container);
}

static void ui_main_menu(int32_t index_id)
{
    if (!g_page_body) {
        // 菜单页面（功能选择）
        g_page_body = lv_obj_create(lv_scr_act());
        lv_obj_set_size(g_page_body, lv_obj_get_width(lv_obj_get_parent(g_page_body)), lv_obj_get_height(lv_obj_get_parent(g_page_body)) - lv_obj_get_height(ui_main_get_status_bar()));
        lv_obj_set_style_border_width(g_page_body, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_page_body, lv_obj_get_style_bg_color(lv_scr_act(), LV_STATE_DEFAULT), LV_PART_MAIN);
        lv_obj_clear_flag(g_page_body, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align_to(g_page_body, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
    }
    ui_status_bar_set_visible(true);

    // 首页容器
    g_container = lv_obj_create(g_page_body);
    lv_obj_set_size(g_container, 220, 100);
    lv_obj_set_style_bg_color(g_container, lv_color_make(0x33, 0x33, 0x33), LV_PART_MAIN);
    lv_obj_clear_flag(g_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_container, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_container, 0, LV_STATE_DEFAULT);
    lv_obj_align(g_container, LV_ALIGN_TOP_MID, 0, -12);

    int g_led_count = sizeof(g_led_item) / sizeof(g_led_item[0]);
    short g_led_size = 5;
    short gap = 10;
    short outer_width =  g_led_size * g_led_count + gap * (g_led_count - 1);
    short half_width =  outer_width / 2;
    short offset_x = (g_led_size) / 2;
    for (size_t i = 0; i < g_led_count; i++) {
        if (NULL == g_led_item[i]) {
            g_led_item[i] = lv_led_create(g_page_body);
            lv_led_set_color(g_led_item[i], lv_color_make(0x5D, 0xE8, 0xE6));
        } else {
            lv_obj_clear_flag(g_led_item[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_led_off(g_led_item[i]);
        lv_obj_set_size(g_led_item[i], g_led_size, g_led_size);
        int g_led_item_off_x = i / (g_led_count-1.0) * outer_width - half_width;
        lv_obj_align_to(g_led_item[i], g_page_body, LV_ALIGN_BOTTOM_MID, g_led_item_off_x - offset_x, 0);
    }
    lv_led_on(g_led_item[index_id]);

    menu_new_item_select(index_id);
}

static void ui_after_boot(void)
{
    // TODO: 播放启动动画

    ui_main_menu(g_item_index);
}

static void clock_run_cb(lv_timer_t *timer)
{
    if (!app_wifi_is_connected()) {
        return;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // time did not set
    if (timeinfo.tm_year < (2016 - 1900)) {
        return;
    }

    lv_label_set_text_fmt(lab_time, "%02u-%02u %02u:%02u", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
    lv_obj_align_to(lab_weather, lab_time, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
}


static void weather_run_cb(lv_timer_t *timer)
{
    if (!app_wifi_is_connected()) {
        return;
    }

    if (timer->period == 1000) {
        lv_timer_set_period(timer, 120 * 1000);
    }

    weather_result_t result;
    esp_err_t ret = http_get_weather(&result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http_get_weather failed");
        return;
    }

    lv_label_set_text_fmt(lab_weather, "%s%s%s℃", result.city, result.weather, result.temp);
    lv_obj_align_to(lab_weather, lab_time, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
}

static void ui_create_status_bar()
{
    g_status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_status_bar, lv_obj_get_width(lv_obj_get_parent(g_status_bar)), 30);
    lv_obj_clear_flag(g_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_status_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_status_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_status_bar, 0, LV_PART_MAIN);
    lv_obj_align(g_status_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_color_t text_color = lv_color_make(0x99,0x99, 0x99);

    lab_time = lv_label_create(g_status_bar);
    lv_obj_set_style_text_font(lab_time, main_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_time, text_color, LV_STATE_DEFAULT);
    lv_label_set_text_static(lab_time, "----- --:--");
    lv_obj_align(lab_time, LV_ALIGN_LEFT_MID, 0, 0);
    lv_timer_t *timer = lv_timer_create(clock_run_cb, 1000, (void *) lab_time);
    clock_run_cb(timer);

    lab_weather = lv_label_create(g_status_bar);
    lv_obj_set_style_text_font(lab_weather, main_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_weather, text_color, LV_STATE_DEFAULT);
    lv_label_set_text_static(lab_weather, "地区 天气 --℃");
//    lv_obj_align(lab_time, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_align_to(lab_weather, lab_time, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_timer_t *timer2 = lv_timer_create(weather_run_cb, 1000, (void *) lab_weather);
    weather_run_cb(timer2);

    g_lab_wifi = lv_label_create(g_status_bar);
    lv_obj_set_size(g_lab_wifi, 30, 30);
//    lv_obj_set_pos(g_lab_wifi, lv_obj_get_width(g_status_bar) - 30, 30);
    lv_obj_align(g_lab_wifi, LV_ALIGN_RIGHT_MID, 10, 5);
    lv_obj_set_style_text_font(g_lab_wifi, &font_icon_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lab_wifi, text_color, LV_STATE_DEFAULT);
    lv_obj_add_flag(g_lab_wifi, LV_OBJ_FLAG_FLOATING);
//    lv_obj_align_to(g_lab_wifi, g_status_bar, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    ui_status_bar_set_visible(0);
}

static void button_single_click_cb(void *arg,void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");

    menu_new_item_select(g_item_index + 1);
}

static void ui_reset_wifi(void)
{
    lv_obj_clean(g_container);

    lv_obj_t *lab_text = lv_label_create(g_container);

    lv_label_set_text_static(lab_text, "正在重置系统...");
    lv_obj_set_style_text_font(lab_text, main_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab_text, lv_color_make(0xDC, 0x64, 0x64), LV_STATE_DEFAULT);

    lv_obj_align(g_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(lab_text, LV_ALIGN_CENTER, 0, 0);

    esp_wifi_restore();
    esp_restart();
}

static void button_long_click_start_cb(void *arg,void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_LONG_CLICK_START");

    switch (g_item_index) {
        case PAGE_HOME:
            ESP_LOGW(TAG, "WiFi credential reset");
            ui_reset_wifi();
            break;
        case PAGE_LED:
            page_led_next_color();
            break;
    }
}

esp_err_t ui_main_start(void)
{
    ui_acquire();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
    ui_button_style_init();

    lv_indev_t *indev = lv_indev_get_next(NULL);

    lv_indev_type_t indev_type = lv_indev_get_type(indev);

    if ((indev_type == LV_INDEV_TYPE_KEYPAD) || indev_type == LV_INDEV_TYPE_ENCODER) {
        ESP_LOGI(TAG, "Input device type is keypad");
        g_btn_op_group = lv_group_create();
        lv_indev_set_group(indev, g_btn_op_group);
    } else if (indev_type == LV_INDEV_TYPE_BUTTON) {
        ESP_LOGI(TAG, "Input device type have button");
    } else if (indev_type == LV_INDEV_TYPE_POINTER) {
        ESP_LOGI(TAG, "Input device type have pointer");
    }

    // Create status bar
    ui_create_status_bar();
    // status bar end

    // create gpio button
    button_config_t gpio_btn_cfg = {
            .type = BUTTON_TYPE_GPIO,
            .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
            .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
            .gpio_button_config = {
                    .gpio_num = GPIO_NUM_0,
                    .active_level = 0,
            },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if(NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }


    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb,NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, button_long_click_start_cb,NULL);

    ui_after_boot();

    ui_release();
    return ESP_OK;
}
