/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <sys/time.h>
#include <driver/ledc.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "lv_symbol_extra_def.h"
#include "app_wifi.h"
#include "ui_main.h"
//#include "ui_about_us.h"
#include "ui_net_config.h"
#include "esp_lvgl_port.h"
#include "bsp/tft-feather.h"

#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8
#define LCD_LEDC_CH            1


static const char *TAG = "ui_main";

LV_FONT_DECLARE(font_icon_16);
//LV_FONT_DECLARE(font_HarmonyOS_Sans_Light_16);
LV_FONT_DECLARE(font_cn_gb1_16);

static const lv_font_t *main_font = &font_cn_gb1_16;

static int g_item_index = 0;
static lv_group_t *g_btn_op_group = NULL;
static button_style_t g_btn_styles;
static lv_obj_t *g_page_menu = NULL;

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t g_guisemaphore;
static lv_obj_t *g_lab_wifi = NULL;
static lv_obj_t *g_status_bar = NULL;

static void ui_main_menu(int32_t index_id);
static void ui_led_set_visible(bool visible);

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

static void hint_end_cb(void)
{
    ESP_LOGI(TAG, "hint end");
    ui_main_menu(g_item_index);
}

static void about_us_end_cb(void)
{
    ESP_LOGI(TAG, "about_us end");
    ui_main_menu(g_item_index);
}

static void book_end_cb(void)
{
    ESP_LOGI(TAG, "book end");
    ui_main_menu(g_item_index);
}

static void net_end_cb(void)
{
    ESP_LOGI(TAG, "net end");
    ui_main_menu(g_item_index);
}

static void ui_help(void)
{
//    ui_hint_start(hint_end_cb);
}

typedef struct {
    char *name;
    void *img_src;
} item_desc_t;

LV_IMG_DECLARE(icon_about_us)
LV_IMG_DECLARE(icon_network)
LV_IMG_DECLARE(icon_book)
//LV_IMG_DECLARE(icon_bilibili)

static item_desc_t item[] = {
    { .name = "书籍",      .img_src = (void *) &icon_book},
    { .name = "网络设置",   .img_src = (void *) &icon_network},
    { .name = "关于",      .img_src = (void *) &icon_about_us},
};

static enum page_index_t {
    PAGE_BOOK_INDEX,
    PAGE_NET_CONFIG_INDEX,
    PAGE_ABOUT_US_INDEX,
};

static lv_obj_t *g_img_btn, *g_img_item = NULL;
static lv_obj_t *g_lab_item = NULL;
static lv_obj_t *g_led_item[3];
static size_t g_item_size = sizeof(item) / sizeof(item[0]);

static lv_obj_t *g_focus_last_obj = NULL;
static lv_obj_t *g_group_list[3] = {0};

static uint32_t menu_get_num_offset(uint32_t focus, int32_t max, int32_t offset)
{
    if (focus >= max) {
        ESP_LOGI(TAG, "[ERROR] focus should less than max");
        return focus;
    }

    uint32_t i;
    if (offset >= 0) {
        i = (focus + offset) % max;
    } else {
        offset = max + (offset % max);
        i = (focus + offset) % max;
    }
    return i;
}

static int8_t menu_direct_probe(lv_obj_t *focus_obj)
{
    int8_t direct;
    uint32_t index_max_sz, index_focus, index_prev;

    index_focus = 0;
    index_prev = 0;
    index_max_sz = sizeof(g_group_list)/ sizeof(g_group_list[0]);

    for(int i = 0; i< index_max_sz; i++){
        if(focus_obj == g_group_list[i]){
            index_focus = i;
        }
        if(g_focus_last_obj == g_group_list[i]){
            index_prev = i;
        }
    }

    if(NULL == g_focus_last_obj){
        direct = 0;
    } else if(index_focus == menu_get_num_offset(index_prev, index_max_sz, 1)){
        direct = 1;
    } else if(index_focus == menu_get_num_offset(index_prev, index_max_sz, -1)){
        direct = -1;
    } else {
        direct = 0;
    }

    g_focus_last_obj = focus_obj;
    return direct;
}

void menu_new_item_select(lv_obj_t *obj)
{
    int8_t direct = menu_direct_probe(obj);
    g_item_index = menu_get_num_offset(g_item_index, g_item_size, direct);
    ESP_LOGI(TAG, "slected:%d, direct:%d", g_item_index, direct);

    lv_led_on(g_led_item[g_item_index]);
    lv_img_set_src(g_img_item, item[g_item_index].img_src);
    lv_label_set_text_static(g_lab_item, item[g_item_index].name);
}

static void menu_prev_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_user_data(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_led_off(g_led_item[g_item_index]);
        menu_new_item_select(obj);
    } else if (LV_EVENT_CLICKED == code) {
        lv_event_send(g_img_btn, LV_EVENT_CLICKED, g_img_btn);

    }
}

static void menu_next_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_user_data(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_led_off(g_led_item[g_item_index]);
        menu_new_item_select(obj);
    } else if (LV_EVENT_CLICKED == code) {
        lv_event_send(g_img_btn, LV_EVENT_CLICKED, g_img_btn);
    }
}

static void menu_enter_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_user_data(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_led_off(g_led_item[g_item_index]);
        menu_new_item_select(obj);
    } else if (LV_EVENT_CLICKED == code) {
        lv_obj_t * menu_btn_parent = lv_obj_get_parent(obj);
        ESP_LOGI(TAG, "menu click, item index = %d", g_item_index);
        if (ui_get_btn_op_group()) {
            lv_group_remove_all_objs(ui_get_btn_op_group());
        }
        ui_btn_rm_all_cb();
        ui_led_set_visible(false);
        lv_obj_del(menu_btn_parent);
        g_focus_last_obj = NULL;

        switch (g_item_index) {
        case PAGE_BOOK_INDEX:
            ui_status_bar_set_visible(true);
//            ui_book_start(book_end_cb);
            break;
        case PAGE_NET_CONFIG_INDEX:
            ui_status_bar_set_visible(true);
            ui_net_config_start(net_end_cb);
            break;
        case PAGE_ABOUT_US_INDEX:
            ui_status_bar_set_visible(true);
//            ui_about_us_start(about_us_end_cb);
            break;
        default:
            break;
        }
    }
}

static void ui_main_menu(int32_t index_id)
{
    if (!g_page_menu) {
        g_page_menu = lv_obj_create(lv_scr_act());
        lv_obj_set_size(g_page_menu, lv_obj_get_width(lv_obj_get_parent(g_page_menu)), lv_obj_get_height(lv_obj_get_parent(g_page_menu)) - lv_obj_get_height(ui_main_get_status_bar()));
        lv_obj_set_style_border_width(g_page_menu, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_page_menu, lv_obj_get_style_bg_color(lv_scr_act(), LV_STATE_DEFAULT), LV_PART_MAIN);
        lv_obj_clear_flag(g_page_menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align_to(g_page_menu, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
    }
    ui_status_bar_set_visible(true);

    lv_obj_t *obj = lv_obj_create(g_page_menu);
    lv_obj_set_size(obj, 290, 174);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, -10);

    g_img_btn = lv_btn_create(obj);
    lv_obj_set_size(g_img_btn, 80, 80);
    lv_obj_add_style(g_img_btn, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(g_img_btn, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(g_img_btn, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(g_img_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(g_img_btn, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_img_btn, 15, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_x(g_img_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(g_img_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(g_img_btn, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_radius(g_img_btn, 40, LV_PART_MAIN);
    lv_obj_align(g_img_btn, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_event_cb(g_img_btn, menu_enter_cb, LV_EVENT_ALL, g_img_btn);

    g_img_item = lv_img_create(g_img_btn);
    lv_img_set_src(g_img_item, item[index_id].img_src);
    lv_obj_center(g_img_item);

    g_lab_item = lv_label_create(obj);
    lv_label_set_text_static(g_lab_item, item[index_id].name);
    lv_obj_set_style_text_font(g_lab_item, main_font, LV_PART_MAIN);
    lv_obj_align(g_lab_item, LV_ALIGN_CENTER, 0, 60);

    int g_led_count = sizeof(g_led_item) / sizeof(g_led_item[0]);
    short g_led_size = 5;
    short gap = 10;
    short outer_width =  g_led_size * g_led_count + gap * (g_led_count - 1);
    short half_width =  outer_width / 2;
    short offset_x = (g_led_size) / 2;
    for (size_t i = 0; i < g_led_count; i++) {
        if (NULL == g_led_item[i]) {
            g_led_item[i] = lv_led_create(g_page_menu);
        } else {
            lv_obj_clear_flag(g_led_item[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_led_off(g_led_item[i]);
        lv_obj_set_size(g_led_item[i], g_led_size, g_led_size);
        int g_led_item_off_x = i / (g_led_count-1.0) * outer_width - half_width;
        lv_obj_align_to(g_led_item[i], g_page_menu, LV_ALIGN_BOTTOM_MID, g_led_item_off_x - offset_x, 0);
    }
    lv_led_on(g_led_item[index_id]);

    lv_obj_t *btn_prev = lv_btn_create(obj);
    lv_obj_add_style(btn_prev, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_prev, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_prev, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);

    lv_obj_set_size(btn_prev, 40, 40);
    lv_obj_set_style_bg_color(btn_prev, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn_prev, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_prev, 15, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_prev, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_x(btn_prev, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(btn_prev, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_prev, 20, LV_PART_MAIN);
    lv_obj_align_to(btn_prev, obj, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(btn_prev);
    lv_label_set_text_static(label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(label, lv_color_make(5, 5, 5), LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn_prev, menu_prev_cb, LV_EVENT_ALL, btn_prev);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_prev);
    }
    g_group_list[0] = btn_prev;

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), g_img_btn);
    }
    g_group_list[1] = g_img_btn;

    lv_obj_t *btn_next = lv_btn_create(obj);
    lv_obj_add_style(btn_next, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_next, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_next, &ui_button_styles()->style_focus_no_outline, LV_STATE_FOCUSED);

    lv_obj_set_size(btn_next, 40, 40);
    lv_obj_set_style_bg_color(btn_next, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn_next, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_next, 15, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_next, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_x(btn_next, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(btn_next, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_next, 20, LV_PART_MAIN);
    lv_obj_align_to(btn_next, obj, LV_ALIGN_RIGHT_MID, 0, 0);
    label = lv_label_create(btn_next);
    lv_label_set_text_static(label, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(label, lv_color_make(5, 5, 5), LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn_next, menu_next_cb, LV_EVENT_ALL, btn_next);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_next);
    }
    g_group_list[2] = btn_next;
}

static void ui_after_boot(void)
{
    ui_main_menu(g_item_index);
}

static void clock_run_cb(lv_timer_t *timer)
{
    lv_obj_t *lab_time = (lv_obj_t *) timer->user_data;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    lv_label_set_text_fmt(lab_time, "%02u-%02u %02u:%02u", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
}


static void weather_run_cb(lv_timer_t *timer)
{
    /*
    lv_obj_t *lab_weather = (lv_obj_t *) timer->user_data;
    weather_result_t result;
    esp_err_t ret = http_get_weather(&result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "http_get_weather failed");
        return;
    }

    lv_label_set_text_fmt(lab_weather, "%s %s %s℃", result.city, result.weather, result.temp);
     */
}

void weather_run_task(void *timer2)
{
    while (!app_wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    weather_run_cb((lv_timer_t *)timer2);
    vTaskDelete(NULL);
}

esp_err_t ui_main_start(void)
{
    ui_acquire();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(237, 238, 239), LV_STATE_DEFAULT);
    ui_button_style_init();

    lv_indev_t *indev = lv_indev_get_next(NULL);

    lv_indev_type_t indev_type = lv_indev_get_type(indev);

    if ((indev_type == LV_INDEV_TYPE_KEYPAD) || \
            indev_type == LV_INDEV_TYPE_ENCODER) {
        ESP_LOGI(TAG, "Input device type is keypad");
        g_btn_op_group = lv_group_create();
        lv_indev_set_group(indev, g_btn_op_group);
    } else if (indev_type == LV_INDEV_TYPE_BUTTON) {
        ESP_LOGI(TAG, "Input device type have button");
    } else if (indev_type == LV_INDEV_TYPE_POINTER) {
        ESP_LOGI(TAG, "Input device type have pointer");
    }

    // Create status bar
    g_status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_status_bar, lv_obj_get_width(lv_obj_get_parent(g_status_bar)), 36);
    lv_obj_clear_flag(g_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_status_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_status_bar, lv_obj_get_style_bg_color(lv_scr_act(), LV_STATE_DEFAULT), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_status_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_status_bar, 0, LV_PART_MAIN);
    lv_obj_align(g_status_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lab_time = lv_label_create(g_status_bar);
    lv_label_set_text_static(lab_time, "12-12 23:59");
    lv_obj_align(lab_time, LV_ALIGN_LEFT_MID, 0, 0);
    lv_timer_t *timer = lv_timer_create(clock_run_cb, 1000, (void *) lab_time);
    clock_run_cb(timer);

    lv_obj_t *lab_weather = lv_label_create(g_status_bar);
    lv_obj_set_style_text_font(lab_weather, main_font, LV_PART_MAIN);
    lv_label_set_text_static(lab_weather, "梅州 多云 25℃");
//    lv_obj_align(lab_time, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_align_to(lab_weather, lab_time, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_timer_t *timer2 = lv_timer_create(weather_run_cb, 120*1000, (void *) lab_weather);
    xTaskCreate(weather_run_task, "weather_run_task", 1024*4, timer2, 5, NULL);

    g_lab_wifi = lv_label_create(g_status_bar);
    lv_obj_align_to(g_lab_wifi, lab_weather, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    ui_status_bar_set_visible(0);

    ui_after_boot();

    ui_release();
    return ESP_OK;
}

/* **************** MISC FUNCTION **************** */
static void ui_led_set_visible(bool visible)
{
    for (size_t i = 0; i < sizeof(g_led_item) / sizeof(g_led_item[0]); i++) {
        if (NULL != g_led_item[i]) {
            if (visible) {
                lv_obj_clear_flag(g_led_item[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(g_led_item[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void ui_btn_rm_all_cb(void)
{
}
